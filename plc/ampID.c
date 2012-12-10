/*====================================================================*
 *   
 *   Copyright (c) 2011 by Qualcomm Atheros.
 *   
 *   Permission to use, copy, modify, and/or distribute this software 
 *   for any purpose with or without fee is hereby granted, provided 
 *   that the above copyright notice and this permission notice appear 
 *   in all copies.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL 
 *   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL  
 *   THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR 
 *   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 *   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
 *   NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 *   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *   
 *--------------------------------------------------------------------*/

/*====================================================================*
 *
 *   ampID.c - Atheros Powerline Device Identity 
 *
 *.  Qualcomm Atheros HomePlug AV Powerline Toolkit
 *:  Published 2009-2011 by Qualcomm Atheros. ALL RIGHTS RESERVED
 *;  For demonstration and evaluation only. Not for production use
 *
 *   Contributor(s):
 *      Charles Maier <cmaier@qualcomm.com>
 *
 *--------------------------------------------------------------------*/


/*====================================================================*"
 *   system header files;
 *--------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>

/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "../tools/getoptv.h"
#include "../tools/putoptv.h"
#include "../tools/memory.h"
#include "../tools/number.h"
#include "../tools/symbol.h"
#include "../tools/types.h"
#include "../tools/flags.h"
#include "../tools/files.h"
#include "../tools/error.h"
#include "../plc/plc.h"
#include "../ram/nvram.h"
#include "../ram/sdram.h"
#include "../nvm/nvm.h"
#include "../pib/pib.h"
#include "../mme/mme.h"

/*====================================================================*
 *   custom source files;
 *--------------------------------------------------------------------*/

#ifndef MAKEFILE
#include "../tools/getoptv.c"
#include "../tools/putoptv.c"
#include "../tools/version.c"
#include "../tools/uintspec.c"
#include "../tools/hexencode.c"
#include "../tools/hexdecode.c"
#include "../tools/todigit.c"
#include "../tools/hexdump.c"
#include "../tools/hexout.c"
#include "../tools/error.c"
#include "../tools/synonym.c"
#endif

#ifndef MAKEFILE
#include "../mme/EthernetHeader.c"
#include "../mme/QualcommHeader.c"
#include "../mme/UnwantedMessage.c"
#include "../plc/Display.c"
#include "../plc/Devices.c"
#endif

#ifndef MAKEFILE
#include "../ether/openchannel.c"
#include "../ether/closechannel.c"
#include "../ether/readpacket.c"
#include "../ether/sendpacket.c"
#include "../ether/channel.c"
#endif

#ifndef MAKEFILE
#include "../mme/MMECode.c"
#endif

/*====================================================================*
 *   program constants;
 *--------------------------------------------------------------------*/

#define INT6KID_DAK 0
#define INT6KID_NMK 1
#define INT6KID_MAC 2
#define INT6KID_MFG 3
#define INT6KID_USR 4
#define INT6KID_NET 5

/*====================================================================*
 *
 *   void ReadKey1 (struct channel * channel, unsigned c, int key);
 *   
 *   read the first block of the PIB from a device then echo one of 
 *   several parameters on stdout as a string; program output can be 
 *   used in scripts to define variables or compare strings;
 *
 *   this function is an abridged version of ReadParameters(); it reads only
 *   the first 1024 bytes of the PIB then stops; most parameters of
 *   general interest occur in that block;
 *   
 *.  Qualcomm Atheros HomePlug AV Powerline Toolkit
 *:  Published 2009-2011 by Qualcomm Atheros. ALL RIGHTS RESERVED
 *;  For demonstration and evaluation only. Not for production use
 *
 *--------------------------------------------------------------------*/

static void ReadKey1 (struct channel * channel, unsigned c, int key) 

{
	struct message message;
	static signed count = 0;
	signed packetsize;

#ifndef __GNUC__
#pragma pack (push,1)
#endif

	struct __packed vs_rd_mod_request 
	{
		struct ethernet_std ethernet;
		struct qualcomm_std qualcomm;
		uint8_t MODULEID;
		uint8_t RESERVED;
		uint16_t MLENGTH;
		uint32_t MOFFSET;
		uint8_t DAK [16];
	}
	* request = (struct vs_rd_mod_request *)(&message);
	struct __packed vs_rd_mod_confirm 
	{
		struct ethernet_std ethernet;
		struct qualcomm_std qualcomm;
		uint8_t MSTATUS;
		uint8_t RESERVED1 [3];
		uint8_t MODULEID;
		uint8_t RESERVED2;
		uint16_t MLENGTH;
		uint32_t MOFFSET;
		uint32_t MCHKSUM;
		struct simple_pib pib;
	}
	* confirm = (struct vs_rd_mod_confirm *)(&message);

#ifndef __GNUC__
#pragma pack (pop)
#endif

	memset (&message, 0, sizeof (message));
	EthernetHeader (&message.ethernet, channel->peer, channel->host, HOMEPLUG_MTYPE);
	QualcommHeader (&message.qualcomm, 0, (VS_RD_MOD | MMTYPE_REQ));
	request->MODULEID = VS_MODULE_PIB;
	request->MLENGTH = HTOLE16 (PLC_RECORD_SIZE);
	request->MOFFSET = HTOLE32 (0);
	if (sendpacket (channel, &message, (ETHER_MIN_LEN - ETHER_CRC_LEN)) < 0) 
	{
		error (1, errno, CHANNEL_CANTSEND);
	}
	while ((packetsize = readpacket (channel, &message, sizeof (message))) > 0) 
	{
		if (UnwantedMessage (&message, packetsize, 0, (VS_RD_MOD | MMTYPE_CNF))) 
		{
			continue;
		}
		if (confirm->MSTATUS) 
		{
			error (0, 0, "%s (%0X): ", MMECode (confirm->qualcomm.MMTYPE, confirm->MSTATUS), confirm->MSTATUS);
			continue;
		}
		if (count++ > 0) 
		{
			putc (c, stdout);
		}
		if (key == INT6KID_MAC) 
		{
			hexout (confirm->pib.MAC, sizeof (confirm->pib.MAC), HEX_EXTENDER, 0, stdout);
			continue;
		}
		if (key == INT6KID_DAK) 
		{
			hexout (confirm->pib.DAK, sizeof (confirm->pib.DAK), HEX_EXTENDER, 0, stdout);
			continue;
		}
		if (key == INT6KID_NMK) 
		{
			hexout (confirm->pib.NMK, sizeof (confirm->pib.NMK), HEX_EXTENDER, 0, stdout);
			continue;
		}
		if (key == INT6KID_MFG) 
		{
			confirm->pib.MFG [PIB_HFID_LEN - 1] = (char)(0);
			printf ("%s", confirm->pib.MFG);
			continue;
		}
		if (key == INT6KID_USR) 
		{
			confirm->pib.USR [PIB_HFID_LEN - 1] = (char)(0);
			printf ("%s", confirm->pib.USR);
			continue;
		}
		if (key == INT6KID_NET) 
		{
			confirm->pib.NET [PIB_HFID_LEN - 1] = (char)(0);
			printf ("%s", confirm->pib.NET);
			continue;
		}
	}
	if (packetsize < 0) 
	{
		error (1, errno, CHANNEL_CANTREAD);
	}
	return;
}


/*====================================================================*
 *
 *   int main (int argc, char const * argv []);
 *   
 *   
 *.  Qualcomm Atheros HomePlug AV Powerline Toolkit
 *:  Published 2009-2011 by Qualcomm Atheros. ALL RIGHTS RESERVED
 *;  For demonstration and evaluation only. Not for production use
 *
 *--------------------------------------------------------------------*/

int main (int argc, char const * argv []) 

{
	extern struct channel channel;
	static char const * optv [] = 
	{
		"Ac:Dei:MnNqSUv",
		"device",
		"Qualcomm Atheros Powerline Device Identity",
		"A\tEthernet address (MAC)",
		"c c\tcharacter delimiter is (c)",
		"D\tDevice Access Key (DAK)",
		"e\tredirect stderr to stdout",

#if defined (WINPCAP) || defined (LIBPCAP)

		"i n\thost interface is (n) [" LITERAL (CHANNEL_ETHNUMBER) "]",

#else

		"i s\thost interface is (s) [" LITERAL (CHANNEL_ETHDEVICE) "]",

#endif

		"M\tNetwork Membership Key (NMK)",
		"n\tappend newline on output",
		"N\tnetwork HFID",
		"q\tquiet mode",
		"S\tmanufacturer HFID",
		"U\tuser HFID",
		"v\tverbose mode",
		(char const *) (0)
	};
	signed newline = '\n';
	signed key = INT6KID_DAK;
	flag_t flags = (flag_t)(0);
	signed c;
	if (getenv (PLCDEVICE)) 
	{

#if defined (WINPCAP) || defined (LIBPCAP)

		channel.ifindex = atoi (getenv (PLCDEVICE));

#else

		channel.ifname = strdup (getenv (PLCDEVICE));

#endif

	}
	optind = 1;
	while ((c = getoptv (argc, argv, optv)) != -1) 
	{
		switch (c) 
		{
		case 'A':
			key = INT6KID_MAC;
			break;
		case 'c':
			newline = * optarg;
			break;
		case 'D':
			key = INT6KID_DAK;
			break;
		case 'e':
			dup2 (STDOUT_FILENO, STDERR_FILENO);
			break;
		case 'i':

#if defined (WINPCAP) || defined (LIBPCAP)

			channel.ifindex = atoi (optarg);

#else

			channel.ifname = optarg;

#endif

			break;
		case 'M':
			key = INT6KID_NMK;
			break;
		case 'n':
			_setbits (flags, PLC_NEWLINE);
			break;
		case 'N':
			key = INT6KID_NET;
			break;
		case 'q':
			_setbits (channel.flags, CHANNEL_SILENCE);
			_setbits (flags, PLC_SILENCE);
			break;
		case 'S':
			key = INT6KID_MFG;
			break;
		case 'U':
			key = INT6KID_USR;
			break;
		case 'v':
			_setbits (channel.flags, CHANNEL_VERBOSE);
			_setbits (flags, PLC_VERBOSE);
			break;
		default:
			break;
		}
	}
	argc -= optind;
	argv += optind;
	openchannel (&channel);
	if (!argc) 
	{
		ReadKey1 (&channel, newline, key);
		if (_anyset (flags, PLC_NEWLINE)) 
		{
			putc (newline, stdout);
		}
	}
	while ((argc) && (* argv)) 
	{
		if (!hexencode (channel.peer, sizeof (channel.peer), synonym (* argv, devices, SIZEOF (devices)))) 
		{
			error (1, errno, PLC_BAD_MAC, * argv);
		}
		ReadKey1 (&channel, newline, key);
		if (_anyset (flags, PLC_NEWLINE)) 
		{
			putc (newline, stdout);
		}
		argv++;
		argc--;
	}
	closechannel (&channel);
	return (0);
}


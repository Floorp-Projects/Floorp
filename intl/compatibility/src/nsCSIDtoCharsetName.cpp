/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * IBM Corporation
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 1999
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 12/09/1999   IBM Corp.       Support for IBM codepages - 850,852,855,857,862,864
 *
 */

#include "nsI18nCompatibility.h"

////////////////////////////////////////////////////////////////////////////////

/* Codeset type */
#define SINGLEBYTE   0x0000 /* 0000 0000 0000 0000 =    0 */
#define MULTIBYTE    0x0100 /* 0000 0001 0000 0000 =  256 */
#define STATEFUL     0x0200 /* 0000 0010 0000 0000 =  512 */
#define WIDECHAR     0x0300 /* 0000 0011 0000 0000 =  768 */
#define CODESET_MASK 0x0F00 /* 0000 1111 0000 0000 = 3840 */

/*
 * Check for double byte encodings
 * (should distinguish 2 byte from true multibyte)
 */
#define IS_16BIT_ENCODING(x) (((x)&CODESET_MASK) == MULTIBYTE)

/* line-break on spaces */
#define CS_SPACE   0x0400 /* 0000 0100 0000 0000 = 1024 */

/* Auto Detect Mode */
#define CS_AUTO    0x0800 /* 0000 1000 0000 0000 = 2048 */


/* Code Set IDs */
/* CS_DEFAULT: used if no charset param in header */
/* CS_UNKNOWN: used for unrecognized charset */

                    /* type                  id   */
#define CS_DEFAULT    (SINGLEBYTE         |   0) /*    0 */
#define CS_ASCII      (SINGLEBYTE         |   1) /*    1 */
#define CS_LATIN1     (SINGLEBYTE         |   2) /*    2 */
#define CS_JIS        (STATEFUL           |   3) /*  515 */
#define CS_SJIS       (MULTIBYTE          |   4) /*  260 */
#define CS_EUCJP      (MULTIBYTE          |   5) /*  261 */

#define CS_JIS_AUTO   (CS_AUTO|STATEFUL   |   3) /* 2563 */
#define CS_SJIS_AUTO  (CS_AUTO|MULTIBYTE  |   4) /* 2308 */
#define CS_EUCJP_AUTO (CS_AUTO|MULTIBYTE  |   5) /* 2309 */

#define CS_MAC_ROMAN  (SINGLEBYTE         |   6) /*    6 */
#define CS_BIG5       (MULTIBYTE          |   7) /*  263 */
#define CS_GB_8BIT    (MULTIBYTE          |   8) /*  264 */
#define CS_CNS_8BIT   (MULTIBYTE          |   9) /*  265 */
#define CS_LATIN2     (SINGLEBYTE         |  10) /*   10 */
#define CS_MAC_CE     (SINGLEBYTE         |  11) /*   11 */
#define CS_KSC_8BIT   (MULTIBYTE|CS_SPACE |  12) /* 1292 */

/* Jack Liu adds the following two entries */
#define CS_KSC_8BIT_AUTO   (CS_AUTO | MULTIBYTE|CS_SPACE |  12)

#define CS_2022_KR    (STATEFUL           |  13) /*  525 */
#define CS_8859_3     (SINGLEBYTE         |  14) /*   14 */
#define CS_8859_4     (SINGLEBYTE         |  15) /*   15 */
#define CS_8859_5     (SINGLEBYTE         |  16) /*   16 ISO Cyrillic */
#define CS_8859_6     (SINGLEBYTE         |  17) /*   17 ISO Arabic */
#define CS_8859_7     (SINGLEBYTE         |  18) /*   18 ISO Greek */
#define CS_8859_8     (SINGLEBYTE         |  19) /*   19 ISO Hebrew */
#define CS_8859_9     (SINGLEBYTE         |  20) /*   20 */
#define CS_SYMBOL     (SINGLEBYTE         |  21) /*   21 */
#define CS_DINGBATS   (SINGLEBYTE         |  22) /*   22 */
#define CS_DECTECH    (SINGLEBYTE         |  23) /*   23 */
#define CS_CNS11643_1 (MULTIBYTE          |  24) /*  280 */
#define CS_CNS11643_2 (MULTIBYTE          |  25) /*  281 */
#define CS_JISX0208   (MULTIBYTE          |  26) /*  282 */
#define CS_JISX0201   (SINGLEBYTE         |  27) /*   27 */
#define CS_KSC5601    (MULTIBYTE          |  28) /*  284 */
#define CS_TIS620     (SINGLEBYTE         |  29) /*   29 */
#define CS_JISX0212   (MULTIBYTE          |  30) /*  286 */
#define CS_GB2312     (MULTIBYTE          |  31) /*  287 */
#define CS_UCS2       (WIDECHAR           |  32) /*  810 */
#define CS_UCS4       (WIDECHAR           |  33) /*  811 */
#define CS_UTF8       (MULTIBYTE          |  34) /*  290 */
#define CS_UTF7       (STATEFUL           |  35) /*   35 */
#define CS_NPC        (MULTIBYTE          |  36) /*  292 */
#define CS_X_BIG5     (MULTIBYTE          |  37) /*  293 */
#define CS_USRDEF2    (SINGLEBYTE         |  38) /*   38 */

#define CS_KOI8_R     (SINGLEBYTE         |  39) /*   39 */
#define CS_MAC_CYRILLIC     (SINGLEBYTE   |  40) /*   40 */
#define CS_CP_1251    (SINGLEBYTE         |  41) /*   41 CS_CP_1251 is window Cyrillic */
#define CS_MAC_GREEK  (SINGLEBYTE         |  42) /*   42 */
/* CS_CP_1253 should be delete we should use CS_8859_7 instead */
#define CS_CP_1253    (SINGLEBYTE         |  43) /*   43 CS_CP_1253 is window Greek */
#define CS_CP_1250    (SINGLEBYTE         |  44) /*   44 CS_CP_1250 is window Centrl Europe */
/* CS_CP_1254 should be delete we should use CS_8859_9 instead */
#define CS_CP_1254    (SINGLEBYTE         |  45) /*   45 CS_CP_1254 is window Turkish */
#define CS_MAC_TURKISH (SINGLEBYTE        |  46) /*   46 */
#define CS_GB2312_11  (MULTIBYTE          |  47) /*  303 */
#define CS_JISX0208_11 (MULTIBYTE         |  48) /*  304 */
#define CS_KSC5601_11 (MULTIBYTE          |  49) /*  305 */
#define CS_CNS11643_1110 (MULTIBYTE       |  50) /*  306 */
#define CS_UCS2_SWAP    (WIDECHAR         |  51) /*  819 */
#define CS_IMAP4_UTF7       (STATEFUL     |  52) /*  564 */
#define CS_T61		(MULTIBYTE	  |  53) /* This line should not merged into 5.0  */
#define CS_HZ         (STATEFUL           |  54) /*   566 */

#define CS_CP_850     (SINGLEBYTE         |  55) /*   55  PC Latin 1 */
#define CS_CP_852     (SINGLEBYTE         |  56) /*   56  PC Latin 2 */
#define CS_CP_855     (SINGLEBYTE         |  57) /*   57  PC Cyrillic */
#define CS_CP_857     (SINGLEBYTE         |  58) /*   58  PC Turkish */
#define CS_CP_862     (SINGLEBYTE         |  59) /*   59  PC Hebrew */
#define CS_CP_864     (SINGLEBYTE         |  60) /*   60  PC Arabic */
#define CS_CP_866     (SINGLEBYTE         |  61) /*   61  PC Russian */
#define CS_CP_1255    (SINGLEBYTE         |  62) /*   62  Windows Hebrew */
#define CS_CP_1256    (SINGLEBYTE         |  63) /*   63  Windows Arabic */
#define CS_CP_1257    (SINGLEBYTE         |  64) /*   64  Windows Baltic */
#define CS_CP_1258    (SINGLEBYTE         |  65) /*   65  Windows Vietnamese */
#define CS_8859_15    (SINGLEBYTE         |  66) /*   66  EURO Support latin */
#define INTL_CHAR_SET_MAX                    67  /* must be highest + 1 */

#define CS_USER_DEFINED_ENCODING (SINGLEBYTE | 254) /* 254 */
#define CS_UNKNOWN    (SINGLEBYTE         | 255) /* 255 */

#define IS_UTF8_CSID(x) (((x)&0xFF)== (CS_UTF8&0xFF))
#define IS_UNICODE_CSID(x) \
			(   (((x)&0xFF)== (CS_UCS2&0xFF)) \
			 || (((x)&0xFF)== (CS_UTF8&0xFF)) \
			 || (((x)&0xFF)== (CS_UTF7&0xFF)) )

/* Jack Liu (jliu) add the following. The trigger is passing the parameter to 
   PA_FetchParamValue() to satisfy its signature */
#ifdef XP_MAC
#define CS_FE_ASCII CS_MAC_ROMAN
#else
#define CS_FE_ASCII CS_LATIN1
#endif

////////////////////////////////////////////////////////////////////////////////

#ifndef MAX_CSNAME
#define MAX_CSNAME	64
#endif

typedef struct _csname2id_t {
	char	cs_name[MAX_CSNAME];
	char	java_name[MAX_CSNAME];
	PRUint16		cs_id;
	char	fill[3];
} csname2id_t;


/* Charset names and aliases from RFC 1700. Also encloded equivelend Java encoding names. Names are case
 * insenstive. Currently searches table linearly, so keep commonly used names at the beginning.
 */
static csname2id_t csname2id_tbl[] = {
											/* default if not specified */
			{"x-default", "", CS_DEFAULT},			/* or unknown charset		*/

			{"us-ascii", "8859_1", CS_ASCII},
			{"iso-8859-1", "8859_1", CS_LATIN1},
			{"iso-2022-jp", "JIS", CS_JIS},
			{"iso-2022-jp-2", "JIS", CS_JIS},		/* treat same as iso-2022-jp*/
			{"Shift_JIS", "SJIS", CS_SJIS},
			{"euc-jp", "EUCJIS", CS_EUCJP},
			{"jis_x0208-1983", "JIS0208", CS_JISX0208},
			{"x-jisx0208-11", "JIS0208", CS_JISX0208_11},
			{"jis_x0201", "JIS0208", CS_JISX0201},
			{"jis_x0212-1990", "JIS0208", CS_JISX0212},
			{"x-mac-roman", "MacRoman", CS_MAC_ROMAN}, 
			{"iso-8859-2", "8859_2", CS_LATIN2},
			{"iso-8859-3", "8859_3", CS_8859_3},
			{"iso-8859-4", "8859_4", CS_8859_4},
			{"iso-8859-5", "8859_5", CS_8859_5},
			{"iso-8859-6", "8859_6", CS_8859_6},
			{"iso-8859-7", "8859_7", CS_8859_7},
			{"iso-8859-8", "8859_8", CS_8859_8},
			{"iso-8859-9", "8859_9", CS_8859_9},
			{"iso-8859-15", "8859_15", CS_8859_15},
			{"iso8859-15", "8859_15", CS_8859_15},
			{"x-mac-ce", "MacCentralEurope", CS_MAC_CE},
			{"EUC-KR", "KSC5601", CS_KSC_8BIT},	/* change to UPPER case per Jungshik Shin <jshin@pantheon.yale.edu> request to work around Korean SendMail Decode bug */
			{"ks_c_5601-1987", "KSC5601", CS_KSC5601},
			{"x-ksc5601-11", "KSC5601", CS_KSC5601_11},
			{"gb2312", "GB2312", CS_GB_8BIT},
			{"gb_2312-80", "GB2312", CS_GB2312},
			{"x-gb2312-11", "GB2312", CS_GB2312_11},
			{"x-euc-tw", "CNS11643", CS_CNS_8BIT},
			{"x-cns11643-1", "CNS11643", CS_CNS11643_1},
			{"x-cns11643-2", "CNS11643", CS_CNS11643_2},
			{"x-cns11643-1110", "CNS11643", CS_CNS11643_1110},
			{"iso-2022-kr", "KSC5601", CS_2022_KR},
			{"big5", "Big5", CS_BIG5},
			{"x-x-big5", "Big5", CS_X_BIG5},
			{"x-tis620", "TIS620", CS_TIS620},
			{"adobe-symbol-encoding", "Symbol", CS_SYMBOL},
			{"x-dingbats", "DingBats", CS_DINGBATS},
			{"x-dectech", "DECTECH", CS_DECTECH},
			{"koi8-r", "KOI8_R", CS_KOI8_R},
			{"x-mac-cyrillic", "MacCyrillic", CS_MAC_CYRILLIC}, 
			{"x-mac-greek", "MacGreek", CS_MAC_GREEK}, 
			{"x-mac-turkish", "MacTurkish", CS_MAC_TURKISH}, 
			{"windows-1250", "Cp1250", CS_CP_1250},
			{"windows-1251", "Cp1251", CS_CP_1251}, /* cyrillic */
			{"windows-1253", "Cp1253", CS_CP_1253},  /* greek */
			{"windows-1257", "Cp1257", CS_CP_1257},  /* baltic */
			{"UTF-8", "UTF8", CS_UTF8},
			{"UTF-7", "UTF7", CS_UTF7},
			{"ISO-10646-UCS-2", "Unicode", CS_UCS2},
			{"ISO-10646-UCS-4", "UCS4", CS_UCS4},
			{"x-imap4-modified-utf7", "", CS_IMAP4_UTF7},
			{"T.61-8bit", "", CS_T61},
			{"HZ-GB-2312", "", CS_HZ},

	/* cP866 support for ibm */
			{"ibm866", "Cp866", CS_CP_866},         /* PC Russian */
			{"cp866", "Cp866", CS_CP_866},          /* PC Russian */

	/* Baltic support for ibm */
			{"windows-1257", "Cp1257", CS_CP_1257},  /* greek */

	/* cP850 support for ibm */
			{"ibm850", "Cp850", CS_CP_850},         /* PC Latin-1 */
			{"cp850", "Cp850", CS_CP_850},          /* PC Latin-1 */

	/* cP852 support for ibm */
			{"ibm852", "Cp852", CS_CP_852},         /* PC Latin-2 */
			{"cp852", "Cp852", CS_CP_852},          /* PC Latin-2 */

	/* cP855 support for ibm */
			{"ibm855", "Cp855", CS_CP_855},         /* PC Cyrillic */
			{"cp855", "Cp855", CS_CP_855},          /* PC Cyrillic */

	/* cP857 support for ibm */
			{"ibm857", "Cp857", CS_CP_857},         /* PC Turkish */
			{"cp857", "Cp857", CS_CP_857},          /* PC Turkish */

	/* cP862 support for ibm */
			{"ibm862", "Cp862", CS_CP_862},         /* PC Hebrew */
			{"cp862", "Cp862", CS_CP_862},          /* PC Hebrew */

	/* cP864 support for ibm */
			{"ibm864", "Cp864", CS_CP_864},         /* PC Arabic */
			{"cp864", "Cp864", CS_CP_864},          /* PC Arabic */

	/* aliases for us-ascii: */
			{"ansi_x3.4-1968", "", CS_ASCII},
			{"iso-ir-6", "", CS_ASCII},
			{"ansi_x3.4-1986", "", CS_ASCII},
			{"iso_646.irv:1991", "", CS_ASCII},
			{"ascii", "", CS_ASCII},
			{"iso646-us", "", CS_ASCII},
			{"us", "", CS_ASCII},
			{"ibm367", "", CS_ASCII},
			{"cp367", "", CS_ASCII},
			{"csASCII", "", CS_ASCII},

	/* aliases for iso_8859-1:	*/
			{"latin1", "", CS_LATIN1},
			{"iso_8859-1", "", CS_LATIN1},
			{"iso_8859-1:1987", "", CS_LATIN1},
			{"iso-ir-100", "", CS_LATIN1},
			{"l1", "", CS_LATIN1},
			{"ibm819", "", CS_LATIN1},
			{"cp819", "", CS_LATIN1},
			{"ISO-8859-1-Windows-3.0-Latin-1", "", CS_LATIN1},
			{"ISO-8859-1-Windows-3.1-Latin-1", "", CS_LATIN1},
			{"windows-1252", "", CS_LATIN1},
			{"iso8859-1", "", CS_LATIN1},
			
	/* aliases for ISO_8859-2:	*/
			{"latin2", "", CS_LATIN2},			
			{"iso_8859-2", "", CS_LATIN2},
			{"iso_8859-2:1987", "", CS_LATIN2},
			{"iso-ir-101", "", CS_LATIN2},
			{"l2", "", CS_LATIN2},											
			{"ISO-8859-2-Windows-Latin-2", "", CS_LATIN2},

	/* aliases for KS_C_5601-1987:	*/
			{"ks_c_5601-1987", "", CS_KSC5601},
			{"iso-ir-149", "", CS_KSC5601},
			{"ks_c_5601-1989", "", CS_KSC5601},
			{"ksc_5601", "", CS_KSC5601},
			{"ks_c_5601", "", CS_KSC5601},
			{"korean", "", CS_KSC5601},
			{"csKSC56011987", "", CS_KSC5601},

	/* aliases for iso-2022-kr:	*/
			{"csISO2022KR", "", CS_2022_KR},

	/* aliases for euc-kr:	*/
			{"csEUCKR", "", CS_KSC_8BIT},
			{"ksc5601", "", CS_KSC_8BIT},

	/* aliases for iso-2022-jp:	*/
			{"csISO2022JP", "", CS_JIS},

	/* aliases for iso-2022-jp-2:	*/
			{"csISO2022JP2", "", CS_JIS},

	/* aliases for GB_2312-80:	*/
			{"iso-ir-58", "", CS_GB2312},
			{"chinese", "", CS_GB2312},
			{"csISO58GB231280", "", CS_GB2312},

	/* aliases for gb2312:	*/
			{"csGB2312", "", CS_GB_8BIT},
			{"CN-GB", "", CS_GB_8BIT},  /* Simplified Chinese */
			{"CN-GB-ISOIR165", "", CS_GB_8BIT},  /* Simplified Chinese */

	/* aliases for big5:	*/
			{"csBig5", "", CS_BIG5},
			{"CN-Big5", "", CS_BIG5},  /* Traditional Chinese */

	/* aliases for iso-8859-7:	*/
			{"iso-ir-126", "", CS_8859_7},
			{"iso_8859-7", "", CS_8859_7},
			{"iso_8859-7:1987", "", CS_8859_7},
			{"elot_928", "", CS_8859_7},
			{"ecma-118", "", CS_8859_7},
			{"greek", "", CS_8859_7},
			{"greek8", "", CS_8859_7},
			{"csISOLatinGreek", "", CS_8859_7},

	/* aliases for iso-8859-5:	*/
			{"iso-ir-144", "", CS_8859_5},
			{"iso_8859-5", "", CS_8859_5},
			{"iso_8859-5:1988", "", CS_8859_5},
			{"cyrillic", "", CS_8859_5},
			{"csISOLatinCyrillic", "", CS_8859_5},

	/* aliases for iso-8859-4:	*/
			{"iso-ir-110", "", CS_8859_4},
			{"iso_8859-4", "", CS_8859_4},
			{"iso_8859-4:1988", "", CS_8859_4},
			{"latin4", "", CS_8859_4},
			{"l4", "", CS_8859_4},
			{"csISOLatin4", "", CS_8859_4},

	/* aliases for jis_x0212-1990:	*/
			{"x0212", "", CS_JISX0212},
			{"iso-ir-159", "", CS_JISX0212},
			{"csISO159JISX02121990", "", CS_JISX0212},

	/* aliases for jis_x0201:	*/
			{"x0201", "", CS_JISX0201},
			{"csHalfWidthKatakana", "", CS_JISX0201},

	/* aliases for koi8-r:	*/
			{"csKOI8R", "", CS_KOI8_R},

	/* aliases for Shift_JIS:	*/
			{"x-sjis", "", CS_SJIS},
			{"ms_Kanji", "", CS_SJIS},
			{"csShiftJIS", "", CS_SJIS},
			{"Windows-31J", "", CS_SJIS},

	/* aliases for x-euc-jp:	*/
			{"Extended_UNIX_Code_Packed_Format_for_Japanese", "", CS_EUCJP},
			{"csEUCPkdFmtJapanese", "", CS_EUCJP},
			{"x-euc-jp", "", CS_EUCJP},

	/* aliases for adobe-symbol-encoding:	*/
			{"csHPPSMath", "", CS_SYMBOL},

	/* aliases for iso-8859-5-windows-latin-5:	*/
			{"csWindows31Latin5", "", CS_CP_1251},
			{"iso-8859-5-windows-latin-5", "", CS_CP_1251},
			{"x-cp1251", "", CS_CP_1251},

	/* aliases for windows-1250:	*/
			{"x-cp1250", "", CS_CP_1250},

	/* aliases for windows-1253:	*/
			{"x-cp1253", "", CS_CP_1253}, 

	/* aliases for windows-1254:	*/
			{"windows-1254", "", CS_8859_9},  /* turkish */

	/* aliases for UNICODE-1-1:	*/
			{"csUnicode11", "", CS_UCS2},
			{"ISO-10646-UCS-BASIC", "", CS_UCS2},
			{"csUnicodeASCII", "", CS_UCS2},
			{"ISO-10646-Unicode-Latin1", "", CS_UCS2},
			{"csUnicodeLatin1", "", CS_UCS2},
			{"ISO-10646", "", CS_UCS2},
			{"ISO-10646-J-1", "", CS_UCS2},

	/* aliases for UTF-7:	*/
			{"x-UNICODE-2-0-UTF-7", "", CS_UTF7},
			{"UNICODE-1-1-UTF-7", "", CS_UTF7},
			{"UNICODE-2-0-UTF-7", "", CS_UTF7},	/* Appeared in UTF-7 RFC Draft */
			{"csUnicode11UTF7", "", CS_UTF7},

	/* aliases for T.61-8bit:	*/
			{"T.61", "", CS_T61},
			{"iso-ir-103", "", CS_T61},
			{"csISO103T618bit", "", CS_T61},

	/* aliases for UNICODE-1-1-UTF-8:	*/
			{"UNICODE-1-1-UTF-8", "", CS_UTF8},

			{"x-user-defined", "", CS_USER_DEFINED_ENCODING},											
			{"x-user-defined", "", CS_USRDEF2},											

			{"RESERVED", "", CS_DEFAULT},			/* or unknown charset		*/

			{"", 	"", CS_UNKNOWN}
};

static const char *INTL_CsidToCharsetNamePt(PRUint16 csid)
{
	csname2id_t	*csn2idp;

	csid &= ~CS_AUTO;
	csn2idp = &csname2id_tbl[1];	/* First one is reserved, skip it. */
	csid &= 0xff;

	/* Linear search for charset string */
	while (*(csn2idp->cs_name) != '\0') {
		if ((csn2idp->cs_id & 0xff) == csid)
			return csn2idp->cs_name;
 		csn2idp++;
	}
	return "";
}

////////////////////////////////////////////////////////////////////////////////

extern "C" const char * I18N_CSIDtoCharsetName(PRUint16 csid)
{
  const char *charset = INTL_CsidToCharsetNamePt(csid);

  return *charset ? charset : "ISO-8859-1";
}

////////////////////////////////////////////////////////////////////////////////

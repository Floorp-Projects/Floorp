/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*	csnametb.c	*/

#include "intlpriv.h"

/* Charset names and aliases from RFC 1700. Also encloded equivelend Java encoding names. Names are case
 * insenstive. Currently searches table linearly, so keep commonly used names at the beginning.
 */
MODULE_PRIVATE csname2id_t csname2id_tbl[] = {
											/* default if not specified */
			{"x-default", "", CS_DEFAULT},			/* or unknown charset		*/

			{"us-ascii", "8859_1", CS_ASCII},
			{"iso-8859-1", "8859_1", CS_LATIN1},
			{"iso-2022-jp", "JIS", CS_JIS},
			{"iso-2022-jp-2", "JIS", CS_JIS},		/* treat same as iso-2022-jp*/
			{"Shift_JIS", "SJIS", CS_SJIS},
			{"euc-jp", "EUC", CS_EUCJP},
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
			{"x-mac-ce", "MacCentralEurope", CS_MAC_CE},
			{"EUC-KR", "KSC5601", CS_KSC_8BIT},	/* change to UPPER case per Jungshik Shin <jshin@pantheon.yale.edu> request to work around Korean SendMail Decode bug */
			{"ks_c_5601-1987", "KSC5601", CS_KSC5601},
			{"x-ksc5601-11", "KSC5601", CS_KSC5601_11},
			{"gb2312", "GB2312", CS_GB_8BIT},
			{"gb_2312-80", "GB2312", CS_GB2312},
			{"x-gb2312-11", "GB2312", CS_GB2312_11},
			{"x-euc-tw", "", CS_CNS_8BIT},
			{"x-cns11643-1", "CNS11643", CS_CNS11643_1},
			{"x-cns11643-2", "CNS11643", CS_CNS11643_2},
			{"x-cns11643-1110", "CNS11643", CS_CNS11643_1110},
			{"iso-2022-kr", "KR2022", CS_2022_KR},
			{"big5", "Big5", CS_BIG5},
			{"x-x-big5", "Big5", CS_X_BIG5},
			{"x-tis620", "TIS620", CS_TIS620},
			{"adobe-symbol-encoding", "Symbol", CS_SYMBOL},
			{"x-dingbats", "DingBats", CS_DINGBATS},
			{"x-dectech", "DECTECH", CS_DECTECH},
			{"koi8-r", "KOI8", CS_KOI8_R},
			{"x-mac-cyrillic", "MacCyrillic", CS_MAC_CYRILLIC}, 
			{"x-mac-greek", "MacGreek", CS_MAC_GREEK}, 
			{"x-mac-turkish", "MacTurkish", CS_MAC_TURKISH}, 
			{"windows-1250", "Cp1250", CS_CP_1250},
			{"windows-1251", "Cp1251", CS_CP_1251}, /* cyrillic */
			{"windows-1253", "Cp1253", CS_CP_1253},  /* greek */
			{"UTF-8", "UTF8", CS_UTF8},
			{"UTF-7", "UTF7", CS_UTF7},
			{"ISO-10646-UCS-2", "UCS2", CS_UCS2},
			{"ISO-10646-UCS-4", "UCS4", CS_UCS4},
			{"x-imap4-modified-utf7", "", CS_IMAP4_UTF7},
			{"armscii-8", "", CS_ARMSCII8},

#ifdef XP_OS2
	/* Additional OS/2 codepages. These are IANA primary names */
			{"ibm850", "Cp850", CS_CP_850},         /* PC Latin 1 */
			{"ibm852", "Cp852", CS_CP_852},         /* PC Latin 2 */
			{"ibm855", "Cp855", CS_CP_855},         /* PC Cyrillic */
			{"ibm857", "Cp857", CS_CP_857},         /* PC Turkish */
			{"ibm862", "Cp862", CS_CP_862},         /* PC Hebrew */
			{"ibm864", "Cp864", CS_CP_864},         /* PC Arabic */
			{"ibm866", "Cp866", CS_CP_866},         /* PC Russian */
			{"ibm874", "Cp874", CS_CP_874},         /* PC Thai */
			{"windows-1257", "Cp1257", CS_CP_1257}, /* Windows Baltic */

	/* OS/2 IANA alias entries */
			{"cp850", "", CS_CP_850},               /* PC Latin 1 */
			{"cp852", "", CS_CP_852},               /* PC Latin 2 */
			{"cp857", "", CS_CP_857},               /* PC Turkish */
			{"cp862", "", CS_CP_862},               /* PC Hebrew */
			{"cp864", "", CS_CP_864},               /* PC Arabic */
			{"cp874", "", CS_CP_874},               /* PC Thai */
#endif

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
#ifdef XP_OS2
			{"windows-1252", "Cp1252", CS_LATIN1},
#endif            
			
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
#ifdef XP_OS2
			{"csUnicode", "", CS_UCS2},
#endif      
			{"csUnicode11", "", CS_UCS2},
			{"ISO-10646-UCS-BASIC", "", CS_UCS2},
			{"csUnicodeASCII", "", CS_UCS2},
			{"ISO-10646-Unicode-Latin1", "", CS_UCS2},
			{"csUnicodeLatin1", "", CS_UCS2},
			{"ISO-10646", "", CS_UCS2},
			{"ISO-10646-J-1", "", CS_UCS2},

	/* aliases for UNICODE-1-1-UTF-7:	*/
			{"x-UNICODE-2-0-UTF-7", "", CS_UTF7},			/* This is not in INAN */
			{"UNICODE-1-1-UTF-7", "", CS_UTF7},
			{"UNICODE-2-0-UTF-7", "", CS_UTF7},	/* Appeared in UTF-7 RFC Draft */
			{"csUnicode11UTF7", "", CS_UTF7},

	/* aliases for UNICODE-1-1-UTF-8:	*/
			{"UNICODE-1-1-UTF-8", "", CS_UTF8},

			{"x-user-defined", "", CS_USER_DEFINED_ENCODING},											
			{"x-user-defined", "", CS_USRDEF2},											

			{"RESERVED", "", CS_DEFAULT},			/* or unknown charset		*/

			{"", 	"", CS_UNKNOWN}
};




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
/*	fe_ccc.c	*/
/* Test harness code to be replaced by FE specific code	*/

#ifdef XP_OS2
#define INCL_DOS
#endif
#include "intlpriv.h"

#include <stdio.h>
#include "xp.h"
#include "intl_csi.h"

#ifdef XP_MAC
#include "resgui.h"
#endif

/* for XP_GetString() */
#include "xpgetstr.h"
extern int MK_OUT_OF_MEMORY;


/*
IMPORTANT NOTE:

	mz_euc2euc
	mz_b52b5
	mz_cns2cns
	mz_ksc2ksc
	mz_sjis2sjis
	mz_utf82utf8
	
	is now replaced by mz_mbNullConv 
	we eventually should replacing mz_hz2gb after we extract the hz -> gb conversion
*/
PRIVATE unsigned char *
mz_hz2gb(CCCDataObject obj, const unsigned char *kscbuf, int32 kscbufsz);
PRIVATE unsigned char *
mz_gb2gb(CCCDataObject obj, const unsigned char *kscbuf, int32 kscbufsz);

PRIVATE unsigned char *
mz_mbNullConv(CCCDataObject obj, const unsigned char *buf, int32 bufsz);

PRIVATE unsigned char *
mz_AnyToAnyThroughUCS2(CCCDataObject obj, const unsigned char *buf, int32 bufsz);

/*	intl_CharLenFunc is designed to used with mz_mbNullConv */
typedef int16 (*intl_CharLenFunc) (	unsigned char ch);
PRIVATE int16 intl_CharLen_SJIS(		unsigned char ch);
PRIVATE int16 intl_CharLen_EUC_JP(	unsigned char ch);
PRIVATE int16 intl_CharLen_CGK(		unsigned char ch);
PRIVATE int16 intl_CharLen_CNS_8BIT(	unsigned char ch);
PRIVATE int16 intl_CharLen_UTF8(		unsigned char ch);
PRIVATE int16 intl_CharLen_SingleByte(unsigned char ch);

#define INTL_CHARLEN_SJIS 		0
#define INTL_CHARLEN_EUC_JP 	1
#define INTL_CHARLEN_CGK 		2
#define INTL_CHARLEN_CNS_8BIT 	3
#define INTL_CHARLEN_UTF8 		4
#define INTL_CHARLEN_SINGLEBYTE 5

PRIVATE intl_CharLenFunc intl_char_len_func[]=
{
	intl_CharLen_SJIS,
	intl_CharLen_EUC_JP,
	intl_CharLen_CGK,
	intl_CharLen_CNS_8BIT,
	intl_CharLen_UTF8,
 	intl_CharLen_SingleByte,
};

int haveBig5 = 0;
PRIVATE int16 *availableFontCharSets = NULL;


/* Table that maps the FROM char, codeset to all other relevant info:
 *	- TO character codeset
 *	- Fonts (fixe & proportional) for TO character codeset
 *	- Type of conversion (func for Win/Mac, value for X)
 *	- Argument for conversion routine.  Routine-defined.
 *
 * Not all of these may be available.  Depends upon available fonts,
 * scripts, codepages, etc.  Need to query system to build valid table.
 *
 * What info do I need to make the font change API on the 3 platforms?
 *  Is just a 32bit font ID sufficient?
 *
 * Some X Windows can render Japanese in either EUC or SJIS, how do we
 * choose?
 */
/* The ***first*** match of a "FROM" encoding (1st col.) will be
 * used as the URL->native encoding.  Be careful of the
 * ordering.
 * Additional entries for the same "FROM" encoding, specifies
 * how to convert going out (e.g., sending mail, news or forms).
 */

/*
	What is the flag mean ?

	For Mac the flag in One2OneCCC is the resouce number of a 256 byte mapping table
	For all platform the flag in  mz_mbNullConv is a pointer to a intl_CharLenFunc routine

*/
#ifdef XP_MAC
MODULE_PRIVATE cscvt_t		cscvt_tbl[] = {
		/*	SINGLE BYTE */
		/*	LATIN1 */
		{CS_LATIN1,		CS_MAC_ROMAN,	0, (CCCFunc)One2OneCCC,	xlat_LATIN1_TO_MAC_ROMAN},
		{CS_ASCII,		CS_MAC_ROMAN,	0, (CCCFunc)One2OneCCC,	xlat_LATIN1_TO_MAC_ROMAN},
		{CS_MAC_ROMAN,	CS_MAC_ROMAN,	0, (CCCFunc)0,			0},
		{CS_MAC_ROMAN,	CS_LATIN1,		0, (CCCFunc)One2OneCCC,	xlat_MAC_ROMAN_TO_LATIN1},
		{CS_MAC_ROMAN,	CS_ASCII,		0, (CCCFunc)One2OneCCC,	xlat_MAC_ROMAN_TO_LATIN1},

		/*	LATIN2 */
		{CS_LATIN2,		CS_MAC_CE,		0, (CCCFunc)One2OneCCC, xlat_LATIN2_TO_MAC_CE},
		{CS_MAC_CE,		CS_MAC_CE,		0, (CCCFunc)0,			0},
		{CS_MAC_CE,		CS_LATIN2,		0, (CCCFunc)One2OneCCC, xlat_MAC_CE_TO_LATIN2},
		{CS_MAC_CE,		CS_ASCII,		0, (CCCFunc)One2OneCCC, xlat_MAC_CE_TO_LATIN2},

		{CS_CP_1250,	CS_MAC_CE,		0, (CCCFunc)One2OneCCC, xlat_CP_1250_TO_MAC_CE},
		{CS_MAC_CE,		CS_CP_1250,		0, (CCCFunc)One2OneCCC, xlat_MAC_CE_TO_CP_1250},

		/*	CYRILLIC */
		{CS_8859_5,		CS_MAC_CYRILLIC,0, (CCCFunc)One2OneCCC, xlat_8859_5_TO_MAC_CYRILLIC},
		{CS_MAC_CYRILLIC,CS_MAC_CYRILLIC,		0, (CCCFunc)0,			0},
		{CS_MAC_CYRILLIC,CS_8859_5,		0, (CCCFunc)One2OneCCC, xlat_MAC_CYRILLIC_TO_8859_5},
		{CS_MAC_CYRILLIC,CS_ASCII,		0, (CCCFunc)One2OneCCC, xlat_MAC_CYRILLIC_TO_8859_5},

		{CS_CP_1251,	CS_MAC_CYRILLIC,0, (CCCFunc)One2OneCCC, xlat_CP_1251_TO_MAC_CYRILLIC},
		{CS_MAC_CYRILLIC,CS_CP_1251,	0, (CCCFunc)One2OneCCC, xlat_MAC_CYRILLIC_TO_CP_1251},

		{CS_KOI8_R,		CS_MAC_CYRILLIC,0, (CCCFunc)One2OneCCC, xlat_KOI8_R_TO_MAC_CYRILLIC},
		{CS_MAC_CYRILLIC,CS_KOI8_R,		0, (CCCFunc)One2OneCCC, xlat_MAC_CYRILLIC_TO_KOI8_R},

		/*	GREEK */
		{CS_8859_7,		CS_MAC_GREEK,	0, (CCCFunc)One2OneCCC, xlat_8859_7_TO_MAC_GREEK},
		{CS_MAC_GREEK,	CS_MAC_GREEK,	0, (CCCFunc)0,			0},
		{CS_MAC_GREEK,	CS_8859_7,		0, (CCCFunc)One2OneCCC, xlat_MAC_GREEK_TO_8859_7},
		{CS_MAC_GREEK,	CS_ASCII,		0, (CCCFunc)One2OneCCC, xlat_MAC_GREEK_TO_8859_7},
		
		{CS_CP_1253,	CS_MAC_GREEK,	0, (CCCFunc)One2OneCCC, xlat_CP_1253_TO_MAC_GREEK},
		{CS_MAC_GREEK,	CS_CP_1253,		0, (CCCFunc)One2OneCCC, xlat_MAC_GREEK_TO_CP_1253},

		/*	TURKISH */
		{CS_8859_9,		CS_MAC_TURKISH,	0, (CCCFunc)One2OneCCC, xlat_8859_9_TO_MAC_TURKISH},
		{CS_MAC_TURKISH,CS_MAC_TURKISH,	0, (CCCFunc)0,			0},
		{CS_MAC_TURKISH,CS_8859_9,		0, (CCCFunc)One2OneCCC, xlat_MAC_TURKISH_TO_8859_9},
		{CS_MAC_TURKISH,CS_ASCII,		0, (CCCFunc)One2OneCCC, xlat_MAC_TURKISH_TO_8859_9},

		/*  MULTIBYTE */
		/*	JAPANESE */
		{CS_SJIS,		CS_SJIS,		1, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_SJIS },
		{CS_SJIS,		CS_JIS,			1, (CCCFunc)mz_sjis2jis,	0},
		{CS_JIS,		CS_SJIS,		1, (CCCFunc)jis2other,		0},
		{CS_EUCJP,		CS_SJIS,		1, (CCCFunc)mz_euc2sjis,	0},
		{CS_JIS,		CS_EUCJP,		1, (CCCFunc)jis2other,		1},
		{CS_EUCJP,		CS_JIS,			1, (CCCFunc)mz_euc2jis,		0},
		{CS_SJIS,		CS_EUCJP,		1, (CCCFunc)mz_sjis2euc,	0},
		/* auto-detect Japanese conversions						*/
		{CS_SJIS_AUTO,	CS_SJIS,		1, (CCCFunc)autoJCCC,		0},
		{CS_JIS_AUTO,	CS_SJIS,		1, (CCCFunc)autoJCCC,		0},
		{CS_EUCJP_AUTO,	CS_SJIS,		1, (CCCFunc)autoJCCC,		0},

		/*	KOREAN */
		{CS_KSC_8BIT,	CS_KSC_8BIT,	0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_CGK },
		{CS_2022_KR,	CS_KSC_8BIT,	0, (CCCFunc)mz_iso2euckr,	0},
		{CS_KSC_8BIT,	CS_2022_KR,		0, (CCCFunc)mz_euckr2iso,	0},
		/* auto-detect Korean conversions						*/
		{CS_KSC_8BIT_AUTO,  CS_KSC_8BIT,1, (CCCFunc)autoKCCC,		0},
 		{(CS_2022_KR|CS_AUTO) ,  CS_KSC_8BIT,1, (CCCFunc)autoKCCC,		0},

		/*  SIMPLIFIED CHINESE */
		{CS_GB_8BIT,	CS_GB_8BIT,		0, (CCCFunc)mz_gb2gb,		0},

		/*  TRADITIONAL CHINESE */
		{CS_BIG5,		CS_BIG5,		0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_CGK },
#ifdef FEATURE_BIG5CNS
		{CS_BIG5,		CS_CNS_8BIT,	0, (CCCFunc)mz_b52cns,		0},
		{CS_CNS_8BIT,	CS_BIG5,		0, (CCCFunc)mz_cns2b5,		0},
#endif

		/*  UNICODE */
		{CS_UTF8,		CS_UTF8,		0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_UTF8 },

		{CS_UTF8,		CS_UCS2,		0, (CCCFunc)mz_utf82ucs,	0},
		{CS_UTF8,		CS_UTF7,		0, (CCCFunc)mz_utf82utf7,	0},
		{CS_UTF8,		CS_UCS2_SWAP,	0, (CCCFunc)mz_utf82ucsswap,	0},
		{CS_UTF8,		CS_IMAP4_UTF7,		0, (CCCFunc)mz_utf82imap4utf7,	0},
		{CS_UCS2,		CS_UTF8,		0, (CCCFunc)mz_ucs2utf8,	0},
		{CS_UCS2,		CS_UTF7,		0, (CCCFunc)mz_ucs2utf7,	0},
		{CS_UCS2_SWAP,	CS_UTF8,		0, (CCCFunc)mz_ucs2utf8,	0},
		{CS_UCS2_SWAP,	CS_UTF7,		0, (CCCFunc)mz_ucs2utf7,	0},
		{CS_UTF7,		CS_UTF8,		0, (CCCFunc)mz_utf72utf8,	0},
		{CS_IMAP4_UTF7,		CS_UTF8,		0, (CCCFunc)mz_imap4utf72utf8,	0},

 		{CS_MAC_ROMAN,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_MAC_CE,			CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_MAC_CYRILLIC,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_KOI8_R,			CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_MAC_GREEK,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_MAC_TURKISH,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
		{CS_SJIS,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SJIS},
 		{CS_KSC_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 		{CS_BIG5,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 		{CS_GB_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 
  		{CS_UTF8,		CS_MAC_ROMAN,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_MAC_CE,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_MAC_CYRILLIC,0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_KOI8_R,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_MAC_GREEK,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_MAC_TURKISH,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
		{CS_UTF8,		CS_SJIS,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_KSC_8BIT,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_BIG5,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_GB_8BIT,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},


		{CS_USER_DEFINED_ENCODING,	CS_USER_DEFINED_ENCODING,	0, (CCCFunc)0,			0},	
		{0,					0,				1, (CCCFunc)0,			0}
};

#endif	/* XP_MAC */

#if defined(XP_WIN) || defined(XP_OS2)
MODULE_PRIVATE cscvt_t		cscvt_tbl[] = {
		/*	SINGLE BYTE */
		/*	LATIN1 */
		{CS_LATIN1,		CS_LATIN1,		0,	(CCCFunc)0,				0},
		{CS_LATIN1,		CS_ASCII,		0, 	(CCCFunc)0,				0},
		{CS_ASCII,		CS_LATIN1,		0, 	(CCCFunc)0,				0},
		{CS_ASCII,		CS_ASCII,		0, 	(CCCFunc)0,				0},
	
		/* LATIN2 */
		{CS_CP_1250,	CS_CP_1250,		0,	(CCCFunc)0,				0},
		{CS_CP_1250,	CS_LATIN2,		0,	(CCCFunc)One2OneCCC,	0},
		{CS_LATIN2,		CS_CP_1250,		0,	(CCCFunc)One2OneCCC,	0},
		{CS_LATIN2,		CS_LATIN2,		0,	(CCCFunc)0,				0},
		{CS_LATIN2,		CS_ASCII,		0,	(CCCFunc)0,				0},

		/* CYRILLIC */
		{CS_CP_1251,	CS_CP_1251,		0, (CCCFunc)0,				0},
		{CS_8859_5,		CS_CP_1251,		0, (CCCFunc)One2OneCCC,		0},
		{CS_CP_1251,	CS_8859_5,		0, (CCCFunc)One2OneCCC,		0},
		{CS_CP_1251,	CS_CP_1251,		0, (CCCFunc)0,				0},

		{CS_KOI8_R,		CS_CP_1251,		0, (CCCFunc)One2OneCCC,		0},
		{CS_CP_1251,	CS_KOI8_R,		0, (CCCFunc)One2OneCCC,		0},

		/* GREEK */
		{CS_CP_1253,	CS_CP_1253,		0,	(CCCFunc)0,				0},
		{CS_CP_1253,	CS_8859_7,		0,	(CCCFunc)One2OneCCC,	0},
		{CS_8859_7,		CS_CP_1253,		0,	(CCCFunc)One2OneCCC,	0},
		{CS_8859_7,		CS_8859_7,		0,	(CCCFunc)0,				0},

		/* TURKISH */
#ifdef XP_OS2
                {CS_CP_1254,    CS_CP_1254,             0, (CCCFunc)0,                          0},
                {CS_CP_1254,    CS_8859_9,              0, (CCCFunc)One2OneCCC,         0},
                {CS_8859_9,             CS_CP_1254,             0, (CCCFunc)One2OneCCC,         0},
#endif
		{CS_8859_9,		CS_8859_9,		0, (CCCFunc)0,				0},

		/*  MULTIBYTE */
		/*	JAPANESE */
		{CS_SJIS,		CS_SJIS,		1, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_SJIS},
		{CS_SJIS,		CS_JIS,			1, (CCCFunc)mz_sjis2jis,	0},
		{CS_JIS,		CS_SJIS,		1, (CCCFunc)jis2other,		0},
		{CS_EUCJP,		CS_SJIS,		1, (CCCFunc)mz_euc2sjis,	0},
		{CS_JIS,		CS_EUCJP,		1, (CCCFunc)jis2other,		1},
		{CS_EUCJP,		CS_JIS,			1, (CCCFunc)mz_euc2jis,		0},
		{CS_SJIS,		CS_EUCJP,		1, (CCCFunc)mz_sjis2euc,	0},
		/* auto-detect Japanese conversions						*/
		{CS_SJIS_AUTO,	CS_SJIS,		1, (CCCFunc)autoJCCC,		0},
		{CS_JIS_AUTO,	CS_SJIS,		1, (CCCFunc)autoJCCC,		0},
		{CS_EUCJP_AUTO,	CS_SJIS,		1, (CCCFunc)autoJCCC,		0},

		/*	KOREAN */
		{CS_KSC_8BIT,	CS_KSC_8BIT,	0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_CGK},
		{CS_2022_KR,	CS_KSC_8BIT,	0, (CCCFunc)mz_iso2euckr,	0},
		{CS_KSC_8BIT,	CS_2022_KR,		0, (CCCFunc)mz_euckr2iso,	0},
		/* auto-detect Korean conversions						*/
		{CS_KSC_8BIT_AUTO,  CS_KSC_8BIT,1, (CCCFunc)autoKCCC,		0},
 		{(CS_2022_KR|CS_AUTO) ,  CS_KSC_8BIT,1, (CCCFunc)autoKCCC,		0},

		/*  SIMPLIFIED CHINESE */
		{CS_GB_8BIT,	CS_GB_8BIT,		0, (CCCFunc)mz_gb2gb,		0},

		/*  TRADITIONAL CHINESE */
		{CS_BIG5,		CS_BIG5,		0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_CGK},
#ifdef FEATURE_BIG5CNS
		{CS_BIG5,		CS_CNS_8BIT,	0, (CCCFunc)mz_b52cns,		0},
		{CS_CNS_8BIT,	CS_BIG5,		0, (CCCFunc)mz_cns2b5,		0},
#endif

		/* UNICODE */
		{CS_UTF8,		CS_UTF8,		0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_UTF8},

		{CS_UTF8,		CS_UCS2,		0, (CCCFunc)mz_utf82ucs,	0},
		{CS_UTF8,		CS_UTF7,		0, (CCCFunc)mz_utf82utf7,	0},
		{CS_UTF8,		CS_UCS2_SWAP,	0, (CCCFunc)mz_utf82ucsswap,	0},
		{CS_UCS2,		CS_UTF8,		0, (CCCFunc)mz_ucs2utf8,	0},
		{CS_UCS2,		CS_UTF7,		0, (CCCFunc)mz_ucs2utf7,	0},
		{CS_UTF8,		CS_IMAP4_UTF7,		0, (CCCFunc)mz_utf82imap4utf7,	0},
		{CS_UCS2_SWAP,	CS_UTF8,		0, (CCCFunc)mz_ucs2utf8,	0},
		{CS_UCS2_SWAP,	CS_UTF7,		0, (CCCFunc)mz_ucs2utf7,	0},
		{CS_UTF7,		CS_UTF8,		0, (CCCFunc)mz_utf72utf8,	0},
		{CS_IMAP4_UTF7,		CS_UTF8,		0, (CCCFunc)mz_imap4utf72utf8,	0},

 		{CS_LATIN1,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_1250,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_1251,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_CP_1253,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_8859_9,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
		{CS_SJIS,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SJIS},
 		{CS_KSC_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 		{CS_BIG5,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 		{CS_GB_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 
  		{CS_UTF8,		CS_LATIN1,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_CP_1250,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_CP_1251,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_CP_1253,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_8859_9,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
		{CS_UTF8,		CS_SJIS,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_KSC_8BIT,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_BIG5,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_GB_8BIT,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},

#ifdef XP_OS2
                /*
                 * Define additional codepage conversions for OS/2.  All of these use the unicode
                 * based conversion tables.
 		           */
 		/* Thai */
 		{CS_CP_874,             CS_CP_874,              0, (CCCFunc)0,                          0},
 		{CS_CP_874,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_UTF8,               CS_CP_874,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},

 		/* Baltic */
 		{CS_CP_1257,    CS_CP_1257,             0, (CCCFunc)0,                          0},
 		{CS_CP_1257,    CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_UTF8,               CS_CP_1257,             0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},

 		/* Hebrew */
 		{CS_CP_862,             CS_CP_862,              0, (CCCFunc)0,                          0},
 		{CS_CP_862,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_UTF8,               CS_CP_862,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},

 		/* Arabic */
 		{CS_CP_864,             CS_CP_864,              0, (CCCFunc)0,                          0},
 		{CS_CP_864,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_UTF8,               CS_CP_864,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},

 		/* PC codepages - Default convert to windows codepages */
 		{CS_CP_850,             CS_LATIN1,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_852,             CS_LATIN2,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_855,             CS_CP_1251,     0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_857,             CS_CP_1254,             0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_866,             CS_CP_1251,     0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},

 		{CS_CP_850,             CS_CP_850,              0, (CCCFunc)0,                          0},
 		{CS_CP_852,             CS_CP_852,              0, (CCCFunc)0,                          0},
 		{CS_CP_855,             CS_CP_855,              0, (CCCFunc)0,                          0},
 		{CS_CP_857,             CS_CP_857,              0, (CCCFunc)0,                          0},
 		{CS_CP_866,             CS_CP_866,              0, (CCCFunc)0,                          0},

 		{CS_LATIN1,             CS_CP_850,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_LATIN2,             CS_CP_852,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_1251,    CS_CP_855,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_1254,    CS_CP_857,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_1251,    CS_CP_866,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},

 		{CS_CP_850,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_852,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_855,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_857,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_CP_866,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_KOI8_R,             CS_UTF8,                0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},

 		{CS_UTF8,               CS_CP_850,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},
 		{CS_UTF8,               CS_CP_852,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},
 		{CS_UTF8,               CS_CP_855,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},
 		{CS_UTF8,               CS_CP_857,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},
 		{CS_UTF8,               CS_CP_866,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},
 		{CS_UTF8,               CS_KOI8_R,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_UTF8},

 		{CS_MAC_ROMAN,  CS_LATIN1,              0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
 		{CS_LATIN1,             CS_MAC_ROMAN,   0, (CCCFunc)mz_AnyToAnyThroughUCS2,     INTL_CHARLEN_SINGLEBYTE},
#endif /* XP_OS2 */


		{CS_USER_DEFINED_ENCODING,	CS_USER_DEFINED_ENCODING,	0, (CCCFunc)0,			0},	
		{0,					0,				1, (CCCFunc)0,			0}
};

#endif	/* XP_WIN || XP_OS2 */
#ifdef XP_UNIX
MODULE_PRIVATE cscvt_t		cscvt_tbl[] = {
		/*	SINGLE BYTE */
		/*	LATIN1 */
		{CS_LATIN1,		CS_LATIN1,		0, (CCCFunc)One2OneCCC,	0},
		{CS_LATIN1,		CS_ASCII,		0, NULL,			0},
		{CS_ASCII,		CS_LATIN1,		0, NULL,			0},

		/* LATIN2 */
		{CS_LATIN2,		CS_LATIN2,		0, NULL,			0},
		{CS_LATIN2,		CS_ASCII,		0, NULL,			0},
		{CS_LATIN2,		CS_CP_1250,		0, (CCCFunc)One2OneCCC,	0},
		{CS_CP_1250,	CS_LATIN2,		0, (CCCFunc)One2OneCCC,	0},

		/* CYRILLIC */
		{CS_KOI8_R,		CS_KOI8_R,		0, NULL,			0},	
		{CS_8859_5,		CS_8859_5,		0, NULL,			0},	
		{CS_8859_5,		CS_KOI8_R,		0, (CCCFunc)One2OneCCC, 0},	
		{CS_KOI8_R,		CS_8859_5,		0, (CCCFunc)One2OneCCC, 0},	

		/* GREEK */
		{CS_8859_7,		CS_8859_7,		0, NULL,			0},	
		{CS_8859_7,		CS_CP_1253,		0, (CCCFunc)One2OneCCC,	0},
		{CS_CP_1253,	CS_8859_7,		0, (CCCFunc)One2OneCCC,	0},

		/* TURKISH */
		{CS_8859_9,		CS_8859_9,		0, NULL,			0},	

		/*  MULTIBYTE */
		/*	JAPANESE */
		{CS_EUCJP,		CS_EUCJP,		1, mz_mbNullConv,	INTL_CHARLEN_EUC_JP},
		{CS_JIS,		CS_EUCJP,		1, jis2other,		1},
		{CS_SJIS,		CS_EUCJP,		1, mz_sjis2euc,		0},
		{CS_EUCJP,		CS_SJIS,		1, mz_euc2sjis,		0},
		{CS_JIS,		CS_SJIS,		1, jis2other,		0},
		{CS_SJIS,		CS_SJIS,		1, mz_mbNullConv,   INTL_CHARLEN_SJIS},
		{CS_EUCJP,		CS_JIS,			1, mz_euc2jis,		0},
		{CS_SJIS,		CS_JIS,			1, mz_sjis2jis,		0},
		/* auto-detect Japanese conversions					*/
		{CS_JIS_AUTO,	CS_EUCJP,		1, autoJCCC,		1},
		{CS_SJIS_AUTO,	CS_EUCJP,		1, autoJCCC,		0},
		{CS_EUCJP_AUTO,	CS_EUCJP,		1, autoJCCC,		0},
		{CS_EUCJP_AUTO,	CS_SJIS,		1, autoJCCC,		0},
		{CS_JIS_AUTO,	CS_SJIS,		1, autoJCCC,		0},
		{CS_SJIS_AUTO,	CS_SJIS,		1, autoJCCC,		0},

		/*	KOREAN */
		{CS_KSC_8BIT,	CS_KSC_8BIT,	0, (CCCFunc)mz_mbNullConv,	INTL_CHARLEN_CGK},
		{CS_2022_KR,	CS_KSC_8BIT,	0, (CCCFunc)mz_iso2euckr,	0},
		{CS_KSC_8BIT,	CS_2022_KR,		0, (CCCFunc)mz_euckr2iso,	0},
		/* auto-detect Korean conversions						*/
		{CS_KSC_8BIT_AUTO,  CS_KSC_8BIT,1, (CCCFunc)autoKCCC,		0},
 		{(CS_2022_KR|CS_AUTO) ,  CS_KSC_8BIT,1, (CCCFunc)autoKCCC,		0},

		/*  SIMPLIFIED CHINESE */
		{CS_GB_8BIT,	CS_GB_8BIT,		0, (CCCFunc)mz_gb2gb,		0},

		/*  TRADITIONAL CHINESE */
		{CS_CNS_8BIT,	CS_CNS_8BIT,	0, mz_mbNullConv,	INTL_CHARLEN_CNS_8BIT},
#ifdef FEATURE_BIG5CNS
		{CS_BIG5,		CS_CNS_8BIT,	0, mz_b52cns,		0},
		{CS_CNS_8BIT,	CS_BIG5,		0, mz_cns2b5,		0},
#endif
		{CS_BIG5,		CS_BIG5,		0, mz_mbNullConv,	INTL_CHARLEN_CGK},

		{CS_USRDEF2,	CS_USRDEF2,		0, NULL,			0},	

		/* UNICODE                                           */
		{CS_UTF8,		CS_UTF8,		0, mz_mbNullConv,	INTL_CHARLEN_UTF8},
		{CS_UTF8,		CS_UCS2,		0, (CCCFunc)mz_utf82ucs,	0},
		{CS_UTF8,		CS_UTF7,		0, (CCCFunc)mz_utf82utf7,	0},
		{CS_UTF8,		CS_UCS2_SWAP,	0, (CCCFunc)mz_utf82ucsswap,	0},
		{CS_UTF8,		CS_IMAP4_UTF7,	0, (CCCFunc)mz_utf82imap4utf7,	0},
		{CS_UCS2,		CS_UTF8,		0, (CCCFunc)mz_ucs2utf8,	0},
		{CS_UCS2,		CS_UTF7,		0, (CCCFunc)mz_ucs2utf7,	0},
		{CS_UCS2_SWAP,	CS_UTF8,		0, (CCCFunc)mz_ucs2utf8,	0},
		{CS_UCS2_SWAP,	CS_UTF7,		0, (CCCFunc)mz_ucs2utf7,	0},
		{CS_UTF7,		CS_UTF8,		0, (CCCFunc)mz_utf72utf8,	0},

		{CS_LATIN1,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_LATIN2,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_8859_5,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
 		{CS_KOI8_R,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_8859_7,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
  		{CS_8859_9,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SINGLEBYTE},
		{CS_SJIS,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_SJIS},
 		{CS_EUCJP,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_EUC_JP},
 		{CS_KSC_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 		{CS_BIG5,		CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 		{CS_CNS_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CNS_8BIT},
 		{CS_GB_8BIT,	CS_UTF8,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_CGK},
 
  		{CS_UTF8,		CS_LATIN1,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_LATIN2,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_8859_5,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_KOI8_R,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_8859_7,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_8859_9,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
		{CS_UTF8,		CS_SJIS,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_EUCJP,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_KSC_8BIT,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
  		{CS_UTF8,		CS_BIG5,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
		{CS_UTF8,		CS_CNS_8BIT,	0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},
 		{CS_UTF8,		CS_GB_8BIT,		0, (CCCFunc)mz_AnyToAnyThroughUCS2,	INTL_CHARLEN_UTF8},

		{CS_IMAP4_UTF7,	CS_UTF8,		0, (CCCFunc)mz_imap4utf72utf8,	0},
		{0,				0,				0, NULL,			0}

};

#endif /* XP_UNIX */


/*
 * this routine is needed to make sure parser and layout see whole
 * characters, not partial characters
 */
PRIVATE unsigned char *
mz_gb2gb(CCCDataObject obj, const unsigned char *gbbuf, int32 gbbufsz)
{
	return mz_hz2gb(obj, gbbuf, gbbufsz);
}
PRIVATE unsigned char *
mz_hz2gb(CCCDataObject obj, const unsigned char *gbbuf, int32 gbbufsz)
{
	unsigned char *start, *p, *q;
	unsigned char *output;
	int i, j, len;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);

	q = output = XP_ALLOC(strlen((char*)uncvtbuf) + gbbufsz + 1);
	if (q == NULL)
		return NULL;

	start = NULL;

	for (j = 0; j < 2; j++)
	{
		len = 0;
		if (j == 0)
			len = strlen((char *)uncvtbuf);
		if (len)
			p = (unsigned char *) uncvtbuf;
		else
		{
			p = (unsigned char *) gbbuf ;
			len = gbbufsz;
			j = 100;  /* quit this loop next time */
		}
		for (i = 0; i < len;)
		{
			if (start)
			{
				if (*p == '~' && *(p+1) == '}')   /* switch back to ASCII mode */
				{
					for (; start < p; start++)
						*q++ = *start | 0x80;
					p += 2;
					i += 2;
					start = NULL;
				}
				else if (*p == 0x0D && *(p+1) == 0x0A)  /* Unix or Mac return */
				{
					for (; start < p; start++)
						*q++ = *start | 0x80;
					i += 2;
					*q++ = *p++;   /* 0x0D  */
					*q++ = *p++;   /* 0x0A  */
					start = NULL;   /* reset start if we see normal line return */
				}
				else if (*p == 0x0A)  /* Unix or Mac return */
				{
					for (; start < p; start++)
						*q++ = *start | 0x80;
					i ++;
					*q++ = *p++;   /* LF  */
					start = NULL;   /* reset start if we see normal line return */
				}
				else if (*p == 0x0D)  /* Unix or Mac return */
				{
					for (; start < p; start++)
						*q++ = *start | 0x80;
					i ++;
					*q++ = *p++;   /* LF  */
					start = NULL;   /* reset start if we see normal line return */
				}
				else
				{
					i ++ ;
					p ++ ;
				}
			}
			else
			{
				if (*p == '~' && *(p+1) == '{')    /* switch to GB mode */
				{
					start = p + 2;
					p += 2;
					i += 2;
				}
				else if (*p == '~' && *(p+1) == 0x0D && *(p+2) == 0x0A)  /* line-continuation marker */
				{
					i += 3;
					p += 3;
				}
				else if (*p == '~' && *(p+1) == 0x0A)  /* line-continuation marker */
				{
					i += 2;
					p += 2;
				}
				else if (*p == '~' && *(p+1) == 0x0D)  /* line-continuation marker */
				{
					i += 2;
					p += 2;
				}
				else if (*p == '~' && *(p+1) == '~')   /* ~~ means ~ */
				{
					*q++ = '~';
					p += 2;
					i += 2;
				}
				else
				{
					i ++;
					*q++ = *p++;
				}
			}
		}
	}
	*q = '\0';
	INTL_SetCCCLen(obj, q - output);
	if (start)
	{

		/* Consider UNCVTBUF_SIZE is only 8 byte long, it's not enough 
		   for HZ anyway. Let's convert leftover to GB first and deal with
		   unfinished buffer in the coming block.
		*/
		INTL_SetCCCLen(obj, INTL_GetCCCLen(obj) + p - start);
		for (; start < p; start++)
			*q++ = *start | 0x80;
		*q = '\0';

		q = uncvtbuf;
		XP_STRCPY((char *)q, "~{");
	}

    return output;
}



/*	mz_mbNullConv
 * this routine is needed to make sure parser and layout see whole
 * characters, not partial characters
 */
/* This routine is designed to replace the following routine:
	mz_euc2euc
	mz_b52b5
	mz_cns2cns
	mz_ksc2ksc
	mz_sjis2sjis
	mz_utf82utf8

   It should also replace
		mz_gb2gb
   but currently mz_gb2gb also handle hz to gb. We need to move that functionality out of mz_gb2gb
 */
PRIVATE unsigned char *
mz_mbNullConv(CCCDataObject obj, const unsigned char *buf, int32 bufsz)
{
	int32			left_over;
	int32			len;
	unsigned char	*p;
	unsigned char	*ret;
	int32			total;
	intl_CharLenFunc	CharLenFunc = intl_char_len_func[INTL_GetCCCCvtflag(obj)];
	int charlen = 0;
	
	/*	Get the unconverted buffer */
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);
	int32			uncvtsz = strlen((char *)uncvtbuf);

	/*  return in the input is nonsense */
	if ((!obj) || (! buf) || (bufsz < 0))
		return NULL;

	/*	Allocate Output Buffer */
	total = uncvtsz + bufsz;
	ret = (unsigned char *) XP_ALLOC(total + 1);
	if (!ret)
	{
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return NULL;
	}

	/*	Copy unconverted buffer into the output bufer */
	memcpy(ret, uncvtbuf, uncvtsz);
	/* Copy the current input buffer into the output buffer */
	memcpy(ret+uncvtsz, buf, bufsz);

	/*	Walk through the buffer and figure out the left_over length */
	for (p=ret, len=total, left_over=0; len > 0; p += charlen, len -= charlen)
	{
		if((charlen = CharLenFunc(*p)) > 1)
		{	/* count left_over only if it is multibyte char */
			if(charlen > len)	/* count left_over only if the len is less than charlen */
				left_over = len;
		};
	}

	/*	Copy the left over into the uncvtbuf */
	if(left_over)
		memcpy(uncvtbuf, p - charlen, left_over);
	/*  Null terminated the uncvtbuf */
	uncvtbuf[left_over] = '\0';

	/* Null terminate the return buffer and set the length */
	INTL_SetCCCLen(obj, total - left_over);
	ret[total - left_over] = 0;

	return ret;
}

/*
	buf -> mz_mbNullConv -> frombuf -> INTL_TextToUnicode -> ucs2buf
	-> INTL_UnicodeToStr -> tobuf
*/
PRIVATE unsigned char* mz_AnyToAnyThroughUCS2(CCCDataObject obj, const unsigned char *buf, int32 bufsz)
{
	/* buffers */
	unsigned char* fromBuf = NULL;
	INTL_Unicode* ucs2Buf = NULL;
	unsigned char* toBuf   = NULL;
	/* buffers' length */
	uint32 ucs2BufLen = 0;
	uint32 fromBufLen = 0;
	uint32 toBufLen   = 0;
	/* from & to csid */
	uint16 fromCsid = INTL_GetCCCFromCSID(obj);
	uint16 toCsid   = INTL_GetCCCToCSID(obj);

	/* get the fromBuf */
	if( !( fromBuf = mz_mbNullConv( obj, buf,  bufsz) ) )
		return NULL;

	/* map fromBuf -> ucs2Buf */
	fromBufLen = INTL_GetCCCLen(obj);
	ucs2BufLen = INTL_TextToUnicodeLen( fromCsid, fromBuf, fromBufLen );

	if( !( ucs2Buf = XP_ALLOC( (ucs2BufLen + 1 ) * 2)) ){
		return NULL;
	}

	/* be care, the return value is HOW MANY UNICODE IN THIS UCS2BUF, not how many bytes */
	ucs2BufLen = INTL_TextToUnicode( fromCsid, fromBuf, fromBufLen, ucs2Buf, ucs2BufLen );

	/* map ucs2Buf -> toBuf */
	toBufLen = INTL_UnicodeToStrLen( toCsid, ucs2Buf, ucs2BufLen );	/* we get BYTES here :) */

	if( !( toBuf = XP_ALLOC( toBufLen + 1 ) ) )
		return NULL;

	INTL_UnicodeToStr( toCsid, ucs2Buf, ucs2BufLen, toBuf, toBufLen );


	/* clean up after myself */
	free( fromBuf );
	free( ucs2Buf );

	/* In order to let the caller know how long the buffer is, i have to set its tail NULL. */
	toBuf[ toBufLen ] = 0;
	return toBuf;
}


PRIVATE int16 intl_CharLen_SJIS(		unsigned char ch)
{
	return (	(((ch >= 0x81) && (ch <= 0x9f)) || ((ch >= 0xe0) && (ch <= 0xfc))) ?	2 : 1);
}
PRIVATE int16 intl_CharLen_EUC_JP(	unsigned char ch)
{
	return (	(((ch >= 0xa1) && (ch <= 0xfe)) || (ch == 0x8e)) ?	2 : ((ch ==0x8f) ?	3 : 1));
}
PRIVATE int16 intl_CharLen_CGK(		unsigned char ch)
{
	return (	((ch >= 0xa1) && (ch <= 0xfe))  ?	2 :		1);
}	
PRIVATE int16 intl_CharLen_CNS_8BIT(	unsigned char ch)
{
	return (	((ch >= 0xa1) && (ch <= 0xfe))  ?	2 : ((ch == 0x8e) ?		4 : 1));
}
PRIVATE int16 intl_CharLen_UTF8(		unsigned char ch)
{
	return (	((ch >= 0xc0) && (ch <= 0xdf))  ?	2 : (((ch >= 0xe0) && (ch <= 0xef)) ?		3 : 1));
}
PRIVATE int16 intl_CharLen_SingleByte( unsigned char ch)
{
	return 1;
}


/*
 INTL_DefaultWinCharSetID,
   Based on DefaultDocCSID, it determines which Win CSID to use for Display
*/
PUBLIC int16 INTL_DefaultWinCharSetID(iDocumentContext context)
{

	if (context) {
		INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);
		if (INTL_GetCSIWinCSID(csi))
			return INTL_GetCSIWinCSID(csi);
	}

	return INTL_DocToWinCharSetID(INTL_DefaultDocCharSetID(context));
}

/*
 INTL_DocToWinCharSetID,
   Based on DefaultDocCSID, it determines which Win CSID to use for Display
*/
/*

	To Do: (ftang)

	We should seperate the DocToWinCharSetID logic from the cscvt_t table
	for Cyrillic users. 

*/
PUBLIC int16 INTL_DocToWinCharSetID(int16 csid)
{
	cscvt_t		*cscvtp;
	int16       from_csid = 0,  to_csid = 0;

	from_csid = csid & ~CS_AUTO;	/* remove auto bit  */

	/* Look-up conversion method given FROM and TO char. code sets	*/
	cscvtp = cscvt_tbl;
	while (cscvtp->from_csid)
	{
		if (cscvtp->from_csid == from_csid)
		{
/*
 * disgusting hack...
 */
#ifdef XP_UNIX
			if ((cscvtp->to_csid == CS_CNS_8BIT) && haveBig5) {
				cscvtp++;
				continue;
			}
#endif
			to_csid = cscvtp->to_csid;
			break ;
		}
		cscvtp++;
	}
	return to_csid == 0 ? CS_FE_ASCII: to_csid ;
}


XP_Bool
INTL_CanAutoSelect(int16 csid)
{
	register cscvt_t		*cscvtp;

	cscvtp = cscvt_tbl;
	while (cscvtp->from_csid) {
		if (cscvtp->from_csid == csid) {
			return (cscvtp->autoselect);	
		}
		cscvtp++;
	}
	return FALSE;
}


PUBLIC int16
INTL_DefaultTextAttributeCharSetID(iDocumentContext context)
{
	if (context)
	{
		INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
		if (INTL_GetCSIWinCSID(c))
			return INTL_GetCSIWinCSID(c);
	}

	return INTL_DefaultWinCharSetID(context);
}

void
INTL_ReportFontCharSets(int16 *charsets)
{
	uint16	len;

	if (!charsets)
	{
		return;
	}

	if (availableFontCharSets)
	{
		free(availableFontCharSets);
	}

	availableFontCharSets = charsets;

	while (*charsets)
	{
		if (*charsets == CS_X_BIG5)
		{
			haveBig5 = 1;
		}
		charsets++;
	}
	len = (charsets - availableFontCharSets);

#ifdef XP_UNIX
	INTL_SetUnicodeCSIDList(len, availableFontCharSets);

#endif

}

/*	Code for CSID Iterator */

#define NUMOFCSIDINITERATOR 15
struct INTL_CSIDIteratorPriv
{
	int16 cur;
	int16 csidlist[NUMOFCSIDINITERATOR];
};
typedef struct INTL_CSIDIteratorPriv INTL_CSIDIteratorPriv;

#ifdef MOZ_MAIL_NEWS

PRIVATE void intl_FillTryIMAP4SearchIterator(INTL_CSIDIteratorPriv* p, int16 csid);
PRIVATE void intl_FillTryIMAP4SearchIterator(INTL_CSIDIteratorPriv* p, int16 csid)
{	
	int idx = 0;
	cscvt_t		*cscvtp = cscvt_tbl;
	p->csidlist[idx++] = INTL_DefaultMailCharSetID(csid);	/* add mailcsid first */
	p->csidlist[idx++] = INTL_DefaultNewsCharSetID(csid);	/* If the news csid is different add it */
	if(p->csidlist[0] == p->csidlist[1])
		idx--;
	/* Add all the csid that we know how to convert to (Without CS_AUTO bit on */
	while (cscvtp->from_csid)
	{
		if ( (cscvtp->from_csid & ~CS_AUTO) == (csid & ~CS_AUTO))
		{	
			int16 foundcsid = cscvtp->to_csid & ~CS_AUTO;
			XP_Bool notInTheList = TRUE;
			int i;
			for(i = 0; i < idx ;i++)
			{
				if(foundcsid == p->csidlist[i])
					notInTheList = FALSE;
			}		
			if(notInTheList)
			{
				p->csidlist[idx++] = foundcsid;
				XP_ASSERT(NUMOFCSIDINITERATOR == idx);
				if(NUMOFCSIDINITERATOR == idx)
					break;
			}
		}
		cscvtp++;
	}
	p->csidlist[idx] = 0;	/* terminate the list by 0 */
}

PUBLIC void INTL_CSIDIteratorCreate( INTL_CSIDIterator* iterator, int16 csid, int flag)
{
	INTL_CSIDIteratorPriv* priv = 
		(INTL_CSIDIteratorPriv*) XP_ALLOC(sizeof(INTL_CSIDIteratorPriv));
	*iterator = (INTL_CSIDIterator) priv;
	if(priv)
	{
		priv->cur = 0;
		switch(flag)
		{
			case csiditerate_TryIMAP4Search:
				intl_FillTryIMAP4SearchIterator	(priv, 	(int16)(csid & ~CS_AUTO));
			break;
			default:
				XP_ASSERT(FALSE);
			break;
		}
	}
	return;
}

#endif  /* MOZ_MAIL_NEWS */

PUBLIC void INTL_CSIDIteratorDestroy(INTL_CSIDIterator* iterator)
{
	INTL_CSIDIteratorPriv* priv = (INTL_CSIDIteratorPriv*) *iterator;
	*iterator = NULL;
	XP_FREE(priv);
}

PUBLIC XP_Bool INTL_CSIDIteratorNext( INTL_CSIDIterator* iterator, int16* pCsid)
{
	INTL_CSIDIteratorPriv* priv = (INTL_CSIDIteratorPriv*) *iterator;
	int16 csid = priv->csidlist[(priv->cur)++];
	if(0 == csid)
	{
		return FALSE;
	}
	else
	{
		*pCsid = csid;
		return TRUE;
	}
}


#ifdef XP_OS2
/*
 * Map Netscape charset to OS/2 codepage
 */

/*
 * This is tricker then you think. For a given charset, first entry should
 * be windows codepage, second entry should be OS/2 codepage.
 */

static uint16 CS2CodePage[] = {
    CS_LATIN1     , 1004,  /*    2 */
    CS_ASCII      , 1252,  /*    1 */
    CS_UTF8       , 1208,  /*  290 */
    CS_SJIS       ,  943,  /*  260 */
    CS_8859_3     ,  913,  /*   14 */
    CS_8859_4     ,  914,  /*   15 */
    CS_8859_5     ,  915,  /*   16 ISO Cyrillic */
    CS_8859_6     , 1089,  /*   17 ISO Arabic */
    CS_8859_7     ,  813,  /*   18 ISO Greek */
    CS_8859_8     ,  916,  /*   19 ISO Hebrew */
    CS_8859_9     ,  920,  /*   20 */
    CS_BIG5       ,  950,  /*  263 */
    CS_GB2312     , 1386,  /*  287 */
    CS_CP_1250    , 1250,  /*   44 CS_CP_1250 is window Centrl Europe */
    CS_CP_1251    , 1251,  /*   41 CS_CP_1251 is window Cyrillic */
    CS_LATIN2     ,  912,  /*   10 */
    CS_CP_1253    , 1253,  /*   43 CS_CP_1253 is window Greek */
    CS_CP_1254    , 1254,  /*   45 CS_CP_1254 is window Turkish */
    CS_CP_1257    , 1257,  /*   61  Windows Baltic */
    CS_CP_1258    , 1258,  /*   62  Windows Vietnamese */
    CS_CP_850     ,  850,  /*   53  PC Latin 1 */
    CS_CP_852     ,  852,  /*   54  PC Latin 2 */
    CS_CP_855     ,  855,  /*   55  PC Cyrillic */
    CS_CP_857     ,  857,  /*   56  PC Turkish */
    CS_CP_862     ,  862,  /*   57  PC Hebrew */
    CS_CP_864     ,  864,  /*   58  PC Arabic */
    CS_CP_866     ,  866,  /*   59  PC Russian */
    CS_CP_874     ,  874,  /*   60  PC Thai    */
    CS_EUCJP      ,  930,  /*  261 */
    CS_GB_8BIT    , 1386,  /*  264 */
    CS_KOI8_R     ,  878,  /*   39 */
    CS_KSC5601    ,  949,  /*  284 */
    CS_MAC_CE     , 1282,  /*   11 */
    CS_MAC_CYRILLIC, 1283, /*   40 */
    CS_MAC_GREEK  , 1280,  /*   42 */
    CS_MAC_ROMAN  , 1275,  /*    6 */
    CS_MAC_TURKISH, 1281,  /*   46 */
    CS_UCS2       , 1200,  /*  810 */
    CS_USRDEF2    , 1252,  /*   38 */
    0,              0,
};

/*
 * MapCpToCsNum: Search table and return netscape codeset name
 */
uint16  INTL_MapCpToCsNum(uint16 cpid) {
    uint16 * up;

    up = CS2CodePage;
    while (*up) {
        if (up[1] == cpid) {
            return up[0];
        }
        up += 2;
    }
    return 0;
}


/*
 * MapCsToCpNum: Search table and return codepage
 */
uint16  INTL_MapCsToCpNum(uint16 csid) {
    uint16 * up;

    up = CS2CodePage;
    while (*up) {
        if (up[0] == csid) {
            return up[1];
        }
        up += 2;
    }
    return 0;
}


/*
 * Map from process codepage to default charset
 */
int16 INTL_MenuFontCSID(void) {
        ULONG   codepage, xxx;

    DosQueryCp(4, &codepage, &xxx);
    return INTL_MapCpToCsNum(codepage);
}


/*
 * This returns the ID for the
 */
int   INTL_MenuFontID() {
        return 0;
}

#endif /* XP_OS2 */

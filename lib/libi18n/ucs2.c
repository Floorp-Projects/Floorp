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
/*----------------------------------------------------------------------------

	Function UCS2ToValueAndInfo
	
----------------------------------------------------------------------------*/
#include "intlpriv.h"

#include "ugen.h"
#include "umap.h"
#include "csid.h"
#include "xp_mem.h"
#include "xpassert.h"
#include "unicpriv.h"
#include "libi18n.h"
#if defined(XP_WIN) || defined(XP_OS2)
#include "prlink.h"
#endif



/*	
	The following table is moved from npc.c npctocsid[] I rename it and try to get ride of npc.c
*/
PRIVATE int16	csidtable[MAXCSIDINTBL] =
{
	CS_DEFAULT,		CS_ASCII,		CS_LATIN1,		CS_JIS,			/*  0 -  3 */
	CS_SJIS,		CS_EUCJP,		CS_MAC_ROMAN,	CS_BIG5,		/*  4 -  7 */ 
	CS_GB_8BIT,		CS_CNS_8BIT,	CS_LATIN2,		CS_MAC_CE,		/*  8 - 11 */ 
	CS_KSC_8BIT,	CS_2022_KR, 	CS_8859_3,		CS_8859_4,		/* 12 - 15 */
	CS_8859_5,		CS_8859_6,		CS_8859_7,		CS_8859_8,		/* 16 - 19 */
	CS_8859_9,		CS_SYMBOL,		CS_DINGBATS,	CS_DECTECH,		/* 20 - 23 */
	CS_CNS11643_1,	CS_CNS11643_2,	CS_JISX0208,	CS_JISX0201,	/* 24 - 27 */
	CS_KSC5601,		CS_TIS620,		CS_JISX0212,	CS_GB2312,		/* 28 - 31 */
	CS_UNKNOWN,		CS_UNKNOWN,		CS_UNKNOWN,		CS_UNKNOWN,		/* 32 - 35 */
	CS_UNKNOWN,		CS_X_BIG5,		CS_UNKNOWN,		CS_KOI8_R,		/* 36 - 39 */
	CS_MAC_CYRILLIC,CS_CP_1251,		CS_MAC_GREEK,	CS_CP_1253,		/* 40 - 43 */
	CS_CP_1250,		CS_CP_1254,		CS_MAC_TURKISH,	CS_UNKNOWN,		/* 44 - 47 */
	CS_UNKNOWN,		CS_UNKNOWN,		CS_UNKNOWN,		CS_UNKNOWN,		/* 48 - 51 */
	CS_UNKNOWN,		CS_CP_850,		CS_CP_852,		CS_CP_855,		/* 52 - 55 */
	CS_CP_857,		CS_CP_862,		CS_CP_864,		CS_CP_866,		/* 56 - 59 */
	CS_CP_874,		CS_CP_1257,		CS_CP_1258,		CS_UNKNOWN,		/* 60 - 63 */	
};

#define intl_GetValidCSID(fb)	(csidtable[(fb) & (MAXCSIDINTBL - 1)])

/*
	Our global table are deivded into 256 row
	each row have 256 entries
	each entry have one value and one info
	Info field contains csid index
*/

typedef struct {
	uint16 value[256];
	unsigned char info[256];
} uRowTable;

PRIVATE uRowTable *uRowTablePtArray[256];

PRIVATE uTable* 	LoadToUCS2Table(uint16 csid);
PRIVATE void 	UnloadToUCS2Table(uint16 csid, uTable *utblPtr);
PRIVATE uTable* 	LoadFromUCS2Table(uint16 csid);
PRIVATE void 	UnloadFromUCS2Table(uint16 csid, uTable *utblPtr);
PRIVATE void 	CheckAndAddEntry(uint16 ucs2, uint16 med , uint16 csid);
PRIVATE XP_Bool 	UCS2ToValueAndInfo(uint16 ucs2, uint16* med, unsigned char* info);
PRIVATE void 	InitUCS2Table(void);

/*	
	UCS2 Table- is build into the navigator
*/
PRIVATE uint16 Ucs2Tbl[] = {
	0x0001,	0x0004,	0x0005,	0x0008,	0x0000, 0x0000, 0xFFFF, 0x0000
};
PRIVATE uint16 asciiTbl[] = {
	0x0001,	0x0004,	0x0005,	0x0008,	0x0000, 0x0000, 0x007F, 0x0000
};

#ifdef XP_UNIX

/*	Currently, we only support the Latin 1 and Japanese Table. */
/*  We will add more table here after the first run */
/*--------------------------------------------------------------------------*/
/*	Latin stuff */
PRIVATE uint16 iso88591FromTbl[] = {
#include "8859-1.uf"
};
PRIVATE uint16 iso88591ToTbl[] = {
#include "8859-1.ut"
};
/*--------------------------------------------------------------------------*/
/*	Japanese stuff */
PRIVATE uint16 JIS0208FromTbl[] = {
#include "jis0208.uf"
};
PRIVATE uint16 JIS0208ToTbl[] = {
#include "jis0208.ut"
};
PRIVATE uint16 JIS0201FromTbl[] = {
#include "jis0201.uf"
};
PRIVATE uint16 JIS0201ToTbl[] = {
#include "jis0201.ut"
};
PRIVATE uint16 JIS0212FromTbl[] = {
#include "jis0212.uf"
};
PRIVATE uint16 JIS0212ToTbl[] = {
#include "jis0212.ut"
};
PRIVATE uint16 SJISFromTbl[] = {
#include "sjis.uf"
};
PRIVATE uint16 SJISToTbl[] = {
#include "sjis.ut"
};

/*--------------------------------------------------------------------------*/
/*	Latin2 Stuff */
PRIVATE uint16 iso88592FromTbl[] = {
#include "8859-2.uf"
};
PRIVATE uint16 iso88592ToTbl[] = {
#include "8859-2.ut"
};
/*--------------------------------------------------------------------------*/
/*	Traditional Chinese Stuff */
PRIVATE uint16 CNS11643_1FromTbl[] = {
#include "cns_1.uf"
};
PRIVATE uint16 CNS11643_1ToTbl[] = {
#include "cns_1.ut"
};
PRIVATE uint16 CNS11643_2FromTbl[] = {
#include "cns_2.uf"
};
PRIVATE uint16 CNS11643_2ToTbl[] = {
#include "cns_2.ut"
};
PRIVATE uint16 Big5FromTbl[] = {
#include "big5.uf"
};
PRIVATE uint16 Big5ToTbl[] = {
#include "big5.ut"
};
/*--------------------------------------------------------------------------*/
/*	Simplified Chinese Stuff */
PRIVATE uint16 GB2312FromTbl[] = {
#include "gb2312.uf"
};
PRIVATE uint16 GB2312ToTbl[] = {
#include "gb2312.ut"
};
/*--------------------------------------------------------------------------*/
/*	Korean Stuff */
#ifdef UNICODE_1_1_BASE_KOREAN	

PRIVATE uint16 KSC5601FromTbl[] = {
#include "ksc5601.uf"
};
PRIVATE uint16 KSC5601ToTbl[] = {
#include "ksc5601.ut"
};

#else /* UNICODE_1_1_BASE_KOREAN	*/

/*	
	For UNIX the Korean UNICODE 2.0 table is u20kscgl.u[tf]
	They are GL base table that contains minimun set of Korean table that
	the UNIX actually can handle.
*/
PRIVATE uint16 KSC5601FromTbl[] = {
#include "u20kscgl.uf"			
};
PRIVATE uint16 KSC5601ToTbl[] = {
#include "u20kscgl.ut"
};

#endif /* UNICODE_1_1_BASE_KOREAN	*/


/*--------------------------------------------------------------------------*/
/*	Symbol Stuff */
PRIVATE uint16 SymbolFromTbl[] = {
#include "macsymbo.uf"
};
PRIVATE uint16 SymbolToTbl[] = {
#include "macsymbo.ut"
};

/*--------------------------------------------------------------------------*/
/*	Dingbats Stuff */
PRIVATE uint16 DingbatsFromTbl[] = {
#include "macdingb.uf"
};
PRIVATE uint16 DingbatsToTbl[] = {
#include "macdingb.ut"
};
/*--------------------------------------------------------------------------*/
PRIVATE uTable* LoadToUCS2Table(uint16 csid)
{
	switch(csid) {
	case CS_ASCII:
		return (uTable*) asciiTbl;
		
	/*	Latin stuff */
	case CS_LATIN1:
		return (uTable*) iso88591ToTbl;
		
	/*	Japanese */
	case CS_JISX0208:
		return (uTable*) JIS0208ToTbl;

	case CS_JISX0201:
		return (uTable*) JIS0201ToTbl;

	case CS_JISX0212:
		return (uTable*) JIS0212ToTbl;
	case CS_SJIS:
		return (uTable*) SJISToTbl;

	/*	Latin2 Stuff */
	case CS_LATIN2:
		return (uTable*) iso88592ToTbl;
	/*	Traditional Chinese Stuff */
	case CS_CNS11643_1:
		return (uTable*) CNS11643_1ToTbl;
	case CS_CNS11643_2:
		return (uTable*) CNS11643_2ToTbl;
	case CS_BIG5:
	case CS_X_BIG5:
		return (uTable*) Big5ToTbl;


	/*	Simplified Chinese Stuff */
	case CS_GB2312:
	case CS_GB_8BIT:
		return (uTable*) GB2312ToTbl;

	/*	Korean Stuff */
	case CS_KSC5601:
	case CS_KSC_8BIT:
		return (uTable*) KSC5601ToTbl;

	/*	Symbol Stuff */
	case CS_SYMBOL:
		return (uTable*) SymbolToTbl;

	/*	Dingbats Stuff */
	case CS_DINGBATS:
		return (uTable*) DingbatsToTbl;

	/*	UTF8 */
	case CS_UTF8:
	case CS_UCS2:
		return (uTable*) Ucs2Tbl;

	/*	Other Stuff */
	default:
		XP_ASSERT(TRUE);
		return NULL;
	}
}
PRIVATE uTable* LoadFromUCS2Table(uint16 csid)
{
	switch(csid) {
	case CS_ASCII:
		return (uTable*) asciiTbl;
		
	/*	Latin stuff */
	case CS_LATIN1:
		return (uTable*) iso88591FromTbl;
		
	/*	Japanese */
	case CS_JISX0208:
		return (uTable*) JIS0208FromTbl;

	case CS_JISX0201:
		return (uTable*) JIS0201FromTbl;

	case CS_JISX0212:
		return (uTable*) JIS0212FromTbl;

	case CS_SJIS:
		return (uTable*) SJISFromTbl;

	/*	Latin2 Stuff */
	case CS_LATIN2:
		return (uTable*) iso88592FromTbl;
	/*	Traditional Chinese Stuff */
	case CS_CNS11643_1:
		return (uTable*) CNS11643_1FromTbl;
	case CS_CNS11643_2:
		return (uTable*) CNS11643_2FromTbl;
	case CS_X_BIG5:
	case CS_BIG5:
		return (uTable*) Big5FromTbl;


	/*	Simplified Chinese Stuff */
	case CS_GB2312:
	case CS_GB_8BIT:
		return (uTable*) GB2312FromTbl;

	/*	Korean Stuff */
	case CS_KSC5601:
	case CS_KSC_8BIT:
		return (uTable*) KSC5601FromTbl;

	/*	Symbol Stuff */
	case CS_SYMBOL:
		return (uTable*) SymbolFromTbl;

	/*	Dingbats Stuff */
	case CS_DINGBATS:
		return (uTable*) DingbatsFromTbl;

	/*	UTF8 */
	case CS_UTF8:
	case CS_UCS2:
		return (uTable*) Ucs2Tbl;

		/*	Other Stuff */
	default:
		XP_ASSERT(TRUE);
		return NULL;
	}
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void UnloadToUCS2Table(uint16 csid, uTable *utblPtr)
{
	/* If we link those table in our code. We don't need to do anything to 
		unload them */
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void UnloadFromUCS2Table(uint16 csid, uTable *utblPtr)
{
	/* If we link those table in our code. We don't need to do anything to 
		unload them */
}

#endif /* XP_UNIX */

#ifdef XP_MAC
PRIVATE XP_Bool isIcelandicRoman()
{
	static int region = -1;
	if(region == -1)
	{
		region = GetScriptManagerVariable(smRegionCode);
	}
	return (verIceland == region);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE uTable* LoadUCS2Table(uint16 csid,int from)
{
	/* We need to add reference count here */
	Handle tableHandle;
	switch(csid)
	{
		case CS_ASCII:
			return (uTable*) asciiTbl;
			break;
		case CS_UCS2:
		case CS_UTF8:
			return (uTable*) Ucs2Tbl;
			break;
		case CS_MAC_ROMAN:
		/* Handle MacRoman Variant here */
			if(isIcelandicRoman())
				csid = CS_MAC_ROMAN | 0x1000; /* if this is Icelandic variant */
			break;
		default: 
			break;
	}
	tableHandle = GetResource((from ? 'UFRM' : 'UTO '), csid);
	if(tableHandle == NULL || ResError()!=noErr)
		return NULL;
	if(*tableHandle == NULL)
		LoadResource(tableHandle);
	HNoPurge(tableHandle);
	HLock(tableHandle);
	return((uTable*) *tableHandle);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void UnloadUCS2Table(uint16 csid, uTable *utblPtr, int from)
{
	/* We need to add reference count here */
	Handle tableHandle;
	switch(csid)
	{
		case CS_ASCII:
		case CS_UCS2:
		case CS_UTF8:
			return;
		case CS_MAC_ROMAN:
		/* Handle MacRoman Variant here */
			if(isIcelandicRoman())
				csid = CS_MAC_ROMAN | 0x1000; /* if this is Icelandic variant */
			break;
		default: 
			break;
	}
	tableHandle = GetResource((from ? 'UFRM' : 'UTO '), csid);
	if(tableHandle == NULL || ResError()!=noErr)
		return;
	HUnlock((Handle) tableHandle);
	HPurge(tableHandle);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE uTable* LoadToUCS2Table(uint16 csid)
{
	return LoadUCS2Table(csid, FALSE);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void UnloadToUCS2Table(uint16 csid, uTable *utblPtr)
{
	UnloadUCS2Table(csid, utblPtr, FALSE);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE uTable* LoadFromUCS2Table(uint16 csid)
{
	return LoadUCS2Table(csid, TRUE);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void UnloadFromUCS2Table(uint16 csid, uTable *utblPtr)
{
	UnloadUCS2Table(csid, utblPtr, TRUE);
}
#endif /* XP_MAC */

#if defined(XP_WIN)

#ifdef XP_WIN32
#define UNICODEDLL "UNI3200.DLL"
#define LIBRARYLOADOK(l)	(l != NULL)
#define UNICODE_LOADUCS2TABLE_SYM	"UNICODE_LOADUCS2TABLE"
#define UNICODE_UNLOADUCS2TABLE_SYM	"UNICODE_UNLOADUCS2TABLE"
#else
#define UNICODEDLL "UNI1600.DLL"
#define LIBRARYLOADOK(l)	(l >= 32)
#define UNICODE_LOADUCS2TABLE_SYM	"_UNICODE_LOADUCS2TABLE"
#define UNICODE_UNLOADUCS2TABLE_SYM	"_UNICODE_UNLOADUCS2TABLE"
#endif	/* !XP_WIN32 */

PRLibrary* uniLib = NULL;

PRIVATE uTable* LoadUCS2Table(uint16 csid, int from)
{
	uTable* ret = NULL;
	switch(csid)
	{
		case CS_ASCII:
			ret = (uTable*) asciiTbl;
			break;
		case CS_UCS2:
		case CS_UTF8:
			ret = (uTable*) Ucs2Tbl;
			break;
		default:
			if(uniLib == NULL )
				uniLib = PR_LoadLibrary(UNICODEDLL);
			if(uniLib)
			{
				typedef uTable* (*f) (uint16 i1, int i2);
				static f p = NULL;

                                if (p == NULL) {
#ifndef NSPR20
				    p = (f)PR_FindSymbol(UNICODE_LOADUCS2TABLE_SYM, uniLib);
#else
				    p = (f)PR_FindSymbol(uniLib, UNICODE_LOADUCS2TABLE_SYM);
#endif
                                }
				XP_ASSERT(p);
				if(p)
					ret = (*p)(csid, from);
			}
			break;
	}
	return ret;
}
PRIVATE void UnloadUCS2Table(uint16 csid, uTable* utblPtr, int from)
{
	switch(csid)
	{
		case CS_ASCII:
		case CS_UCS2:
		case CS_UTF8:
			break;
		default:
			if(uniLib == NULL )
				uniLib = PR_LoadLibrary(UNICODEDLL);
			if(uniLib)
			{
				typedef void (*f) (uint16 i1, uTable* i2, int i3);
				static f p = NULL;

                                if (p == NULL) {
#ifndef NSPR20
				    p = (f)PR_FindSymbol(UNICODE_UNLOADUCS2TABLE_SYM, uniLib);
#else
				    p = (f)PR_FindSymbol(uniLib, UNICODE_UNLOADUCS2TABLE_SYM);
#endif
                                }
				XP_ASSERT(p);
				if(p)
					(*p)(csid, utblPtr, from);
			}
		break;
	}
}
PRIVATE uTable* LoadToUCS2Table(uint16 csid)
{
	return(LoadUCS2Table(csid,0));
}
PRIVATE void UnloadToUCS2Table(uint16 csid, uTable *utblPtr)
{
	UnloadUCS2Table(csid, utblPtr, 0);
}
PRIVATE uTable* LoadFromUCS2Table(uint16 csid)
{
	return(LoadUCS2Table(csid,1));
}
PRIVATE void UnloadFromUCS2Table(uint16 csid, uTable *utblPtr)
{
	UnloadUCS2Table(csid, utblPtr, 1);
}
#endif /* XP_WIN */


#ifdef XP_OS2
/*
 * The basic design for OS/2 is to place all of the tables inline.
 * Since we reference most of the from tables during init to form
 * the row tables, we might as well just put them here.
 */

/*	
 * Latin 1 
 */
PRIVATE uint16 cp1252FromTbl[] = {
#include "cp1252.uf"
};
PRIVATE uint16 cp1252ToTbl[] = {
#include "cp1252.ut"
};
/*	
 * Latin 2 
 */
PRIVATE uint16 iso8859_2FromTbl[] = {
#include "8859-2.uf"
};
PRIVATE uint16 iso8859_2ToTbl[] = {
#include "8859-2.ut"
};
/*	
 * Japan
 */
PRIVATE uint16 japanFromTbl[] = {
#include "sjis.uf"
};
PRIVATE uint16 japanToTbl[] = {
#include "sjis.ut"
};
/*	
 * China (may need update for GBK)
 */
PRIVATE uint16 chinaFromTbl[] = {
#include "gb2312.uf"
};
PRIVATE uint16 chinaToTbl[] = {
#include "gb2312.ut"
};
/*	
 * Taiwan
 */
PRIVATE uint16 taiwanFromTbl[] = {
#include "big5.uf"
};
PRIVATE uint16 taiwanToTbl[] = {
#include "big5.ut"
};

/*	
 * ISO Codepages
 */
PRIVATE uint16 iso8859_3FromTbl[] = {
#include "8859-3.uf"
};
PRIVATE uint16 iso8859_3ToTbl[] = {
#include "8859-3.ut"
};
PRIVATE uint16 iso8859_4FromTbl[] = {
#include "8859-4.uf"
};
PRIVATE uint16 iso8859_4ToTbl[] = {
#include "8859-4.ut"
};
PRIVATE uint16 iso8859_5FromTbl[] = {
#include "8859-5.uf"
};
PRIVATE uint16 iso8859_5ToTbl[] = {
#include "8859-5.ut"
};
PRIVATE uint16 iso8859_6FromTbl[] = {
#include "8859-6.uf"
};
PRIVATE uint16 iso8859_6ToTbl[] = {
#include "8859-6.ut"
};
PRIVATE uint16 iso8859_7FromTbl[] = {
#include "8859-7.uf"
};
PRIVATE uint16 iso8859_7ToTbl[] = {
#include "8859-7.ut"
};
PRIVATE uint16 iso8859_8FromTbl[] = {
#include "8859-8.uf"
};
PRIVATE uint16 iso8859_8ToTbl[] = {
#include "8859-8.ut"
};
PRIVATE uint16 iso8859_9FromTbl[] = {
#include "8859-9.uf"
};
PRIVATE uint16 iso8859_9ToTbl[] = {
#include "8859-9.ut"
};

/*	
 * Windows Codepages
 */
PRIVATE uint16 cp1250FromTbl[] = {
#include "cp1250.uf"
};
PRIVATE uint16 cp1250ToTbl[] = {
#include "cp1250.ut"
};
PRIVATE uint16 cp1251FromTbl[] = {
#include "cp1251.uf"
};
PRIVATE uint16 cp1251ToTbl[] = {
#include "cp1251.ut"
};
PRIVATE uint16 cp1253FromTbl[] = {
#include "cp1253.uf"
};
PRIVATE uint16 cp1253ToTbl[] = {
#include "cp1253.ut"
};
PRIVATE uint16 cp1254FromTbl[] = {
#include "cp1254.uf"
};
PRIVATE uint16 cp1254ToTbl[] = {
#include "cp1254.ut"
};
PRIVATE uint16 cp1257FromTbl[] = {
#include "cp1257.uf"
};
PRIVATE uint16 cp1257ToTbl[] = {
#include "cp1257.ut"
};

/*	
 * Russian
 */
PRIVATE uint16 koi8rFromTbl[] = {
#include "koi8r.uf"
};
PRIVATE uint16 koi8rToTbl[] = {
#include "koi8r.ut"
};

/*	
 * OS/2 Codepages
 */
PRIVATE uint16 cp850FromTbl[] = {
#include "cp850.uf"
};
PRIVATE uint16 cp850ToTbl[] = {
#include "cp850.ut"
};
PRIVATE uint16 cp852FromTbl[] = {
#include "cp852.uf"
};
PRIVATE uint16 cp852ToTbl[] = {
#include "cp852.ut"
};
PRIVATE uint16 cp855FromTbl[] = {
#include "cp855.uf"
};
PRIVATE uint16 cp855ToTbl[] = {
#include "cp855.ut"
};
PRIVATE uint16 cp857FromTbl[] = {
#include "cp857.uf"
};
PRIVATE uint16 cp857ToTbl[] = {
#include "cp857.ut"
};
PRIVATE uint16 cp862FromTbl[] = {
#include "cp862.uf"
};
PRIVATE uint16 cp862ToTbl[] = {
#include "cp862.ut"
};
PRIVATE uint16 cp864FromTbl[] = {
#include "cp864.uf"
};
PRIVATE uint16 cp864ToTbl[] = {
#include "cp864.ut"
};
PRIVATE uint16 cp866FromTbl[] = {
#include "cp866.uf"
};
PRIVATE uint16 cp866ToTbl[] = {
#include "cp866.ut"
};
PRIVATE uint16 cp874FromTbl[] = {
#include "cp874.uf"
};
PRIVATE uint16 cp874ToTbl[] = {
#include "cp874.ut"
};

/*	
 * Korea
 */
PRIVATE uint16 koreaFromTbl[] = {
#include "ksc5601.uf"
};
PRIVATE uint16 koreaToTbl[] = {
#include "ksc5601.ut"
};


/*	
 * Symbol 
 */
PRIVATE uint16 symbolFromTbl[] = {
#include "macsymbo.uf"
};
PRIVATE uint16 symbolToTbl[] = {
#include "macsymbo.ut"
};

/*	
 * Mac roman
 */
PRIVATE uint16 macromanFromTbl[] = {
#include "macroman.uf"
};
PRIVATE uint16 macromanToTbl[] = {
#include "macroman.ut"
};

/*	
 * Dingbats Stuff 
 */
PRIVATE uint16 dingbatFromTbl[] = {
#include "macdingb.uf"
};
PRIVATE uint16 dingbatToTbl[] = {
#include "macdingb.ut"
};

/*
 * Return the address of the To table given the codeset
 */
PRIVATE uTable* LoadToUCS2Table(uint16 csid) {
	switch(csid) {
	case CS_ASCII:
	case CS_LATIN1:		return (uTable*) cp1252ToTbl;
 	case CS_UTF8:
	case CS_UTF7:
	case CS_UCS2:       return (uTable*) Ucs2Tbl;
	case CS_LATIN2:		return (uTable*) iso8859_2ToTbl;
	case CS_SJIS:		return (uTable*) japanToTbl;
	case CS_BIG5:		return (uTable*) taiwanToTbl;
	case CS_GB_8BIT:	return (uTable*) chinaToTbl;
	case CS_8859_3:		return (uTable*) iso8859_3ToTbl;
	case CS_8859_4:		return (uTable*) iso8859_4ToTbl;
	case CS_8859_5:		return (uTable*) iso8859_5ToTbl;
	case CS_8859_6:		return (uTable*) iso8859_6ToTbl;
	case CS_8859_7:		return (uTable*) iso8859_7ToTbl;
	case CS_8859_8:		return (uTable*) iso8859_8ToTbl;
	case CS_8859_9:		return (uTable*) iso8859_9ToTbl;
	case CS_CP_1250:	return (uTable*) cp1250ToTbl;
	case CS_CP_1251:	return (uTable*) cp1251ToTbl;
	case CS_CP_1253:	return (uTable*) cp1253ToTbl;
	case CS_CP_1254:	return (uTable*) cp1254ToTbl;
	case CS_CP_1257:	return (uTable*) cp1257ToTbl;
	case CS_CP_850:		return (uTable*) cp850ToTbl;
	case CS_CP_852:		return (uTable*) cp852ToTbl;
	case CS_CP_855:		return (uTable*) cp855ToTbl;
	case CS_CP_857:		return (uTable*) cp857ToTbl;
	case CS_CP_862:		return (uTable*) cp862ToTbl;
	case CS_CP_864:		return (uTable*) cp864ToTbl;
	case CS_CP_866:		return (uTable*) cp866ToTbl;
	case CS_CP_874:		return (uTable*) cp874ToTbl;
	case CS_KOI8_R:		return (uTable*) koi8rToTbl;
	case CS_KSC_8BIT:	return (uTable*) koreaToTbl;
	case CS_MAC_ROMAN:	return (uTable*) macromanToTbl;
	case CS_SYMBOL:		return (uTable*) symbolToTbl;
	case CS_DINGBATS:	return (uTable*) dingbatToTbl;
	}
	return (uTable*) cp1252ToTbl;   /* This should not happen */
}

/*
 * Return the address of the From table given the codeset
 */
PRIVATE uTable* LoadFromUCS2Table(uint16 csid) {
	switch(csid) {
	case CS_ASCII:
	case CS_LATIN1:		return (uTable*) cp1252FromTbl;
 	case CS_UTF8:
	case CS_UTF7:
	case CS_UCS2:       return (uTable*) Ucs2Tbl;
	case CS_LATIN2:		return (uTable*) iso8859_2FromTbl;
	case CS_SJIS:		return (uTable*) japanFromTbl;
	case CS_BIG5:		return (uTable*) taiwanFromTbl;
	case CS_GB_8BIT:	return (uTable*) chinaFromTbl;
	case CS_8859_3:		return (uTable*) iso8859_3FromTbl;
	case CS_8859_4:		return (uTable*) iso8859_4FromTbl;
	case CS_8859_5:		return (uTable*) iso8859_5FromTbl;
	case CS_8859_6:		return (uTable*) iso8859_6FromTbl;
	case CS_8859_7:		return (uTable*) iso8859_7FromTbl;
	case CS_8859_8:		return (uTable*) iso8859_8FromTbl;
	case CS_8859_9:		return (uTable*) iso8859_9FromTbl;
	case CS_CP_1250:	return (uTable*) cp1250FromTbl;
	case CS_CP_1251:	return (uTable*) cp1251FromTbl;
	case CS_CP_1253:	return (uTable*) cp1253FromTbl;
	case CS_CP_1254:	return (uTable*) cp1254FromTbl;
	case CS_CP_1257:	return (uTable*) cp1257FromTbl;
	case CS_CP_850:		return (uTable*) cp850FromTbl;
	case CS_CP_852:		return (uTable*) cp852FromTbl;
	case CS_CP_855:		return (uTable*) cp855FromTbl;
	case CS_CP_857:		return (uTable*) cp857FromTbl;
	case CS_CP_862:		return (uTable*) cp862FromTbl;
	case CS_CP_864:		return (uTable*) cp864FromTbl;
	case CS_CP_866:		return (uTable*) cp866FromTbl;
	case CS_CP_874:		return (uTable*) cp874FromTbl;
	case CS_KOI8_R:		return (uTable*) koi8rFromTbl;
	case CS_KSC_8BIT:	return (uTable*) koreaFromTbl;
	case CS_MAC_ROMAN:	return (uTable*) macromanFromTbl;
	case CS_SYMBOL:		return (uTable*) symbolFromTbl;
	case CS_DINGBATS:	return (uTable*) dingbatFromTbl;
	}
	return (uTable*) cp1252FromTbl;   /* This should not happen */
}


/*
 * Null functions since the tables are inline
 */
PRIVATE void UnloadToUCS2Table(uint16 csid, uTable *utblPtr) {}
PRIVATE void UnloadFromUCS2Table(uint16 csid, uTable *utblPtr) {}

#endif /* XP_OS2 */

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE uRowTable* AddAndInitOneRow(uint16 hb)
{
	/*	Allocate uRowTablePtArray[hb] and initialize it */
	uint16 i;
	uRowTable *row = XP_ALLOC(sizeof(uRowTable));
	if(row == NULL)
	{
		XP_ASSERT(row != 0);
		return NULL;
	}
	else
	{
		for(i = 0; i < 256 ;i++)
		{
			row->value[i] = NOMAPPING;
			row->info[i] = 0;
		}
		uRowTablePtArray[hb] = row;
	}
	return row;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void AddAndInitAllRows(void)
{
	uint16 i;
	for(i=0;i<256;i++)
		(void)  AddAndInitOneRow(i);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE XP_Bool RowUsed(uint16 rownum)
{
	uint16 c;
	uRowTable *row = uRowTablePtArray[ rownum] ;
	
	for(c=0;c<256;c++)
	{
		if(row->value[c] != NOMAPPING)
			return TRUE;
	}
	return FALSE;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void FreeRow(uint16 row)
{
	XP_FREE(uRowTablePtArray[row]);
	uRowTablePtArray[row] = NULL;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void FreeUnusedRows(void)
{
	uint16 i;
	for(i=0;i<256;i++)
	{
		if(! RowUsed(i))
			FreeRow(i);
	}
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE void CheckAndAddEntry(uint16 ucs2,  uint16 med, uint16 csid)
{
	uint16 lb = ucs2 & 0x00FF;
	uRowTable *row = uRowTablePtArray[ucs2 >> 8];
	if(row->value[lb] == NOMAPPING)
	{
		row->value[lb]= med;
		row->info[lb]= (csid & 0xFF);
	}
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE 
XP_Bool 	
UCS2ToValueAndInfo(uint16 ucs2, uint16* med, unsigned char* info)
{
	uRowTable *uRowTablePtr = uRowTablePtArray[(ucs2 >> 8)];
	if( uRowTablePtr == NULL)
		return FALSE;
	*med = 	uRowTablePtr->value[(ucs2 & 0x00ff)];
	if(*med == NOMAPPING)
	{
		return FALSE; 
	}
	else
	{
		*info = uRowTablePtr->info[(ucs2 & 0x00ff)];
		return TRUE; 
	}
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

PRIVATE void InitUCS2Table(void)
{
	int16 i;
	for(i=0;i<256; i++)
		uRowTablePtArray[i] = NULL;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC XP_Bool 
UCS2_To_Other(
	uint16 ucs2, 
	unsigned char *out, 
	uint16 outbuflen, 
	uint16* outlen,
	int16 *outcsid
)
{
	uint16 med;
	unsigned char info;
	uShiftTable* shiftTable;
#ifdef XP_MAC
	if(ucs2 == 0x000a)
		ucs2 = 0x000d;
#endif
	if(UCS2ToValueAndInfo(ucs2, &med, &info))
	{
		*outcsid = intl_GetValidCSID(info);
		XP_ASSERT(*outcsid != CS_UNKNOWN);	
		shiftTable = InfoToShiftTable(info);
		XP_ASSERT(shiftTable);	
	 	return uGenerate(shiftTable, (int32*)0, med, out,outbuflen, outlen);
 	}
	return FALSE;
}


PRIVATE int16* unicodeCSIDList = NULL;
PRIVATE unsigned char** unicodeCharsetNameList = NULL;
PRIVATE uint16 numOfUnicodeList = 0;

PUBLIC int16*  INTL_GetUnicodeCSIDList(int16 * outnum)
{
	*outnum  = numOfUnicodeList;
	return unicodeCSIDList;
}
PUBLIC unsigned char **INTL_GetUnicodeCharsetList(int16 * outnum)
{
	*outnum  = numOfUnicodeList;
	return unicodeCharsetNameList;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC void	INTL_SetUnicodeCSIDList(uint16 numOfItems, int16* csidlist)
{
	int i;
	uTable* utbl;

	/* This function should be called once only */
	XP_ASSERT(unicodeCSIDList == NULL);	
	XP_ASSERT(unicodeCharsetNameList == NULL);	

	unicodeCSIDList = XP_ALLOC(sizeof(int16) * numOfItems);
	/* needs to handle no memory */
	XP_ASSERT(unicodeCSIDList != NULL);

	unicodeCharsetNameList = XP_ALLOC(sizeof(unsigned char*) * numOfItems);
	/* needs to handle no memory*/
	XP_ASSERT(unicodeCharsetNameList != NULL);

	numOfUnicodeList = numOfItems;
	InitUCS2Table();

	AddAndInitAllRows();
	/*	Add the first table */
	for(i = 0 ; i < numOfItems; i++)
	{
		unicodeCSIDList[i] = csidlist[i];
		unicodeCharsetNameList[i]=  INTL_CsidToCharsetNamePt(csidlist[i]);
		if(	(csidlist[i] != CS_UTF8 ) &&
			((utbl = LoadFromUCS2Table(csidlist[i])) != NULL))
		{
			uMapIterate(utbl,CheckAndAddEntry, csidlist[i]);
			UnloadFromUCS2Table(csidlist[i],utbl);
			
		}
	}
	FreeUnusedRows();
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
typedef struct UnicodeConverterPriv UnicodeConverterPriv;

typedef UnicodeConverterPriv* INTL_UnicodeToStrIteratorPriv;
struct UnicodeConverterPriv
{
	INTL_Unicode		*ustr;
	uint32			ustrlen;
};


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*	Pricate Function Declartion */
PRIVATE
XP_Bool 
UnicodeToStrWithFallback_p(
	uint16 ucs2, 
	unsigned char *out, 
	uint32  outbuflen, 
	uint32* outlen,
	uint16 *outcsid
);
PRIVATE 
XP_Bool 
UnicodeToStrFirst_p(
	uint16 ucs2, 
	unsigned char *out, 
	uint32 outbuflen, 
	uint32* outlen,
	uint16 *outcsid
);
PRIVATE
XP_Bool 
UnicodeToStrNext_p(
	uint16 ucs2, 
	unsigned char *out, 
	uint32 outbuflen, 
	uint32* outlen, 
	uint16 lastcsid
);



/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/* 	the return of FLASE of this funciton only mean one thing - the outbuf 
	is not enough for this conversion */
PRIVATE
XP_Bool 
UnicodeToStrWithFallback_p(
	uint16 ucs2, 
	unsigned char *out, 
	uint32 outbuflen, 
	uint32* outlen,
	uint16 *outcsid)
{
	uint16 outlen16;
	if(! UCS2_To_Other(ucs2, out, (uint16)outbuflen, &outlen16, (int16 *)outcsid))
	{
		if(outbuflen > 2)
		{
#ifdef XP_MAC
			*outcsid = CS_MAC_ROMAN;
#else
			*outcsid = CS_LATIN1;
#endif
			out[0]= '?';
			*outlen =1;
			return TRUE;
		}
		else
			return FALSE;
	}
	else
	{
		*outlen = outlen16;
	}
	return TRUE;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE 
XP_Bool 
UnicodeToStrFirst_p(
	uint16 ucs2, 
	unsigned char *out, 
	uint32 outbuflen, 
	uint32* outlen,
	uint16 *outcsid)
{
  return  UnicodeToStrWithFallback_p(ucs2,out,outbuflen,outlen,outcsid);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE
XP_Bool 
UnicodeToStrNext_p(
	uint16 ucs2, 
	unsigned char *out, 
	uint32 outbuflen, 
	uint32* outlen, 
	uint16 lastcsid)
{
	uint16 thiscsid;
	XP_Bool retval = 
		UnicodeToStrWithFallback_p(ucs2,out,outbuflen,outlen,&thiscsid);
	return (retval && (thiscsid == lastcsid));
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

PUBLIC INTL_UnicodeToStrIterator    
INTL_UnicodeToStrIteratorCreate(
	INTL_Unicode*        	 ustr,
    uint32            		 ustrlen,
    INTL_Encoding_ID     	*encoding,
    unsigned char*         	 dest, 
    uint32            		 destbuflen
)
{
	UnicodeConverterPriv* priv=0;
	priv=XP_ALLOC(sizeof(UnicodeConverterPriv));
	if(priv)
	{
		priv->ustrlen = ustrlen;
		priv->ustr = ustr;
		(void)INTL_UnicodeToStrIterate((INTL_UnicodeToStrIterator)priv, 
									encoding, dest, destbuflen);
	}
	else
	{
		*encoding = 0;
		dest[0] = '\0';
	}
	return (INTL_UnicodeToStrIterator)priv;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

PUBLIC int INTL_UnicodeToStrIterate(
    INTL_UnicodeToStrIterator     	iterator,
    INTL_Encoding_ID         		*encoding,
    unsigned char*            		dest, 
    uint32                    		destbuflen
)
{
    	unsigned char* orig = dest;
	UnicodeConverterPriv* priv = (UnicodeConverterPriv*)iterator;
	if(destbuflen < 2)		/* we want to make sure there at least two byte in the buffer */
		return 0;			/* first one for the first char, second one for the NULL */ 
    destbuflen -= 1;		/* resever one byte for NULL terminator */
	if((priv == NULL) || ((priv->ustrlen) == 0))
	{
		*encoding = 0;
		dest[0]='\0';
		return 0;
	}
	else
	{
		uint32 len = 0;
		if(UnicodeToStrFirst_p(*(priv->ustr),  
				dest,destbuflen,&len,encoding))
		{
			do{
				dest += len;
				destbuflen -= len;
				priv->ustr += 1;
				priv->ustrlen -= 1 ;
			} while(	(destbuflen > 0) && 
						((priv->ustrlen > 0)) && 
						UnicodeToStrNext_p(*(priv->ustr), dest, destbuflen, 
											&len, *encoding));
		}
		dest[0] = '\0';
		return (orig != dest);
	}
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC void    
INTL_UnicodeToStrIteratorDestroy(
	INTL_UnicodeToStrIterator iterator
)
{
	UnicodeConverterPriv* priv = (UnicodeConverterPriv*)iterator;
	if(priv)
		XP_FREE(priv);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC uint32 INTL_UnicodeLen(INTL_Unicode *ustr)
{
	uint32 i;
	for(i=0;*ustr++;i++)	
		;
	return i;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC uint32    INTL_UnicodeToStrLen(
    INTL_Encoding_ID encoding,
    INTL_Unicode*    ustr,
    uint32 ustrlen
)
{
	/* for now, put a dump algorithm to caculate the length */
	return ustrlen * ((encoding & MULTIBYTE) ?  4 : 1) + 1;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE 
int
LoadUCS2TableSet(uint16 csid, uTableSet* tableset,int from)
{
	UnicodeTableSet* set;
	int i;
	for(i=0;i<MAXINTERCSID;i++)
	{
		tableset->range[i].intercsid=CS_DEFAULT;
		tableset->tables[i]=NULL;
		tableset->shift[i] = NULL;
		tableset->range[i].min = 0xff;
		tableset->range[i].max = 0x00;
	}
	set = GetUnicodeTableSet(csid);
	/* If the conversion is a combination of several csid conversion, We try  	*/
	/* to load all of them now.   												*/
	/* Otherwise, we simply load the one for the csid						   	*/
	if(set == NULL)
	{
		tableset->range[0].intercsid=csid;
		if(from)
			  tableset->tables[0]=LoadFromUCS2Table(csid);
		else
			  tableset->tables[0]=LoadToUCS2Table(csid);
		tableset->shift[0] = GetShiftTableFromCsid(csid);
		tableset->range[0].min = 0x00;
		tableset->range[0].max = 0xff;
		return 1;
	}
	else 
	{
		for(i=0;((i<MAXINTERCSID) && (set->range[i].intercsid != CS_DEFAULT));i++)
		{
			tableset->range[i].intercsid=set->range[i].intercsid;
			tableset->range[i].min = set->range[i].min;
			tableset->range[i].max = set->range[i].max;
			if(from)
				  tableset->tables[i]=LoadFromUCS2Table(set->range[i].intercsid);
			else
				  tableset->tables[i]=LoadToUCS2Table(set->range[i].intercsid);
			tableset->shift[i] = GetShiftTableFromCsid(set->range[i].intercsid);
			XP_ASSERT(tableset->shift[i]);
			XP_ASSERT(tableset->tables[i]);
		}
		return i;
	}
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PRIVATE 
void 	
UnloadUCS2TableSet(uTableSet *tableset,int from)
{
	int i;
	if(tableset == NULL)
		return;
	for(i=0;i<MAXINTERCSID;i++)
	{
        if((tableset->range[i].intercsid != CS_DEFAULT) && (tableset->tables[i] != NULL))
		{
			if(from)
				UnloadFromUCS2Table(tableset->range[i].intercsid, tableset->tables[i]);
			else
				UnloadToUCS2Table(tableset->range[i].intercsid, tableset->tables[i]);
		}
		tableset->range[i].intercsid=CS_DEFAULT;
		tableset->tables[i]=NULL;
		tableset->shift[i] = NULL;
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*
 * utf8_to_local_encoding - UTF8 to Locally Encoded segment
 *
 * Convert a utf8 string to a Locally Encoded string.
 * Convert as characters until the encoding changes or
 * input/output space runs out.
 *
 * The segment is NOT NULL TERMINATED
 *
 * inputs: utf8 string & length
 *         buffer (pre-allocated) to hold Locally Encoded string
 *         pointer to return encoding csid
 *         pointer to return strlen of Encoded string
 *
 * output: values written to Locally Encoded string buffer
 *         encoding csid set: 
 *              >0 if successful
 *              -1 if not unicode
 *              -2 if no local encoding
 *         length of utf8 string converted returned
 *         strlen of Locally Encoded string 
 *         
 */
PUBLIC int
utf8_to_local_encoding(const unsigned char *utf8p, const int utf8len,
						unsigned char *LE_string, int LE_string_len,
						int *LE_written_len, int16 *LE_string_csid)
{
	int parsed_len = 0;
	int written_len = 0;
	int16 i, utf8_char_len;
	uint16 ucs2_char;
	int16 seg_encoding;
	int16 out_char_len, out_char_encoding;
	unsigned char tmpbuf[10];
	XP_Bool result;

	/*
	 * get segment encoding (encoding of first character)
	 */
	utf8_char_len = utf8_to_ucs2_char(utf8p, (int16)utf8len, &ucs2_char);
	if (utf8_char_len == -1) {
		/* its not unicode/utf8 but try to convert */
		/* it anyway so the user can see something */
		seg_encoding = -1;
	}
	else if (utf8_char_len == -2) /* not enough input characters */
		return 0;
	else {
		result = UCS2_To_Other(ucs2_char, tmpbuf, (uint16)10,
											(uint16*)&out_char_len, (int16*)&seg_encoding);
		if (result == FALSE) /* failed to convert */
			seg_encoding = -2; /* no local encoding */
	}

	/*
	 * loop converting the string
	 */
	while (1) {
		/*
		 * convert utf8 to UCS2
		 */
		utf8_char_len = utf8_to_ucs2_char(utf8p+parsed_len, (int16)(utf8len-parsed_len),
														&ucs2_char);
		if (utf8_char_len == -1) { /* not utf8 */
			utf8_char_len = 1;
			out_char_encoding = -1;
			tmpbuf[0] = *(utf8p+parsed_len);
			out_char_len = 1;
		}
		else if (utf8_char_len == -2) /* no input/output space */
			break;
		else {
			/*
			 * convert UCS2 to local encoding
			 */
			result = UCS2_To_Other(ucs2_char, tmpbuf, (uint16)10,
											(uint16*)&out_char_len, (int16*)&out_char_encoding);
			if (result == FALSE) { /* failed to convert */
				out_char_encoding = -2; /* no local encoding */
				tmpbuf[0] = '?'; /* place holder */
				out_char_len = 1;
			}
		}

		/* stop if not the same encoding */
		if (out_char_encoding != seg_encoding)
			break;

		/* stop if out of space for output characters */
		if ((written_len+out_char_len) >= LE_string_len-1)
			break;

		/*
		 * add this character to the segment
		 */
		for (i=0; i<out_char_len; i++) {
			LE_string[written_len+i] = tmpbuf[i];
		}
		written_len += out_char_len;
		parsed_len += utf8_char_len;
	}

	/* return encoding */
	*LE_string_csid = seg_encoding;
	LE_string[written_len] = '\0';
	*LE_written_len = written_len;
	/* return # of utf8 bytes parsed */
	return parsed_len;
}




/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC void    INTL_UnicodeToStr(
    INTL_Encoding_ID encoding,
    INTL_Unicode*    ustr,
    uint32 			 ustrlen,
    unsigned char*   dest,
    uint32           destbuflen
)
{
#define INVALID_ENCODING_ID	-999

	uint16 u;
	uint16 med;
	uint32 cur;
	static uint16			num;
	static uTableSet		tableset;
	static INTL_Encoding_ID	lastEncoding = (INTL_Encoding_ID)INVALID_ENCODING_ID;

	if (encoding != lastEncoding)
	{
		/* Unload all the table we need */
		if (lastEncoding != INVALID_ENCODING_ID)
			UnloadUCS2TableSet(&tableset,TRUE);

		/* load all the table we need */
		num = LoadUCS2TableSet(encoding, &tableset,TRUE);

		lastEncoding = encoding;
	}

	/* For every character */
	for(cur=0; cur < ustrlen ;cur++)
	{
		int i;
		u = (*ustr++);
#ifdef XP_MAC
		if(u == 0x000a)
			u = 0x000d;
#endif
	/* Loop to every table it need to convert */
		for(i=0;i<num;i++)
		{
			if((tableset.tables[i] != NULL) &&
			   (uMapCode(tableset.tables[i],u, &med)))
					break;
		}
		if(i!=num)
		{
			uint16 outlen;
			XP_Bool ret;
			/* MAP one, gen it */
			ret = uGenerate(tableset.shift[i], 
				(int32*)0, 
				med, 
				dest,
				(uint16)destbuflen, 
				&outlen);

			XP_ASSERT(ret);

			dest+=outlen;
			destbuflen += outlen;
		}
		else
		{
			/* Ok! right before we fall back. We take care C0 area here */
			if(u <= 0x0020)
			{
				/* cannot map one, gen the fallback */
				*dest++ = (unsigned char)u;
				destbuflen--;
			}
			else
			{			
				XP_ASSERT(destbuflen > 1);

				/* cannot map one, gen the fallback */
				*dest++ = '?';
				destbuflen--;
			}
		}
	}
	XP_ASSERT(destbuflen > 0);
	*dest = '\0';	/* NULL terminate it */
}
/*
	intl_check_unicode_question
	Used by INTL_UnicodeToEncodingStr
*/
PRIVATE uint32 intl_check_unicode_question(
    INTL_Unicode*    ustr,
    uint32 			 ustrlen
)
{
	INTL_Unicode* p;
	uint32 count = 0;
	for(p=ustr; ustrlen > 0  ;ustrlen--, p++)
		if(*p == 0x003F)
			count++;
	return count;	
}
/*
	intl_check_unknown_unicode
	Used by INTL_UnicodeToEncodingStr
*/
PRIVATE uint32 intl_check_unknown_unicode(unsigned char*   buf)
{
	unsigned char* p;
	uint32 count = 0;
	for(p=buf; *p != '\0'; p++)
		if(*p == '?')
			count++;
	return count;	
}
/*
	INTL_UnicodeToEncodingStr
	This is an Trail and Error function which may wast a lot of performance in "THE WORST CASE"
	However, it do it's best in the best case and average case.
	IMPORTANT ASSUMPTION: The unknown Unicode is fallback to '?'
*/
PUBLIC INTL_Encoding_ID    INTL_UnicodeToEncodingStr(
    INTL_Unicode*    ustr,
    uint32 			 ustrlen,
    unsigned char*   dest,
    uint32           destbuflen
)
{
    INTL_Encoding_ID latin1_encoding, encoding, min_error_encoding, last_convert_encoding;
    uint32 min, question;
    int16 *encodingList;
    int16 itemCount;
    int16 idx;
    
#ifdef XP_MAC
	encoding = latin1_encoding = CS_MAC_ROMAN;
#else
	encoding = latin1_encoding =  CS_LATIN1;
#endif
	/* Ok, let's try them with Latin 1 first. I believe this is for most of the case */
	INTL_UnicodeToStr(encoding,ustr,ustrlen,dest,destbuflen);
	/* Try to find the '?' in the converted string */
	min = intl_check_unknown_unicode(dest);
	if(min == 0)		/* No '?' in the converted string, it could be convert to Latin 1 */
		return encoding;
	/* The origional Unicode may contaion some '?' in unicode. Let's count it */		
	question = intl_check_unicode_question(ustr,ustrlen );
	/* The number of  '?' in the converted string match the number in unicode */
	if(min == question)	
		return encoding;
	
	last_convert_encoding = min_error_encoding = encoding;
		
	encodingList = INTL_GetUnicodeCSIDList(&itemCount);
	for(idx = 0; idx < itemCount ; idx++)
	{
		encoding = encodingList[idx];
		/* Let's ignore the following three csid
			the latin1 (we already try it 
			Symbol an Dingbat
		*/
		if((encoding != latin1_encoding) && 
		   (encoding != CS_SYMBOL) &&
		   (encoding != CS_DINGBATS))
		{   
	    	uint32 unknowInThis;
	    	last_convert_encoding = encoding;
			INTL_UnicodeToStr(encoding,ustr,ustrlen,dest,destbuflen);
			unknowInThis = intl_check_unknown_unicode(dest);
			/* The number of  '?' in the converted string match the number in unicode */
			if(unknowInThis == question)	/* what a perfect candidcate */
				return encoding;
			/* The number of  '?' is less then the previous smallest */
			if(unknowInThis < min)
			{   /* let's remember the encoding and the number of '?' */
				min = unknowInThis;
				min_error_encoding = encoding;
			}
		}
	}
	/* The min_error_encoding is not the last one we try to convert to. 
		We need to convert it again */
	if(min_error_encoding != last_convert_encoding)
		INTL_UnicodeToStr(min_error_encoding,ustr,ustrlen,dest,destbuflen);
	return min_error_encoding;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC uint32    INTL_StrToUnicodeLen(
    INTL_Encoding_ID encoding,
    unsigned char*    src
)
{
	/* for now, put a dump algorithm to caculate the length */
	return INTL_TextToUnicodeLen(encoding, src, XP_STRLEN((char*)src));
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
PUBLIC uint32    INTL_StrToUnicode(
    INTL_Encoding_ID encoding,
    unsigned char*   src,
    INTL_Unicode*    ustr,
    uint32           ubuflen
)
{
	uint32  len = XP_STRLEN((char*)src);
	return INTL_TextToUnicode(encoding,src,len,ustr,ubuflen);
}
PUBLIC uint32    INTL_TextToUnicodeLen(
    INTL_Encoding_ID encoding,
    unsigned char*   src,
    uint32			 srclen
)
{
	/* for now, put a dump algorithm to caculate the length */
	return srclen + 1;
}
PUBLIC uint32    INTL_TextToUnicode(
    INTL_Encoding_ID encoding,
    unsigned char*   src,
    uint32			 srclen,
    INTL_Unicode*    ustr,
    uint32           ubuflen
)
{
	/*
 	 * Use the Netscape conversion tables
	 */
	uint32	validlen;
	uint16 num,scanlen, med;
	uTableSet tableset;
	num = LoadUCS2TableSet(encoding, &tableset,FALSE);
	for(validlen=0;	((srclen > 0) && ((*src) != '\0') && (ubuflen > 1));
		srclen -= scanlen, src += scanlen, ustr++, ubuflen--,validlen++)
	{
		uint16 i;
		if(*src < 0x20)
		{
			*ustr = (INTL_Unicode)(*src);
			scanlen = 1;
			continue;
		}
		for(i=0;i<num;i++)
		{
			if((tableset.tables[i] != NULL) &&
			   (tableset.range[i].min <= src[0]) &&	
			   (src[0] <= tableset.range[i].max) &&	
			   (uScan(tableset.shift[i],(int32*) 0,src,&med,(uint16)srclen,&scanlen)))
			{
				uMapCode(tableset.tables[i],med, ustr);
				if(*ustr != NOMAPPING)
					break;
			}
		}
		if(i==num)
		{
#ifdef STRICTUNICODETEST
			XP_ASSERT(i!=num);
#endif
			*ustr= NOMAPPING;
			scanlen=1;
		}
	}
	*ustr = (INTL_Unicode) 0;
	/* Unload all the table we need */
	UnloadUCS2TableSet(&tableset,FALSE);
	return validlen;
}

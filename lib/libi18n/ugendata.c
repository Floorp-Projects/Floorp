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
#include "intlpriv.h"
#include "ugen.h"
#include "unicpriv.h"
#include "prtypes.h"	/* For IS_LITTLE_ENDIAN */

#ifdef XP_WIN
/*	We try to move to UNICODE 2.0 BASE 
	The FULL_KOREAN_TABLE is only defined in Window platform when we use 
	UNICODE 2.0 table .
	We do not defined FULL_KOREAN_TABLE if we use UNICODE 1.1 BASE
	We do not defined FULL_KOREAN_TABLE for Mac and UNIX since the 2.0 Korean 
	Table for Mac and UNIX is only the part that these platform can handle
	(So it is still a GL table )
*/


#define FULL_KOREAN_TABLE 1 

#endif

/*
	GenTableData.c

*/
/*=========================================================================================
		Generator Table
=========================================================================================*/
#define PACK(h,l)		(int16)(( (h) << 8) | (l))

#if	defined(IS_LITTLE_ENDIAN)
#define ShiftCell(sub,len,min,max,minh,minl,maxh,maxl)	\
		PACK(len,sub), PACK(max,min), PACK(minl,minh), PACK(maxl,maxh)
#else
#define ShiftCell(sub,len,min,max,minh,minl,maxh,maxl)	\
		PACK(sub,len), PACK(min, max), PACK(minh,minl), PACK(maxh,maxl)
#endif
/*-----------------------------------------------------------------------------------
		ShiftTable for single byte encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 sbShiftT[] = 	{ 
	0, u1ByteCharset, 
	ShiftCell(0,0,0,0,0,0,0,0) 
};
/*-----------------------------------------------------------------------------------
		ShiftTable for two byte encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 tbShiftT[] = 	{ 
	0, u2BytesCharset, 
	ShiftCell(0,0,0,0,0,0,0,0) 
};
/*-----------------------------------------------------------------------------------
		ShiftTable for two byte encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 tbGRShiftT[] = 	{ 
	0, u2BytesGRCharset, 
	ShiftCell(0,0,0,0,0,0,0,0) 
};
/*-----------------------------------------------------------------------------------
		ShiftTable for KSC encoding 
-----------------------------------------------------------------------------------*/
#ifdef FULL_KOREAN_TABLE
#define tbKSCShiftT	tbShiftT
#else	
#define tbKSCShiftT	tbGRShiftT	
#endif  
/*-----------------------------------------------------------------------------------
		ShiftTable for shift jis encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 sjisShiftT[] = { 
	4, uMultibytesCharset, 	
	ShiftCell(u1ByteChar, 	1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
	ShiftCell(u1ByteChar, 	1, 0xA1, 0xDF, 0x00, 0xA1, 0x00, 0xDF),
	ShiftCell(u2BytesChar,	2, 0x81, 0x9F, 0x81, 0x40, 0x9F, 0xFC),
	ShiftCell(u2BytesChar, 	2, 0xE0, 0xFC, 0xE0, 0x40, 0xFC, 0xFC)
};
/*-----------------------------------------------------------------------------------
		ShiftTable for JIS0212 in EUCJP encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 x0212ShiftT[] = 	{ 
	0, u2BytesGRPrefix8FCharset, 
	ShiftCell(0,0,0,0,0,0,0,0) 
};
/*-----------------------------------------------------------------------------------
		ShiftTable for CNS11643-2 in EUC_TW encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 cns2ShiftT[] = 	{ 
	0, u2BytesGRPrefix8EA2Charset, 
	ShiftCell(0,0,0,0,0,0,0,0) 
};
/*-----------------------------------------------------------------------------------
		ShiftTable for big5 encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 big5ShiftT[] = { 
	2, uMultibytesCharset, 	
	ShiftCell(u1ByteChar, 	1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
	ShiftCell(u2BytesChar, 	2, 0xA1, 0xFE, 0xA1, 0x40, 0xFE, 0xFE)
};
/*-----------------------------------------------------------------------------------
		ShiftTable for jis0201 for euc_jp encoding 
-----------------------------------------------------------------------------------*/
PRIVATE int16 x0201ShiftT[] = { 
	2, uMultibytesCharset, 	
	ShiftCell(u1ByteChar, 		1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
	ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
/*-----------------------------------------------------------------------------------
		ShiftTable for utf8
-----------------------------------------------------------------------------------*/
PRIVATE int16 utf8ShiftT[] = { 
	3, uMultibytesCharset, 	
	ShiftCell(u1ByteChar, 		1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
	ShiftCell(u2BytesUTF8,		2, 0xC0, 0xDF, 0x00, 0x00, 0x07, 0xFF),
	ShiftCell(u3BytesUTF8,		3, 0xE0, 0xEF, 0x08, 0x00, 0xFF, 0xFF)
};
/*-----------------------------------------------------------------------------------
		Array of ShiftTable Pointer
-----------------------------------------------------------------------------------*/
/* This table is used for npc and unicode to unknow encoding conversion */
/* for those font csid, it do not shift GR/GL */
PRIVATE	int16*	npcShiftTable[MAXCSIDINTBL] =
{
	sbShiftT,	sbShiftT,	sbShiftT,	0,				sjisShiftT,	0,			sbShiftT,	big5ShiftT,	
	tbGRShiftT,	0,			sbShiftT,	sbShiftT,		tbKSCShiftT,	0,			sbShiftT,	sbShiftT,
	sbShiftT,	sbShiftT,	sbShiftT,	sbShiftT,		sbShiftT,	sbShiftT,	sbShiftT,	sbShiftT,
	tbShiftT,	tbShiftT,	tbShiftT,	sbShiftT,		tbShiftT,	sbShiftT,	tbShiftT,	tbShiftT,
	0,			0,			0,			0,				0,			big5ShiftT,	0,			sbShiftT,
	sbShiftT,	sbShiftT,	sbShiftT,	sbShiftT,		sbShiftT,	sbShiftT,	sbShiftT,			0,
	0,			0,			0,			0,				0,			0,			0,			0,
	0,			0,			0,			0,				0,			0,			sbShiftT,	0
};

/* This table is used for unicode to single encoding conversion */
/* for those font csid, it always shift GR/GL */
PRIVATE	int16*	strShiftTable[MAXCSIDINTBL] =
{
	sbShiftT,	sbShiftT,	sbShiftT,	0,				sjisShiftT,	0,			sbShiftT,	big5ShiftT,	
	tbGRShiftT,	0,			sbShiftT,	sbShiftT,		tbKSCShiftT,	0,			sbShiftT,	sbShiftT,
	sbShiftT,	sbShiftT,	sbShiftT,	sbShiftT,		sbShiftT,	sbShiftT,	sbShiftT,	sbShiftT,
	tbGRShiftT,	cns2ShiftT,	tbGRShiftT,	x0201ShiftT,	tbGRShiftT,	sbShiftT,	x0212ShiftT,tbGRShiftT,
	0,			0,			utf8ShiftT,	0,				0,			big5ShiftT,	0,			sbShiftT,
	sbShiftT,	sbShiftT,	sbShiftT,	sbShiftT,		sbShiftT,	sbShiftT,	sbShiftT,			0,
	0,			0,			0,			0,				0,			0,			0,			0,
	0,	0,	0,	0,	0,	0,	sbShiftT,	sbShiftT
};

PRIVATE UnicodeTableSet unicodetableset[] =
{
#ifdef XP_MAC
	{	CS_BIG5,	{		
		{CS_BIG5,0x81,0xFC},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},		
		{CS_DEFAULT,0xFF,0x00}
	}},
	{	CS_GB_8BIT,	{		
		{CS_GB_8BIT,0xA1,0xFE},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
		/*	We do not change the Mac Korean even we  use 2.0 base table */
		/*	This is because the Mac table is not the full table	    */
		/* 	It is still a GL table					    */
	{	CS_KSC_8BIT,	{		
		{CS_KSC_8BIT,0xA1,0xFE},		 
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
	{	CS_USER_DEFINED_ENCODING, {
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00} 
	}},
	{	CS_DEFAULT,	{		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}}
#endif
#if defined(XP_WIN) || defined(XP_OS2)
	{	CS_BIG5,	{		
		{CS_BIG5,0x81,0xFC},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},		
		{CS_DEFAULT,0xFF,0x00}
	}},
	{	CS_GB_8BIT,	{		
		{CS_GB_8BIT,0xA1,0xFE},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
#ifdef	FULL_KOREAN_TABLE
	{	CS_KSC_8BIT,	{		
		{CS_KSC_8BIT,0x81,0xFE},		/* CAREFUL: it is 0x81 not 0xA1 here */
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
#else
	{	CS_KSC_8BIT,	{		
		{CS_KSC_8BIT,0xA1,0xFE},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
#endif
	{	CS_USER_DEFINED_ENCODING, {
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00} 
	}},
	{	CS_DEFAULT,	{		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}}
#endif
#ifdef XP_UNIX
	{	CS_EUCJP,	{		
		{CS_JISX0208,0xA1,0xFE},		
		{CS_JISX0201,0x20,0x7E},		
		{CS_JISX0201,0x8E,0x8E},		
		{CS_JISX0212,0x8F,0x8F}
	}},
	{	CS_BIG5,	{		
		{CS_X_BIG5,0x81,0xFC},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},		
		{CS_DEFAULT,0xFF,0x00}
	}},
	{	CS_CNS_8BIT,	{		
		{CS_CNS11643_1,0xA1,0xFE},		
		{CS_CNS11643_2,0x8E,0x8E},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00}
	}},
	{	CS_GB_8BIT,	{		
		{CS_GB2312,0xA1,0xFE},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
		/*	We do not change the UNIX Korean even we  use 2.0 base table */
		/*	This is because the UNIX table is not the full table	    */
		/* 	It is still a GL table					    */
	{	CS_KSC_8BIT,	{		
		{CS_KSC5601,0xA1,0xFE},		
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}},
	{	CS_USER_DEFINED_ENCODING, {
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00} 
	}},
	{	CS_USRDEF2, {
		{CS_ASCII,0x00,0x7E},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00},		
		{CS_DEFAULT,	0xFF,0x00} 
	}},
	{	CS_DEFAULT,	{		
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00},
		{CS_DEFAULT,0xFF,0x00}
	}}
#endif

};

MODULE_PRIVATE UnicodeTableSet* GetUnicodeTableSet(uint16 csid)
{
	int i;
	for(i=0;unicodetableset[i].maincsid != CS_DEFAULT;i++)
	{
		if(unicodetableset[i].maincsid == csid)
			return &(unicodetableset[i]);	 
	}
	return NULL;
}

/*-----------------------------------------------------------------------------------
		Public Function
-----------------------------------------------------------------------------------*/
MODULE_PRIVATE uShiftTable* GetShiftTableFromCsid(uint16 csid)
{
	return (uShiftTable*)(strShiftTable[csid & 0x3F]); 
}
MODULE_PRIVATE uShiftTable* InfoToShiftTable(unsigned char info)
{
	return (uShiftTable*)(npcShiftTable[info & (MAXCSIDINTBL - 1)]); 
}

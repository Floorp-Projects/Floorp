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

#ifndef __UGEN__
#define __UGEN__

#include "xp_core.h"


/* =================================================
					uShiftTable
================================================= */
/*=================================================================================

=================================================================================*/

enum {
	u1ByteCharset = 0,
	u2BytesCharset,
	uMultibytesCharset,
	u2BytesGRCharset,
	u2BytesGRPrefix8FCharset,
	u2BytesGRPrefix8EA2Charset,
	uNumOfCharsetType
};
/*=================================================================================

=================================================================================*/

enum {
	u1ByteChar			= 0,
	u2BytesChar,
	u2BytesGRChar,
	u1BytePrefix8EChar,		/* Used by JIS0201 GR in EUC_JP */
	u2BytesUTF8,			/* Used by UTF8 */
	u3BytesUTF8,			/* Used by UTF8 */
	uNumOfCharType
};
/*=================================================================================

=================================================================================*/
typedef struct  {
	unsigned char MinHB;
	unsigned char MinLB;
	unsigned char MaxHB;
	unsigned char MaxLB;
} uShiftOut;
/*=================================================================================

=================================================================================*/
typedef struct  {
	unsigned char Min;
	unsigned char Max;
} uShiftIn;
/*=================================================================================

=================================================================================*/
typedef struct  {
	unsigned char   classID;
	unsigned char   reserveLen;
	uShiftIn	shiftin;
	uShiftOut	shiftout;
} uShiftCell;
/*=================================================================================

=================================================================================*/
typedef struct  {
	int16		numOfItem;
	int16		classID;
	uShiftCell	shiftcell[1];
} uShiftTable;
/*=================================================================================

=================================================================================*/

#define MAXINTERCSID 4
typedef struct StrRangeMap StrRangeMap;
struct StrRangeMap {
	uint16 intercsid;
	unsigned char min;
	unsigned char max;	
};
typedef struct UnicodeTableSet UnicodeTableSet;
struct UnicodeTableSet {
 uint16 maincsid;
 StrRangeMap range[MAXINTERCSID];
};
#endif

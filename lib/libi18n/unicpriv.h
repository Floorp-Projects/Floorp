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
#ifndef __UNIPRIV__
#define __UNIPRIV__

#include "intlpriv.h"
#include "umap.h"
#include "ugen.h"

typedef struct uTableSet uTableSet;
struct uTableSet
{
	uTable *tables[MAXINTERCSID];
	uShiftTable   *shift[MAXINTERCSID];
	StrRangeMap	range[MAXINTERCSID];
};

MODULE_PRIVATE XP_Bool		uMapCode(uTable *uTable, uint16 in, uint16* out);
MODULE_PRIVATE XP_Bool 	uGenerate(uShiftTable *shift,int32* state, uint16 in, 
						unsigned char* out, uint16 outbuflen, uint16* outlen);
MODULE_PRIVATE XP_Bool 	uScan(uShiftTable *shift, int32 *state, unsigned char *in,
						uint16 *out, uint16 inbuflen, uint16* inscanlen);
MODULE_PRIVATE XP_Bool 	UCS2_To_Other(uint16 ucs2, unsigned char *out, uint16 outbuflen, uint16* outlen,int16 *outcsid);
typedef void (*uMapIterateFunc)(uint16 ucs2, uint16 med, uint16 context);
MODULE_PRIVATE void		uMapIterate(uTable *uTable, uMapIterateFunc callback, uint16 context);

MODULE_PRIVATE uShiftTable* 	InfoToShiftTable(unsigned char info);
MODULE_PRIVATE uShiftTable* 	GetShiftTableFromCsid(uint16 csid);
MODULE_PRIVATE UnicodeTableSet* GetUnicodeTableSet(uint16 csid);


#endif /* __UNIPRIV__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Bogus implementation of nsIMalloc ('out' param mem manager). */

#include "xpcprivate.h"

NS_IMPL_ISUPPORTS(nsMalloc, NS_IMALLOC_IID);

nsMalloc::nsMalloc()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}        

void*  nsMalloc::Alloc(uint32 cb){return malloc(cb);}
void*  nsMalloc::Realloc(void *pv, uint32 cb){return realloc(pv,cb);}
void   nsMalloc::Free(void *pv) {free(pv);}
uint32 nsMalloc::GetSize(void *pv) {return 0;}
int32  nsMalloc::DidAlloc(void *pv) {return 1;}
void   nsMalloc::HeapMinimize(void) {}

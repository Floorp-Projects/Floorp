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

/* Temporary nsIMalloc related stuff. */

#ifndef xpcbogusmem_h___
#define xpcbogusmem_h___

// {7BB253C0-AC2D-11d2-BA68-00805F8A5DD7}
#define NS_IMALLOC_IID          \
{ 0x7bb253c0, 0xac2d, 0x11d2,   \
  { 0xba, 0x68, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIMalloc : public nsISupports
{
public:
    NS_IMETHOD_(void*)  Alloc(uint32 cb) = 0;
    NS_IMETHOD_(void*)  Realloc(void *pv, uint32 cb) = 0;
    NS_IMETHOD_(void)   Free(void *pv) = 0;
    NS_IMETHOD_(uint32) GetSize(void *pv) = 0;
    NS_IMETHOD_(int32)  DidAlloc(void *pv) = 0;
    NS_IMETHOD_(void)   HeapMinimize(void) = 0;
};

class nsMalloc : public nsIMalloc
{
public:
    nsMalloc();
    NS_DECL_ISUPPORTS;
    NS_IMETHOD_(void*)  Alloc(uint32 cb);
    NS_IMETHOD_(void*)  Realloc(void *pv, uint32 cb);
    NS_IMETHOD_(void)   Free(void *pv);
    NS_IMETHOD_(uint32) GetSize(void *pv);
    NS_IMETHOD_(int32)  DidAlloc(void *pv);
    NS_IMETHOD_(void)   HeapMinimize(void);
};
    
#endif  /* xpcbogusmem_h___ */

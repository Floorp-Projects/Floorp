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

/* Module local methods to use 'standard' interfaces in other modules. */

#include "xpcprivate.h"

// all these methods are class statics

nsIAllocator* XPCMem::Allocator()
{
    static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
    static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
    static nsIAllocator* al = NULL;
    if(!al)
        nsServiceManager::GetService(kAllocatorCID, kIAllocatorIID, 
                                     (nsISupports **)&al);
    NS_ASSERTION(al, "failed to get Allocator!");
    return al;    
}    

void* XPCMem::Alloc(PRUint32 size) 
    {return Allocator()->Alloc(size);}

void* XPCMem::Realloc(void* ptr, PRUint32 size)
    {return Allocator()->Realloc(ptr, size);}

void  XPCMem::Free(void* ptr)
    {Allocator()->Free(ptr);}

void  XPCMem::HeapMinimize()
    {Allocator()->HeapMinimize();}

void* XPCMem::Clone(const void* ptr,  PRUint32 size)
{
    if(!ptr) return NULL;
    void* p = Allocator()->Alloc(size);
    if(p) memcpy(p, ptr, size);
    return p;
}        

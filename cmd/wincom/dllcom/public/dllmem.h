/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#ifndef __DllMemory_H
#define __DllMemory_H

#ifndef _WIN32
// CoTaskMemAlloc uses the default OLE allocator to allocate a memory
// block in the same way that IMalloc::Alloc does
LPVOID	CoTaskMemAlloc(ULONG cb);

// CoTaskMemRealloc changes the size of a previously allocated memory
// block in the same way that IMalloc::Realloc does
LPVOID  CoTaskMemRealloc(LPVOID lpv, ULONG cb);

// CoTaskMemFree uses the default OLE allocator to free a block of
// memory previously allocated through a call to CoTaskMemAlloc
void	CoTaskMemFree(LPVOID lpv);
#endif

#endif // __DllMemory_H
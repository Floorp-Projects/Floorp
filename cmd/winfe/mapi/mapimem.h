/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef __MY_MEM_HPP__
#define __MY_MEM_HPP__

#ifndef MAPI_OLE	// Because MSFT doesn't do this for us :-(
#include <mapi.h>                                                 
#endif

//
// Needed for turning NULL's into ""'s for string sequence routines...
//
LPSTR     CheckNullString(LPSTR inStr);

//
// Memory allocation functions...
//

//
// This will free an lpMapiMessage structure allocated by this DLL
//
void      FreeMAPIMessage(lpMapiMessage pv);

//
// This will free an lpMapiRecipDesc structure allocated by this DLL
//
void      FreeMAPIRecipient(lpMapiRecipDesc pv);

//
// Frees a mapi file object...
//
void      FreeMAPIFile(lpMapiFileDesc pv);

//
// This routine will take an lpMapiMessage structure and "flatten" it into
// one contiguous chunk of memory that can be easily passed around. 
// 
LPVOID    FlattenMAPIMessageStructure(lpMapiMessage msg, DWORD *totalSize);



#endif  // __MY_MEM_HPP__
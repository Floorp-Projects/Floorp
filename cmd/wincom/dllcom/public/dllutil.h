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

#ifndef __ComDllUtility_H
#define __ComDllUtility_H

// Useful utility functions. DLL_StringFromGUID returns
// an ANSI string that the caller must use CoTaskMemFree
// to free the memory
#define DLL_StringFromCLSID DLL_StringFromGUID
#define DLL_StringFromIID DLL_StringFromGUID
LPSTR DLL_StringFromGUID(REFGUID rGuid);

// Converts an OLE string (UNICODE) to an ANSI string. The caller
// must use CoTaskMemFree to free the memory
LPSTR AllocTaskAnsiString(LPOLESTR lpszString);

//  Required function, not implemented anywhere in the library,
//      and is your responsiblilty as the end consumer to
//      implement.
//  Create a CComDll derived class, and return the base pointer.
//  Manage all state data in the derived class that is needed to
//      track an instance of a DLL being used by a specific TASK.
CComDll *DLL_ConsumerCreateInstance(void);

#ifdef DLL_WIN16
//  The following are WIN32 COM Dll optional, but the functionality
//      is badly needed on WIN16, so support it for installation
//      consistancy and ease.
STDAPI DllRegisterServer(void);
STDAPI DllUnregisterServer(void);
#endif

#endif // __ComDllUtility_H
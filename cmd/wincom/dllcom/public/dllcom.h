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

#ifndef __DllCommon_H
#define __DllCommon_H

//  Common dll code to be compiled to library form, and then
//      used as a framework by other windows dlls which support
//      both 16 and 32 bits through a COM interface.

#include "dlltypes.h"
#include "dlldbg.h"
#include "dlliface.h"
#include "dllmem.h"
#include "dlltask.h"
#include "dllref.h"
#include "dllobj.h"
#include "dllutil.h"

//  End consumer must derive from this class and implement the
//      abstract/virtual interfaces.
//  Please note that a CComDll derived class will self destruct
//      once it detects that no more instance data is at hand (no
//      CComClass instances for this CComDll are in existence).
class CComDll : public IUnknown  {
private:
    CProcessEntry *m_pEntry;
public:
    CComDll();
    virtual ~CComDll();

    //  Had to break down and track the module for resources and
    //      path resolution situations.
    //  Static is OK, as on 16 bits, the DLL will continue to
    //      have the same module handle no matter how many
    //      different EXEs are looking at it.
public:
    static HINSTANCE m_hInstance;
    static HINSTANCE GetInstanceHandle();

    //  Reference counting is done on a per COM object basis in
    //      the instance data.
    //  You must derive your COM related classes from
    //      CComClass as they know how to properly utilize
    //      the following functions of the CComDll instance.
private:
    DWORD m_ulCount;
public:
    STDMETHODIMP QueryInterface(REFIID iid, void **ppObj);
    STDMETHODIMP_(DWORD) AddRef();
    STDMETHODIMP_(DWORD) Release();

public:
    //  Provide way to get the module name.
    DWORD GetModuleFileName(char *pOutName, size_t stOutSize, BOOL bFullPath = FALSE);

public:
    //  Consumer implementation of DllGetClassObject per DLL
    //      task instance.
    virtual HRESULT GetClassObject(REFCLSID rClsid, REFIID rIid, LPVOID *ppObj) = 0;
    //  Consumer implemenation of Dll[Un]RegisterServer to
    //      write / delete CLSIDs for objects it implements.
    //  No need to override unless you need to do more than
    //      base implementation.
    virtual HRESULT RegisterServer();
    virtual HRESULT UnregisterServer();
    //  Needed by RegisterServer and UnregisterServer.
    //  Simply return an array of CLSIDs, terminated by a
    //      null.
    //  Must return an allocated buffer which must be freed by the
    //      caller.
    virtual const CLSID **GetCLSIDs() = 0;

#ifdef DLL_DEBUG
public:
    //  Mainly needed to distinguish trace output.
    //  Set to your own string when you need to.
    static const char *m_pTraceID;
#endif
};

#endif // __DllCommon_H
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

#include "dllcom.h"

CProcessEntry *CProcess::m_pProcessList = NULL;

CProcessEntry::CProcessEntry(CComDll *pDll)
{
    DLL_ASSERT(pDll);

    m_dwProcessID = CProcess::GetProcessID();
    m_pDllInstance = pDll;

    m_pNext = CProcess::m_pProcessList;
    CProcess::m_pProcessList = this;
}

CProcessEntry::~CProcessEntry()
{
    //  find ourselves in the list and remove us.
    CProcessEntry **ppChange = &CProcess::m_pProcessList;

    while(*ppChange != this)    {
        ppChange = &((*ppChange)->m_pNext);
    }

    *ppChange = m_pNext;

    m_dwProcessID = 0;
    m_pDllInstance = NULL;
    m_pNext = NULL;
}

DWORD CProcess::GetProcessID()
{
    DWORD dwRetval = 0;

    //  This is only needed to multiplex instance data on win16.
#ifdef DLL_WIN16
    dwRetval = CoGetCurrentProcess();
#else
    //  Just use the HINSTANCE unless we need to do this on a per
    //      thread bases (then as above like WIN16).
    dwRetval = (DWORD)CComDll::GetInstanceHandle();
#endif

    return(dwRetval);
}

CComDll *CProcess::GetProcessDll()
{
    CComDll *pRetval = NULL;

    //  Before we can find the Dll instance data, we need to
    //      get the ID of the current process calling us.
    DWORD dwCurrent = GetProcessID();

    //  See if the object already exists.
    CProcessEntry *pTraverse = m_pProcessList;
    while(pTraverse && pTraverse->m_dwProcessID != dwCurrent)   {
        pTraverse = pTraverse->m_pNext;
    }

    if(pTraverse)   {
        //  Have it already.
        //  Return it.
        pRetval = pTraverse->m_pDllInstance;

        //  We must be sure to properly reference count the returned
        //      object.
        pRetval->AddRef();
    }
    else    {
        //  Do not have it.
        //  Ask the end consumer to create it.
        pRetval = DLL_ConsumerCreateInstance();
    }

    return(pRetval);
}

BOOL CProcess::CanUnloadNow()
{
    //  We can unload if we have no more process entries;
    //      all objects are freed off.
    return(m_pProcessList == NULL);
}
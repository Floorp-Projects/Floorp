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

//  ALL COM IMPLEMENTED OBJECTS MUST DERIVE FROM THIS
//      CLASS (or do what it does).

//  Perform simple minimal maintenance such that
//      the remainder of the library can perform
//      more powerful house keeping.
//  All we do here is inform the instance data
//      (CComDll) that there is yet another
//      object (more instance data) alive and
//      well.  This way, the CComDll class won't
//      remove itself from memory until there are
//      no more objects hanging around.

CComClass::CComClass(IUnknown *pAggregate)
{
    m_ulCount = 0;
    m_IUnknown.m_pObject = this;
    m_pAggregate = NULL;
    m_pAggregate = pAggregate;

    //  Need to add a reference to ourselves.
    //  Use IUnknown to avoid aggregate.
    m_IUnknown.AddRef();
}

CComClass::~CComClass()
{
    DLL_ASSERT(m_ulCount == 0);
    m_IUnknown.m_pObject = NULL;
    m_pAggregate = NULL;
}

HRESULT CComClass::ObjectQueryInterface(REFIID iid, void **ppObj, BOOL bIUnknown)
{
    //  Determine if we should aggregate the call.
    if(m_pAggregate && !bIUnknown)  {
        return(m_pAggregate->QueryInterface(iid, ppObj));
    }
    else    {
        return(CustomQueryInterface(iid, ppObj));
    }
}

HRESULT CComClass::CustomQueryInterface(REFIID iid, void **ppObj)
{
    HRESULT hRetval = ResultFromScode(E_NOINTERFACE);
    *ppObj = NULL;

    //  This is virtual.
    //  Just check to see if we should return our IUnknown
    //      implementation.
    if(IsEqualIID(iid, IID_IUnknown))  {
        m_IUnknown.AddRef();
        *ppObj = (void *)&m_IUnknown;
        hRetval = ResultFromScode(S_OK);
    }

    return(hRetval);
}

DWORD CComClass::ObjectAddRef(BOOL bIUnknown)
{
    DWORD dwRetval;

    if(m_pAggregate && !bIUnknown)    {
        dwRetval = m_pAggregate->AddRef();
    }
    else    {
        dwRetval = CustomAddRef();
    }

    return(dwRetval);
}

DWORD CComClass::CustomAddRef()
{
    m_ulCount++;
    return(m_ulCount);
}

DWORD CComClass::ObjectRelease(BOOL bIUnknown)
{
    DWORD dwRetval;

    if(m_pAggregate && !bIUnknown)    {
        dwRetval = m_pAggregate->Release();
    }
    else    {
        dwRetval = CustomRelease();
    }

    return(dwRetval);
}

DWORD CComClass::CustomRelease()
{
    DWORD dwRetval;
    
    m_ulCount--;
    dwRetval = m_ulCount;

    if(!dwRetval)   {
        delete this;
    }

    return(dwRetval);
}

STDMETHODIMP CComClass::CImplUnknown::QueryInterface(REFIID iid, void **ppObj)
{
    //  Avoid aggregation in actual IUnknown implementation.
    HRESULT hRetval = m_pObject->ObjectQueryInterface(iid, ppObj, TRUE);
    return(hRetval);
}

STDMETHODIMP_(DWORD) CComClass::CImplUnknown::AddRef()
{
    //  Avoid aggregation in actual IUnknown implementation.
    DWORD dwRetval = m_pObject->ObjectAddRef(TRUE);
    return(dwRetval);
}

STDMETHODIMP_(DWORD) CComClass::CImplUnknown::Release()
{
    //  Avoid aggregation in actual IUnknown implementation.
    DWORD dwRetval = m_pObject->ObjectRelease(TRUE);
    return(dwRetval);
}
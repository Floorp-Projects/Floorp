/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */

#include "stdafx.h"

CItemContainer::CItemContainer()
{
}

CItemContainer::~CItemContainer()
{
}

///////////////////////////////////////////////////////////////////////////////
// IParseDisplayName implementation


HRESULT STDMETHODCALLTYPE CItemContainer::ParseDisplayName(/* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ LPOLESTR pszDisplayName, /* [out] */ ULONG __RPC_FAR *pchEaten, /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmkOut)
{
    // TODO
    return E_NOTIMPL;
}


///////////////////////////////////////////////////////////////////////////////
// IOleContainer implementation


HRESULT STDMETHODCALLTYPE CItemContainer::EnumObjects(/* [in] */ DWORD grfFlags, /* [out] */ IEnumUnknown __RPC_FAR *__RPC_FAR *ppenum)
{
    HRESULT hr = E_NOTIMPL;
/*
    if (ppenum == NULL)
    {
        return E_POINTER;
    }

    *ppenum = NULL;
    typedef CComObject<CComEnumOnSTL<IEnumUnknown, &IID_IEnumUnknown, IUnknown*, _CopyInterface<IUnknown>, CNamedObjectList > > enumunk;
    enumunk* p = NULL;
    p = new enumunk;
    if(p == NULL)
    {
        return E_OUTOFMEMORY;
    }

    hr = p->Init();
    if (SUCCEEDED(hr))
    {
        hr = p->QueryInterface(IID_IEnumUnknown, (void**) ppenum);
    }
    if (FAILED(hRes))
    {
        delete p;
    }
*/
    return hr;
}
        

HRESULT STDMETHODCALLTYPE CItemContainer::LockContainer(/* [in] */ BOOL fLock)
{
    // TODO
    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IOleItemContainer implementation


HRESULT STDMETHODCALLTYPE CItemContainer::GetObject(/* [in] */ LPOLESTR pszItem, /* [in] */ DWORD dwSpeedNeeded, /* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ REFIID riid, /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
    if (pszItem == NULL)
    {
        return E_INVALIDARG;
    }
    if (ppvObject == NULL)
    {
        return E_INVALIDARG;
    }

    *ppvObject = NULL;
    
    return MK_E_NOOBJECT;
}


HRESULT STDMETHODCALLTYPE CItemContainer::GetObjectStorage(/* [in] */ LPOLESTR pszItem, /* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ REFIID riid, /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvStorage)
{
    // TODO
    return MK_E_NOOBJECT;
}


HRESULT STDMETHODCALLTYPE CItemContainer::IsRunning(/* [in] */ LPOLESTR pszItem)
{
    // TODO
    return MK_E_NOOBJECT;
}

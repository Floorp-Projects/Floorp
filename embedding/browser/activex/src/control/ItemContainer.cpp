/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

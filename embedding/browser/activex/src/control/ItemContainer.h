/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ITEMCONTAINER_H
#define ITEMCONTAINER_H

// Class for managing a list of named objects.

class CItemContainer :    public CComObjectRootEx<CComSingleThreadModel>,
                        public IOleItemContainer
{
    // List of all named objects managed by the container
    CNamedObjectList m_cObjectList;
public:

    CItemContainer();
    virtual ~CItemContainer();

BEGIN_COM_MAP(CItemContainer)
    COM_INTERFACE_ENTRY_IID(IID_IParseDisplayName, IOleItemContainer)
    COM_INTERFACE_ENTRY_IID(IID_IOleContainer, IOleItemContainer)
    COM_INTERFACE_ENTRY_IID(IID_IOleItemContainer, IOleItemContainer)
END_COM_MAP()

    // IParseDisplayName implementation
    virtual HRESULT STDMETHODCALLTYPE ParseDisplayName(/* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ LPOLESTR pszDisplayName, /* [out] */ ULONG __RPC_FAR *pchEaten, /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmkOut);

    // IOleContainer implementation
    virtual HRESULT STDMETHODCALLTYPE EnumObjects(/* [in] */ DWORD grfFlags, /* [out] */ IEnumUnknown __RPC_FAR *__RPC_FAR *ppenum);
    virtual HRESULT STDMETHODCALLTYPE LockContainer(/* [in] */ BOOL fLock);

    // IOleItemContainer implementation
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE GetObject(/* [in] */ LPOLESTR pszItem, /* [in] */ DWORD dwSpeedNeeded, /* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ REFIID riid, /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE GetObjectStorage(/* [in] */ LPOLESTR pszItem, /* [unique][in] */ IBindCtx __RPC_FAR *pbc, /* [in] */ REFIID riid, /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvStorage);
    virtual HRESULT STDMETHODCALLTYPE IsRunning(/* [in] */ LPOLESTR pszItem);  
};

typedef CComObject<CItemContainer> CItemContainerInstance;


#endif
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
 *   Adam Lock <adamlock@eircom.net>
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

#ifndef CONTROLEVENTSINK_H
#define CONTROLEVENTSINK_H

// This class listens for events from the specified control

class CControlEventSink : 
    public CComObjectRootEx<CComSingleThreadModel>,
    public IDispatch
{
public:
    CControlEventSink();

    // Current event connection point
    CComPtr<IConnectionPoint> m_spEventCP;
    CComPtr<ITypeInfo> m_spEventSinkTypeInfo;
    DWORD m_dwEventCookie;
    IID m_EventIID;

protected:
    virtual ~CControlEventSink();

    static HRESULT WINAPI SinkQI(void* pv, REFIID riid, LPVOID* ppv, DWORD dw)
    {
        CControlEventSink *pThis = (CControlEventSink *) pv;
        if (!IsEqualIID(pThis->m_EventIID, GUID_NULL) && 
             IsEqualIID(pThis->m_EventIID, riid))
        {
            return pThis->QueryInterface(__uuidof(IDispatch), ppv);
        }
        return E_NOINTERFACE;
    }

public:

BEGIN_COM_MAP(CControlEventSink)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY_FUNC_BLIND(0, SinkQI)
END_COM_MAP()

    virtual HRESULT SubscribeToEvents(IUnknown *pControl);
    virtual void UnsubscribeFromEvents();
    virtual BOOL GetEventSinkIID(IUnknown *pControl, IID &iid, ITypeInfo **typeInfo);
    virtual HRESULT InternalInvoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);

// IDispatch
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(/* [out] */ UINT __RPC_FAR *pctinfo);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(/* [in] */ UINT iTInfo, /* [in] */ LCID lcid, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(/* [in] */ REFIID riid, /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames, /* [in] */ UINT cNames, /* [in] */ LCID lcid, /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke(/* [in] */ DISPID dispIdMember, /* [in] */ REFIID riid, /* [in] */ LCID lcid, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams, /* [out] */ VARIANT __RPC_FAR *pVarResult, /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo, /* [out] */ UINT __RPC_FAR *puArgErr);
};

typedef CComObject<CControlEventSink> CControlEventSinkInstance;

#endif
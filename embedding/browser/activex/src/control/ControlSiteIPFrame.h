/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef CONTROLSITEIPFRAME_H
#define CONTROLSITEIPFRAME_H

class CControlSiteIPFrame :    public CComObjectRootEx<CComSingleThreadModel>,
                            public IOleInPlaceFrame
{
public:
    CControlSiteIPFrame();
    virtual ~CControlSiteIPFrame();

    HWND m_hwndFrame;

BEGIN_COM_MAP(CControlSiteIPFrame)
    COM_INTERFACE_ENTRY_IID(IID_IOleWindow, IOleInPlaceFrame)
    COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceUIWindow, IOleInPlaceFrame)
    COM_INTERFACE_ENTRY(IOleInPlaceFrame)
END_COM_MAP()

    // IOleWindow
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetWindow(/* [out] */ HWND __RPC_FAR *phwnd);
    virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(/* [in] */ BOOL fEnterMode);

    // IOleInPlaceUIWindow implementation
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetBorder(/* [out] */ LPRECT lprectBorder);
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE RequestBorderSpace(/* [unique][in] */ LPCBORDERWIDTHS pborderwidths);
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE SetBorderSpace(/* [unique][in] */ LPCBORDERWIDTHS pborderwidths);
    virtual HRESULT STDMETHODCALLTYPE SetActiveObject(/* [unique][in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject, /* [unique][string][in] */ LPCOLESTR pszObjName);

    // IOleInPlaceFrame implementation
    virtual HRESULT STDMETHODCALLTYPE InsertMenus(/* [in] */ HMENU hmenuShared, /* [out][in] */ LPOLEMENUGROUPWIDTHS lpMenuWidths);
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE SetMenu(/* [in] */ HMENU hmenuShared, /* [in] */ HOLEMENU holemenu, /* [in] */ HWND hwndActiveObject);
    virtual HRESULT STDMETHODCALLTYPE RemoveMenus(/* [in] */ HMENU hmenuShared);
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE SetStatusText(/* [in] */ LPCOLESTR pszStatusText);
    virtual HRESULT STDMETHODCALLTYPE EnableModeless(/* [in] */ BOOL fEnable);
    virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(/* [in] */ LPMSG lpmsg, /* [in] */ WORD wID);
};

typedef CComObject<CControlSiteIPFrame> CControlSiteIPFrameInstance;

#endif
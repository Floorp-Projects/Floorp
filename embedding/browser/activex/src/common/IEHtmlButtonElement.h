/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Alexandre Trémon <atremon@elansoftware.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Declaration of IHTMLButtonElement and related classes
//
#ifndef IHTMLBUTTONELEMENT_H
#define IHTMLBUTTONELEMENT_H

#include "IEHtmlNode.h"

class CIEHtmlElement;

class CIEHtmlButtonElement :
    public CNode,
    public IDispatchImpl<IHTMLButtonElement, &IID_IHTMLButtonElement, &LIBID_MSHTML>
{
public:
    CIEHtmlButtonElement() {
    };

    HRESULT FinalConstruct( );
    virtual HRESULT GetHtmlElement(CIEHtmlElement **ppHtmlElement);
    virtual HRESULT SetDOMNode(nsIDOMNode *pDomNode);
    virtual HRESULT SetParent(CNode *pParent);

DECLARE_GET_CONTROLLING_UNKNOWN()

protected:
    virtual ~CIEHtmlButtonElement() {
    };

public:

BEGIN_COM_MAP(CIEHtmlButtonElement)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLButtonElement)
    COM_INTERFACE_ENTRY_AGGREGATE(IID_IHTMLElement, m_pHtmlElementAgg.p)
    COM_INTERFACE_ENTRY_AGGREGATE(IID_IHTMLDOMNode, m_pHtmlElementAgg.p)
END_COM_MAP()

    // IHTMLButtonElement Implementation:
    virtual HRESULT STDMETHODCALLTYPE get_type(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_value(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_value(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_name(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_name(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_status(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_status(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_disabled(VARIANT_BOOL v);
    virtual HRESULT STDMETHODCALLTYPE get_disabled(VARIANT_BOOL __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_form(IHTMLFormElement __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE createTextRange(IHTMLTxtRange __RPC_FAR *__RPC_FAR *range);

protected:
    CComPtr<IUnknown> m_pHtmlElementAgg;
};

typedef CComObject<CIEHtmlButtonElement> CIEHtmlButtonElementInstance;

#endif //IHTMLBUTTONELEMENT_H

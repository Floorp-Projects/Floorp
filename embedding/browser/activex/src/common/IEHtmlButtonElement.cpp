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

#include "stdafx.h"
#include "IEHTMLButtonElement.h"
#include "IEHtmlElement.h"
#include "nsIDOMHTMLButtonElement.h"

HRESULT CIEHtmlButtonElement::FinalConstruct( )
{
    return CComCreator<CComAggObject<CIEHtmlElement> >::CreateInstance(GetControllingUnknown(),
        IID_IUnknown, reinterpret_cast<void**>(&m_pHtmlElementAgg));
}

HRESULT CIEHtmlButtonElement::GetHtmlElement(CIEHtmlElement **ppHtmlElement)
{
    if (ppHtmlElement == NULL)
        return E_FAIL;
    *ppHtmlElement = NULL;
    IHTMLElement* pHtmlElement = NULL;
    // This causes an AddRef on outer unknown:
    HRESULT hr = m_pHtmlElementAgg->QueryInterface(IID_IHTMLElement, (void**)&pHtmlElement);
    *ppHtmlElement = (CIEHtmlElement*)pHtmlElement;
    return hr;
}

HRESULT CIEHtmlButtonElement::SetDOMNode(nsIDOMNode *pDomNode)
{
    mDOMNode = pDomNode;
    //Forward to aggregated object:
    CIEHtmlElement *pHtmlElement;
    GetHtmlElement(&pHtmlElement);
    HRESULT hr = pHtmlElement->SetDOMNode(pDomNode);
    // Release on outer unknown because GetHtmlDomNode does AddRef on it:
    GetControllingUnknown()->Release();
    return hr;
}

HRESULT CIEHtmlButtonElement::SetParent(CNode *pParent)
{
    CNode::SetParent(pParent);
    //Forward to aggregated object:
    CIEHtmlElement *pHtmlElement;
    GetHtmlElement(&pHtmlElement);
    HRESULT hr = pHtmlElement->SetParent(pParent);
    // Release on outer unknown because GetHtmlDomNode does AddRef on it:
    GetControllingUnknown()->Release();
    return hr;
}

// -----------------------------------------------------------------------
// IHTMLButtonElement Implementation
// -----------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::get_type(BSTR __RPC_FAR *p)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::put_value(BSTR v)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::get_value(BSTR __RPC_FAR *p)
{
    if (p == NULL)
        return E_INVALIDARG;

    *p = NULL;
    nsCOMPtr<nsIDOMHTMLButtonElement> domHtmlButtonElement = do_QueryInterface(mDOMNode);
    if (!domHtmlButtonElement)
        return E_UNEXPECTED;
    nsAutoString strValue;
    domHtmlButtonElement->GetValue(strValue);
    *p = SysAllocString(strValue.get());
    if (!*p)
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::put_name(BSTR v)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::get_name(BSTR __RPC_FAR *p)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::put_status(VARIANT v)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::get_status(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::put_disabled(VARIANT_BOOL v)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::get_disabled(VARIANT_BOOL __RPC_FAR *p)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::get_form(IHTMLFormElement __RPC_FAR *__RPC_FAR *p)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlButtonElement::createTextRange(IHTMLTxtRange __RPC_FAR *__RPC_FAR *range)
{
    return E_NOTIMPL;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "IEHtmlSelectionObject.h"
#include "IEHtmlTxtRange.h"

CIEHtmlSelectionObject::CIEHtmlSelectionObject()
{
}

CIEHtmlSelectionObject::~CIEHtmlSelectionObject()
{
}

void CIEHtmlSelectionObject::SetSelection(nsISelection *pSelection)
{
    mSelection = pSelection;
}

void CIEHtmlSelectionObject::SetDOMDocumentRange(nsIDOMDocumentRange *pDOMDocumentRange) {
    mDOMDocumentRange = pDOMDocumentRange;
}

HRESULT STDMETHODCALLTYPE CIEHtmlSelectionObject::createRange(IDispatch __RPC_FAR *__RPC_FAR *range)
{
    if (range == NULL)
        return E_INVALIDARG;
    *range = NULL;
    if (!mSelection)
        return E_FAIL;
    // get range corresponding to mSelection:
    nsCOMPtr<nsIDOMRange> domRange;
    int rc;
    mSelection->GetRangeCount(&rc);
    if (rc>0)
        mSelection->GetRangeAt(0, getter_AddRefs(domRange));
    else
    {
        // create empty range
        mDOMDocumentRange->CreateRange(getter_AddRefs(domRange));
    }
    if (!domRange)
        return E_FAIL;
    // create com object:
    CComBSTR strType;
    get_type(&strType);
    if (wcscmp(OLE2W(strType), L"Control") == 0)
    {
        // IHTMLControlRange:
        CIEHtmlControlRangeInstance *pControlRange = NULL;
        CIEHtmlControlRangeInstance::CreateInstance(&pControlRange);
        if (!pControlRange)
            return E_FAIL;
        // gives the range to the com object:
        pControlRange->SetRange(domRange);
        // return com object:
        pControlRange->QueryInterface(IID_IDispatch, (void **)range);
    } else
    {
        // IHTMLTxtRange:
        CIEHtmlTxtRangeInstance *pTxtRange = NULL;
        CIEHtmlTxtRangeInstance::CreateInstance(&pTxtRange);
        if (!pTxtRange)
            return E_FAIL;
        // gives the range to the com object:
        pTxtRange->SetRange(domRange);
        // return com object:
        pTxtRange->QueryInterface(IID_IDispatch, (void **)range);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlSelectionObject::get_type(BSTR __RPC_FAR *p)
{
    static NS_NAMED_LITERAL_STRING(strText,"Text");
    static NS_NAMED_LITERAL_STRING(strControl,"Control");
    static NS_NAMED_LITERAL_STRING(strNone,"None");
    nsCOMPtr<nsIDOMRange> domRange;
    nsCOMPtr<nsIDOMNode> domNode;

    if (p == NULL)
        return E_INVALIDARG;
    *p = NULL;
    
    // get range corresponding to mSelection:
    int rc;
    mSelection->GetRangeCount(&rc);
    if (rc<1) {
        *p = SysAllocString(strNone.get());
        return p ? S_OK : E_OUTOFMEMORY;
    }
    mSelection->GetRangeAt(0, getter_AddRefs(domRange));
    if (!domRange)
        return E_FAIL;
    domRange->GetStartContainer(getter_AddRefs(domNode));
    if (!domNode)
        return E_FAIL;
    unsigned short nodeType;
    domNode->GetNodeType(&nodeType);
    switch (nodeType)
    {
    case nsIDOMNode::ELEMENT_NODE:
        *p = SysAllocString(strControl.get());
        break;
    case nsIDOMNode::TEXT_NODE:
        *p = SysAllocString(strText.get());
        break;
    default:
        return E_UNEXPECTED;
    }
    return p ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE CIEHtmlSelectionObject::empty()
{
    if (!mSelection)
        return E_FAIL;
    mSelection->RemoveAllRanges();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlSelectionObject::clear()
{
    if (!mSelection)
        return E_FAIL;
    mSelection->DeleteFromDocument();
    return S_OK;
}


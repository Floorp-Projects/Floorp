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
#include "IEHtmlTxtRange.h"
#include "IEHtmlNode.h"
#include "IEHtmlElement.h"

#include "nsIDomNsRange.h"
#include "nsIDOMDocumentFragment.h"

CRange::CRange()
{
}

CRange::~CRange()
{
}

void CRange::SetRange(nsIDOMRange *pRange)
{
    mRange = pRange;
}

HRESULT CRange::GetParentElement(IHTMLElement **ppParent)
{
    if (ppParent == NULL)
        return E_INVALIDARG;
    *ppParent = NULL;
    // get common ancestor property:
    nsCOMPtr<nsIDOMNode> domNode;
    mRange->GetCommonAncestorContainer(getter_AddRefs(domNode));
    if (!domNode)
        return S_OK;
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(domNode);
    if (!domElement)
    {
        // domNode can be a nsITextNode. In this case, its parent is a nsIDOMElement:
        nsCOMPtr<nsIDOMNode> parentNode;
        domNode->GetParentNode(getter_AddRefs(parentNode));
        domElement = do_QueryInterface(parentNode);
        // Is a textrange always supposed to have a parentElement? Remove 2 lines if not:
        if (!domElement)
            return E_UNEXPECTED;
        domNode = parentNode;
    }
    // get or create com object:
    CComPtr<IUnknown> pNode;
    HRESULT hr = CIEHtmlDomNode::FindOrCreateFromDOMNode(domNode, &pNode);
    if (FAILED(hr))
        return hr;
    if (FAILED(pNode->QueryInterface(IID_IHTMLElement, (void **)ppParent)))
        return E_UNEXPECTED;

    return S_OK;
}

// CIEHtmlTxtRange

CIEHtmlTxtRange::CIEHtmlTxtRange()
{
}

CIEHtmlTxtRange::~CIEHtmlTxtRange()
{
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::pasteHTML(BSTR html)
{
    nsCOMPtr<nsIDOMDocumentFragment> domDocFragment;
    nsAutoString nsStrHtml(OLE2W(html));

    if (NS_FAILED(mRange->DeleteContents()))
        return E_FAIL;
    nsCOMPtr<nsIDOMNSRange> domNSRange = do_QueryInterface(mRange);
    if (!domNSRange)
        return E_FAIL;
    domNSRange->CreateContextualFragment(nsStrHtml, getter_AddRefs(domDocFragment));
    if (!domDocFragment)
        return E_FAIL;
    mRange->InsertNode(domDocFragment);
    mRange->Detach();
    
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::get_text(BSTR __RPC_FAR *p)
{
    if (p == NULL)
        return E_INVALIDARG;
    *p = NULL;

    nsAutoString strText;
    mRange->ToString(strText);
    
    *p = SysAllocString(strText.get());
    return *p ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::parentElement(IHTMLElement __RPC_FAR *__RPC_FAR *Parent)
{
    return GetParentElement(Parent);
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::get_htmlText(BSTR __RPC_FAR *p)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::put_text(BSTR v)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::duplicate(IHTMLTxtRange __RPC_FAR *__RPC_FAR *Duplicate)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::inRange(IHTMLTxtRange __RPC_FAR *Range, VARIANT_BOOL __RPC_FAR *InRange)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::isEqual(IHTMLTxtRange __RPC_FAR *Range, VARIANT_BOOL __RPC_FAR *IsEqual)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::scrollIntoView(VARIANT_BOOL fStart/* = -1*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::collapse(VARIANT_BOOL Start/* = -1*/)
{
    nsresult rv = mRange->Collapse(Start?PR_TRUE:PR_FALSE);
    return FAILED(rv)?E_FAIL:S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::expand(BSTR Unit,VARIANT_BOOL __RPC_FAR *Success)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::move(BSTR Unit, long Count, long __RPC_FAR *ActualCount)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::moveStart(BSTR Unit, long Count, long __RPC_FAR *ActualCount)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::moveEnd(BSTR Unit, long Count, long __RPC_FAR *ActualCount)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::select( void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::moveToElementText(IHTMLElement __RPC_FAR *element)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::setEndPoint(BSTR how, IHTMLTxtRange __RPC_FAR *SourceRange)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::compareEndPoints(BSTR how, IHTMLTxtRange __RPC_FAR *SourceRange, long __RPC_FAR *ret)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::findText(BSTR String, long count, long Flags, VARIANT_BOOL __RPC_FAR *Success)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::moveToPoint(long x, long y)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::getBookmark(BSTR __RPC_FAR *Boolmark)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::moveToBookmark(BSTR Bookmark, VARIANT_BOOL __RPC_FAR *Success)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlTxtRange::execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

// IHTMLControlRange

CIEHtmlControlRange::CIEHtmlControlRange()
{
}

CIEHtmlControlRange::~CIEHtmlControlRange()
{
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::commonParentElement(IHTMLElement __RPC_FAR *__RPC_FAR *parent)
{
    return GetParentElement(parent);
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::select( void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::add(IHTMLControlElement __RPC_FAR *item)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::remove(long index)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::item(long index, IHTMLElement __RPC_FAR *__RPC_FAR *pdisp)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::scrollIntoView(VARIANT varargStart)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CIEHtmlControlRange::get_length(long __RPC_FAR *p)
{
    return E_NOTIMPL;
}


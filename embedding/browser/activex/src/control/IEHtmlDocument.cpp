/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "IEHtmlDocument.h"
#include "IEHtmlElementCollection.h"
#include "IEHtmlElement.h"

#include "MozillaBrowser.h"

#include <stack>

CIEHtmlDocument::CIEHtmlDocument()
{
    m_pParent = NULL;
    m_pNative = nsnull;
}


CIEHtmlDocument::~CIEHtmlDocument()
{
}


void CIEHtmlDocument::SetParent(CMozillaBrowser *parent)
{
    m_pParent = parent;
}


void CIEHtmlDocument::SetNative(nsIDOMHTMLDocument *native)
{
    m_pNative = native;
}


HRESULT CIEHtmlDocument::GetIDispatch(IDispatch **pDispatch)
{
    if (pDispatch == NULL)
    {
        return E_INVALIDARG;
    }

    IDispatch *pDisp = (IDispatch *) this;
    NG_ASSERT(pDisp);
    pDisp->AddRef();
    *pDispatch = pDisp;

    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IHTMLDocument methods

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_Script(IDispatch __RPC_FAR *__RPC_FAR *p)
{
    return E_NOTIMPL;
}

///////////////////////////////////////////////////////////////////////////////
// IHTMLDocument2 methods

struct HtmlPos
{
    CIPtr(IHTMLElementCollection) m_cpCollection;
    long m_nPos;

    HtmlPos(IHTMLElementCollection *pCol, long nPos) :
        m_cpCollection(pCol),
        m_nPos(nPos)
    {
    }
};


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_all(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    // Get all elements
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollection::CreateFromParentNode(this, TRUE, (CIEHtmlElementCollection **) &pCollection);
    if (pCollection)
    {
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_body(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }
    *p = NULL;

    nsCOMPtr<nsIDOMHTMLElement> bodyElement;

    m_pNative->GetBody(getter_AddRefs(bodyElement));
    if (bodyElement)
    {
        nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);

        CIEHtmlElementInstance *pElement = NULL;
        CIEHtmlElementInstance::CreateInstance(&pElement);
        if (pElement)
        {
            pElement->SetDOMNode(bodyNode);
            pElement->SetParentNode(this);
            pElement->QueryInterface(IID_IHTMLElement, (void **) p);
        }
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_activeElement(IHTMLElement __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_images(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    nsCOMPtr<nsIDOMHTMLCollection> nodeList;
    m_pNative->GetImages(getter_AddRefs(nodeList));

    // Get all elements
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollection::CreateFromDOMHTMLCollection(this, nodeList, (CIEHtmlElementCollection **) &pCollection);
    if (pCollection)
    {
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_applets(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    nsCOMPtr<nsIDOMHTMLCollection> nodeList;
    m_pNative->GetApplets(getter_AddRefs(nodeList));

    // Get all elements
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollection::CreateFromDOMHTMLCollection(this, nodeList, (CIEHtmlElementCollection **) &pCollection);
    if (pCollection)
    {
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_links(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    nsCOMPtr<nsIDOMHTMLCollection> nodeList;
    m_pNative->GetLinks(getter_AddRefs(nodeList));

    // Get all elements
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollection::CreateFromDOMHTMLCollection(this, nodeList, (CIEHtmlElementCollection **) &pCollection);
    if (pCollection)
    {
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_forms(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    nsCOMPtr<nsIDOMHTMLCollection> nodeList;
    m_pNative->GetForms(getter_AddRefs(nodeList));

    // Get all elements
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollection::CreateFromDOMHTMLCollection(this, nodeList, (CIEHtmlElementCollection **) &pCollection);
    if (pCollection)
    {
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_anchors(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    // Validate parameters
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    nsCOMPtr<nsIDOMHTMLCollection> nodeList;
    m_pNative->GetAnchors(getter_AddRefs(nodeList));

    // Get all elements
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollection::CreateFromDOMHTMLCollection(this, nodeList, (CIEHtmlElementCollection **) &pCollection);
    if (pCollection)
    {
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_title(BSTR v)
{
    if (m_pNative)
    {
        nsAutoString newTitle((PRUnichar *) v);
        m_pNative->SetTitle(newTitle);
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_title(BSTR __RPC_FAR *p)
{
    nsAutoString value;
    if (m_pNative == NULL || m_pNative->GetTitle(value))
    {
        return E_FAIL;
    }
    *p = SysAllocString(value.get()); 
    return p ? S_OK : E_OUTOFMEMORY;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_scripts(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_designMode(BSTR v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_designMode(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_selection(IHTMLSelectionObject __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_readyState(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_frames(IHTMLFramesCollection2 __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_embeds(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_plugins(IHTMLElementCollection __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_alinkColor(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_alinkColor(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_bgColor(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_bgColor(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_fgColor(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fgColor(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_linkColor(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_linkColor(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_vlinkColor(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_vlinkColor(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_referrer(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_location(IHTMLLocation __RPC_FAR *__RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_lastModified(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_URL(BSTR v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_URL(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_domain(BSTR v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_domain(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_cookie(BSTR v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_cookie(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_expando(VARIANT_BOOL v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_expando(VARIANT_BOOL __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_charset(BSTR v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_charset(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_defaultCharset(BSTR v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_defaultCharset(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_mimeType(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileSize(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileCreatedDate(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileModifiedDate(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_fileUpdatedDate(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_security(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_protocol(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_nameProp(BSTR __RPC_FAR *p)
{
    *p = NULL;
    return E_NOTIMPL;
}


HRESULT CIEHtmlDocument::WriteCommon(SAFEARRAY __RPC_FAR * psarray, int bLn)
{
    if (psarray->cDims != 1)
    {
        return E_INVALIDARG;
    }
    if (!(psarray->fFeatures & (FADF_BSTR | FADF_VARIANT)))
    {
        return E_INVALIDARG;
    }

    HRESULT hr;

    hr = SafeArrayLock(psarray);
    if (FAILED(hr))
    {
        return hr;
    }

    for (DWORD i = 0; i < psarray->rgsabound[0].cElements; i++)
    {
        nsString str;
        if (psarray->fFeatures & FADF_BSTR)
        {
            BSTR *bstrArray = (BSTR *) psarray->pvData;
            str = nsString(bstrArray[i], SysStringLen(bstrArray[i]));
        }
        else if (psarray->fFeatures & FADF_VARIANT)
        {
            VARIANT *vArray = (VARIANT *) psarray->pvData;
            VARIANT vStr;
            VariantInit(&vStr);
            hr = VariantChangeType(&vStr, &vArray[i], 0, VT_BSTR);
            if (FAILED(hr))
            {
                SafeArrayUnlock(psarray);
                return hr;
            }
            str = nsString(vStr.bstrVal, SysStringLen(vStr.bstrVal));
            VariantClear(&vStr);
        }

        if (bLn && !i)
        {
            if (m_pNative->Writeln(str))
            {
                SafeArrayUnlock(psarray);
                return E_FAIL;
            }
        }
        else
        {
            if (m_pNative->Write(str))
            {
                SafeArrayUnlock(psarray);
                return E_FAIL;
            }
        }
    }
    
    hr = SafeArrayUnlock(psarray);
    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::write(SAFEARRAY __RPC_FAR * psarray)
{
       return WriteCommon(psarray, 0);
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::writeln(SAFEARRAY __RPC_FAR * psarray)
{
      return WriteCommon(psarray, 1);
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::open(BSTR url, VARIANT name, VARIANT features, VARIANT replace, IDispatch __RPC_FAR *__RPC_FAR *pomWindowResult)
{
    if (m_pNative == NULL || m_pNative->Open())
    {
        return E_FAIL;
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::close(void)
{
    if (m_pNative == NULL || m_pNative->Close())
    {
        return E_FAIL;
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::clear(void)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::createElement(BSTR eTag, IHTMLElement __RPC_FAR *__RPC_FAR *newElem)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onhelp(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onhelp(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onclick(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onclick(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_ondblclick(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_ondblclick(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onkeyup(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onkeyup(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;

}

HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onkeydown(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onkeydown(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onkeypress(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onkeypress(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmouseup(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmouseup(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmousedown(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmousedown(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmousemove(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmousemove(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmouseout(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmouseout(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onmouseover(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onmouseover(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onreadystatechange(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onreadystatechange(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onafterupdate(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onafterupdate(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onrowexit(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onrowexit(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onrowenter(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onrowenter(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_ondragstart(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_ondragstart(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onselectstart(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onselectstart(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::elementFromPoint(long x, long y, IHTMLElement __RPC_FAR *__RPC_FAR *elementHit)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_parentWindow(IHTMLWindow2 __RPC_FAR *__RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_styleSheets(IHTMLStyleSheetsCollection __RPC_FAR *__RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onbeforeupdate(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onbeforeupdate(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::put_onerrorupdate(VARIANT v)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::get_onerrorupdate(VARIANT __RPC_FAR *p)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::toString(BSTR __RPC_FAR *String)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::createStyleSheet(BSTR bstrHref, long lIndex, IHTMLStyleSheet __RPC_FAR *__RPC_FAR *ppnewStyleSheet)
{
    return E_NOTIMPL;
}


///////////////////////////////////////////////////////////////////////////////
// IOleCommandTarget implementation


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::QueryStatus(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[], OLECMDTEXT __RPC_FAR *pCmdText)
{
    HRESULT hr = E_NOTIMPL;
    if(m_pParent)
    {
        hr = m_pParent->QueryStatus(pguidCmdGroup,cCmds,prgCmds,pCmdText);
    }
    return hr;
}


HRESULT STDMETHODCALLTYPE CIEHtmlDocument::Exec(const GUID __RPC_FAR *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
{
    HRESULT hr = E_NOTIMPL;
    if(m_pParent)
    {
        hr = m_pParent->Exec(pguidCmdGroup,nCmdID,nCmdexecopt,pvaIn,pvaOut);
    }
    return hr;
}


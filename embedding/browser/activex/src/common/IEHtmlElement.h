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
 *   Adam Lock <adamlock@netscape.com>
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
#ifndef IEHTMLELEMENT_H
#define IEHTMLELEMENT_H

#include "IEHtmlNode.h"
#include "IEHtmlElementCollection.h"

class CIEHtmlElement :
    public CNode,
    public IDispatchImpl<IHTMLElement, &IID_IHTMLElement, &LIBID_MSHTML>
{
public:
    DECLARE_AGGREGATABLE(CIEHtmlElement)
    CIEHtmlElement();
    HRESULT FinalConstruct( );
    void FinalRelease( );

DECLARE_GET_CONTROLLING_UNKNOWN()

protected:
    virtual ~CIEHtmlElement();

public:

BEGIN_COM_MAP(CIEHtmlElement)
    COM_INTERFACE_ENTRY2(IDispatch, IHTMLElement)
    COM_INTERFACE_ENTRY(IHTMLElement)
    COM_INTERFACE_ENTRY_AGGREGATE(IID_IHTMLDOMNode, m_pNodeAgg)
END_COM_MAP()

    virtual HRESULT GetChildren(CIEHtmlElementCollectionInstance **ppCollection);
    virtual HRESULT GetHtmlDomNode(CIEHtmlDomNode **ppHtmlDomNode);
    virtual HRESULT SetDOMNode(nsIDOMNode *pDomNode);
    virtual HRESULT SetParent(CNode *pParent);

    // Implementation of IHTMLElement
    virtual HRESULT STDMETHODCALLTYPE setAttribute(BSTR strAttributeName, VARIANT AttributeValue, LONG lFlags);
    virtual HRESULT STDMETHODCALLTYPE getAttribute(BSTR strAttributeName, LONG lFlags, VARIANT __RPC_FAR *AttributeValue);
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(BSTR strAttributeName, LONG lFlags, VARIANT_BOOL __RPC_FAR *pfSuccess);
    virtual HRESULT STDMETHODCALLTYPE put_className(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_className(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_id(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_id(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_tagName(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_parentElement(IHTMLElement __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_style(IHTMLStyle __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onhelp(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onhelp(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onclick(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onclick(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_ondblclick(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_ondblclick(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onkeydown(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onkeydown(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onkeyup(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onkeyup(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onkeypress(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onkeypress(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onmouseout(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onmouseout(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onmouseover(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onmouseover(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onmousemove(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onmousemove(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onmousedown(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onmousedown(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onmouseup(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onmouseup(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_document(IDispatch __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_title(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_title(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_language(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_language(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onselectstart(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onselectstart(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE scrollIntoView(VARIANT varargStart);
    virtual HRESULT STDMETHODCALLTYPE contains(IHTMLElement __RPC_FAR *pChild, VARIANT_BOOL __RPC_FAR *pfResult);
    virtual HRESULT STDMETHODCALLTYPE get_sourceIndex(long __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_recordNumber(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_lang(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_lang(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_offsetLeft(long __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_offsetTop(long __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_offsetWidth(long __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_offsetHeight(long __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_offsetParent(IHTMLElement __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_innerHTML(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_innerHTML(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_innerText(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_innerText(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_outerHTML(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_outerHTML(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_outerText(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE get_outerText(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE insertAdjacentHTML(BSTR where, BSTR html);
    virtual HRESULT STDMETHODCALLTYPE insertAdjacentText(BSTR where, BSTR text);
    virtual HRESULT STDMETHODCALLTYPE get_parentTextEdit(IHTMLElement __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_isTextEdit(VARIANT_BOOL __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE click(void);
    virtual HRESULT STDMETHODCALLTYPE get_filters(IHTMLFiltersCollection __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_ondragstart(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_ondragstart(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE toString(BSTR __RPC_FAR *String);
    virtual HRESULT STDMETHODCALLTYPE put_onbeforeupdate(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onbeforeupdate(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onafterupdate(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onafterupdate(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onerrorupdate(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onerrorupdate(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onrowexit(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onrowexit(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onrowenter(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onrowenter(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_ondatasetchanged(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_ondatasetchanged(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_ondataavailable(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_ondataavailable(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_ondatasetcomplete(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_ondatasetcomplete(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_onfilterchange(VARIANT v);
    virtual HRESULT STDMETHODCALLTYPE get_onfilterchange(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_children(IDispatch __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_all(IDispatch __RPC_FAR *__RPC_FAR *p);
    
protected:
    IUnknown* m_pNodeAgg;
};

#define CIEHTMLELEMENT_INTERFACES \
    COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLElement) \
    COM_INTERFACE_ENTRY_IID(IID_IHTMLElement, IHTMLElement)

typedef CComObject<CIEHtmlElement> CIEHtmlElementInstance;

#endif
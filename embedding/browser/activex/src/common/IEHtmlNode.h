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
 *   Adam Lock <adamlock@eircom.net>
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
#ifndef IEHTMLNODE_H
#define IEHTMLNODE_H


#include "nsCOMPtr.h"
#include "nsIDOMNode.h"

// NOTE: Nasty hack in case arcane SDK doesn't define IHTMLDOMNode
#ifndef __IHTMLDOMNode_INTERFACE_DEFINED__
#define __IHTMLDOMNode_INTERFACE_DEFINED__
    MIDL_INTERFACE("3050f5da-98b5-11cf-bb82-00aa00bdce0b")
    IHTMLDOMNode : public IDispatch
    {
    public:
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeType( 
            /* [out][retval] */ long *p) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_parentNode( 
            /* [out][retval] */ IHTMLDOMNode **p) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE hasChildNodes( 
            /* [out][retval] */ VARIANT_BOOL *fChildren) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_childNodes( 
            /* [out][retval] */ IDispatch **p) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_attributes( 
            /* [out][retval] */ IDispatch **p) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE insertBefore( 
            /* [in] */ IHTMLDOMNode *newChild,
            /* [in][optional] */ VARIANT refChild,
            /* [out][retval] */ IHTMLDOMNode **node) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE removeChild( 
            /* [in] */ IHTMLDOMNode *oldChild,
            /* [out][retval] */ IHTMLDOMNode **node) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE replaceChild( 
            /* [in] */ IHTMLDOMNode *newChild,
            /* [in] */ IHTMLDOMNode *oldChild,
            /* [out][retval] */ IHTMLDOMNode **node) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE cloneNode( 
            /* [in] */ VARIANT_BOOL fDeep,
            /* [out][retval] */ IHTMLDOMNode **clonedNode) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE removeNode( 
            /* [in][defaultvalue] */ VARIANT_BOOL fDeep,
            /* [out][retval] */ IHTMLDOMNode **removed) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE swapNode( 
            /* [in] */ IHTMLDOMNode *otherNode,
            /* [out][retval] */ IHTMLDOMNode **swappedNode) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE replaceNode( 
            /* [in] */ IHTMLDOMNode *replacement,
            /* [out][retval] */ IHTMLDOMNode **replaced) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE appendChild( 
            /* [in] */ IHTMLDOMNode *newChild,
            /* [out][retval] */ IHTMLDOMNode **node) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeName( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_nodeValue( 
            /* [in] */ VARIANT v) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeValue( 
            /* [out][retval] */ VARIANT *p) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_firstChild( 
            /* [out][retval] */ IHTMLDOMNode **p) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_lastChild( 
            /* [out][retval] */ IHTMLDOMNode **p) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_previousSibling( 
            /* [out][retval] */ IHTMLDOMNode **p) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nextSibling( 
            /* [out][retval] */ IHTMLDOMNode **p) = 0;
        
    };
#endif

class CNode :
    public CComObjectRootEx<CComMultiThreadModel>
{
protected:
    CNode();
    virtual ~CNode();

public:
    CNode *mParent;
    nsCOMPtr<nsIDOMNode> mDOMNode;

    static HRESULT FindFromDOMNode(nsIDOMNode *pIDOMNode, CNode **pNode);
    virtual HRESULT SetParent(CNode *pParent);
    virtual HRESULT SetDOMNode(nsIDOMNode *pIDOMNode);
};

class CIEHtmlDomNode :
    public CNode,
    public IDispatchImpl<IHTMLDOMNode, &__uuidof(IHTMLDOMNode), &LIBID_MSHTML>
{
public:
    DECLARE_AGGREGATABLE(CIEHtmlDomNode)
    CIEHtmlDomNode();

    static HRESULT FindFromDOMNode(nsIDOMNode *pIDOMNode, IUnknown **pNode);
    static HRESULT FindOrCreateFromDOMNode(nsIDOMNode *pIDOMNode, IUnknown **pNode);
    static HRESULT CreateFromDOMNode(nsIDOMNode *pIDOMNode, IUnknown **pNode);
    virtual HRESULT SetDOMNode(nsIDOMNode *pIDOMNode);

    DECLARE_GET_CONTROLLING_UNKNOWN()
protected:
    virtual ~CIEHtmlDomNode();

public:

BEGIN_COM_MAP(CIEHtmlDomNode)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLDOMNode)
    //COM_INTERFACE_ENTRY_FUNC(IID_IHTMLElement, 0, QueryInterfaceOnNode)
END_COM_MAP()

    static HRESULT WINAPI QueryInterfaceOnNode(void* pv, REFIID riid, LPVOID* ppv, DWORD dw);

    //IID_IHTMLDOMNode
    virtual HRESULT STDMETHODCALLTYPE get_nodeType(long __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_parentNode(IHTMLDOMNode __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(VARIANT_BOOL __RPC_FAR *fChildren);
    virtual HRESULT STDMETHODCALLTYPE get_childNodes(IDispatch __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_attributes(IDispatch __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE insertBefore(IHTMLDOMNode __RPC_FAR *newChild, VARIANT refChild, IHTMLDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual HRESULT STDMETHODCALLTYPE removeChild(IHTMLDOMNode __RPC_FAR *oldChild, IHTMLDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual HRESULT STDMETHODCALLTYPE replaceChild(IHTMLDOMNode __RPC_FAR *newChild, IHTMLDOMNode __RPC_FAR *oldChild, IHTMLDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual HRESULT STDMETHODCALLTYPE cloneNode(VARIANT_BOOL fDeep, IHTMLDOMNode __RPC_FAR *__RPC_FAR *clonedNode);
    virtual HRESULT STDMETHODCALLTYPE removeNode(VARIANT_BOOL fDeep, IHTMLDOMNode __RPC_FAR *__RPC_FAR *removed);
    virtual HRESULT STDMETHODCALLTYPE swapNode(IHTMLDOMNode __RPC_FAR *otherNode, IHTMLDOMNode __RPC_FAR *__RPC_FAR *swappedNode);
    virtual HRESULT STDMETHODCALLTYPE replaceNode(IHTMLDOMNode __RPC_FAR *replacement, IHTMLDOMNode __RPC_FAR *__RPC_FAR *replaced);
    virtual HRESULT STDMETHODCALLTYPE appendChild(IHTMLDOMNode __RPC_FAR *newChild, IHTMLDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual HRESULT STDMETHODCALLTYPE get_nodeName(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_nodeValue(VARIANT p);
    virtual HRESULT STDMETHODCALLTYPE get_nodeValue(VARIANT __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_firstChild(IHTMLDOMNode __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_lastChild(IHTMLDOMNode __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_previousSibling(IHTMLDOMNode __RPC_FAR *__RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE get_nextSibling(IHTMLDOMNode __RPC_FAR *__RPC_FAR *p);
};

typedef CComObject<CIEHtmlDomNode> CIEHtmlDomNodeInstance;

#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef IEHTMLNODE_H
#define IEHTMLNODE_H

class CIEHtmlNode :    public CComObjectRootEx<CComSingleThreadModel>
{
protected:
    nsIDOMNode *m_pIDOMNode;
    IDispatch  *m_pIDispParent;

public:
    CIEHtmlNode();
protected:
    virtual ~CIEHtmlNode();

public:
    static HRESULT FindFromDOMNode(nsIDOMNode *pIDOMNode, CIEHtmlNode **pHtmlNode);
    
    virtual HRESULT SetDOMNode(nsIDOMNode *pIDOMNode);
    virtual HRESULT GetDOMNode(nsIDOMNode **pIDOMNode);
    virtual HRESULT GetDOMElement(nsIDOMElement **pIDOMElement);
    virtual HRESULT SetParentNode(IDispatch *pIDispParent);
    virtual HRESULT GetIDispatch(IDispatch **pDispatch);
};

typedef CComObject<CIEHtmlNode> CIEHtmlNodeInstance;

#endif
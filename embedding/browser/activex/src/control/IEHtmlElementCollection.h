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
#ifndef IEHTMLNODECOLLECTION_H
#define IEHTMLNODECOLLECTION_H

#include "IEHtmlNode.h"

class CIEHtmlElement;



class CIEHtmlElementCollection : public CComObjectRootEx<CComSingleThreadModel>,
							  public IDispatchImpl<IHTMLElementCollection, &IID_IHTMLElementCollection, &LIBID_MSHTML>
{
public:
	typedef std::vector< CIPtr(IDispatch) > ElementList;

private:
	ElementList m_cNodeList;

public:
	CIEHtmlElementCollection();

protected:
	virtual ~CIEHtmlElementCollection();

	// Pointer to parent node/document
	IDispatch *m_pIDispParent;

public:
	// Adds a node to the collection
	virtual HRESULT AddNode(IDispatch *pNode);

	// Sets the parent node of this collection
	virtual HRESULT SetParentNode(IDispatch *pIDispParent);

	// Populates the collection with items from the DOM node
	virtual HRESULT PopulateFromDOMNode(nsIDOMNode *pIDOMNode, BOOL bRecurseChildren);

	// Helper method creates a collection from a parent node
	static HRESULT CreateFromParentNode(CIEHtmlNode *pParentNode, CIEHtmlElementCollection **pInstance, BOOL bRecurseChildren = FALSE);


	ElementList &GetElements()
	{
		return m_cNodeList;
	}

BEGIN_COM_MAP(CIEHtmlElementCollection)
	COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLElementCollection)
	COM_INTERFACE_ENTRY_IID(IID_IHTMLElementCollection, IHTMLElementCollection)
END_COM_MAP()

	// IHTMLElementCollection methods
	virtual HRESULT STDMETHODCALLTYPE toString(BSTR __RPC_FAR *String);
	virtual HRESULT STDMETHODCALLTYPE put_length(long v);
	virtual HRESULT STDMETHODCALLTYPE get_length(long __RPC_FAR *p);
	virtual HRESULT STDMETHODCALLTYPE get__newEnum(IUnknown __RPC_FAR *__RPC_FAR *p);
	virtual HRESULT STDMETHODCALLTYPE item(VARIANT name, VARIANT index, IDispatch __RPC_FAR *__RPC_FAR *pdisp);
	virtual HRESULT STDMETHODCALLTYPE tags(VARIANT tagName, IDispatch __RPC_FAR *__RPC_FAR *pdisp);
};

typedef CComObject<CIEHtmlElementCollection> CIEHtmlElementCollectionInstance;

#endif
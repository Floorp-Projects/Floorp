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
#include "stdafx.h"

#include <stack>

#include "IEHtmlElement.h"
#include "IEHtmlElementCollection.h"

CIEHtmlElementCollection::CIEHtmlElementCollection()
{
    m_pIDispParent = NULL;
    mNodeList = NULL;
    mNodeListCount = 0;
    mNodeListCapacity = 0;
}

CIEHtmlElementCollection::~CIEHtmlElementCollection()
{
    // Clean the node list
    if (mNodeList)
    {
        for (PRUint32 i = 0; i < mNodeListCount; i++)
        {
            IDispatch *pDisp = mNodeList[i];
            if (pDisp)
            {
                pDisp->Release();
            }
        }
        free(mNodeList);
        mNodeList = NULL;
        mNodeListCount = 0;
        mNodeListCapacity = 0;
    }
}

HRESULT CIEHtmlElementCollection::SetParentNode(IDispatch *pIDispParent)
{
    m_pIDispParent = pIDispParent;
    return S_OK;
}

template <class nodeListType>
HRESULT PopulateFromList(CIEHtmlElementCollection *pCollection, nodeListType *pNodeList, BOOL bRecurseChildren)
{
    if (pNodeList == nsnull)
    {
        return S_OK;
    }

    // Recurse through the children of the node (and the children of that)
    // to populate the collection

    // Iterate through items in list
    PRUint32 length = 0;
    pNodeList->GetLength(&length);
    for (PRUint32 i = 0; i < length; i++)
    {
        // Get the next item from the list
        nsCOMPtr<nsIDOMNode> childNode;
        pNodeList->Item(i, getter_AddRefs(childNode));
        if (!childNode)
        {
            // Empty node (unexpected, but try and carry on anyway)
            NG_ASSERT(0);
            continue;
        }

        // Skip nodes representing, text, attributes etc.
        PRUint16 nodeType;
        childNode->GetNodeType(&nodeType);
        if (nodeType != nsIDOMNode::ELEMENT_NODE)
        {
            continue;
        }

        // Create an equivalent IE element
        CIEHtmlNode *pHtmlNode = NULL;
        CIEHtmlElementInstance *pHtmlElement = NULL;
        CIEHtmlElementInstance::FindFromDOMNode(childNode, &pHtmlNode);
        if (!pHtmlNode)
        {
            CIEHtmlElementInstance::CreateInstance(&pHtmlElement);
            pHtmlElement->SetDOMNode(childNode);
            pHtmlElement->SetParentNode(pCollection->m_pIDispParent);
        }
        else
        {
            pHtmlElement = (CIEHtmlElementInstance *) pHtmlNode;
        }
        if (pHtmlElement)
        {
            pCollection->AddNode(pHtmlElement);
        }

        if (bRecurseChildren)
        {
            // Test if the node has children and recursively add them too
            nsCOMPtr<nsIDOMNodeList> childNodeList;
            childNode->GetChildNodes(getter_AddRefs(childNodeList));
            PRUint32 childListLength = 0;
            if (childNodeList)
            {
                childNodeList->GetLength(&childListLength);
            }
            if (childListLength > 0)
            {
                PopulateFromList<nsIDOMNodeList>(pCollection, childNodeList, bRecurseChildren);
            }
        }
    }

    return S_OK;
}


HRESULT CIEHtmlElementCollection::PopulateFromDOMNodeList(nsIDOMNodeList *pNodeList, BOOL bRecurseChildren)
{
    return PopulateFromList<nsIDOMNodeList>(this, pNodeList, bRecurseChildren);
}


HRESULT CIEHtmlElementCollection::PopulateFromDOMHTMLCollection(nsIDOMHTMLCollection *pNodeList, BOOL bRecurseChildren)
{
    return PopulateFromList<nsIDOMHTMLCollection>(this, pNodeList, bRecurseChildren);
}


HRESULT CIEHtmlElementCollection::PopulateFromDOMNode(nsIDOMNode *pIDOMNode, BOOL bRecurseChildren)
{
    if (pIDOMNode == nsnull)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    // Get elements from the DOM node
    nsCOMPtr<nsIDOMNodeList> nodeList;
    pIDOMNode->GetChildNodes(getter_AddRefs(nodeList));
    if (!nodeList)
    {
        return S_OK;
    }

    return PopulateFromDOMNodeList(nodeList, bRecurseChildren);
}


HRESULT CIEHtmlElementCollection::CreateFromDOMHTMLCollection(CIEHtmlNode *pParentNode, nsIDOMHTMLCollection *pNodeList, CIEHtmlElementCollection **pInstance)
{
    if (pInstance == NULL || pParentNode == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    // Get the DOM node from the parent node
    nsCOMPtr<nsIDOMNode> domNode;
    pParentNode->GetDOMNode(getter_AddRefs(domNode));
    if (!domNode)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    *pInstance = NULL;

    // Create a collection object
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
    if (pCollection == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    // Initialise and populate the collection
    CIPtr(IDispatch) cpDispNode;
    pParentNode->GetIDispatch(&cpDispNode);
    pCollection->SetParentNode(cpDispNode);
    pCollection->PopulateFromDOMHTMLCollection(pNodeList, FALSE);

    *pInstance = pCollection;

    return S_OK;
}


HRESULT CIEHtmlElementCollection::CreateFromDOMNodeList(CIEHtmlNode *pParentNode, nsIDOMNodeList *pNodeList, CIEHtmlElementCollection **pInstance)
{
    if (pInstance == NULL || pParentNode == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    // Get the DOM node from the parent node
    nsCOMPtr<nsIDOMNode> domNode;
    pParentNode->GetDOMNode(getter_AddRefs(domNode));
    if (!domNode)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    *pInstance = NULL;

    // Create a collection object
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
    if (pCollection == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    // Initialise and populate the collection
    CIPtr(IDispatch) cpDispNode;
    pParentNode->GetIDispatch(&cpDispNode);
    pCollection->SetParentNode(cpDispNode);
    pCollection->PopulateFromDOMNodeList(pNodeList, FALSE);

    *pInstance = pCollection;

    return S_OK;
}

HRESULT CIEHtmlElementCollection::CreateFromParentNode(CIEHtmlNode *pParentNode, BOOL bRecurseChildren, CIEHtmlElementCollection **pInstance)
{
    if (pInstance == NULL || pParentNode == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    // Get the DOM node from the parent node
    nsCOMPtr<nsIDOMNode> domNode;
    pParentNode->GetDOMNode(getter_AddRefs(domNode));
    if (!domNode)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    *pInstance = NULL;

    // Create a collection object
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
    if (pCollection == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    // Initialise and populate the collection
    CIPtr(IDispatch) cpDispNode;
    pParentNode->GetIDispatch(&cpDispNode);
    pCollection->SetParentNode(cpDispNode);
    pCollection->PopulateFromDOMNode(domNode, bRecurseChildren);

    *pInstance = pCollection;

    return S_OK;
}


HRESULT CIEHtmlElementCollection::AddNode(IDispatch *pNode)
{
    if (pNode == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    const PRUint32 c_NodeListResizeBy = 10;

    if (mNodeList == NULL)
    {
        mNodeListCapacity = c_NodeListResizeBy;
        mNodeList = (IDispatch **) malloc(sizeof(IDispatch *) * mNodeListCapacity);
        mNodeListCount = 0;
    }
    else if (mNodeListCount == mNodeListCapacity)
    {
        mNodeListCapacity += c_NodeListResizeBy;
        mNodeList = (IDispatch **) realloc(mNodeList, sizeof(IDispatch *) * mNodeListCapacity);
    }

    if (mNodeList == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    pNode->AddRef();
    mNodeList[mNodeListCount++] = pNode;

    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IHTMLElementCollection methods


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::toString(BSTR __RPC_FAR *String)
{
    if (String == NULL)
    {
        return E_INVALIDARG;
    }
    *String = SysAllocString(OLESTR("ElementCollection"));
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::put_length(long v)
{
    // What is the point of this method?
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::get_length(long __RPC_FAR *p)
{
    if (p == NULL)
    {
        return E_INVALIDARG;
    }
    
    // Return the size of the collection
    *p = mNodeListCount;
    return S_OK;
}

typedef CComObject<CComEnum<IEnumVARIANT, &IID_IEnumVARIANT, VARIANT, _Copy<VARIANT> > > CComEnumVARIANT;

HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::get__newEnum(IUnknown __RPC_FAR *__RPC_FAR *p)
{
    if (p == NULL)
    {
        return E_INVALIDARG;
    }

    *p = NULL;

    // Create a new IEnumVARIANT object
    CComEnumVARIANT *pEnumVARIANT = NULL;
    CComEnumVARIANT::CreateInstance(&pEnumVARIANT);
    if (pEnumVARIANT == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    int nObject;
    int nObjects = mNodeListCount;

    // Create an array of VARIANTs
    VARIANT *avObjects = new VARIANT[nObjects];
    if (avObjects == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    // Copy the contents of the collection to the array
    for (nObject = 0; nObject < nObjects; nObject++)
    {
        VARIANT *pVariant = &avObjects[nObject];
        IUnknown *pUnkObject = mNodeList[nObject];
        VariantInit(pVariant);
        pVariant->vt = VT_UNKNOWN;
        pVariant->punkVal = pUnkObject;
        pUnkObject->AddRef();
    }

    // Copy the variants to the enumeration object
    pEnumVARIANT->Init(&avObjects[0], &avObjects[nObjects], NULL, AtlFlagCopy);

    // Cleanup the array
    for (nObject = 0; nObject < nObjects; nObject++)
    {
        VARIANT *pVariant = &avObjects[nObject];
        VariantClear(pVariant);
    }
    delete []avObjects;

    return pEnumVARIANT->QueryInterface(IID_IUnknown, (void**) p);
}


HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::item(VARIANT name, VARIANT index, IDispatch __RPC_FAR *__RPC_FAR *pdisp)
{
    if (pdisp == NULL)
    {
        return E_INVALIDARG;
    }
    
    *pdisp = NULL;

    // Note: parameter "name" contains the index unless its a string
    //       in which case index does. Sensible huh?

    CComVariant vIndex;
    if (SUCCEEDED(vIndex.ChangeType(VT_I4, &name)) ||
        SUCCEEDED(vIndex.ChangeType(VT_I4, &index)))
    {
        // Test for stupid values
        int nIndex = vIndex.lVal;
        if (nIndex < 0 || nIndex >= mNodeListCount)
        {
            return E_INVALIDARG;
        }
    
        *pdisp = NULL;
        IDispatch *pNode = mNodeList[nIndex];
        if (pNode == NULL)
        {
            NG_ASSERT(0);
            return E_UNEXPECTED;
        }

        pNode->QueryInterface(IID_IDispatch, (void **) pdisp);
        return S_OK;
    }
    else if (SUCCEEDED(vIndex.ChangeType(VT_BSTR, &index)))
    {
        // If this parameter contains a string, the method returns
        // a collection of objects, where the value of the name or
        // id property for each object is equal to the string. 

        // TODO all of the above!
        return E_NOTIMPL;
    }
    else
    {
        return E_INVALIDARG;
    }
}

HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::tags(VARIANT tagName, IDispatch __RPC_FAR *__RPC_FAR *pdisp)
{
    if (pdisp == NULL)
    {
        return E_INVALIDARG;
    }
    
    *pdisp = NULL;

    // TODO
    // iterate through collection looking for elements with matching tags
    
    return E_NOTIMPL;
}


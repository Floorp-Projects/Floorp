/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "stdafx.h"

#include <stack>

#include "IEHtmlElement.h"
#include "IEHtmlElementCollection.h"

#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNodeFilter.h"

CIEHtmlElementCollection::CIEHtmlElementCollection()
{
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

HRESULT CIEHtmlElementCollection::PopulateFromDOMHTMLCollection(nsIDOMHTMLCollection *pNodeList)
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
            pHtmlElement->SetParent(mParent);
        }
        else
        {
            pHtmlElement = (CIEHtmlElementInstance *) pHtmlNode;
        }
        if (pHtmlElement)
        {
            AddNode(pHtmlElement);
        }
    }
    return S_OK;
}

HRESULT CIEHtmlElementCollection::PopulateFromDOMNode(nsIDOMNode *aDOMNode, BOOL bRecurseChildren)
{
    if (aDOMNode == nsnull)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    PRBool hasChildNodes = PR_FALSE;
    aDOMNode->HasChildNodes(&hasChildNodes);
    if (hasChildNodes)
    {
        if (bRecurseChildren)
        {
            nsresult rv;

            // Search through parent nodes, looking for the DOM document
            nsCOMPtr<nsIDOMNode> docAsNode = aDOMNode;
            nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDOMNode);
            while (!doc) {
                docAsNode->GetParentNode(getter_AddRefs(docAsNode));
                if (!docAsNode)
                {
                    return E_FAIL;
                }
                doc = do_QueryInterface(docAsNode);
            }

            // Walk the DOM
            nsCOMPtr<nsIDOMDocumentTraversal> trav = do_QueryInterface(doc, &rv);
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
            nsCOMPtr<nsIDOMTreeWalker> walker;
            rv = trav->CreateTreeWalker(docAsNode, 
                nsIDOMNodeFilter::SHOW_ELEMENT,
                nsnull, PR_TRUE, getter_AddRefs(walker));
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

            // We're not interested in the document node, so we always start
            // with the next one, walking through them all to make the collection
            nsCOMPtr<nsIDOMNode> currentNode;
            walker->NextNode(getter_AddRefs(currentNode));
            while (currentNode)
            {
                // Create an equivalent IE element
                CIEHtmlNode *pHtmlNode = NULL;
                CIEHtmlElementInstance *pHtmlElement = NULL;
                CIEHtmlElementInstance::FindFromDOMNode(currentNode, &pHtmlNode);
                if (!pHtmlNode)
                {
                    CIEHtmlElementInstance::CreateInstance(&pHtmlElement);
                    pHtmlElement->SetDOMNode(currentNode);
                    pHtmlElement->SetParent(mParent);
                }
                else
                {
                    pHtmlElement = (CIEHtmlElementInstance *) pHtmlNode;
                }
                if (pHtmlElement)
                {
                    AddNode(pHtmlElement);
                }
                walker->NextNode(getter_AddRefs(currentNode));
            }
        }
        else
        {
            nsCOMPtr<nsIDOMNodeList> nodeList;
            aDOMNode->GetChildNodes(getter_AddRefs(nodeList));
            mDOMNodeList = nodeList;
        }
    }
    return S_OK;
}


HRESULT CIEHtmlElementCollection::CreateFromDOMHTMLCollection(CIEHtmlNode *pParentNode, nsIDOMHTMLCollection *pNodeList, CIEHtmlElementCollection **pInstance)
{
    if (pInstance == NULL || pParentNode == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    // Get the DOM node from the parent node
    if (!pParentNode->mDOMNode)
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
    pCollection->SetParent(pParentNode);
    pCollection->PopulateFromDOMHTMLCollection(pNodeList);

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
    if (!pParentNode->mDOMNode)
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
    pCollection->SetParent(pParentNode);
    pCollection->PopulateFromDOMNode(pParentNode->mDOMNode, bRecurseChildren);

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

    const PRUint32 c_NodeListResizeBy = 100;

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
    if (mDOMNodeList)
    {
        // Count the number of elements in the list
        PRUint32 elementCount = 0;
        PRUint32 length = 0;
        mDOMNodeList->GetLength(&length);
        for (PRUint32 i = 0; i < length; i++)
        {
            // Get the next item from the list
            nsCOMPtr<nsIDOMNode> childNode;
            mDOMNodeList->Item(i, getter_AddRefs(childNode));
            if (!childNode)
            {
                // Empty node (unexpected, but try and carry on anyway)
                NG_ASSERT(0);
                continue;
            }

            // Only count elements
            PRUint16 nodeType;
            childNode->GetNodeType(&nodeType);
            if (nodeType == nsIDOMNode::ELEMENT_NODE)
            {
                elementCount++;
            }
        }
        *p = elementCount;
    }
    else
    {
        *p = mNodeListCount;
    }
    return S_OK;
}

typedef CComObject<CComEnum<IEnumVARIANT, &IID_IEnumVARIANT, VARIANT, _Copy<VARIANT> > > CComEnumVARIANT;

HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::get__newEnum(IUnknown __RPC_FAR *__RPC_FAR *p)
{
    NG_TRACE_METHOD(CIEHtmlElementCollection::get__newEnum);

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

    int nObject = 0;
    long nObjects = 0;
    get_length(&nObjects);

    // Create an array of VARIANTs
    VARIANT *avObjects = new VARIANT[nObjects];
    if (avObjects == NULL)
    {
        NG_ASSERT(0);
        return E_OUTOFMEMORY;
    }

    if (mDOMNodeList)
    {
        // Fill the variant array with elements from the DOM node list
        PRUint32 length = 0;
        mDOMNodeList->GetLength(&length);
        for (PRUint32 i = 0; i < length; i++)
        {
            // Get the next item from the list
            nsCOMPtr<nsIDOMNode> childNode;
            mDOMNodeList->Item(i, getter_AddRefs(childNode));
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

            // Store the element in the array
            CIEHtmlNode *pHtmlNode = NULL;
            CIEHtmlElementInstance *pHtmlElement = NULL;
            CIEHtmlElementInstance::FindFromDOMNode(childNode, &pHtmlNode);
            if (!pHtmlNode)
            {
                CIEHtmlElementInstance::CreateInstance(&pHtmlElement);
                pHtmlElement->SetDOMNode(childNode);
                pHtmlElement->SetParent(mParent);
            }
            else
            {
                pHtmlElement = (CIEHtmlElementInstance *) pHtmlNode;
            }

            VARIANT *pVariant = &avObjects[nObject++];
            VariantInit(pVariant);
            pVariant->vt = VT_UNKNOWN;
            pHtmlElement->QueryInterface(IID_IUnknown, (void **) &pVariant->punkVal);
        }
    }
    else
    {
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
    NG_TRACE_METHOD(CIEHtmlElementCollection::item);

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
        int nIndex = vIndex.lVal;
        if (nIndex < 0)
        {
            return E_INVALIDARG;
        }

        if (mDOMNodeList)
        {
            // Search for the Nth element in the list
            PRUint32 elementCount = 0;
            PRUint32 length = 0;
            mDOMNodeList->GetLength(&length);
            for (PRUint32 i = 0; i < length; i++)
            {
                // Get the next item from the list
                nsCOMPtr<nsIDOMNode> childNode;
                mDOMNodeList->Item(i, getter_AddRefs(childNode));
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

                // Have we found the element we need?
                if (elementCount == nIndex)
                {
                    // Return the element
                    CIEHtmlNode *pHtmlNode = NULL;
                    CIEHtmlElementInstance *pHtmlElement = NULL;
                    CIEHtmlElementInstance::FindFromDOMNode(childNode, &pHtmlNode);
                    if (!pHtmlNode)
                    {
                        CIEHtmlElementInstance::CreateInstance(&pHtmlElement);
                        pHtmlElement->SetDOMNode(childNode);
                        pHtmlElement->SetParent(mParent);
                    }
                    else
                    {
                        pHtmlElement = (CIEHtmlElementInstance *) pHtmlNode;
                    }
                    pHtmlElement->QueryInterface(IID_IDispatch, (void **) pdisp);
                    return S_OK;
                }
                elementCount++;
            }
            // Index must have been out of range
            return E_INVALIDARG;
        }
        else
        {
            // Test for stupid values
            if (nIndex >= mNodeListCount)
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
        }

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


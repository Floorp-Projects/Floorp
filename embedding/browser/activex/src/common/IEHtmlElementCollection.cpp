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
#include "stdafx.h"

#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHtmlElement.h"

#include "nsString.h"

#include "IEHtmlElement.h"
#include "IEHtmlElementCollection.h"

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

HRESULT CIEHtmlElementCollection::FindOrCreateIEElement(nsIDOMNode* domNode, IHTMLElement** pIHtmlElement)
{
    CComPtr<IUnknown> pNode;
    HRESULT hr = CIEHtmlDomNode::FindOrCreateFromDOMNode(domNode, &pNode);
    if (FAILED(hr))
        return hr;
    if (FAILED(pNode->QueryInterface(IID_IHTMLElement, (void**)pIHtmlElement)))
        return E_UNEXPECTED;
    return S_OK;
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
            NS_ASSERTION(0, "Empty node");
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
        CComQIPtr<IHTMLElement> pHtmlElement;
        HRESULT hr = FindOrCreateIEElement(childNode, &pHtmlElement);
        if (FAILED(hr))
            return hr;
        AddNode(pHtmlElement);
    }
    return S_OK;
}

HRESULT CIEHtmlElementCollection::PopulateFromDOMNode(nsIDOMNode *aDOMNode, BOOL bRecurseChildren)
{
    if (aDOMNode == nsnull)
    {
        NS_ASSERTION(0, "No dom node");
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
                nsCOMPtr<nsIDOMNode> parentNode;
                docAsNode->GetParentNode(getter_AddRefs(parentNode));
                docAsNode = parentNode;
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
            rv = trav->CreateTreeWalker(aDOMNode, 
                nsIDOMNodeFilter::SHOW_ELEMENT,
                nsnull, PR_TRUE, getter_AddRefs(walker));
            NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

            // We're not interested in the document node, so we always start
            // with the next one, walking through them all to make the collection
            nsCOMPtr<nsIDOMNode> currentNode;
            walker->FirstChild(getter_AddRefs(currentNode));
            while (currentNode)
            {
                // Create an equivalent IE element
                CComQIPtr<IHTMLElement> pHtmlElement;
                HRESULT hr = FindOrCreateIEElement(currentNode, &pHtmlElement);
                if (FAILED(hr))
                    return hr;
                AddNode(pHtmlElement);
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


HRESULT CIEHtmlElementCollection::CreateFromDOMHTMLCollection(CNode *pParentNode, nsIDOMHTMLCollection *pNodeList, CIEHtmlElementCollection **pInstance)
{
    if (pInstance == NULL || pParentNode == NULL)
    {
        NS_ASSERTION(0, "No instance or parent node");
        return E_INVALIDARG;
    }

    // Get the DOM node from the parent node
    if (!pParentNode->mDOMNode)
    {
        NS_ASSERTION(0, "Parent has no DOM node");
        return E_INVALIDARG;
    }

    *pInstance = NULL;

    // Create a collection object
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
    if (pCollection == NULL)
    {
        NS_ASSERTION(0, "Could not create collection");
        return E_OUTOFMEMORY;
    }

    // Initialise and populate the collection
    pCollection->SetParent(pParentNode);
    pCollection->PopulateFromDOMHTMLCollection(pNodeList);

    *pInstance = pCollection;

    return S_OK;
}

HRESULT CIEHtmlElementCollection::CreateFromParentNode(CNode *pParentNode, BOOL bRecurseChildren, CIEHtmlElementCollection **pInstance)
{
    if (pInstance == NULL || pParentNode == NULL)
    {
        NS_ASSERTION(0, "No instance or parent node");
        return E_INVALIDARG;
    }

    // Get the DOM node from the parent node
    if (!pParentNode->mDOMNode)
    {
        NS_ASSERTION(0, "Parent has no DOM node");
        return E_INVALIDARG;
    }

    *pInstance = NULL;

    // Create a collection object
    CIEHtmlElementCollectionInstance *pCollection = NULL;
    CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
    if (pCollection == NULL)
    {
        NS_ASSERTION(0, "Could not create collection");
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
        NS_ASSERTION(0, "No node");
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
        NS_ASSERTION(0, "Could not realloc node list");
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
                NS_ASSERTION(0, "Empty node");
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
    TRACE_METHOD(CIEHtmlElementCollection::get__newEnum);

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
        NS_ASSERTION(0, "Could not creat Enum");
        return E_OUTOFMEMORY;
    }

    int nObject = 0;
    long nObjects = 0;
    get_length(&nObjects);

    // Create an array of VARIANTs
    VARIANT *avObjects = new VARIANT[nObjects];
    if (avObjects == NULL)
    {
        NS_ASSERTION(0, "Could not create variant array");
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
                NS_ASSERTION(0, "Could not get node");
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
            CComQIPtr<IHTMLElement> pHtmlElement;
            HRESULT hr = FindOrCreateIEElement(childNode, &pHtmlElement);
            if (FAILED(hr))
                return hr;
            VARIANT *pVariant = &avObjects[nObject++];
            VariantInit(pVariant);
            pVariant->vt = VT_DISPATCH;
            pHtmlElement->QueryInterface(IID_IDispatch, (void **) &pVariant->pdispVal);
        }
    }
    else
    {
        // Copy the contents of the collection to the array
        for (nObject = 0; nObject < nObjects; nObject++)
        {
            VARIANT *pVariant = &avObjects[nObject];
            IDispatch *pDispObject = mNodeList[nObject];
            VariantInit(pVariant);
            pVariant->vt = VT_DISPATCH;
            pVariant->pdispVal = pDispObject;
            pDispObject->AddRef();
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
    TRACE_METHOD(CIEHtmlElementCollection::item);

    if (pdisp == NULL)
    {
        return E_INVALIDARG;
    }
    
    *pdisp = NULL;

    // Parameter name is either a string or a number

    PRBool searchForName = PR_FALSE;
    nsAutoString nameToSearch;
    PRInt32 idxForSearch = 0;

    if (name.vt == VT_BSTR && name.bstrVal && wcslen(name.bstrVal) > 0)
    {
        nameToSearch.Assign(name.bstrVal);
        searchForName = PR_TRUE;
    }
    else switch (name.vt)
    {
    case VT_UI1:
    case VT_UI2:
    case VT_UI4:
    case VT_I1:
    case VT_I2:
    case VT_I1 | VT_BYREF:
    case VT_I2 | VT_BYREF:
        // Coerce the variant into a long
        if (FAILED(VariantChangeType(&name, &name, 0, VT_I4)))
        {
            return E_INVALIDARG;
        }
        // Fall through
    case VT_I4:
        idxForSearch = name.lVal;
        if (idxForSearch < 0)
            return E_INVALIDARG;
        break;
    default:
        // Unknown arg.
        // As per documentation, no attempt to be lenient with crappy clients
        // for the time being.
        return E_INVALIDARG;
    }

    CIEHtmlElementCollectionInstance* pCollection = NULL;

    if (mDOMNodeList)
    {
        CComQIPtr<IHTMLElement> pHtmlElement;
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
                NS_ASSERTION(0, "Could not get node");
                continue;
            }

            // Skip nodes representing, text, attributes etc.
            nsCOMPtr<nsIDOMElement> nodeAsElement = do_QueryInterface(childNode);
            if (!nodeAsElement)
            {
                continue;
            }

            // Have we found the element we need?
            PRBool grabThisNode = PR_FALSE;
            if (searchForName)
            {
                nsCOMPtr<nsIDOMHTMLElement> nodeAsHtmlElement = do_QueryInterface(childNode);
                if (nodeAsHtmlElement)
                {
                    NS_NAMED_LITERAL_STRING(nameAttr, "name");
                    nsAutoString nodeName;
                    nsAutoString nodeId;
                    nodeAsHtmlElement->GetAttribute(nameAttr, nodeName);
                    nodeAsHtmlElement->GetId(nodeId);
                    if (nodeName.Equals(nameToSearch) || nodeId.Equals(nameToSearch))
                    {
                        grabThisNode = PR_TRUE;
                    }
                }
            }
            else if (elementCount == idxForSearch)
            {
                grabThisNode = PR_TRUE;
            }

            if (grabThisNode)
            {
                if (pHtmlElement)
                {
                    if (pCollection == NULL)
                        CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
                    // Add existing to collection
                    pCollection->AddNode(pHtmlElement);
                }
                // Create new element:
                HRESULT hr = FindOrCreateIEElement(childNode, &pHtmlElement);
                if (FAILED(hr))
                    return hr; 
                ((CIEHtmlElement*)pHtmlElement.p)->SetParent(mParent);
                }
            elementCount++;
        }
        // Return the element or collection :
        if (pCollection != NULL)
        {
            // Add last created element to collection
            pCollection->AddNode(pHtmlElement);
            pCollection->QueryInterface(IID_IDispatch, (void **) pdisp);
        }
        else if (pHtmlElement != NULL)
            pHtmlElement->QueryInterface(IID_IDispatch, (void **) pdisp);
    }
    else
    {
        if (searchForName)
        {
            CComPtr<IHTMLElement> element = NULL;
            for (PRUint32 i = 0; i < mNodeListCount; i++)
            {
                CComQIPtr<IHTMLElement> currElement = mNodeList[i];
                if (currElement.p)
                {
                    CComVariant elementName;
                    CComBSTR elementId;
                    currElement->get_id(&elementId);
                    currElement->getAttribute(L"name", 0, &elementName);
                    if ((elementId && wcscmp(elementId, name.bstrVal) == 0) ||
                        (elementName.vt == VT_BSTR && elementName.bstrVal &&
                        wcscmp(elementName.bstrVal, name.bstrVal) == 0))
                    {
                        if (element != NULL)
                        {
                            if (!pCollection)
                                CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
                            pCollection->AddNode(element);
                        }
                        element = currElement;
                    }
                }
            }
            // Return the element or collection :
            if (pCollection != NULL)
            {
                pCollection->AddNode(element);
                pCollection->QueryInterface(IID_IDispatch, (void **) pdisp);
            }
            else if (element != NULL)
                element->QueryInterface(IID_IDispatch, (void **) pdisp);
        }
        else
        {
            // Test for stupid values
            if (idxForSearch >= mNodeListCount)
            {
                return E_INVALIDARG;
            }

            *pdisp = NULL;
            IDispatch *pNode = mNodeList[idxForSearch];
            if (pNode == NULL)
            {
                NS_ASSERTION(0, "No node");
                return E_UNEXPECTED;
            }
            pNode->QueryInterface(IID_IDispatch, (void **) pdisp);
        }
    }

    // Note: As per docs S_OK is fine even if no node is returned
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CIEHtmlElementCollection::tags(VARIANT tagName, IDispatch __RPC_FAR *__RPC_FAR *pdisp)
{
    if (pdisp == NULL || tagName.vt != VT_BSTR)
    {
        return E_INVALIDARG;
    }
    
    *pdisp = NULL;

    CIEHtmlElementCollectionInstance* pCollection = NULL;
    CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
    if (mNodeList)
    {
        for (PRUint32 i = 0; i < mNodeListCount; i++)
        {
            CComQIPtr<IHTMLElement> element = mNodeList[i];
            if (element.p)
            {
                CComBSTR elementTagName;
                element->get_tagName(&elementTagName);
                if (elementTagName && _wcsicmp(elementTagName, tagName.bstrVal) == 0)
                    pCollection->AddNode(element);
            }
        }
        pCollection->QueryInterface(IID_IDispatch, (void**)pdisp);
        return S_OK;
    }
    else if (mDOMNodeList)
    {
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
                NS_ASSERTION(0, "Could not get node");
                continue;
            }

            // Skip nodes representing, text, attributes etc.
            nsCOMPtr<nsIDOMElement> nodeAsElement = do_QueryInterface(childNode);
            if (!nodeAsElement)
            {
                continue;
            }
    
            nsCOMPtr<nsIDOMHTMLElement> nodeAsHtmlElement = do_QueryInterface(childNode);
            if (nodeAsHtmlElement)
            {
                nsAutoString elementTagName;
                nodeAsHtmlElement->GetTagName(elementTagName);
                if (_wcsicmp(elementTagName.get(), OLE2CW(tagName.bstrVal)) == 0)
                {
                    CComQIPtr<IHTMLElement> pHtmlElement;
                    HRESULT hr = FindOrCreateIEElement(childNode, &pHtmlElement);
                    if (FAILED(hr))
                        return hr;
                    //Add to collection :
                    pCollection->AddNode(pHtmlElement);
                }
            }
        }
        pCollection->QueryInterface(IID_IDispatch, (void**)pdisp);
        return S_OK;
    }
    return E_UNEXPECTED;
}


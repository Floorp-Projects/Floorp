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
#include "IEHtmlNode.h"

#include "plhash.h"

static PLHashTable *g_NodeLookupTable;

static PLHashNumber PR_CALLBACK HashFunction(const void *key)
{
    return (PRUint32) key;
}

PRIntn PR_CALLBACK HashComparator(const void *v1, const void *v2)
{
    if (v1 == v2)
    {
        return 0;
    }
    return (v1 > v2) ? 1 : 0;
}


CIEHtmlNode::CIEHtmlNode()
{
    m_pIDOMNode = nsnull;
    m_pIDispParent = NULL;
}

CIEHtmlNode::~CIEHtmlNode()
{
    SetDOMNode(nsnull);
}


HRESULT CIEHtmlNode::SetParentNode(IDispatch *pIDispParent)
{
    m_pIDispParent = pIDispParent;
    return S_OK;
}


HRESULT CIEHtmlNode::FindFromDOMNode(nsIDOMNode *pIDOMNode, CIEHtmlNode **pHtmlNode)
{
    if (pIDOMNode == nsnull)
    {
        return E_FAIL;
    }

    if (g_NodeLookupTable == NULL)
    {
        return E_FAIL;
    }

    nsISupports *pISupports = nsnull;
    pIDOMNode->QueryInterface(NS_GET_IID(nsISupports), (void **) &pISupports);
    NG_ASSERT(pISupports);
    *pHtmlNode = (CIEHtmlNode *) PL_HashTableLookup(g_NodeLookupTable, pISupports);
    NS_RELEASE(pISupports);

    return S_OK;
}




HRESULT CIEHtmlNode::SetDOMNode(nsIDOMNode *pIDOMNode)
{
    if (pIDOMNode)
    {
        NS_IF_RELEASE(m_pIDOMNode);
        m_pIDOMNode = pIDOMNode;
        NS_ADDREF(m_pIDOMNode);

        if (g_NodeLookupTable == NULL)
        {
            g_NodeLookupTable = PL_NewHashTable(123, HashFunction, HashComparator, NULL, NULL, NULL);
        }

        nsISupports *pISupports = nsnull;
        m_pIDOMNode->QueryInterface(NS_GET_IID(nsISupports), (void **) &pISupports);
        PL_HashTableAdd(g_NodeLookupTable, m_pIDOMNode, this);
        NS_RELEASE(pISupports);
    }
    else if (m_pIDOMNode)
    {
        // Remove the entry from the hashtable
        nsISupports *pISupports = nsnull;
        m_pIDOMNode->QueryInterface(NS_GET_IID(nsISupports), (void **) &pISupports);
        PL_HashTableRemove(g_NodeLookupTable, pISupports);
        NS_RELEASE(pISupports);
     
        if (g_NodeLookupTable->nentries == 0)
        {
            PL_HashTableDestroy(g_NodeLookupTable);
            g_NodeLookupTable = NULL;
        }

        NS_RELEASE(m_pIDOMNode);
    }
    return S_OK;
}

HRESULT CIEHtmlNode::GetDOMNode(nsIDOMNode **pIDOMNode)
{
    if (pIDOMNode == NULL)
    {
        return E_INVALIDARG;
    }

    *pIDOMNode = nsnull;
    if (m_pIDOMNode)
    {
        NS_ADDREF(m_pIDOMNode);
        *pIDOMNode = m_pIDOMNode;
    }

    return S_OK;
}

HRESULT CIEHtmlNode::GetDOMElement(nsIDOMElement **pIDOMElement)
{
    if (pIDOMElement == NULL)
    {
        return E_INVALIDARG;
    }

    if (m_pIDOMNode == nsnull)
    {
        return E_NOINTERFACE;
    }

    *pIDOMElement = nsnull;
    m_pIDOMNode->QueryInterface(NS_GET_IID(nsIDOMElement), (void **) pIDOMElement);
    return (*pIDOMElement) ? S_OK : E_NOINTERFACE;
}

HRESULT CIEHtmlNode::GetIDispatch(IDispatch **pDispatch)
{
    if (pDispatch == NULL)
    {
        return E_INVALIDARG;
    }
    
    *pDispatch = NULL;
    return E_NOINTERFACE;
}

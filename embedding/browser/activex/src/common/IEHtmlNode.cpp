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
        return 1;
    }
    return 0;
}


CIEHtmlNode::CIEHtmlNode() :
    mParent(NULL)
{
}

CIEHtmlNode::~CIEHtmlNode()
{
    SetDOMNode(nsnull);
}


HRESULT CIEHtmlNode::SetParent(CIEHtmlNode *pParent)
{
    mParent = pParent;
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

    nsCOMPtr<nsISupports> nodeAsSupports = do_QueryInterface(pIDOMNode);
    *pHtmlNode = (CIEHtmlNode *) PL_HashTableLookup(g_NodeLookupTable, nodeAsSupports);

    return S_OK;
}

HRESULT CIEHtmlNode::SetDOMNode(nsIDOMNode *pIDOMNode)
{
    if (pIDOMNode)
    {
        if (g_NodeLookupTable == NULL)
        {
            g_NodeLookupTable = PL_NewHashTable(123, HashFunction, HashComparator, HashComparator, NULL, NULL);
        }

        mDOMNode = pIDOMNode;
        nsCOMPtr<nsISupports> nodeAsSupports= do_QueryInterface(mDOMNode);
        PL_HashTableAdd(g_NodeLookupTable, nodeAsSupports, this);
    }
    else if (mDOMNode)
    {
        // Remove the entry from the hashtable
        nsCOMPtr<nsISupports> nodeAsSupports = do_QueryInterface(mDOMNode);
        PL_HashTableRemove(g_NodeLookupTable, nodeAsSupports);
        mDOMNode = nsnull;

        if (g_NodeLookupTable->nentries == 0)
        {
            PL_HashTableDestroy(g_NodeLookupTable);
            g_NodeLookupTable = NULL;
        }
    }
    return S_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "txNodeSorter.h"
#include "txExecutionState.h"
#include "txXPathResultComparator.h"
#include "nsGkAtoms.h"
#include "txNodeSetContext.h"
#include "txExpr.h"
#include "txStringUtils.h"
#include "prmem.h"
#include "nsQuickSort.h"

/*
 * Sorts Nodes as specified by the W3C XSLT 1.0 Recommendation
 */

txNodeSorter::txNodeSorter() : mNKeys(0)
{
}

txNodeSorter::~txNodeSorter()
{
    txListIterator iter(&mSortKeys);
    while (iter.hasNext()) {
        SortKey* key = (SortKey*)iter.next();
        delete key->mComparator;
        delete key;
    }
}

nsresult
txNodeSorter::addSortElement(Expr* aSelectExpr, Expr* aLangExpr,
                             Expr* aDataTypeExpr, Expr* aOrderExpr,
                             Expr* aCaseOrderExpr, txIEvalContext* aContext)
{
    nsAutoPtr<SortKey> key(new SortKey);
    NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = NS_OK;

    // Select
    key->mExpr = aSelectExpr;

    // Order
    MBool ascending = MB_TRUE;
    if (aOrderExpr) {
        nsAutoString attrValue;
        rv = aOrderExpr->evaluateToString(aContext, attrValue);
        NS_ENSURE_SUCCESS(rv, rv);

        if (TX_StringEqualsAtom(attrValue, nsGkAtoms::descending)) {
            ascending = MB_FALSE;
        }
        else if (!TX_StringEqualsAtom(attrValue, nsGkAtoms::ascending)) {
            // XXX ErrorReport: unknown value for order attribute
            return NS_ERROR_XSLT_BAD_VALUE;
        }
    }


    // Create comparator depending on datatype
    nsAutoString dataType;
    if (aDataTypeExpr) {
        rv = aDataTypeExpr->evaluateToString(aContext, dataType);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!aDataTypeExpr || TX_StringEqualsAtom(dataType, nsGkAtoms::text)) {
        // Text comparator
        
        // Language
        nsAutoString lang;
        if (aLangExpr) {
            rv = aLangExpr->evaluateToString(aContext, lang);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        // Case-order 
        MBool upperFirst = PR_FALSE;
        if (aCaseOrderExpr) {
            nsAutoString attrValue;

            rv = aCaseOrderExpr->evaluateToString(aContext, attrValue);
            NS_ENSURE_SUCCESS(rv, rv);

            if (TX_StringEqualsAtom(attrValue, nsGkAtoms::upperFirst)) {
                upperFirst = PR_TRUE;
            }
            else if (!TX_StringEqualsAtom(attrValue,
                                          nsGkAtoms::lowerFirst)) {
                // XXX ErrorReport: unknown value for case-order attribute
                return NS_ERROR_XSLT_BAD_VALUE;
            }
        }

        key->mComparator = new txResultStringComparator(ascending,
                                                        upperFirst,
                                                        lang);
        NS_ENSURE_TRUE(key->mComparator, NS_ERROR_OUT_OF_MEMORY);
    }
    else if (TX_StringEqualsAtom(dataType, nsGkAtoms::number)) {
        // Number comparator
        key->mComparator = new txResultNumberComparator(ascending);
        NS_ENSURE_TRUE(key->mComparator, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        // XXX ErrorReport: unknown data-type
        return NS_ERROR_XSLT_BAD_VALUE;
    }

    // mSortKeys owns key now. 
    rv = mSortKeys.add(key);
    NS_ENSURE_SUCCESS(rv, rv);

    key.forget();
    mNKeys++;

    return NS_OK;
}

nsresult
txNodeSorter::sortNodeSet(txNodeSet* aNodes, txExecutionState* aEs,
                          txNodeSet** aResult)
{
    if (mNKeys == 0 || aNodes->isEmpty()) {
        NS_ADDREF(*aResult = aNodes);

        return NS_OK;
    }

    *aResult = nsnull;

    nsRefPtr<txNodeSet> sortedNodes;
    nsresult rv = aEs->recycler()->getNodeSet(getter_AddRefs(sortedNodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txNodeSetContext* evalContext = new txNodeSetContext(aNodes, aEs);
    NS_ENSURE_TRUE(evalContext, NS_ERROR_OUT_OF_MEMORY);

    rv = aEs->pushEvalContext(evalContext);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create and set up memoryblock for sort-values and indexarray
    PRUint32 len = static_cast<PRUint32>(aNodes->size());

    // Limit resource use to something sane.
    PRUint32 itemSize = sizeof(PRUint32) + mNKeys * sizeof(TxObject*);
    if (mNKeys > (PR_UINT32_MAX - sizeof(PRUint32)) / sizeof(TxObject*) ||
        len >= PR_UINT32_MAX / itemSize) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    void* mem = PR_Malloc(len * itemSize);
    NS_ENSURE_TRUE(mem, NS_ERROR_OUT_OF_MEMORY);

    PRUint32* indexes = static_cast<PRUint32*>(mem);
    TxObject** sortValues = reinterpret_cast<TxObject**>(indexes + len);

    PRUint32 i;
    for (i = 0; i < len; ++i) {
        indexes[i] = i;
    }
    memset(sortValues, 0, len * mNKeys * sizeof(TxObject*));

    // Sort the indexarray
    SortData sortData;
    sortData.mNodeSorter = this;
    sortData.mContext = evalContext;
    sortData.mSortValues = sortValues;
    sortData.mRv = NS_OK;
    NS_QuickSort(indexes, len, sizeof(PRUint32), compareNodes, &sortData);

    // Delete these here so we don't have to deal with them at every possible
    // failurepoint
    PRUint32 numSortValues = len * mNKeys;
    for (i = 0; i < numSortValues; ++i) {
        delete sortValues[i];
    }

    if (NS_FAILED(sortData.mRv)) {
        PR_Free(mem);
        // The txExecutionState owns the evalcontext so no need to handle it
        return sortData.mRv;
    }

    // Insert nodes in sorted order in new nodeset
    for (i = 0; i < len; ++i) {
        rv = sortedNodes->append(aNodes->get(indexes[i]));
        if (NS_FAILED(rv)) {
            PR_Free(mem);
            // The txExecutionState owns the evalcontext so no need to handle it
            return rv;
        }
    }

    PR_Free(mem);
    delete aEs->popEvalContext();

    NS_ADDREF(*aResult = sortedNodes);

    return NS_OK;
}

// static
int
txNodeSorter::compareNodes(const void* aIndexA, const void* aIndexB,
                           void* aSortData)
{
    SortData* sortData = static_cast<SortData*>(aSortData);
    NS_ENSURE_SUCCESS(sortData->mRv, -1);

    txListIterator iter(&sortData->mNodeSorter->mSortKeys);
    PRUint32 indexA = *static_cast<const PRUint32*>(aIndexA);
    PRUint32 indexB = *static_cast<const PRUint32*>(aIndexB);
    TxObject** sortValuesA = sortData->mSortValues +
                             indexA * sortData->mNodeSorter->mNKeys;
    TxObject** sortValuesB = sortData->mSortValues +
                             indexB * sortData->mNodeSorter->mNKeys;

    unsigned int i;
    // Step through each key until a difference is found
    for (i = 0; i < sortData->mNodeSorter->mNKeys; ++i) {
        SortKey* key = (SortKey*)iter.next();
        // Lazy create sort values
        if (!sortValuesA[i] &&
            !calcSortValue(sortValuesA[i], key, sortData, indexA)) {
            return -1;
        }
        if (!sortValuesB[i] &&
            !calcSortValue(sortValuesB[i], key, sortData, indexB)) {
            return -1;
        }

        // Compare node values
        int compRes = key->mComparator->compareValues(sortValuesA[i],
                                                      sortValuesB[i]);
        if (compRes != 0)
            return compRes;
    }
    // All keys have the same value for these nodes

    return indexA - indexB;
}

//static
bool
txNodeSorter::calcSortValue(TxObject*& aSortValue, SortKey* aKey,
                            SortData* aSortData, PRUint32 aNodeIndex)
{
    aSortData->mContext->setPosition(aNodeIndex + 1); // position is 1-based

    nsresult rv = aKey->mComparator->createSortableValue(aKey->mExpr,
                                                         aSortData->mContext,
                                                         aSortValue);
    if (NS_FAILED(rv)) {
        aSortData->mRv = rv;
        return PR_FALSE;
    }

    return PR_TRUE;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    bool ascending = true;
    if (aOrderExpr) {
        nsAutoString attrValue;
        rv = aOrderExpr->evaluateToString(aContext, attrValue);
        NS_ENSURE_SUCCESS(rv, rv);

        if (TX_StringEqualsAtom(attrValue, nsGkAtoms::descending)) {
            ascending = false;
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
        bool upperFirst = false;
        if (aCaseOrderExpr) {
            nsAutoString attrValue;

            rv = aCaseOrderExpr->evaluateToString(aContext, attrValue);
            NS_ENSURE_SUCCESS(rv, rv);

            if (TX_StringEqualsAtom(attrValue, nsGkAtoms::upperFirst)) {
                upperFirst = true;
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

    *aResult = nullptr;

    nsRefPtr<txNodeSet> sortedNodes;
    nsresult rv = aEs->recycler()->getNodeSet(getter_AddRefs(sortedNodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txNodeSetContext* evalContext = new txNodeSetContext(aNodes, aEs);
    NS_ENSURE_TRUE(evalContext, NS_ERROR_OUT_OF_MEMORY);

    rv = aEs->pushEvalContext(evalContext);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create and set up memoryblock for sort-values and indexarray
    uint32_t len = static_cast<uint32_t>(aNodes->size());

    // Limit resource use to something sane.
    uint32_t itemSize = sizeof(uint32_t) + mNKeys * sizeof(txObject*);
    if (mNKeys > (UINT32_MAX - sizeof(uint32_t)) / sizeof(txObject*) ||
        len >= UINT32_MAX / itemSize) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    void* mem = PR_Malloc(len * itemSize);
    NS_ENSURE_TRUE(mem, NS_ERROR_OUT_OF_MEMORY);

    uint32_t* indexes = static_cast<uint32_t*>(mem);
    txObject** sortValues = reinterpret_cast<txObject**>(indexes + len);

    uint32_t i;
    for (i = 0; i < len; ++i) {
        indexes[i] = i;
    }
    memset(sortValues, 0, len * mNKeys * sizeof(txObject*));

    // Sort the indexarray
    SortData sortData;
    sortData.mNodeSorter = this;
    sortData.mContext = evalContext;
    sortData.mSortValues = sortValues;
    sortData.mRv = NS_OK;
    NS_QuickSort(indexes, len, sizeof(uint32_t), compareNodes, &sortData);

    // Delete these here so we don't have to deal with them at every possible
    // failurepoint
    uint32_t numSortValues = len * mNKeys;
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
    uint32_t indexA = *static_cast<const uint32_t*>(aIndexA);
    uint32_t indexB = *static_cast<const uint32_t*>(aIndexB);
    txObject** sortValuesA = sortData->mSortValues +
                             indexA * sortData->mNodeSorter->mNKeys;
    txObject** sortValuesB = sortData->mSortValues +
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
txNodeSorter::calcSortValue(txObject*& aSortValue, SortKey* aKey,
                            SortData* aSortData, uint32_t aNodeIndex)
{
    aSortData->mContext->setPosition(aNodeIndex + 1); // position is 1-based

    nsresult rv = aKey->mComparator->createSortableValue(aKey->mExpr,
                                                         aSortData->mContext,
                                                         aSortValue);
    if (NS_FAILED(rv)) {
        aSortData->mRv = rv;
        return false;
    }

    return true;
}

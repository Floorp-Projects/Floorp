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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *   Peter Van der Beken <peterv@netscape.com>
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
#include "txAtoms.h"
#include "txForwardContext.h"
#include "ExprResult.h"
#include "Expr.h"
#include "txStringUtils.h"
#include "NodeSet.h"

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
    SortKey* key = new SortKey;
    NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = NS_OK;

    // Select
    key->mExpr = aSelectExpr;

    // Order
    MBool ascending = MB_TRUE;
    if (aOrderExpr) {
        nsRefPtr<txAExprResult> exprRes;
        rv = aOrderExpr->evaluate(aContext, getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString attrValue;
        exprRes->stringValue(attrValue);
        
        if (TX_StringEqualsAtom(attrValue, txXSLTAtoms::descending)) {
            ascending = MB_FALSE;
        }
        else if (!TX_StringEqualsAtom(attrValue, txXSLTAtoms::ascending)) {
            delete key;
            // XXX ErrorReport: unknown value for order attribute
            return NS_ERROR_XSLT_BAD_VALUE;
        }
    }


    // Create comparator depending on datatype
    nsAutoString dataType;
    if (aDataTypeExpr) {
        nsRefPtr<txAExprResult> exprRes;
        rv = aDataTypeExpr->evaluate(aContext, getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);

        exprRes->stringValue(dataType);
    }

    if (!aDataTypeExpr || TX_StringEqualsAtom(dataType, txXSLTAtoms::text)) {
        // Text comparator
        
        // Language
        nsAutoString lang;
        if (aLangExpr) {
            nsRefPtr<txAExprResult> exprRes;
            rv = aLangExpr->evaluate(aContext, getter_AddRefs(exprRes));
            NS_ENSURE_SUCCESS(rv, rv);

            exprRes->stringValue(lang);
        }

        // Case-order 
        MBool upperFirst = PR_FALSE;
        if (aCaseOrderExpr) {
            nsRefPtr<txAExprResult> exprRes;
            rv = aCaseOrderExpr->evaluate(aContext, getter_AddRefs(exprRes));
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString attrValue;
            exprRes->stringValue(attrValue);

            if (TX_StringEqualsAtom(attrValue, txXSLTAtoms::upperFirst)) {
                upperFirst = PR_TRUE;
            }
            else if (!TX_StringEqualsAtom(attrValue,
                                          txXSLTAtoms::lowerFirst)) {
                delete key;
                // XXX ErrorReport: unknown value for case-order attribute
                return NS_ERROR_XSLT_BAD_VALUE;
            }
        }

        key->mComparator = new txResultStringComparator(ascending,
                                                        upperFirst,
                                                        lang);
        NS_ENSURE_TRUE(key->mComparator, NS_ERROR_OUT_OF_MEMORY);
    }
    else if (TX_StringEqualsAtom(dataType, txXSLTAtoms::number)) {
        // Number comparator
        key->mComparator = new txResultNumberComparator(ascending);
        NS_ENSURE_TRUE(key->mComparator, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        // XXX ErrorReport: unknown data-type
        delete key;

        return NS_ERROR_XSLT_BAD_VALUE;
    }

    mSortKeys.add(key);
    mNKeys++;

    return NS_OK;
}

nsresult txNodeSorter::sortNodeSet(NodeSet* aNodes, txExecutionState* aEs)
{
    if (mNKeys == 0)
        return NS_OK;

    txList sortedNodes;
    txListIterator iter(&sortedNodes);

    int len = aNodes->size();

    // Step through each node in NodeSet...
    int i;
    for (i = len - 1; i >= 0; i--) {
        SortableNode* currNode = new SortableNode(aNodes->get(i), mNKeys);
        if (!currNode) {
            // XXX ErrorReport: out of memory
            iter.reset();
            while (iter.hasNext()) {
                SortableNode* sNode = (SortableNode*)iter.next();
                sNode->clear(mNKeys);
                delete sNode;
            }
            return NS_ERROR_OUT_OF_MEMORY;
        }
        iter.reset();
        SortableNode* compNode = (SortableNode*)iter.next();
        while (compNode && (compareNodes(currNode, compNode, aNodes, aEs) > 0)) {
            compNode = (SortableNode*)iter.next();
        }
        // ... and insert in sorted list
        iter.addBefore(currNode);
    }
    
    // Clean up and set nodes in NodeSet
    // Note that the nodeset shouldn't be changed until the sort is done
    // since it's the current-nodeset used during xpath evaluation
    aNodes->clear();
    iter.reset();
    while (iter.hasNext()) {
        SortableNode* sNode = (SortableNode*)iter.next();
        aNodes->append(sNode->mNode);
        sNode->clear(mNKeys);
        delete sNode;
    }
    
    return NS_OK;
}

int txNodeSorter::compareNodes(SortableNode* aSNode1,
                               SortableNode* aSNode2,
                               NodeSet* aNodes,
                               txExecutionState* aEs)
{
    txListIterator iter(&mSortKeys);
    nsresult rv = NS_OK;
    int i;

    // Step through each key until a difference is found
    for (i = 0; i < mNKeys; i++) {
        SortKey* key = (SortKey*)iter.next();
        // Lazy create sort values
        if (!aSNode1->mSortValues[i]) {
            txForwardContext evalContext(aEs->getEvalContext(), aSNode1->mNode, aNodes);
            aEs->pushEvalContext(&evalContext);
            nsRefPtr<txAExprResult> res;
            rv = key->mExpr->evaluate(&evalContext, getter_AddRefs(res));
            NS_ENSURE_SUCCESS(rv, -1);

            aEs->popEvalContext();
            aSNode1->mSortValues[i] = key->mComparator->createSortableValue(res);
            if (!aSNode1->mSortValues[i]) {
                // XXX ErrorReport
                return -1;
            }
        }
        if (!aSNode2->mSortValues[i]) {
            txForwardContext evalContext(aEs->getEvalContext(), aSNode2->mNode, aNodes);
            aEs->pushEvalContext(&evalContext);
            nsRefPtr<txAExprResult> res;
            rv = key->mExpr->evaluate(&evalContext, getter_AddRefs(res));
            NS_ENSURE_SUCCESS(rv, -1);

            aEs->popEvalContext();
            aSNode2->mSortValues[i] = key->mComparator->createSortableValue(res);
            if (!aSNode2->mSortValues[i]) {
                // XXX ErrorReport
                return -1;
            }
        }

        // Compare node values
        int compRes = key->mComparator->compareValues(aSNode1->mSortValues[i],
                                                      aSNode2->mSortValues[i]);
        if (compRes != 0)
            return compRes;
    }
    // All keys have the same value for these nodes
    return 0;
}

txNodeSorter::SortableNode::SortableNode(Node* aNode, int aNValues)
{
    mNode = aNode;
    mSortValues = new TxObject*[aNValues];
    if (!mSortValues) {
        // XXX ErrorReport: out of memory
        return;
    }
    memset(mSortValues, 0, aNValues * sizeof(void *));
}

void txNodeSorter::SortableNode::clear(int aNValues)
{
    int i;
    for (i = 0; i < aNValues; i++) {
        delete mSortValues[i];
    }

    delete [] mSortValues;
}

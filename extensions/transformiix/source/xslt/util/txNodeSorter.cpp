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
#include <string.h>
#include "Names.h"
#include "ProcessorState.h"
#include "txXPathResultComparator.h"
#include "txAtoms.h"
#include "txForwardContext.h"

/*
 * Sorts Nodes as specified by the W3C XSLT 1.0 Recommendation
 */

#define DEFAULT_LANG NS_LITERAL_STRING("en")

txNodeSorter::txNodeSorter(ProcessorState* aPs) : mPs(aPs),
                                                  mNKeys(0),
                                                  mDefaultExpr(0)
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
    delete mDefaultExpr;
}

MBool txNodeSorter::addSortElement(Element* aSortElement)
{
    SortKey* key = new SortKey;
    if (!key) {
        // XXX ErrorReport: out of memory
        return MB_FALSE;
    }

    // Get common attributes
    String attrValue;

    // Select
    if (aSortElement->hasAttr(txXSLTAtoms::select, kNameSpaceID_None))
        key->mExpr = mPs->getExpr(aSortElement, ProcessorState::SelectAttr);
    else {
        if (!mDefaultExpr) {
            txNodeTest* test = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
            mDefaultExpr = new LocationStep(test, LocationStep::SELF_AXIS);
        }
        key->mExpr = mDefaultExpr;
    }
    
    if (!key->mExpr) {
        // XXX ErrorReport: Out of memory
        delete key;
        return MB_FALSE;
    }

    // Order
    MBool ascending;
    MBool hasAttr = getAttrAsAVT(aSortElement, txXSLTAtoms::order, attrValue);
    if (!hasAttr || attrValue.Equals(ASCENDING_VALUE)) {
        ascending = MB_TRUE;
    }
    else if (attrValue.Equals(DESCENDING_VALUE)) {
        ascending = MB_FALSE;
    }
    else {
        delete key;
        // XXX ErrorReport: unknown value for order attribute
        return MB_FALSE;
    }


    // Create comparator depending on datatype
    String dataType;
    hasAttr = getAttrAsAVT(aSortElement, txXSLTAtoms::dataType, dataType);
    if (!hasAttr || dataType.Equals(TEXT_VALUE)) {
        // Text comparator
        
        // Language
        String lang;
        if (!getAttrAsAVT(aSortElement, txXSLTAtoms::lang, lang))
            lang.Append(DEFAULT_LANG);

        // Case-order 
        MBool upperFirst;
        hasAttr = getAttrAsAVT(aSortElement, txXSLTAtoms::caseOrder, attrValue);
        if (!hasAttr || attrValue.Equals(UPPER_FIRST_VALUE)) {
            upperFirst = MB_TRUE;
        }
        else if (attrValue.Equals(LOWER_FIRST_VALUE)) {
            upperFirst = MB_FALSE;
        }
        else {
            // XXX ErrorReport: unknown value for case-order attribute
            delete key;
            return MB_FALSE;
        }

        key->mComparator = new txResultStringComparator(ascending,
                                                        upperFirst,
                                                        lang);
    }
    else if (dataType.Equals(NUMBER_VALUE)) {
        // Number comparator
        key->mComparator = new txResultNumberComparator(ascending);
    }
    else {
        // XXX ErrorReport: unknown data-type
        return MB_FALSE;
    }

    if (!key->mComparator) {
        // XXX ErrorReport: out of memory
        return MB_FALSE;
    }

    mSortKeys.add(key);
    mNKeys++;
    return MB_TRUE;
}

MBool txNodeSorter::sortNodeSet(NodeSet* aNodes)
{
    if (mNKeys == 0)
        return MB_TRUE;

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
            return MB_FALSE;
        }
        iter.reset();
        SortableNode* compNode = (SortableNode*)iter.next();
        while (compNode && (compareNodes(currNode, compNode, aNodes) > 0)) {
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
    
    return MB_TRUE;
}

int txNodeSorter::compareNodes(SortableNode* aSNode1,
                               SortableNode* aSNode2,
                               NodeSet* aNodes)
{
    txListIterator iter(&mSortKeys);
    int i;

    // Step through each key until a difference is found
    for (i = 0; i < mNKeys; i++) {
        SortKey* key = (SortKey*)iter.next();
        // Lazy create sort values
        if (!aSNode1->mSortValues[i]) {
            txForwardContext evalContext(mPs, aSNode1->mNode, aNodes);
            txIEvalContext* priorEC = mPs->setEvalContext(&evalContext);
            ExprResult* res = key->mExpr->evaluate(&evalContext);
            mPs->setEvalContext(priorEC);
            if (!res) {
                // XXX ErrorReport
                return -1;
            }
            aSNode1->mSortValues[i] = key->mComparator->createSortableValue(res);
            if (!aSNode1->mSortValues[i]) {
                // XXX ErrorReport
                return -1;
            }
            delete res;
        }
        if (!aSNode2->mSortValues[i]) {
            txForwardContext evalContext(mPs, aSNode2->mNode, aNodes);
            txIEvalContext* priorEC = mPs->setEvalContext(&evalContext);
            ExprResult* res = key->mExpr->evaluate(&evalContext);
            mPs->setEvalContext(priorEC);
            if (!res) {
                // XXX ErrorReport
                return -1;
            }
            aSNode2->mSortValues[i] = key->mComparator->createSortableValue(res);
            if (!aSNode2->mSortValues[i]) {
                // XXX ErrorReport
                return -1;
            }
            delete res;
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

MBool txNodeSorter::getAttrAsAVT(Element* aSortElement,
                                 txAtom* aAttrName,
                                 String& aResult)
{
    aResult.Truncate();

    String attValue;
    if (!aSortElement->getAttr(aAttrName, kNameSpaceID_None, attValue))
        return MB_FALSE;

    mPs->processAttrValueTemplate(attValue, aSortElement, aResult);
    return MB_TRUE;
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

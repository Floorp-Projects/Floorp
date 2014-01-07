/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_NODESORTER_H
#define TRANSFRMX_NODESORTER_H

#include "txCore.h"
#include "txList.h"

class Expr;
class txExecutionState;
class txNodeSet;
class txObject;
class txXPathResultComparator;
class txIEvalContext;
class txNodeSetContext;

/*
 * Sorts Nodes as specified by the W3C XSLT 1.0 Recommendation
 */

class txNodeSorter
{
public:
    txNodeSorter();
    ~txNodeSorter();

    nsresult addSortElement(Expr* aSelectExpr, Expr* aLangExpr,
                            Expr* aDataTypeExpr, Expr* aOrderExpr,
                            Expr* aCaseOrderExpr, txIEvalContext* aContext);
    nsresult sortNodeSet(txNodeSet* aNodes, txExecutionState* aEs,
                         txNodeSet** aResult);

private:
    struct SortData
    {
        txNodeSorter* mNodeSorter;
        txNodeSetContext* mContext;
        txObject** mSortValues;
        nsresult mRv;
    };
    struct SortKey
    {
        Expr* mExpr;
        txXPathResultComparator* mComparator;
    };

    static int compareNodes(const void* aIndexA, const void* aIndexB,
                            void* aSortData);
    static bool calcSortValue(txObject*& aSortValue, SortKey* aKey,
                              SortData* aSortData, uint32_t aNodeIndex);
    txList mSortKeys;
    unsigned int mNKeys;
};

#endif

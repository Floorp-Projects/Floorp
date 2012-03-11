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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

#include "txResultRecycler.h"
#include "txExprResult.h"
#include "txNodeSet.h"

txResultRecycler::txResultRecycler()
    : mEmptyStringResult(nsnull),
      mTrueResult(nsnull),
      mFalseResult(nsnull)
{
}

txResultRecycler::~txResultRecycler()
{
    txStackIterator stringIter(&mStringResults);
    while (stringIter.hasNext()) {
        delete static_cast<StringResult*>(stringIter.next());
    }
    txStackIterator nodesetIter(&mNodeSetResults);
    while (nodesetIter.hasNext()) {
        delete static_cast<txNodeSet*>(nodesetIter.next());
    }
    txStackIterator numberIter(&mNumberResults);
    while (numberIter.hasNext()) {
        delete static_cast<NumberResult*>(numberIter.next());
    }

    NS_IF_RELEASE(mEmptyStringResult);
    NS_IF_RELEASE(mTrueResult);
    NS_IF_RELEASE(mFalseResult);
}


nsresult
txResultRecycler::init()
{
    NS_ASSERTION(!mEmptyStringResult && !mTrueResult && !mFalseResult,
                 "Already inited");
    mEmptyStringResult = new StringResult(nsnull);
    NS_ENSURE_TRUE(mEmptyStringResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mEmptyStringResult);

    mTrueResult = new BooleanResult(true);
    NS_ENSURE_TRUE(mTrueResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mTrueResult);

    mFalseResult = new BooleanResult(false);
    NS_ENSURE_TRUE(mFalseResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mFalseResult);

    return NS_OK;
}


void
txResultRecycler::recycle(txAExprResult* aResult)
{
    NS_ASSERTION(aResult->mRefCnt == 0, "In-use txAExprResult recycled");
    nsRefPtr<txResultRecycler> kungFuDeathGrip;
    aResult->mRecycler.swap(kungFuDeathGrip);

    nsresult rv = NS_OK;
    switch (aResult->getResultType()) {
        case txAExprResult::STRING:
        {
            rv = mStringResults.push(static_cast<StringResult*>(aResult));
            if (NS_FAILED(rv)) {
                delete aResult;
            }
            return;
        }
        case txAExprResult::NODESET:
        {
            static_cast<txNodeSet*>(aResult)->clear();
            rv = mNodeSetResults.push(static_cast<txNodeSet*>(aResult));
            if (NS_FAILED(rv)) {
                delete aResult;
            }
            return;
        }
        case txAExprResult::NUMBER:
        {
            rv = mNumberResults.push(static_cast<NumberResult*>(aResult));
            if (NS_FAILED(rv)) {
                delete aResult;
            }
            return;
        }
        default:
        {
            delete aResult;
        }
    }
}

nsresult
txResultRecycler::getStringResult(StringResult** aResult)
{
    if (mStringResults.isEmpty()) {
        *aResult = new StringResult(this);
        NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        *aResult = static_cast<StringResult*>(mStringResults.pop());
        (*aResult)->mValue.Truncate();
        (*aResult)->mRecycler = this;
    }
    NS_ADDREF(*aResult);

    return NS_OK;
}

nsresult
txResultRecycler::getStringResult(const nsAString& aValue,
                                  txAExprResult** aResult)
{
    if (mStringResults.isEmpty()) {
        *aResult = new StringResult(aValue, this);
        NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        StringResult* strRes =
            static_cast<StringResult*>(mStringResults.pop());
        strRes->mValue = aValue;
        strRes->mRecycler = this;
        *aResult = strRes;
    }
    NS_ADDREF(*aResult);

    return NS_OK;
}

void
txResultRecycler::getEmptyStringResult(txAExprResult** aResult)
{
    *aResult = mEmptyStringResult;
    NS_ADDREF(*aResult);
}

nsresult
txResultRecycler::getNodeSet(txNodeSet** aResult)
{
    if (mNodeSetResults.isEmpty()) {
        *aResult = new txNodeSet(this);
        NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        *aResult = static_cast<txNodeSet*>(mNodeSetResults.pop());
        (*aResult)->mRecycler = this;
    }
    NS_ADDREF(*aResult);

    return NS_OK;
}

nsresult
txResultRecycler::getNodeSet(txNodeSet* aNodeSet, txNodeSet** aResult)
{
    if (mNodeSetResults.isEmpty()) {
        *aResult = new txNodeSet(*aNodeSet, this);
        NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        *aResult = static_cast<txNodeSet*>(mNodeSetResults.pop());
        (*aResult)->append(*aNodeSet);
        (*aResult)->mRecycler = this;
    }
    NS_ADDREF(*aResult);

    return NS_OK;
}

nsresult
txResultRecycler::getNodeSet(const txXPathNode& aNode, txAExprResult** aResult)
{
    if (mNodeSetResults.isEmpty()) {
        *aResult = new txNodeSet(aNode, this);
        NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        txNodeSet* nodes = static_cast<txNodeSet*>(mNodeSetResults.pop());
        nodes->append(aNode);
        nodes->mRecycler = this;
        *aResult = nodes;
    }
    NS_ADDREF(*aResult);

    return NS_OK;
}

nsresult
txResultRecycler::getNumberResult(double aValue, txAExprResult** aResult)
{
    if (mNumberResults.isEmpty()) {
        *aResult = new NumberResult(aValue, this);
        NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        NumberResult* numRes =
            static_cast<NumberResult*>(mNumberResults.pop());
        numRes->value = aValue;
        numRes->mRecycler = this;
        *aResult = numRes;
    }
    NS_ADDREF(*aResult);

    return NS_OK;
}

void
txResultRecycler::getBoolResult(bool aValue, txAExprResult** aResult)
{
    *aResult = aValue ? mTrueResult : mFalseResult;
    NS_ADDREF(*aResult);
}

nsresult
txResultRecycler::getNonSharedNodeSet(txNodeSet* aNodeSet, txNodeSet** aResult)
{
    if (aNodeSet->mRefCnt > 1) {
        return getNodeSet(aNodeSet, aResult);
    }

    *aResult = aNodeSet;
    NS_ADDREF(*aResult);

    return NS_OK;
}

void
txAExprResult::Release()
{
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "txAExprResult");
    if (mRefCnt == 0) {
        if (mRecycler) {
            mRecycler->recycle(this);
        }
        else {
            delete this;
        }
    }
}

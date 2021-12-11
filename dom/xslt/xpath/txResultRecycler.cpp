/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txResultRecycler.h"
#include "txExprResult.h"
#include "txNodeSet.h"

txResultRecycler::txResultRecycler()
    : mEmptyStringResult(new StringResult(nullptr)),
      mTrueResult(new BooleanResult(true)),
      mFalseResult(new BooleanResult(false)) {}

txResultRecycler::~txResultRecycler() {
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
}

void txResultRecycler::recycle(txAExprResult* aResult) {
  NS_ASSERTION(aResult->mRefCnt == 0, "In-use txAExprResult recycled");
  RefPtr<txResultRecycler> kungFuDeathGrip;
  aResult->mRecycler.swap(kungFuDeathGrip);

  switch (aResult->getResultType()) {
    case txAExprResult::STRING: {
      mStringResults.push(static_cast<StringResult*>(aResult));
      return;
    }
    case txAExprResult::NODESET: {
      static_cast<txNodeSet*>(aResult)->clear();
      mNodeSetResults.push(static_cast<txNodeSet*>(aResult));
      return;
    }
    case txAExprResult::NUMBER: {
      mNumberResults.push(static_cast<NumberResult*>(aResult));
      return;
    }
    default: {
      delete aResult;
    }
  }
}

nsresult txResultRecycler::getStringResult(StringResult** aResult) {
  if (mStringResults.isEmpty()) {
    *aResult = new StringResult(this);
  } else {
    *aResult = static_cast<StringResult*>(mStringResults.pop());
    (*aResult)->mValue.Truncate();
    (*aResult)->mRecycler = this;
  }
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult txResultRecycler::getStringResult(const nsAString& aValue,
                                           txAExprResult** aResult) {
  if (mStringResults.isEmpty()) {
    *aResult = new StringResult(aValue, this);
  } else {
    StringResult* strRes = static_cast<StringResult*>(mStringResults.pop());
    strRes->mValue = aValue;
    strRes->mRecycler = this;
    *aResult = strRes;
  }
  NS_ADDREF(*aResult);

  return NS_OK;
}

void txResultRecycler::getEmptyStringResult(txAExprResult** aResult) {
  *aResult = mEmptyStringResult;
  NS_ADDREF(*aResult);
}

nsresult txResultRecycler::getNodeSet(txNodeSet** aResult) {
  if (mNodeSetResults.isEmpty()) {
    *aResult = new txNodeSet(this);
  } else {
    *aResult = static_cast<txNodeSet*>(mNodeSetResults.pop());
    (*aResult)->mRecycler = this;
  }
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult txResultRecycler::getNodeSet(txNodeSet* aNodeSet,
                                      txNodeSet** aResult) {
  if (mNodeSetResults.isEmpty()) {
    *aResult = new txNodeSet(*aNodeSet, this);
  } else {
    *aResult = static_cast<txNodeSet*>(mNodeSetResults.pop());
    (*aResult)->append(*aNodeSet);
    (*aResult)->mRecycler = this;
  }
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult txResultRecycler::getNodeSet(const txXPathNode& aNode,
                                      txAExprResult** aResult) {
  if (mNodeSetResults.isEmpty()) {
    *aResult = new txNodeSet(aNode, this);
  } else {
    txNodeSet* nodes = static_cast<txNodeSet*>(mNodeSetResults.pop());
    nodes->append(aNode);
    nodes->mRecycler = this;
    *aResult = nodes;
  }
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult txResultRecycler::getNumberResult(double aValue,
                                           txAExprResult** aResult) {
  if (mNumberResults.isEmpty()) {
    *aResult = new NumberResult(aValue, this);
  } else {
    NumberResult* numRes = static_cast<NumberResult*>(mNumberResults.pop());
    numRes->value = aValue;
    numRes->mRecycler = this;
    *aResult = numRes;
  }
  NS_ADDREF(*aResult);

  return NS_OK;
}

void txResultRecycler::getBoolResult(bool aValue, txAExprResult** aResult) {
  *aResult = aValue ? mTrueResult : mFalseResult;
  NS_ADDREF(*aResult);
}

nsresult txResultRecycler::getNonSharedNodeSet(txNodeSet* aNodeSet,
                                               txNodeSet** aResult) {
  if (aNodeSet->mRefCnt > 1) {
    return getNodeSet(aNodeSet, aResult);
  }

  *aResult = aNodeSet;
  NS_ADDREF(*aResult);

  return NS_OK;
}

void txAExprResult::Release() {
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "txAExprResult");
  if (mRefCnt == 0) {
    if (mRecycler) {
      mRecycler->recycle(this);
    } else {
      delete this;
    }
  }
}

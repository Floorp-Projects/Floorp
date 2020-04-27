/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txResultRecycler_h__
#define txResultRecycler_h__

#include "nsCOMPtr.h"
#include "txStack.h"

class txAExprResult;
class StringResult;
class txNodeSet;
class txXPathNode;
class NumberResult;
class BooleanResult;

class txResultRecycler {
 public:
  txResultRecycler();
  ~txResultRecycler();

  void AddRef() {
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "txResultRecycler", sizeof(*this));
  }
  void Release() {
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "txResultRecycler");
    if (mRefCnt == 0) {
      mRefCnt = 1;  // stabilize
      delete this;
    }
  }

  /**
   * Returns an txAExprResult to this recycler for reuse.
   * @param aResult  result to recycle
   */
  void recycle(txAExprResult* aResult);

  /**
   * Functions to return results that will be fully used by the caller.
   * Returns nullptr on out-of-memory and an inited result otherwise.
   */
  nsresult getStringResult(StringResult** aResult);
  nsresult getStringResult(const nsAString& aValue, txAExprResult** aResult);
  nsresult getNodeSet(txNodeSet** aResult);
  nsresult getNodeSet(txNodeSet* aNodeSet, txNodeSet** aResult);
  nsresult getNodeSet(const txXPathNode& aNode, txAExprResult** aResult);
  nsresult getNumberResult(double aValue, txAExprResult** aResult);

  /**
   * Functions to return a txAExprResult that is shared across several
   * clients and must not be modified. Never returns nullptr.
   */
  void getEmptyStringResult(txAExprResult** aResult);
  void getBoolResult(bool aValue, txAExprResult** aResult);

  /**
   * Functions that return non-shared resultsobjects
   */
  nsresult getNonSharedNodeSet(txNodeSet* aNodeSet, txNodeSet** aResult);

 private:
  nsAutoRefCnt mRefCnt;
  txStack mStringResults;
  txStack mNodeSetResults;
  txStack mNumberResults;
  RefPtr<StringResult> mEmptyStringResult;
  RefPtr<BooleanResult> mTrueResult;
  RefPtr<BooleanResult> mFalseResult;
};

#endif  // txResultRecycler_h__

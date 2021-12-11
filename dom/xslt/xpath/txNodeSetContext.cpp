/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txNodeSetContext.h"
#include "txNodeSet.h"

const txXPathNode& txNodeSetContext::getContextNode() {
  return mContextSet->get(mPosition - 1);
}

uint32_t txNodeSetContext::size() { return (uint32_t)mContextSet->size(); }

uint32_t txNodeSetContext::position() {
  NS_ASSERTION(mPosition, "Should have called next() at least once");
  return mPosition;
}

nsresult txNodeSetContext::getVariable(int32_t aNamespace, nsAtom* aLName,
                                       txAExprResult*& aResult) {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->getVariable(aNamespace, aLName, aResult);
}

nsresult txNodeSetContext::isStripSpaceAllowed(const txXPathNode& aNode,
                                               bool& aAllowed) {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->isStripSpaceAllowed(aNode, aAllowed);
}

void* txNodeSetContext::getPrivateContext() {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->getPrivateContext();
}

txResultRecycler* txNodeSetContext::recycler() {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->recycler();
}

void txNodeSetContext::receiveError(const nsAString& aMsg, nsresult aRes) {
  NS_ASSERTION(mInner, "mInner is null!!!");
#ifdef DEBUG
  nsAutoString error(u"forwarded error: "_ns);
  error.Append(aMsg);
  mInner->receiveError(error, aRes);
#else
  mInner->receiveError(aMsg, aRes);
#endif
}

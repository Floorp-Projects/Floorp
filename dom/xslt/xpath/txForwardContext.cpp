/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txForwardContext.h"
#include "txNodeSet.h"

const txXPathNode& txForwardContext::getContextNode() { return mContextNode; }

uint32_t txForwardContext::size() { return (uint32_t)mContextSet->size(); }

uint32_t txForwardContext::position() {
  int32_t pos = mContextSet->indexOf(mContextNode);
  NS_ASSERTION(pos >= 0, "Context is not member of context node list.");
  return (uint32_t)(pos + 1);
}

nsresult txForwardContext::getVariable(int32_t aNamespace, nsAtom* aLName,
                                       txAExprResult*& aResult) {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->getVariable(aNamespace, aLName, aResult);
}

nsresult txForwardContext::isStripSpaceAllowed(const txXPathNode& aNode,
                                               bool& aAllowed) {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->isStripSpaceAllowed(aNode, aAllowed);
}

void* txForwardContext::getPrivateContext() {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->getPrivateContext();
}

txResultRecycler* txForwardContext::recycler() {
  NS_ASSERTION(mInner, "mInner is null!!!");
  return mInner->recycler();
}

void txForwardContext::receiveError(const nsAString& aMsg, nsresult aRes) {
  NS_ASSERTION(mInner, "mInner is null!!!");
#ifdef DEBUG
  nsAutoString error(u"forwarded error: "_ns);
  error.Append(aMsg);
  mInner->receiveError(error, aRes);
#else
  mInner->receiveError(aMsg, aRes);
#endif
}

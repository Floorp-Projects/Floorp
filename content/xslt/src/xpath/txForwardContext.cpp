/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txForwardContext.h"
#include "txNodeSet.h"

const txXPathNode& txForwardContext::getContextNode()
{
    return mContextNode;
}

PRUint32 txForwardContext::size()
{
    return (PRUint32)mContextSet->size();
}

PRUint32 txForwardContext::position()
{
    PRInt32 pos = mContextSet->indexOf(mContextNode);
    NS_ASSERTION(pos >= 0, "Context is not member of context node list.");
    return (PRUint32)(pos + 1);
}

nsresult txForwardContext::getVariable(PRInt32 aNamespace, nsIAtom* aLName,
                                       txAExprResult*& aResult)
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->getVariable(aNamespace, aLName, aResult);
}

bool txForwardContext::isStripSpaceAllowed(const txXPathNode& aNode)
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->isStripSpaceAllowed(aNode);
}

void* txForwardContext::getPrivateContext()
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->getPrivateContext();
}

txResultRecycler* txForwardContext::recycler()
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->recycler();
}

void txForwardContext::receiveError(const nsAString& aMsg, nsresult aRes)
{
    NS_ASSERTION(mInner, "mInner is null!!!");
#ifdef DEBUG
    nsAutoString error(NS_LITERAL_STRING("forwarded error: "));
    error.Append(aMsg);
    mInner->receiveError(error, aRes);
#else
    mInner->receiveError(aMsg, aRes);
#endif
}

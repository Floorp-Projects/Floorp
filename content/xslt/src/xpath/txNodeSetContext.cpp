/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txNodeSetContext.h"
#include "txNodeSet.h"

const txXPathNode& txNodeSetContext::getContextNode()
{
    return mContextSet->get(mPosition - 1);
}

PRUint32 txNodeSetContext::size()
{
    return (PRUint32)mContextSet->size();
}

PRUint32 txNodeSetContext::position()
{
    NS_ASSERTION(mPosition, "Should have called next() at least once");
    return mPosition;
}

nsresult txNodeSetContext::getVariable(PRInt32 aNamespace, nsIAtom* aLName,
                                       txAExprResult*& aResult)
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->getVariable(aNamespace, aLName, aResult);
}

bool txNodeSetContext::isStripSpaceAllowed(const txXPathNode& aNode)
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->isStripSpaceAllowed(aNode);
}

void* txNodeSetContext::getPrivateContext()
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->getPrivateContext();
}

txResultRecycler* txNodeSetContext::recycler()
{
    NS_ASSERTION(mInner, "mInner is null!!!");
    return mInner->recycler();
}

void txNodeSetContext::receiveError(const nsAString& aMsg, nsresult aRes)
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

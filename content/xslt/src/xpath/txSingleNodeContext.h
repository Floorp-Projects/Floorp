/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TX_XPATH_SINGLENODE_CONTEXT
#define __TX_XPATH_SINGLENODE_CONTEXT

#include "txIXPathContext.h"

class txSingleNodeContext : public txIEvalContext
{
public:
    txSingleNodeContext(const txXPathNode& aContextNode,
                        txIMatchContext* aContext)
        : mNode(aContextNode),
          mInner(aContext)
    {
        NS_ASSERTION(aContext, "txIMatchContext must be given");
    }

    nsresult getVariable(PRInt32 aNamespace, nsIAtom* aLName,
                         txAExprResult*& aResult)
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->getVariable(aNamespace, aLName, aResult);
    }

    bool isStripSpaceAllowed(const txXPathNode& aNode)
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->isStripSpaceAllowed(aNode);
    }

    void* getPrivateContext()
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->getPrivateContext();
    }

    txResultRecycler* recycler()
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->recycler();
    }

    void receiveError(const nsAString& aMsg, nsresult aRes)
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

    const txXPathNode& getContextNode()
    {
        return mNode;
    }

    PRUint32 size()
    {
        return 1;
    }

    PRUint32 position()
    {
        return 1;
    }

private:
    const txXPathNode& mNode;
    txIMatchContext* mInner;
};

#endif // __TX_XPATH_SINGLENODE_CONTEXT

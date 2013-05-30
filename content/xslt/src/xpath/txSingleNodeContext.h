/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TX_XPATH_SINGLENODE_CONTEXT
#define __TX_XPATH_SINGLENODE_CONTEXT

#include "mozilla/Attributes.h"
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

    nsresult getVariable(int32_t aNamespace, nsIAtom* aLName,
                         txAExprResult*& aResult) MOZ_OVERRIDE
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->getVariable(aNamespace, aLName, aResult);
    }

    bool isStripSpaceAllowed(const txXPathNode& aNode) MOZ_OVERRIDE
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->isStripSpaceAllowed(aNode);
    }

    void* getPrivateContext() MOZ_OVERRIDE
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->getPrivateContext();
    }

    txResultRecycler* recycler() MOZ_OVERRIDE
    {
        NS_ASSERTION(mInner, "mInner is null!!!");
        return mInner->recycler();
    }

    void receiveError(const nsAString& aMsg, nsresult aRes) MOZ_OVERRIDE
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

    const txXPathNode& getContextNode() MOZ_OVERRIDE
    {
        return mNode;
    }

    uint32_t size() MOZ_OVERRIDE
    {
        return 1;
    }

    uint32_t position() MOZ_OVERRIDE
    {
        return 1;
    }

private:
    const txXPathNode& mNode;
    txIMatchContext* mInner;
};

#endif // __TX_XPATH_SINGLENODE_CONTEXT

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TX_XPATH_CONTEXT
#define __TX_XPATH_CONTEXT

#include "txIXPathContext.h"
#include "nsAutoPtr.h"
#include "txNodeSet.h"

class txForwardContext : public txIEvalContext
{
public:
    txForwardContext(txIMatchContext* aContext,
                     const txXPathNode& aContextNode,
                     txNodeSet* aContextNodeSet)
        : mInner(aContext),
          mContextNode(aContextNode),
          mContextSet(aContextNodeSet)
    {}

    TX_DECL_EVAL_CONTEXT;

private:
    txIMatchContext* mInner;
    const txXPathNode& mContextNode;
    RefPtr<txNodeSet> mContextSet;
};

#endif // __TX_XPATH_CONTEXT

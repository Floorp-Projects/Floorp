/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TX_XPATH_SET_CONTEXT
#define __TX_XPATH_SET_CONTEXT

#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "nsAutoPtr.h"

class txNodeSetContext : public txIEvalContext
{
public:
    txNodeSetContext(txNodeSet* aContextNodeSet, txIMatchContext* aContext)
        : mContextSet(aContextNodeSet), mPosition(0), mInner(aContext)
    {
    }

    // Iteration over the given NodeSet
    bool hasNext()
    {
        return mPosition < size();
    }
    void next()
    {
        NS_ASSERTION(mPosition < size(), "Out of bounds.");
        mPosition++;
    }
    void setPosition(PRUint32 aPosition)
    {
        NS_ASSERTION(aPosition > 0 &&
                     aPosition <= size(), "Out of bounds.");
        mPosition = aPosition;
    }

    TX_DECL_EVAL_CONTEXT;

protected:
    nsRefPtr<txNodeSet> mContextSet;
    PRUint32 mPosition;
    txIMatchContext* mInner;
};

#endif // __TX_XPATH_SET_CONTEXT

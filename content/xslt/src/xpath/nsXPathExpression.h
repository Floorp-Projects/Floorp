/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPathExpression_h__
#define nsXPathExpression_h__

#include "nsIDOMXPathExpression.h"
#include "nsIDOMNSXPathExpression.h"
#include "txIXPathContext.h"
#include "txResultRecycler.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

class Expr;
class txXPathNode;

/**
 * A class for evaluating an XPath expression string
 */
class nsXPathExpression : public nsIDOMXPathExpression,
                          public nsIDOMNSXPathExpression
{
public:
    nsXPathExpression(nsAutoPtr<Expr>& aExpression, txResultRecycler* aRecycler,
                      nsIDOMDocument *aDocument);

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXPathExpression,
                                             nsIDOMXPathExpression)

    // nsIDOMXPathExpression interface
    NS_DECL_NSIDOMXPATHEXPRESSION

    // nsIDOMNSXPathExpression interface
    NS_DECL_NSIDOMNSXPATHEXPRESSION

private:
    nsAutoPtr<Expr> mExpression;
    nsRefPtr<txResultRecycler> mRecycler;
    nsCOMPtr<nsIDOMDocument> mDocument;

    class EvalContextImpl : public txIEvalContext
    {
    public:
        EvalContextImpl(const txXPathNode& aContextNode,
                        PRUint32 aContextPosition, PRUint32 aContextSize,
                        txResultRecycler* aRecycler)
            : mContextNode(aContextNode),
              mContextPosition(aContextPosition),
              mContextSize(aContextSize),
              mLastError(NS_OK),
              mRecycler(aRecycler)
        {
        }

        nsresult getError()
        {
            return mLastError;
        }

        TX_DECL_EVAL_CONTEXT;

    private:
        const txXPathNode& mContextNode;
        PRUint32 mContextPosition;
        PRUint32 mContextSize;
        nsresult mLastError;
        nsRefPtr<txResultRecycler> mRecycler;
    };
};

#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XPathExpression_h
#define mozilla_dom_XPathExpression_h

#include "nsIDOMXPathExpression.h"
#include "nsIDOMNSXPathExpression.h"
#include "txResultRecycler.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/Attributes.h"

class Expr;
class txXPathNode;

namespace mozilla {
namespace dom {

/**
 * A class for evaluating an XPath expression string
 */
class XPathExpression MOZ_FINAL : public nsIDOMXPathExpression,
                                  public nsIDOMNSXPathExpression
{
public:
    XPathExpression(nsAutoPtr<Expr>&& aExpression, txResultRecycler* aRecycler,
                    nsIDOMDocument *aDocument);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMXPathExpression interface
    NS_DECL_NSIDOMXPATHEXPRESSION

    // nsIDOMNSXPathExpression interface
    NS_DECL_NSIDOMNSXPATHEXPRESSION

private:
    ~nsXPathExpression() {}

    nsAutoPtr<Expr> mExpression;
    nsRefPtr<txResultRecycler> mRecycler;
    nsWeakPtr mDocument;
    bool mCheckDocument;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_XPathExpression_h */

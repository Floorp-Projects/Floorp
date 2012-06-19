/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPathNSResolver_h__
#define nsXPathNSResolver_h__

#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

/**
 * A class for evaluating an XPath expression string
 */
class nsXPathNSResolver MOZ_FINAL : public nsIDOMXPathNSResolver
{
public:
    nsXPathNSResolver(nsIDOMNode* aNode);

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(nsXPathNSResolver)

    // nsIDOMXPathNSResolver interface
    NS_DECL_NSIDOMXPATHNSRESOLVER

private:
    nsCOMPtr<nsIDOMNode> mNode;
};

#endif

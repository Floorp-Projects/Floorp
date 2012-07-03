/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPathNSResolver.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMString.h"

NS_IMPL_CYCLE_COLLECTION_1(nsXPathNSResolver, mNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXPathNSResolver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXPathNSResolver)

DOMCI_DATA(XPathNSResolver, nsXPathNSResolver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXPathNSResolver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathNSResolver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathNSResolver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XPathNSResolver)
NS_INTERFACE_MAP_END

nsXPathNSResolver::nsXPathNSResolver(nsIDOMNode* aNode)
  : mNode(aNode)
{
    NS_ASSERTION(mNode, "Need a node to resolve namespaces.");
}

NS_IMETHODIMP
nsXPathNSResolver::LookupNamespaceURI(const nsAString & aPrefix,
                                      nsAString & aResult)
{
    if (aPrefix.EqualsLiteral("xml")) {
        aResult.AssignLiteral("http://www.w3.org/XML/1998/namespace");

        return NS_OK;
    }

    if (!mNode) {
        SetDOMStringToNull(aResult);

        return NS_OK;
    }

    return mNode->LookupNamespaceURI(aPrefix, aResult);
}

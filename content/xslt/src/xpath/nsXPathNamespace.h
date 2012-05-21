/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPathNamespace_h__
#define nsXPathNamespace_h__

#include "nsIDOMXPathNamespace.h"

/* d0a75e07-b5e7-11d5-a7f2-df109fb8a1fc */
#define TRANSFORMIIX_XPATH_NAMESPACE_CID   \
{ 0xd0a75e07, 0xb5e7, 0x11d5, { 0xa7, 0xf2, 0xdf, 0x10, 0x9f, 0xb8, 0xa1, 0xfc } }

#define TRANSFORMIIX_XPATH_NAMESPACE_CONTRACTID \
"@mozilla.org/transformiix/xpath-namespace;1"

/**
 * A class for representing XPath namespace nodes in the DOM.
 */
class nsXPathNamespace : public nsIDOMXPathNamespace
{
public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMNode interface
    NS_DECL_NSIDOMNODE

    // nsIDOMXPathNamespace interface
    NS_DECL_NSIDOMXPATHNAMESPACE
};

#endif

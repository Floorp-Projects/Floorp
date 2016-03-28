/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpandedName.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "txStringUtils.h"
#include "txNamespaceMap.h"
#include "txXMLUtils.h"

nsresult
txExpandedName::init(const nsAString& aQName, txNamespaceMap* aResolver,
                     bool aUseDefault)
{
    const nsAFlatString& qName = PromiseFlatString(aQName);
    const char16_t* colon;
    bool valid = XMLUtils::isValidQName(qName, &colon);
    if (!valid) {
        return NS_ERROR_FAILURE;
    }

    if (colon) {
        nsCOMPtr<nsIAtom> prefix = NS_Atomize(Substring(qName.get(), colon));
        int32_t namespaceID = aResolver->lookupNamespace(prefix);
        if (namespaceID == kNameSpaceID_Unknown)
            return NS_ERROR_FAILURE;
        mNamespaceID = namespaceID;

        const char16_t *end;
        qName.EndReading(end);
        mLocalName = NS_Atomize(Substring(colon + 1, end));
    }
    else {
        mNamespaceID = aUseDefault ? aResolver->lookupNamespace(nullptr) :
                                     kNameSpaceID_None;
        mLocalName = NS_Atomize(aQName);
    }
    return NS_OK;
}

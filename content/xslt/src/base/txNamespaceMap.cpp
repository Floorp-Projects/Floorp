/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txNamespaceMap.h"
#include "nsGkAtoms.h"
#include "txXPathNode.h"

txNamespaceMap::txNamespaceMap()
{
}

txNamespaceMap::txNamespaceMap(const txNamespaceMap& aOther)
    : mPrefixes(aOther.mPrefixes)
{
    mNamespaces = aOther.mNamespaces; //bah! I want a copy-constructor!
}

nsresult
txNamespaceMap::mapNamespace(nsIAtom* aPrefix, const nsAString& aNamespaceURI)
{
    nsIAtom* prefix = aPrefix == nsGkAtoms::_empty ? nullptr : aPrefix;

    int32_t nsId;
    if (prefix && aNamespaceURI.IsEmpty()) {
        // Remove the mapping
        int32_t index = mPrefixes.IndexOf(prefix);
        if (index >= 0) {
            mPrefixes.RemoveObjectAt(index);
            mNamespaces.RemoveElementAt(index);
        }

        return NS_OK;
    }

    if (aNamespaceURI.IsEmpty()) {
        // Set default to empty namespace
        nsId = kNameSpaceID_None;
    }
    else {
        nsId = txNamespaceManager::getNamespaceID(aNamespaceURI);
        NS_ENSURE_FALSE(nsId == kNameSpaceID_Unknown, NS_ERROR_FAILURE);
    }

    // Check if the mapping already exists
    int32_t index = mPrefixes.IndexOf(prefix);
    if (index >= 0) {
        mNamespaces.ElementAt(index) = nsId;

        return NS_OK;
    }

    // New mapping
    if (!mPrefixes.AppendObject(prefix)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    if (mNamespaces.AppendElement(nsId) == nullptr) {
        mPrefixes.RemoveObjectAt(mPrefixes.Count() - 1);

        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

int32_t
txNamespaceMap::lookupNamespace(nsIAtom* aPrefix)
{
    if (aPrefix == nsGkAtoms::xml) {
        return kNameSpaceID_XML;
    }

    nsIAtom* prefix = aPrefix == nsGkAtoms::_empty ? 0 : aPrefix;

    int32_t index = mPrefixes.IndexOf(prefix);
    if (index >= 0) {
        return mNamespaces.SafeElementAt(index, kNameSpaceID_Unknown);
    }

    if (!prefix) {
        return kNameSpaceID_None;
    }

    return kNameSpaceID_Unknown;
}

int32_t
txNamespaceMap::lookupNamespaceWithDefault(const nsAString& aPrefix)
{
    nsCOMPtr<nsIAtom> prefix = do_GetAtom(aPrefix);
    if (prefix != nsGkAtoms::_poundDefault) {
        return lookupNamespace(prefix);
    }

    return lookupNamespace(nullptr);
}

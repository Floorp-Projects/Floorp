/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txNamespaceMap.h"
#include "nsGkAtoms.h"
#include "txXPathNode.h"

txNamespaceMap::txNamespaceMap() = default;

txNamespaceMap::txNamespaceMap(const txNamespaceMap& aOther)
    : mPrefixes(aOther.mPrefixes.Clone()),
      mNamespaces(aOther.mNamespaces.Clone()) {}

nsresult txNamespaceMap::mapNamespace(nsAtom* aPrefix,
                                      const nsAString& aNamespaceURI) {
  nsAtom* prefix = aPrefix == nsGkAtoms::_empty ? nullptr : aPrefix;

  int32_t nsId;
  if (prefix && aNamespaceURI.IsEmpty()) {
    // Remove the mapping
    int32_t index = mPrefixes.IndexOf(prefix);
    if (index >= 0) {
      mPrefixes.RemoveElementAt(index);
      mNamespaces.RemoveElementAt(index);
    }

    return NS_OK;
  }

  if (aNamespaceURI.IsEmpty()) {
    // Set default to empty namespace
    nsId = kNameSpaceID_None;
  } else {
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
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  mPrefixes.AppendElement(prefix);

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  mNamespaces.AppendElement(nsId);

  return NS_OK;
}

int32_t txNamespaceMap::lookupNamespace(nsAtom* aPrefix) {
  if (aPrefix == nsGkAtoms::xml) {
    return kNameSpaceID_XML;
  }

  nsAtom* prefix = aPrefix == nsGkAtoms::_empty ? 0 : aPrefix;

  int32_t index = mPrefixes.IndexOf(prefix);
  if (index >= 0) {
    return mNamespaces.SafeElementAt(index, kNameSpaceID_Unknown);
  }

  if (!prefix) {
    return kNameSpaceID_None;
  }

  return kNameSpaceID_Unknown;
}

int32_t txNamespaceMap::lookupNamespaceWithDefault(const nsAString& aPrefix) {
  RefPtr<nsAtom> prefix = NS_Atomize(aPrefix);
  if (prefix != nsGkAtoms::_poundDefault) {
    return lookupNamespace(prefix);
  }

  return lookupNamespace(nullptr);
}

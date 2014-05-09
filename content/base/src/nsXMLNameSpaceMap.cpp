/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for keeping track of prefix-to-namespace-id mappings
 */

#include "nsXMLNameSpaceMap.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"

template <>
class nsDefaultComparator <nsNameSpaceEntry, nsIAtom*> {
  public:
    bool Equals(const nsNameSpaceEntry& aEntry, nsIAtom* const& aPrefix) const {
      return aEntry.prefix == aPrefix;
    }
};

template <>
class nsDefaultComparator <nsNameSpaceEntry, int32_t> {
  public:
    bool Equals(const nsNameSpaceEntry& aEntry, const int32_t& aNameSpace) const {
      return aEntry.nameSpaceID == aNameSpace;
    }
};


/* static */ nsXMLNameSpaceMap*
nsXMLNameSpaceMap::Create(bool aForXML)
{
  nsXMLNameSpaceMap *map = new nsXMLNameSpaceMap();
  NS_ENSURE_TRUE(map, nullptr);

  if (aForXML) {
    nsresult rv1 = map->AddPrefix(nsGkAtoms::xmlns,
                                  kNameSpaceID_XMLNS);
    nsresult rv2 = map->AddPrefix(nsGkAtoms::xml, kNameSpaceID_XML);

    if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
      delete map;
      map = nullptr;
    }
  }

  return map;
}

nsXMLNameSpaceMap::nsXMLNameSpaceMap()
  : mNameSpaces(4)
{
}

nsresult
nsXMLNameSpaceMap::AddPrefix(nsIAtom *aPrefix, int32_t aNameSpaceID)
{
  if (!mNameSpaces.Contains(aPrefix) && !mNameSpaces.AppendElement(aPrefix)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mNameSpaces[mNameSpaces.IndexOf(aPrefix)].nameSpaceID = aNameSpaceID;
  return NS_OK;
}

nsresult
nsXMLNameSpaceMap::AddPrefix(nsIAtom *aPrefix, nsString &aURI)
{
  int32_t id;
  nsresult rv = nsContentUtils::NameSpaceManager()->RegisterNameSpace(aURI,
                                                                      id);

  NS_ENSURE_SUCCESS(rv, rv);

  return AddPrefix(aPrefix, id);
}

int32_t
nsXMLNameSpaceMap::FindNameSpaceID(nsIAtom *aPrefix) const
{
  size_t index = mNameSpaces.IndexOf(aPrefix);
  if (index != mNameSpaces.NoIndex) {
    return mNameSpaces[index].nameSpaceID;
  }

  // The default mapping for no prefix is no namespace.  If a non-null prefix
  // was specified and we didn't find it, we return an error.

  return aPrefix ? kNameSpaceID_Unknown : kNameSpaceID_None;
}

nsIAtom*
nsXMLNameSpaceMap::FindPrefix(int32_t aNameSpaceID) const
{
  size_t index = mNameSpaces.IndexOf(aNameSpaceID);
  if (index != mNameSpaces.NoIndex) {
    return mNameSpaces[index].prefix;
  }

  return nullptr;
}

void
nsXMLNameSpaceMap::Clear()
{
  mNameSpaces.Clear();
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for keeping track of prefix-to-namespace-id mappings
 */

#include "nsXMLNameSpaceMap.h"
#include "nsINameSpaceManager.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"

template <>
class nsDefaultComparator <nsNameSpaceEntry, nsIAtom*> {
  public:
    bool Equals(const nsNameSpaceEntry& aEntry, nsIAtom* const& aPrefix) const {
      return aEntry.prefix == aPrefix;
    }
};

template <>
class nsDefaultComparator <nsNameSpaceEntry, PRInt32> {
  public:
    bool Equals(const nsNameSpaceEntry& aEntry, const PRInt32& aNameSpace) const {
      return aEntry.nameSpaceID == aNameSpace;
    }
};


/* static */ nsXMLNameSpaceMap*
nsXMLNameSpaceMap::Create(bool aForXML)
{
  nsXMLNameSpaceMap *map = new nsXMLNameSpaceMap();
  NS_ENSURE_TRUE(map, nsnull);

  if (aForXML) {
    nsresult rv = map->AddPrefix(nsGkAtoms::xmlns,
                                 kNameSpaceID_XMLNS);
    rv |= map->AddPrefix(nsGkAtoms::xml, kNameSpaceID_XML);

    if (NS_FAILED(rv)) {
      delete map;
      map = nsnull;
    }
  }

  return map;
}

nsXMLNameSpaceMap::nsXMLNameSpaceMap()
  : mNameSpaces(4)
{
}

nsresult
nsXMLNameSpaceMap::AddPrefix(nsIAtom *aPrefix, PRInt32 aNameSpaceID)
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
  PRInt32 id;
  nsresult rv = nsContentUtils::NameSpaceManager()->RegisterNameSpace(aURI,
                                                                      id);

  NS_ENSURE_SUCCESS(rv, rv);

  return AddPrefix(aPrefix, id);
}

PRInt32
nsXMLNameSpaceMap::FindNameSpaceID(nsIAtom *aPrefix) const
{
  PRUint32 index = mNameSpaces.IndexOf(aPrefix);
  if (index != mNameSpaces.NoIndex) {
    return mNameSpaces[index].nameSpaceID;
  }

  // The default mapping for no prefix is no namespace.  If a non-null prefix
  // was specified and we didn't find it, we return an error.

  return aPrefix ? kNameSpaceID_Unknown : kNameSpaceID_None;
}

nsIAtom*
nsXMLNameSpaceMap::FindPrefix(PRInt32 aNameSpaceID) const
{
  PRUint32 index = mNameSpaces.IndexOf(aNameSpaceID);
  if (index != mNameSpaces.NoIndex) {
    return mNameSpaces[index].prefix;
  }

  return nsnull;
}

void
nsXMLNameSpaceMap::Clear()
{
  mNameSpaces.Clear();
}

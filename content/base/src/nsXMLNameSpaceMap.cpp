/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A class for keeping track of prefix-to-namespace-id mappings
 */

#include "nsXMLNameSpaceMap.h"
#include "nsINameSpaceManager.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"

NS_SPECIALIZE_TEMPLATE
class nsDefaultComparator <nsNameSpaceEntry, nsIAtom*> {
  public:
    bool Equals(const nsNameSpaceEntry& aEntry, nsIAtom* const& aPrefix) const {
      return aEntry.prefix == aPrefix;
    }
};

NS_SPECIALIZE_TEMPLATE
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

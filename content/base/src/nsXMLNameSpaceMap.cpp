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
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"

struct nsNameSpaceEntry
{
  nsNameSpaceEntry(nsIAtom *aPrefix)
    : prefix(aPrefix) {}

  nsCOMPtr<nsIAtom> prefix;
  PRInt32 nameSpaceID;
};

/* static */ nsXMLNameSpaceMap*
nsXMLNameSpaceMap::Create()
{
  nsXMLNameSpaceMap *map = new nsXMLNameSpaceMap();
  NS_ENSURE_TRUE(map, nsnull);

  nsresult rv = map->AddPrefix(nsGkAtoms::xmlns,
                               kNameSpaceID_XMLNS);
  rv |= map->AddPrefix(nsGkAtoms::xml, kNameSpaceID_XML);

  if (NS_FAILED(rv)) {
    delete map;
    map = nsnull;
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
  PRInt32 count = mNameSpaces.Count();
  nsNameSpaceEntry *foundEntry = nsnull;

  for (PRInt32 i = 0; i < count; ++i) {
    nsNameSpaceEntry *entry = NS_STATIC_CAST(nsNameSpaceEntry*,
                                             mNameSpaces.FastElementAt(i));

    NS_ASSERTION(entry, "null entry in namespace map!");

    if (entry->prefix == aPrefix) {
      foundEntry = entry;
      break;
    }
  }

  if (!foundEntry) {
    foundEntry = new nsNameSpaceEntry(aPrefix);
    NS_ENSURE_TRUE(foundEntry, NS_ERROR_OUT_OF_MEMORY);

    if (!mNameSpaces.AppendElement(foundEntry)) {
      delete foundEntry;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  foundEntry->nameSpaceID = aNameSpaceID;
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

void
nsXMLNameSpaceMap::RemovePrefix(nsIAtom *aPrefix)
{
  PRInt32 count = mNameSpaces.Count();

  for (PRInt32 i = 0; i < count; ++i) {
    nsNameSpaceEntry *entry = NS_STATIC_CAST(nsNameSpaceEntry*,
                                             mNameSpaces.FastElementAt(i));

    NS_ASSERTION(entry, "null entry in namespace map!");

    if (entry->prefix == aPrefix) {
      mNameSpaces.RemoveElementAt(i);
      return;
    }
  }
}

PRInt32
nsXMLNameSpaceMap::FindNameSpaceID(nsIAtom *aPrefix) const
{
  PRInt32 count = mNameSpaces.Count();

  for (PRInt32 i = 0; i < count; ++i) {
    nsNameSpaceEntry *entry = NS_STATIC_CAST(nsNameSpaceEntry*,
                                             mNameSpaces.FastElementAt(i));

    NS_ASSERTION(entry, "null entry in namespace map!");

    if (entry->prefix == aPrefix) {
      return entry->nameSpaceID;
    }
  }

  // The default mapping for no prefix is no namespace.  If a non-null prefix
  // was specified and we didn't find it, we return an error.

  return aPrefix ? kNameSpaceID_Unknown : kNameSpaceID_None;
}

nsIAtom*
nsXMLNameSpaceMap::FindPrefix(PRInt32 aNameSpaceID) const
{
  PRInt32 count = mNameSpaces.Count();

  for (PRInt32 i = 0; i < count; ++i) {
    nsNameSpaceEntry *entry = NS_STATIC_CAST(nsNameSpaceEntry*,
                                             mNameSpaces.FastElementAt(i));

    NS_ASSERTION(entry, "null entry in namespace map!");

    if (entry->nameSpaceID == aNameSpaceID) {
      return entry->prefix;
    }
  }

  return nsnull;
}

PR_STATIC_CALLBACK(PRBool) DeleteEntry(void *aElement, void *aData)
{
  delete NS_STATIC_CAST(nsNameSpaceEntry*, aElement);
  return PR_TRUE;
}

void
nsXMLNameSpaceMap::Clear()
{
  mNameSpaces.EnumerateForwards(DeleteEntry, nsnull);
}

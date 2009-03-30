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

#ifndef nsXMLNameSpaceMap_h_
#define nsXMLNameSpaceMap_h_

#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

struct nsNameSpaceEntry
{
  nsNameSpaceEntry(nsIAtom *aPrefix)
    : prefix(aPrefix) {}

  nsCOMPtr<nsIAtom> prefix;
  PRInt32 nameSpaceID;
};

/**
 * nsXMLNameSpaceMap contains a set of prefixes which are mapped onto
 * namespaces.  It allows the set to be searched by prefix or by namespace ID.
 */
class nsXMLNameSpaceMap
{
public:
  /**
   * Allocates a new nsXMLNameSpaceMap (with new()) and initializes it with the
   * xmlns and xml namespaces.
   */
  static NS_HIDDEN_(nsXMLNameSpaceMap*) Create();

  /**
   * Add a prefix and its corresponding namespace ID to the map.
   * Passing a null |aPrefix| corresponds to the default namespace, which may
   * be set to something other than kNameSpaceID_None.
   */
  NS_HIDDEN_(nsresult) AddPrefix(nsIAtom *aPrefix, PRInt32 aNameSpaceID);

  /**
   * Add a prefix and a namespace URI to the map.  The URI will be converted
   * to its corresponding namespace ID.
   */
  NS_HIDDEN_(nsresult) AddPrefix(nsIAtom *aPrefix, nsString &aURI);

  /* Remove a prefix from the map. */
  NS_HIDDEN_(void) RemovePrefix(nsIAtom *aPrefix);

  /*
   * Returns the namespace ID for the given prefix, if it is in the map.
   * If |aPrefix| is null and is not in the map, then a null namespace
   * (kNameSpaceID_None) is returned.  If |aPrefix| is non-null and is not in
   * the map, then kNameSpaceID_Unknown is returned.
   */
  NS_HIDDEN_(PRInt32) FindNameSpaceID(nsIAtom *aPrefix) const;

  /**
   * If the given namespace ID is in the map, then the first prefix which
   * maps to that namespace is returned.  Otherwise, null is returned.
   */
  NS_HIDDEN_(nsIAtom*) FindPrefix(PRInt32 aNameSpaceID) const;

  /* Removes all prefix mappings. */
  NS_HIDDEN_(void) Clear();

  ~nsXMLNameSpaceMap() { Clear(); }

private:
  nsXMLNameSpaceMap() NS_HIDDEN;  // use Create() to create new instances

  nsTArray<nsNameSpaceEntry> mNameSpaces;
};

#endif

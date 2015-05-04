/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLNameSpaceMap_h_
#define nsXMLNameSpaceMap_h_

#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

struct nsNameSpaceEntry
{
  explicit nsNameSpaceEntry(nsIAtom* aPrefix)
    : prefix(aPrefix) {}

  nsCOMPtr<nsIAtom> prefix;
  int32_t nameSpaceID;
};

/**
 * nsXMLNameSpaceMap contains a set of prefixes which are mapped onto
 * namespaces.  It allows the set to be searched by prefix or by namespace ID.
 */
class nsXMLNameSpaceMap
{
public:
  /**
   * Allocates a new nsXMLNameSpaceMap (with new()) and if aForXML is
   * true initializes it with the xmlns and xml namespaces.
   */
  static nsXMLNameSpaceMap* Create(bool aForXML);

  /**
   * Add a prefix and its corresponding namespace ID to the map.
   * Passing a null |aPrefix| corresponds to the default namespace, which may
   * be set to something other than kNameSpaceID_None.
   */
  nsresult AddPrefix(nsIAtom *aPrefix, int32_t aNameSpaceID);

  /**
   * Add a prefix and a namespace URI to the map.  The URI will be converted
   * to its corresponding namespace ID.
   */
  nsresult AddPrefix(nsIAtom *aPrefix, nsString &aURI);

  /*
   * Returns the namespace ID for the given prefix, if it is in the map.
   * If |aPrefix| is null and is not in the map, then a null namespace
   * (kNameSpaceID_None) is returned.  If |aPrefix| is non-null and is not in
   * the map, then kNameSpaceID_Unknown is returned.
   */
  int32_t FindNameSpaceID(nsIAtom *aPrefix) const;

  /**
   * If the given namespace ID is in the map, then the first prefix which
   * maps to that namespace is returned.  Otherwise, null is returned.
   */
  nsIAtom* FindPrefix(int32_t aNameSpaceID) const;

  /* Removes all prefix mappings. */
  void Clear();

  ~nsXMLNameSpaceMap() { Clear(); }

private:
  nsXMLNameSpaceMap();  // use Create() to create new instances

  nsTArray<nsNameSpaceEntry> mNameSpaces;
};

#endif

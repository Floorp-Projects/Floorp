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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Boris Zbarsky <bzbarsky@mit.edu> (original author)
 *    L. David Baron <dbaron@dbaron.org>
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
#ifndef nsContentIndexCache_h__
#define nsContentIndexCache_h__

#include "nscore.h"
#include "jshashtable.h"
#include "mozilla/dom/Element.h"

/*
 * A class that computes and caches the indices used for :nth-* pseudo-class
 * matching.
 */

class nsNthIndexCache {
private:
  typedef mozilla::dom::Element Element;

public:
  /**
   * Constructor and destructor out of line so that we don't try to
   * instantiate the hashtable template all over the place.
   */
  nsNthIndexCache();
  ~nsNthIndexCache();

  // Returns a 1-based index of the child in its parent.  If the child
  // is not in its parent's child list (i.e., it is anonymous content),
  // returns 0.
  // If aCheckEdgeOnly is true, the function will return 1 if the result
  // is 1, and something other than 1 (maybe or maybe not a valid
  // result) otherwise.
  // This must only be called on nodes which have a non-null parent.
  PRInt32 GetNthIndex(Element* aChild, bool aIsOfType, bool aIsFromEnd,
                      bool aCheckEdgeOnly);

  void Reset();

private:
  /**
   * Returns true if aSibling and aElement should be considered in the same
   * list for nth-index purposes, taking aIsOfType into account.
   */
  inline bool SiblingMatchesElement(nsIContent* aSibling, Element* aElement,
                                    bool aIsOfType);

  /**
   * Returns true if aResult has been set to the correct value for aChild and
   * no more work needs to be done.  Returns false otherwise.
   */
  inline bool IndexDetermined(nsIContent* aSibling, Element* aChild,
                              bool aIsOfType, bool aIsFromEnd,
                              bool aCheckEdgeOnly, PRInt32& aResult);

  struct CacheEntry {
    CacheEntry() {
      mNthIndices[0][0] = -2;
      mNthIndices[0][1] = -2;
      mNthIndices[1][0] = -2;
      mNthIndices[1][1] = -2;
    }

    // This node's index for :nth-child(), :nth-last-child(),
    // :nth-of-type(), :nth-last-of-type().  If -2, needs to be computed.
    // If -1, needs to be computed but known not to be 1.
    // If 0, the node is not at any index in its parent.
    // The first subscript is 0 for -child and 1 for -of-type, the second
    // subscript is 0 for nth- and 1 for nth-last-.
    PRInt32 mNthIndices[2][2];
  };

  class SystemAllocPolicy {
  public:
    void *malloc_(size_t bytes) { return ::malloc(bytes); }
    void *realloc_(void *p, size_t bytes) { return ::realloc(p, bytes); }
    void free_(void *p) { ::free(p); }
    void reportAllocOverflow() const {}
  };

  typedef js::HashMap<nsIContent*, CacheEntry, js::DefaultHasher<nsIContent*>,
                      SystemAllocPolicy> Cache;

  Cache mCache;
};

#endif /* nsContentIndexCache_h__ */

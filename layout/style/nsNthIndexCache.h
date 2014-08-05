/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsContentIndexCache_h__
#define nsContentIndexCache_h__

#include "js/HashTable.h"

class nsIContent;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

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
  int32_t GetNthIndex(Element* aChild, bool aIsOfType, bool aIsFromEnd,
                      bool aCheckEdgeOnly);

  void Reset();

private:
  /**
   * Returns true if aSibling and aElement should be considered in the same
   * list for nth-index purposes, taking aIsOfType into account.
   */
  inline bool SiblingMatchesElement(nsIContent* aSibling, Element* aElement,
                                    bool aIsOfType);

  // This node's index for this cache.
  // If -2, needs to be computed.
  // If -1, needs to be computed but known not to be 1.
  // If 0, the node is not at any index in its parent.
  typedef int32_t CacheEntry;

  class SystemAllocPolicy {
  public:
    void *malloc_(size_t bytes) { return ::malloc(bytes); }

    template <typename T>
    T *pod_calloc(size_t numElems) {
      return static_cast<T *>(::calloc(numElems, sizeof(T)));
    }

    void *realloc_(void *p, size_t bytes) { return ::realloc(p, bytes); }
    void free_(void *p) { ::free(p); }
    void reportAllocOverflow() const {}
  };

  typedef js::HashMap<nsIContent*, CacheEntry, js::DefaultHasher<nsIContent*>,
                      SystemAllocPolicy> Cache;

  /**
   * Returns true if aResult has been set to the correct value for aChild and
   * no more work needs to be done.  Returns false otherwise.
   *
   * aResult is an inout parameter.  The in value is the number of elements
   * that are in the half-open range (aSibling, aChild] (so including aChild
   * but not including aSibling) that match aChild.  The out value is the
   * correct index for aChild if this function returns true and the number of
   * elements in the closed range [aSibling, aChild] that match aChild
   * otherwise.
   */
  inline bool IndexDeterminedFromPreviousSibling(nsIContent* aSibling,
                                                 Element* aChild,
                                                 bool aIsOfType,
                                                 bool aIsFromEnd,
                                                 const Cache& aCache,
                                                 int32_t& aResult);

  // Caches of indices for :nth-child(), :nth-last-child(),
  // :nth-of-type(), :nth-last-of-type(), keyed by Element*.
  //
  // The first subscript is 0 for -child and 1 for -of-type, the second
  // subscript is 0 for nth- and 1 for nth-last-.
  Cache mCaches[2][2];
};

#endif /* nsContentIndexCache_h__ */

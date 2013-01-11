/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that computes and caches the indices used for :nth-* pseudo-class
 * matching.
 */

#include "nsNthIndexCache.h"
#include "nsIContent.h"

nsNthIndexCache::nsNthIndexCache()
{
}

nsNthIndexCache::~nsNthIndexCache()
{
}

void
nsNthIndexCache::Reset()
{
  mCaches[0][0].clear();
  mCaches[0][1].clear();
  mCaches[1][0].clear();
  mCaches[1][1].clear();
}

inline bool
nsNthIndexCache::SiblingMatchesElement(nsIContent* aSibling, Element* aElement,
                                       bool aIsOfType)
{
  return aSibling->IsElement() &&
    (!aIsOfType ||
     aSibling->NodeInfo()->NameAndNamespaceEquals(aElement->NodeInfo()));
}

inline bool
nsNthIndexCache::IndexDeterminedFromPreviousSibling(nsIContent* aSibling,
                                                    Element* aChild,
                                                    bool aIsOfType,
                                                    bool aIsFromEnd,
                                                    const Cache& aCache,
                                                    int32_t& aResult)
{
  if (SiblingMatchesElement(aSibling, aChild, aIsOfType)) {
    Cache::Ptr siblingEntry = aCache.lookup(aSibling);
    if (siblingEntry) {
      int32_t siblingIndex = siblingEntry->value;
      NS_ASSERTION(siblingIndex != 0,
                   "How can a non-anonymous node have an anonymous sibling?");
      if (siblingIndex > 0) {
        // At this point, aResult is a count of how many elements matching
        // aChild we have seen after aSibling, including aChild itself.
        // |siblingIndex| is the index of aSibling.
        // So if aIsFromEnd, we want |aResult = siblingIndex - aResult| and
        // otherwise we want |aResult = siblingIndex + aResult|.
        aResult = siblingIndex + aResult * (1 - 2 * aIsFromEnd);
        return true;
      }
    }

    ++aResult;
  }

  return false;
}

int32_t
nsNthIndexCache::GetNthIndex(Element* aChild, bool aIsOfType,
                             bool aIsFromEnd, bool aCheckEdgeOnly)
{
  NS_ASSERTION(aChild->GetParent(), "caller should check GetParent()");

  if (aChild->IsRootOfAnonymousSubtree()) {
    return 0;
  }

  Cache &cache = mCaches[aIsOfType][aIsFromEnd];

  if (!cache.initialized() && !cache.init()) {
    // Give up and just don't match.
    return 0;
  }

  Cache::AddPtr entry = cache.lookupForAdd(aChild);

  // Default the value to -2 when adding
  if (!entry && !cache.add(entry, aChild, -2)) {
    // No good; don't match.
    return 0;
  }

  int32_t &slot = entry->value;
  if (slot != -2 && (slot != -1 || aCheckEdgeOnly)) {
    return slot;
  }
  
  int32_t result = 1;
  if (aCheckEdgeOnly) {
    // The caller only cares whether or not the result is 1, so we can
    // stop as soon as we see any other elements that match us.
    if (aIsFromEnd) {
      for (nsIContent *cur = aChild->GetNextSibling();
           cur;
           cur = cur->GetNextSibling()) {
        if (SiblingMatchesElement(cur, aChild, aIsOfType)) {
          result = -1;
          break;
        }
      }
    } else {
      for (nsIContent *cur = aChild->GetPreviousSibling();
           cur;
           cur = cur->GetPreviousSibling()) {
        if (SiblingMatchesElement(cur, aChild, aIsOfType)) {
          result = -1;
          break;
        }
      }
    }
  } else {
    // In the common case, we already have a cached index for one of
    // our previous siblings, so check that first.
    for (nsIContent *cur = aChild->GetPreviousSibling();
         cur;
         cur = cur->GetPreviousSibling()) {
      if (IndexDeterminedFromPreviousSibling(cur, aChild, aIsOfType,
                                             aIsFromEnd, cache, result)) {
        slot = result;
        return result;
      }
    }

    // Now if aIsFromEnd we lose: need to actually compute our index,
    // since looking at previous siblings wouldn't have told us
    // anything about it.  Note that it doesn't make sense to do cache
    // lookups on our following siblings, since chances are the cache
    // is not primed for them.
    if (aIsFromEnd) {
      result = 1;
      for (nsIContent *cur = aChild->GetNextSibling();
           cur;
           cur = cur->GetNextSibling()) {
        if (SiblingMatchesElement(cur, aChild, aIsOfType)) {
          ++result;
        }
      }
    }
  }

  slot = result;
  return result;
}

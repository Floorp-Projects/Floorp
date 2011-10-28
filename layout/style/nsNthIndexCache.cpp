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
                                                    PRInt32& aResult)
{
  if (SiblingMatchesElement(aSibling, aChild, aIsOfType)) {
    Cache::Ptr siblingEntry = aCache.lookup(aSibling);
    if (siblingEntry) {
      PRInt32 siblingIndex = siblingEntry->value;
      NS_ASSERTION(siblingIndex != 0,
                   "How can a non-anonymous node have an anonymous sibling?");
      if (siblingIndex > 0) {
        // At this point, aResult is a count of how many elements matching
        // aChild we have seen after aSibling, including aChild itself.
        // |siblingIndex| is the index of aSibling.
        // So if aIsFromEnd, we want |aResult = siblingIndex - aResult| and
        // otherwise we want |aResult = siblingIndex + aResult|.
        NS_ABORT_IF_FALSE(aIsFromEnd == 0 || aIsFromEnd == 1,
                          "Bogus bool value");
        aResult = siblingIndex + aResult * (1 - 2 * aIsFromEnd);
        return true;
      }
    }
    
    ++aResult;
  }

  return false;
}

PRInt32
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

  PRInt32 &slot = entry->value;
  if (slot != -2 && (slot != -1 || aCheckEdgeOnly)) {
    return slot;
  }
  
  PRInt32 result = 1;
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

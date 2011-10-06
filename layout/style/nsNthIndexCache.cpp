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
  mCache.clear();
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
nsNthIndexCache::IndexDetermined(nsIContent* aSibling, Element* aChild,
                                 bool aIsOfType, bool aIsFromEnd,
                                 bool aCheckEdgeOnly, PRInt32& aResult)
{
  if (SiblingMatchesElement(aSibling, aChild, aIsOfType)) {
    if (aCheckEdgeOnly) {
      // The caller only cares whether or not the result is 1, and we
      // now know it's not.
      aResult = -1;
      return true;
    }

    Cache::Ptr siblingEntry = mCache.lookup(aSibling);
    if (siblingEntry) {
      PRInt32 siblingIndex = siblingEntry->value.mNthIndices[aIsOfType][aIsFromEnd];
      NS_ASSERTION(siblingIndex != 0,
                   "How can a non-anonymous node have an anonymous sibling?");
      if (siblingIndex > 0) {
        // At this point, aResult is a count of how many elements matching
        // aChild we have seen after aSibling, including aChild itself.  So if
        // |siblingIndex| is the index of aSibling, we need to add aResult to
        // get the right answer here.
        aResult = siblingIndex + aResult;
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

  if (!mCache.initialized() && !mCache.init()) {
    // Give up and just don't match.
    return 0;
  }

  Cache::AddPtr entry = mCache.lookupForAdd(aChild);
  
  if (!entry && !mCache.add(entry, aChild)) {
    // No good; don't match.
    return 0;
  }

  PRInt32 &slot = entry->value.mNthIndices[aIsOfType][aIsFromEnd];
  if (slot != -2 && (slot != -1 || aCheckEdgeOnly)) {
    return slot;
  }
  
  PRInt32 result = 1;
  if (aIsFromEnd) {
    for (nsIContent *cur = aChild->GetNextSibling();
         cur;
         cur = cur->GetNextSibling()) {
      // It doesn't make sense to do cache lookups for siblings when
      // aIsFromEnd.  In general, the cache will only be primed for
      // things that are _before_ us in the DOM.
      if (SiblingMatchesElement(cur, aChild, aIsOfType)) {
        if (aCheckEdgeOnly) {
          // The caller only cares whether or not the result is 1, and we
          // now know it's not.
          result = -1;
          break;
        }
        ++result;
      }
    }
  } else {
    for (nsIContent *cur = aChild->GetPreviousSibling();
         cur;
         cur = cur->GetPreviousSibling()) {
      if (IndexDetermined(cur, aChild, aIsOfType, aIsFromEnd, aCheckEdgeOnly,
                          result)) {
        break;
      }
    }
  }

  slot = result;
  return result;
}

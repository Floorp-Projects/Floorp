/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextLeafRange.h"

#include "mozilla/a11y/Accessible.h"
#include "nsTArray.h"

namespace mozilla::a11y {

/*** TextLeafPoint ***/

TextLeafPoint::TextLeafPoint(Accessible* aAcc, int32_t aOffset) {
  if (aAcc->HasChildren()) {
    // Find a leaf. This might not necessarily be a TextLeafAccessible; it
    // could be an empty container.
    for (Accessible* acc = aAcc->FirstChild(); acc; acc = acc->FirstChild()) {
      mAcc = acc;
    }
    mOffset = 0;
    return;
  }
  mAcc = aAcc;
  mOffset = aOffset;
}

bool TextLeafPoint::operator<(const TextLeafPoint& aPoint) const {
  if (mAcc == aPoint.mAcc) {
    return mOffset < aPoint.mOffset;
  }

  // Build the chain of parents.
  Accessible* thisP = mAcc;
  Accessible* otherP = aPoint.mAcc;
  AutoTArray<Accessible*, 30> thisParents, otherParents;
  do {
    thisParents.AppendElement(thisP);
    thisP = thisP->Parent();
  } while (thisP);
  do {
    otherParents.AppendElement(otherP);
    otherP = otherP->Parent();
  } while (otherP);

  // Find where the parent chain differs.
  uint32_t thisPos = thisParents.Length(), otherPos = otherParents.Length();
  for (uint32_t len = std::min(thisPos, otherPos); len > 0; --len) {
    Accessible* thisChild = thisParents.ElementAt(--thisPos);
    Accessible* otherChild = otherParents.ElementAt(--otherPos);
    if (thisChild != otherChild) {
      return thisChild->IndexInParent() < otherChild->IndexInParent();
    }
  }

  // If the ancestries are the same length (both thisPos and otherPos are 0),
  // we should have returned by now.
  MOZ_ASSERT(thisPos != 0 || otherPos != 0);
  // At this point, one of the ancestries is a superset of the other, so one of
  // thisPos or otherPos should be 0.
  MOZ_ASSERT(thisPos != otherPos);
  // If the other Accessible is deeper than this one (otherPos > 0), this
  // Accessible comes before the other.
  return otherPos > 0;
}

}  // namespace mozilla::a11y

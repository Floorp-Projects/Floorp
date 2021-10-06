/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessibleBase.h"

#include "mozilla/a11y/Accessible.h"
#include "nsAccUtils.h"

namespace mozilla::a11y {

int32_t HyperTextAccessibleBase::GetChildIndexAtOffset(uint32_t aOffset) const {
  const Accessible* thisAcc = Acc();
  uint32_t childCount = thisAcc->ChildCount();
  uint32_t lastTextOffset = 0;
  for (uint32_t childIndex = 0; childIndex < childCount; ++childIndex) {
    Accessible* child = thisAcc->ChildAt(childIndex);
    lastTextOffset += nsAccUtils::TextLength(child);
    if (aOffset < lastTextOffset) {
      return childIndex;
    }
  }
  if (aOffset == lastTextOffset) {
    return childCount - 1;
  }
  return -1;
}

Accessible* HyperTextAccessibleBase::GetChildAtOffset(uint32_t aOffset) const {
  const Accessible* thisAcc = Acc();
  return thisAcc->ChildAt(GetChildIndexAtOffset(aOffset));
}

int32_t HyperTextAccessibleBase::GetChildOffset(const Accessible* aChild,
                                                bool aInvalidateAfter) const {
  const Accessible* thisAcc = Acc();
  if (aChild->Parent() != thisAcc) {
    return -1;
  }
  int32_t index = aChild->IndexInParent();
  if (index == -1) {
    return -1;
  }
  return GetChildOffset(index, aInvalidateAfter);
}

int32_t HyperTextAccessibleBase::GetChildOffset(uint32_t aChildIndex,
                                                bool aInvalidateAfter) const {
  if (aChildIndex == 0) {
    return 0;
  }
  const Accessible* thisAcc = Acc();
  MOZ_ASSERT(aChildIndex <= thisAcc->ChildCount());
  uint32_t lastTextOffset = 0;
  for (uint32_t childIndex = 0; childIndex <= aChildIndex; ++childIndex) {
    if (childIndex == aChildIndex) {
      return lastTextOffset;
    }
    Accessible* child = thisAcc->ChildAt(childIndex);
    lastTextOffset += nsAccUtils::TextLength(child);
  }
  MOZ_ASSERT_UNREACHABLE();
  return lastTextOffset;
}

}  // namespace mozilla::a11y

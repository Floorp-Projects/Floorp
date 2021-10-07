/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessibleBase.h"

#include "mozilla/a11y/Accessible.h"
#include "nsAccUtils.h"
#include "nsIAccessibleText.h"

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

uint32_t HyperTextAccessibleBase::CharacterCount() const {
  return GetChildOffset(Acc()->ChildCount());
}

index_t HyperTextAccessibleBase::ConvertMagicOffset(int32_t aOffset) const {
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT) {
    return CharacterCount();
  }

  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
    return CaretOffset();
  }

  return aOffset;
}

void HyperTextAccessibleBase::TextSubstring(int32_t aStartOffset,
                                            int32_t aEndOffset,
                                            nsAString& aText) const {
  aText.Truncate();

  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  if (!startOffset.IsValid() || !endOffset.IsValid() ||
      startOffset > endOffset || endOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset");
    return;
  }

  int32_t startChildIdx = GetChildIndexAtOffset(startOffset);
  if (startChildIdx == -1) {
    return;
  }

  int32_t endChildIdx = GetChildIndexAtOffset(endOffset);
  if (endChildIdx == -1) {
    return;
  }

  const Accessible* thisAcc = Acc();
  if (startChildIdx == endChildIdx) {
    int32_t childOffset = GetChildOffset(startChildIdx);
    if (childOffset == -1) {
      return;
    }

    Accessible* child = thisAcc->ChildAt(startChildIdx);
    child->AppendTextTo(aText, startOffset - childOffset,
                        endOffset - startOffset);
    return;
  }

  int32_t startChildOffset = GetChildOffset(startChildIdx);
  if (startChildOffset == -1) {
    return;
  }

  Accessible* startChild = thisAcc->ChildAt(startChildIdx);
  startChild->AppendTextTo(aText, startOffset - startChildOffset);

  for (int32_t childIdx = startChildIdx + 1; childIdx < endChildIdx;
       childIdx++) {
    Accessible* child = thisAcc->ChildAt(childIdx);
    child->AppendTextTo(aText);
  }

  int32_t endChildOffset = GetChildOffset(endChildIdx);
  if (endChildOffset == -1) {
    return;
  }

  Accessible* endChild = thisAcc->ChildAt(endChildIdx);
  endChild->AppendTextTo(aText, 0, endOffset - endChildOffset);
}

}  // namespace mozilla::a11y

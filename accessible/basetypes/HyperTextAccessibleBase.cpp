/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessibleBase.h"

#include "AccAttributes.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsAccUtils.h"
#include "TextLeafRange.h"

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

bool HyperTextAccessibleBase::CharAt(int32_t aOffset, nsAString& aChar,
                                     int32_t* aStartOffset,
                                     int32_t* aEndOffset) {
  MOZ_ASSERT(!aStartOffset == !aEndOffset,
             "Offsets should be both defined or both undefined!");

  int32_t childIdx = GetChildIndexAtOffset(aOffset);
  if (childIdx == -1) {
    return false;
  }

  Accessible* child = Acc()->ChildAt(childIdx);
  child->AppendTextTo(aChar, aOffset - GetChildOffset(childIdx), 1);

  if (aStartOffset && aEndOffset) {
    *aStartOffset = aOffset;
    *aEndOffset = aOffset + aChar.Length();
  }
  return true;
}

TextLeafPoint HyperTextAccessibleBase::ToTextLeafPoint(int32_t aOffset,
                                                       bool aDescendToEnd) {
  Accessible* thisAcc = Acc();
  if (!thisAcc->HasChildren()) {
    return TextLeafPoint(thisAcc, 0);
  }
  Accessible* child = GetChildAtOffset(aOffset);
  if (!child) {
    return TextLeafPoint();
  }
  if (HyperTextAccessibleBase* childHt = child->AsHyperTextBase()) {
    return childHt->ToTextLeafPoint(
        aDescendToEnd ? static_cast<int32_t>(childHt->CharacterCount()) : 0,
        aDescendToEnd);
  }
  int32_t offset = aOffset - GetChildOffset(child);
  return TextLeafPoint(child, offset);
}

std::pair<bool, int32_t> HyperTextAccessibleBase::TransformOffset(
    Accessible* aDescendant, int32_t aOffset, bool aIsEndOffset) const {
  const Accessible* thisAcc = Acc();
  // From the descendant, go up and get the immediate child of this hypertext.
  int32_t offset = aOffset;
  Accessible* descendant = aDescendant;
  while (descendant) {
    Accessible* parent = descendant->Parent();
    if (parent == thisAcc) {
      return {true, GetChildOffset(descendant) + offset};
    }

    // This offset no longer applies because the passed-in text object is not
    // a child of the hypertext. This happens when there are nested hypertexts,
    // e.g. <div>abc<h1>def</h1>ghi</div>. Thus we need to adjust the offset
    // to make it relative the hypertext.
    // If the end offset is not supposed to be inclusive and the original point
    // is not at 0 offset then the returned offset should be after an embedded
    // character the original point belongs to.
    if (aIsEndOffset) {
      offset = (offset > 0 || descendant->IndexInParent() > 0) ? 1 : 0;
    } else {
      offset = 0;
    }

    descendant = parent;
  }

  // The given a11y point cannot be mapped to an offset relative to this
  // hypertext accessible. Return the start or the end depending on whether this
  // is a start ofset or an end offset, thus clipping to the relevant endpoint.
  return {false, aIsEndOffset ? static_cast<int32_t>(CharacterCount()) : 0};
}

void HyperTextAccessibleBase::TextAtOffset(int32_t aOffset,
                                           AccessibleTextBoundary aBoundaryType,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset,
                                           nsAString& aText) {
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup());
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  uint32_t adjustedOffset = ConvertMagicOffset(aOffset);
  if (adjustedOffset == std::numeric_limits<uint32_t>::max()) {
    NS_ERROR("Wrong given offset!");
    return;
  }

  switch (aBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
        TextLeafPoint caret = TextLeafPoint::GetCaret(Acc());
        if (caret.IsCaretAtEndOfLine()) {
          // The caret is at the end of the line. Return no character.
          *aStartOffset = *aEndOffset = static_cast<int32_t>(adjustedOffset);
          return;
        }
      }
      CharAt(adjustedOffset, aText, aStartOffset, aEndOffset);
      break;
    case nsIAccessibleText::BOUNDARY_WORD_START:
    case nsIAccessibleText::BOUNDARY_LINE_START:
      TextLeafPoint origStart, end;
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
        origStart = end = TextLeafPoint::GetCaret(Acc());
      } else {
        origStart = ToTextLeafPoint(static_cast<int32_t>(adjustedOffset));
        Accessible* childAcc = GetChildAtOffset(adjustedOffset);
        if (childAcc && childAcc->IsHyperText()) {
          // We're searching for boundaries enclosing an embedded object.
          // An embedded object might contain several boundaries itself.
          // Thus, we must ensure we search for the end boundary from the last
          // text in the subtree, not just the first.
          // For example, if the embedded object is a link and it contains two
          // words, but the second word expands beyond the link, we want to
          // include the part of the second word which is outside of the link.
          end = ToTextLeafPoint(static_cast<int32_t>(adjustedOffset),
                                /* aDescendToEnd */ true);
        } else {
          end = origStart;
        }
      }
      TextLeafPoint start = origStart.FindBoundary(aBoundaryType, eDirPrevious,
                                                   /* aIncludeOrigin */ true);
      bool ok;
      std::tie(ok, *aStartOffset) = TransformOffset(start.mAcc, start.mOffset,
                                                    /* aIsEndOffset */ false);
      end = end.FindBoundary(aBoundaryType, eDirNext);
      std::tie(ok, *aEndOffset) = TransformOffset(end.mAcc, end.mOffset,
                                                  /* aIsEndOffset */ true);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      return;
  }
}

int32_t HyperTextAccessibleBase::CaretOffset() const {
  TextLeafPoint point = TextLeafPoint::GetCaret(const_cast<Accessible*>(Acc()))
                            .ActualizeCaret(/* aAdjustAtEndOfLine */ false);
  if (point.mOffset == 0 && point.mAcc == Acc()) {
    // If a text box is empty, there will be no children, so point.mAcc will be
    // this HyperText.
    return 0;
  }
  auto [ok, htOffset] =
      TransformOffset(point.mAcc, point.mOffset, /* aIsEndOffset */ false);
  if (!ok) {
    // The caret is not within this HyperText.
    return -1;
  }
  return htOffset;
}

bool HyperTextAccessibleBase::IsValidOffset(int32_t aOffset) {
  index_t offset = ConvertMagicOffset(aOffset);
  return offset.IsValid() && offset <= CharacterCount();
}

bool HyperTextAccessibleBase::IsValidRange(int32_t aStartOffset,
                                           int32_t aEndOffset) {
  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  return startOffset.IsValid() && endOffset.IsValid() &&
         startOffset <= endOffset && endOffset <= CharacterCount();
}

Accessible* HyperTextAccessibleBase::LinkAt(uint32_t aIndex) {
  return Acc()->EmbeddedChildAt(aIndex);
}

int32_t HyperTextAccessibleBase::LinkIndexOf(Accessible* aLink) {
  return Acc()->IndexOfEmbeddedChild(aLink);
}

already_AddRefed<AccAttributes> HyperTextAccessibleBase::TextAttributes(
    bool aIncludeDefAttrs, int32_t aOffset, int32_t* aStartOffset,
    int32_t* aEndOffset) {
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup());
  *aStartOffset = *aEndOffset = 0;
  index_t offset = ConvertMagicOffset(aOffset);
  if (!offset.IsValid() || offset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return RefPtr{new AccAttributes()}.forget();
  }

  Accessible* originAcc = GetChildAtOffset(offset);
  if (!originAcc) {
    // Offset 0 is correct offset when accessible has empty text. Include
    // default attributes if they were requested, otherwise return empty set.
    if (offset == 0) {
      if (aIncludeDefAttrs) {
        return DefaultTextAttributes();
      }
    }
    return RefPtr{new AccAttributes()}.forget();
  }

  if (!originAcc->IsText()) {
    // This is an embedded object. One or more consecutive embedded objects
    // form a single attrs run with no attributes.
    *aStartOffset = aOffset;
    *aEndOffset = aOffset + 1;
    Accessible* parent = originAcc->Parent();
    if (!parent) {
      return RefPtr{new AccAttributes()}.forget();
    }
    int32_t originIdx = originAcc->IndexInParent();
    if (originIdx > 0) {
      // Check for embedded objects before the origin.
      for (uint32_t idx = originIdx - 1;; --idx) {
        Accessible* sibling = parent->ChildAt(idx);
        if (sibling->IsText()) {
          break;
        }
        --*aStartOffset;
        if (idx == 0) {
          break;
        }
      }
    }
    // Check for embedded objects after the origin.
    for (uint32_t idx = originIdx + 1;; ++idx) {
      Accessible* sibling = parent->ChildAt(idx);
      if (!sibling || sibling->IsText()) {
        break;
      }
      ++*aEndOffset;
    }
    return RefPtr{new AccAttributes()}.forget();
  }

  TextLeafPoint origin = ToTextLeafPoint(static_cast<int32_t>(offset));
  RefPtr<AccAttributes> attributes = origin.GetTextAttributes(aIncludeDefAttrs);
  TextLeafPoint start = origin.FindTextAttrsStart(
      eDirPrevious, /* aIncludeOrigin */ true, attributes, aIncludeDefAttrs);
  bool ok;
  std::tie(ok, *aStartOffset) = TransformOffset(start.mAcc, start.mOffset,
                                                /* aIsEndOffset */ false);
  TextLeafPoint end = origin.FindTextAttrsStart(
      eDirNext, /* aIncludeOrigin */ false, attributes, aIncludeDefAttrs);
  std::tie(ok, *aEndOffset) = TransformOffset(end.mAcc, end.mOffset,
                                              /* aIsEndOffset */ true);
  return attributes.forget();
}

}  // namespace mozilla::a11y

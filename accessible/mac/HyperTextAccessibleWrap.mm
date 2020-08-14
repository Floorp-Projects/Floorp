/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessibleWrap.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsFrameSelection.h"
#include "TextRange.h"
#include "TreeWalker.h"

using namespace mozilla;
using namespace mozilla::a11y;

// HyperTextIterator

class HyperTextIterator {
 public:
  HyperTextIterator(HyperTextAccessible* aStartContainer, int32_t aStartOffset,
                    HyperTextAccessible* aEndContainer, int32_t aEndOffset)
      : mCurrentContainer(aStartContainer),
        mCurrentStartOffset(aStartOffset),
        mCurrentEndOffset(aStartOffset),
        mEndContainer(aEndContainer),
        mEndOffset(aEndOffset) {}

  bool Next();

  bool Normalize();

  HyperTextAccessible* mCurrentContainer;
  int32_t mCurrentStartOffset;
  int32_t mCurrentEndOffset;

 private:
  int32_t NextLinkOffset();

  HyperTextAccessible* mEndContainer;
  int32_t mEndOffset;
};

bool HyperTextIterator::Normalize() {
  if (mCurrentStartOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT ||
      mCurrentStartOffset >= static_cast<int32_t>(mCurrentContainer->CharacterCount())) {
    // If this is the end of the current container, mutate to its parent's
    // end offset.
    if (!mCurrentContainer->IsLink()) {
      // If we are not a link, it is a root hypertext accessible.
      return false;
    }
    uint32_t endOffset = mCurrentContainer->EndOffset();
    if (endOffset != 0) {
      Accessible* parent = mCurrentContainer->Parent();
      if (!parent->IsLink()) {
        // If the parent is the not a link, its a root doc.
        return false;
      }
      mCurrentContainer = parent->AsHyperText();
      mCurrentStartOffset = endOffset;

      // Call Normalize recursively to get top-most link if at the end of one,
      // or innermost link if at the beginning.
      Normalize();
      return true;
    }
  } else {
    Accessible* link =
        mCurrentContainer->LinkAt(mCurrentContainer->LinkIndexAtOffset(mCurrentStartOffset));

    // If there is a link at this offset, mutate into it.
    if (link && link->IsHyperText()) {
      mCurrentContainer = link->AsHyperText();
      mCurrentStartOffset = 0;

      // Call Normalize recursively to get top-most link if at the end of one,
      // or innermost link if at the beginning.
      Normalize();
      return true;
    }
  }

  return false;
}

int32_t HyperTextIterator::NextLinkOffset() {
  int32_t linkCount = mCurrentContainer->LinkCount();
  for (int32_t i = 0; i < linkCount; i++) {
    Accessible* link = mCurrentContainer->LinkAt(i);
    MOZ_ASSERT(link);
    int32_t linkStartOffset = link->StartOffset();
    if (mCurrentStartOffset < linkStartOffset) {
      return linkStartOffset;
    }
  }

  return -1;
}

bool HyperTextIterator::Next() {
  if (mCurrentContainer == mEndContainer &&
      (mCurrentEndOffset == -1 || mEndOffset <= mCurrentEndOffset)) {
    return false;
  } else {
    mCurrentStartOffset = mCurrentEndOffset;
    Normalize();
  }

  int32_t nextLinkOffset = NextLinkOffset();
  if (mCurrentContainer == mEndContainer && (nextLinkOffset == -1 || nextLinkOffset > mEndOffset)) {
    mCurrentEndOffset = mEndOffset < 0 ? mEndContainer->CharacterCount() : mEndOffset;
  } else {
    mCurrentEndOffset = nextLinkOffset < 0 ? mCurrentContainer->CharacterCount() : nextLinkOffset;
  }

  return mCurrentStartOffset != mCurrentEndOffset;
}

void HyperTextAccessibleWrap::TextForRange(nsAString& aText, int32_t aStartOffset,
                                           HyperTextAccessible* aEndContainer, int32_t aEndOffset) {
  HyperTextIterator iter(this, aStartOffset, aEndContainer, aEndOffset);
  while (iter.Next()) {
    nsAutoString text;
    iter.mCurrentContainer->TextSubstring(iter.mCurrentStartOffset, iter.mCurrentEndOffset, text);
    aText.Append(text);
  }
}

void HyperTextAccessibleWrap::LeftWordAt(int32_t aOffset, HyperTextAccessible** aStartContainer,
                                         int32_t* aStartOffset, HyperTextAccessible** aEndContainer,
                                         int32_t* aEndOffset) {
  TextPoint here(this, aOffset);
  TextPoint start = FindTextPoint(aOffset, eDirPrevious, eSelectWord, eStartWord);
  if (!start.mContainer) {
    return;
  }

  if ((NativeState() & states::EDITABLE) && !(start.mContainer->NativeState() & states::EDITABLE)) {
    // The word search crossed an editable boundary. Return the first word of the editable root.
    return EditableRoot()->RightWordAt(0, aStartContainer, aStartOffset, aEndContainer, aEndOffset);
  }

  TextPoint end = static_cast<HyperTextAccessibleWrap*>(start.mContainer)
                      ->FindTextPoint(start.mOffset, eDirNext, eSelectWord, eEndWord);
  if (end < here) {
    *aStartContainer = end.mContainer;
    *aEndContainer = here.mContainer;
    *aStartOffset = end.mOffset;
    *aEndOffset = here.mOffset;
  } else {
    *aStartContainer = start.mContainer;
    *aEndContainer = end.mContainer;
    *aStartOffset = start.mOffset;
    *aEndOffset = end.mOffset;
  }
}

void HyperTextAccessibleWrap::RightWordAt(int32_t aOffset, HyperTextAccessible** aStartContainer,
                                          int32_t* aStartOffset,
                                          HyperTextAccessible** aEndContainer,
                                          int32_t* aEndOffset) {
  TextPoint here(this, aOffset);
  TextPoint end = FindTextPoint(aOffset, eDirNext, eSelectWord, eEndWord);
  if (!end.mContainer) {
    return;
  }

  if ((NativeState() & states::EDITABLE) && !(end.mContainer->NativeState() & states::EDITABLE)) {
    // The word search crossed an editable boundary. Return the last word of the editable root.
    return EditableRoot()->LeftWordAt(nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT, aStartContainer,
                                      aStartOffset, aEndContainer, aEndOffset);
  }

  TextPoint start = static_cast<HyperTextAccessibleWrap*>(end.mContainer)
                        ->FindTextPoint(end.mOffset, eDirPrevious, eSelectWord, eStartWord);

  if (here < start) {
    *aStartContainer = here.mContainer;
    *aEndContainer = start.mContainer;
    *aStartOffset = here.mOffset;
    *aEndOffset = start.mOffset;
  } else {
    *aStartContainer = start.mContainer;
    *aEndContainer = end.mContainer;
    *aStartOffset = start.mOffset;
    *aEndOffset = end.mOffset;
  }
}

void HyperTextAccessibleWrap::NextClusterAt(int32_t aOffset, HyperTextAccessible** aNextContainer,
                                            int32_t* aNextOffset) {
  TextPoint next = FindTextPoint(aOffset, eDirNext, eSelectCluster, eDefaultBehavior);

  if (next.mOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT && next.mContainer == Document()) {
    *aNextContainer = this;
    *aNextOffset = aOffset;
  } else {
    *aNextContainer = next.mContainer;
    *aNextOffset = next.mOffset;
  }
}

void HyperTextAccessibleWrap::PreviousClusterAt(int32_t aOffset,
                                                HyperTextAccessible** aPrevContainer,
                                                int32_t* aPrevOffset) {
  TextPoint prev = FindTextPoint(aOffset, eDirPrevious, eSelectCluster, eDefaultBehavior);
  *aPrevContainer = prev.mContainer;
  *aPrevOffset = prev.mOffset;
}

TextPoint HyperTextAccessibleWrap::FindTextPoint(int32_t aOffset, nsDirection aDirection,
                                                 nsSelectionAmount aAmount,
                                                 EWordMovementType aWordMovementType) {
  // Layout can remain trapped in an editable. We normalize out of
  // it if we are in its last offset.
  HyperTextIterator iter(this, aOffset, this, CharacterCount());
  iter.Normalize();

  // Find a leaf accessible frame to start with. PeekOffset wants this.
  HyperTextAccessible* text = iter.mCurrentContainer;
  Accessible* child = nullptr;
  int32_t innerOffset = iter.mCurrentStartOffset;

  do {
    int32_t childIdx = text->GetChildIndexAtOffset(innerOffset);

    // We can have an empty text leaf as our only child. Since empty text
    // leaves are not accessible we then have no children, but 0 is a valid
    // innerOffset.
    if (childIdx == -1) {
      NS_ASSERTION(innerOffset == 0 && !text->ChildCount(), "No childIdx?");
      return TextPoint(text, 0);
    }

    child = text->GetChildAt(childIdx);

    innerOffset -= text->GetChildOffset(childIdx);

    text = child->AsHyperText();
  } while (text);

  nsIFrame* childFrame = child->GetFrame();
  if (!childFrame) {
    NS_ERROR("No child frame");
    return TextPoint(this, aOffset);
  }

  int32_t innerContentOffset = innerOffset;
  if (child->IsTextLeaf()) {
    NS_ASSERTION(childFrame->IsTextFrame(), "Wrong frame!");
    RenderedToContentOffset(childFrame, innerOffset, &innerContentOffset);
  }

  nsIFrame* frameAtOffset = childFrame;
  int32_t unusedOffsetInFrame = 0;
  childFrame->GetChildFrameContainingOffset(innerContentOffset, true, &unusedOffsetInFrame,
                                            &frameAtOffset);

  const bool kIsJumpLinesOk = true;       // okay to jump lines
  const bool kIsScrollViewAStop = false;  // do not stop at scroll views
  const bool kIsKeyboardSelect = true;    // is keyboard selection
  const bool kIsVisualBidi = false;       // use visual order for bidi text
  nsPeekOffsetStruct pos(aAmount, aDirection, innerContentOffset, nsPoint(0, 0), kIsJumpLinesOk,
                         kIsScrollViewAStop, kIsKeyboardSelect, kIsVisualBidi, false,
                         nsPeekOffsetStruct::ForceEditableRegion::No, aWordMovementType, false);
  nsresult rv = frameAtOffset->PeekOffset(&pos);

  // PeekOffset fails on last/first lines of the text in certain cases.
  if (NS_FAILED(rv) && aAmount == eSelectLine) {
    pos.mAmount = aDirection == eDirNext ? eSelectEndLine : eSelectBeginLine;
    frameAtOffset->PeekOffset(&pos);
  }
  if (!pos.mResultContent) {
    NS_ERROR("No result content!");
    return TextPoint(this, aOffset);
  }

  if (aDirection == eDirNext &&
      nsContentUtils::PositionIsBefore(pos.mResultContent, mContent, nullptr, nullptr)) {
    // Bug 1652833 makes us sometimes return the first element on the doc.
    return TextPoint(Document(), nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT);
  }

  HyperTextAccessible* container = nsAccUtils::GetTextContainer(pos.mResultContent);
  int32_t offset = container ? container->DOMPointToOffset(pos.mResultContent, pos.mContentOffset,
                                                           aDirection == eDirNext)
                             : 0;
  return TextPoint(container, offset);
}

HyperTextAccessibleWrap* HyperTextAccessibleWrap::EditableRoot() {
  Accessible* editable = nullptr;
  for (Accessible* acc = this; acc && acc != Document(); acc = acc->Parent()) {
    if (acc->NativeState() & states::EDITABLE) {
      editable = acc;
    } else {
      break;
    }
  }

  return static_cast<HyperTextAccessibleWrap*>(editable->AsHyperText());
}
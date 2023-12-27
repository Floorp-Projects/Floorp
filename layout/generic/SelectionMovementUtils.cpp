/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "SelectionMovementUtils.h"
#include "mozilla/CaretAssociationHint.h"
#include "mozilla/Maybe.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "nsBidiPresUtils.h"
#include "nsBlockFrame.h"
#include "nsCaret.h"
#include "nsCOMPtr.h"
#include "nsFrameSelection.h"
#include "nsFrameTraversal.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsTextFrame.h"

namespace mozilla {
using namespace dom;
// FYI: This was done during a call of GetPrimaryFrameForCaretAtFocusNode.
// Therefore, this may not be intended by the original author.

// static
Result<PeekOffsetStruct, nsresult>
SelectionMovementUtils::PeekOffsetForCaretMove(
    nsIContent* aContent, uint32_t aOffset, nsDirection aDirection,
    CaretAssociationHint aHint, intl::BidiEmbeddingLevel aCaretBidiLevel,
    const nsSelectionAmount aAmount, const nsPoint& aDesiredCaretPos,
    PeekOffsetOptions aOptions) {
  const PrimaryFrameData frameForFocus =
      SelectionMovementUtils::GetPrimaryFrameForCaret(
          aContent, aOffset, aOptions.contains(PeekOffsetOption::Visual), aHint,
          aCaretBidiLevel);
  if (!frameForFocus.mFrame) {
    return Err(NS_ERROR_FAILURE);
  }

  aOptions += {PeekOffsetOption::JumpLines, PeekOffsetOption::IsKeyboardSelect};
  PeekOffsetStruct pos(
      aAmount, aDirection,
      static_cast<int32_t>(frameForFocus.mOffsetInFrameContent),
      aDesiredCaretPos, aOptions);
  nsresult rv = frameForFocus.mFrame->PeekOffset(&pos);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  return pos;
}

// static
nsPrevNextBidiLevels SelectionMovementUtils::GetPrevNextBidiLevels(
    nsIContent* aNode, uint32_t aContentOffset, CaretAssociationHint aHint,
    bool aJumpLines) {
  // Get the level of the frames on each side
  nsIFrame* currentFrame;
  uint32_t currentOffset;
  nsDirection direction;

  nsPrevNextBidiLevels levels{};
  levels.SetData(nullptr, nullptr, intl::BidiEmbeddingLevel::LTR(),
                 intl::BidiEmbeddingLevel::LTR());

  currentFrame = SelectionMovementUtils::GetFrameForNodeOffset(
      aNode, aContentOffset, aHint, &currentOffset);
  if (!currentFrame) {
    return levels;
  }

  auto [frameStart, frameEnd] = currentFrame->GetOffsets();

  if (0 == frameStart && 0 == frameEnd) {
    direction = eDirPrevious;
  } else if (static_cast<uint32_t>(frameStart) == currentOffset) {
    direction = eDirPrevious;
  } else if (static_cast<uint32_t>(frameEnd) == currentOffset) {
    direction = eDirNext;
  } else {
    // we are neither at the beginning nor at the end of the frame, so we have
    // no worries
    intl::BidiEmbeddingLevel currentLevel = currentFrame->GetEmbeddingLevel();
    levels.SetData(currentFrame, currentFrame, currentLevel, currentLevel);
    return levels;
  }

  PeekOffsetOptions peekOffsetOptions{PeekOffsetOption::StopAtScroller};
  if (aJumpLines) {
    peekOffsetOptions += PeekOffsetOption::JumpLines;
  }
  nsIFrame* newFrame =
      currentFrame->GetFrameFromDirection(direction, peekOffsetOptions).mFrame;

  FrameBidiData currentBidi = currentFrame->GetBidiData();
  intl::BidiEmbeddingLevel currentLevel = currentBidi.embeddingLevel;
  intl::BidiEmbeddingLevel newLevel =
      newFrame ? newFrame->GetEmbeddingLevel() : currentBidi.baseLevel;

  // If not jumping lines, disregard br frames, since they might be positioned
  // incorrectly.
  // XXX This could be removed once bug 339786 is fixed.
  if (!aJumpLines) {
    if (currentFrame->IsBrFrame()) {
      currentFrame = nullptr;
      currentLevel = currentBidi.baseLevel;
    }
    if (newFrame && newFrame->IsBrFrame()) {
      newFrame = nullptr;
      newLevel = currentBidi.baseLevel;
    }
  }

  if (direction == eDirNext) {
    levels.SetData(currentFrame, newFrame, currentLevel, newLevel);
  } else {
    levels.SetData(newFrame, currentFrame, newLevel, currentLevel);
  }

  return levels;
}

// static
Result<nsIFrame*, nsresult> SelectionMovementUtils::GetFrameFromLevel(
    nsIFrame* aFrameIn, nsDirection aDirection,
    intl::BidiEmbeddingLevel aBidiLevel) {
  if (!aFrameIn) {
    return Err(NS_ERROR_NULL_POINTER);
  }

  intl::BidiEmbeddingLevel foundLevel = intl::BidiEmbeddingLevel::LTR();

  nsFrameIterator frameIterator(aFrameIn->PresContext(), aFrameIn,
                                nsFrameIterator::Type::Leaf,
                                false,  // aVisual
                                false,  // aLockInScrollView
                                false,  // aFollowOOFs
                                false   // aSkipPopupChecks
  );

  nsIFrame* foundFrame = aFrameIn;
  nsIFrame* theFrame = nullptr;
  do {
    theFrame = foundFrame;
    foundFrame = frameIterator.Traverse(aDirection == eDirNext);
    if (!foundFrame) {
      return Err(NS_ERROR_FAILURE);
    }
    foundLevel = foundFrame->GetEmbeddingLevel();

  } while (foundLevel > aBidiLevel);

  MOZ_ASSERT(theFrame);
  return theFrame;
}

bool SelectionMovementUtils::AdjustFrameForLineStart(nsIFrame*& aFrame,
                                                     uint32_t& aFrameOffset) {
  if (!aFrame->HasSignificantTerminalNewline()) {
    return false;
  }

  auto [start, end] = aFrame->GetOffsets();
  if (aFrameOffset != static_cast<uint32_t>(end)) {
    return false;
  }

  nsIFrame* nextSibling = aFrame->GetNextSibling();
  if (!nextSibling) {
    return false;
  }

  aFrame = nextSibling;
  std::tie(start, end) = aFrame->GetOffsets();
  aFrameOffset = start;
  return true;
}

static bool IsDisplayContents(const nsIContent* aContent) {
  return aContent->IsElement() && aContent->AsElement()->IsDisplayContents();
}

// static
nsIFrame* SelectionMovementUtils::GetFrameForNodeOffset(
    nsIContent* aNode, uint32_t aOffset, CaretAssociationHint aHint,
    uint32_t* aReturnOffset /* = nullptr */) {
  if (!aNode) {
    return nullptr;
  }

  if (static_cast<int32_t>(aOffset) < 0) {
    return nullptr;
  }

  if (!aNode->GetPrimaryFrame() && !IsDisplayContents(aNode)) {
    return nullptr;
  }

  nsIFrame* returnFrame = nullptr;
  nsCOMPtr<nsIContent> theNode;
  uint32_t offsetInFrameContent;

  while (true) {
    offsetInFrameContent = aOffset;

    theNode = aNode;

    if (aNode->IsElement()) {
      uint32_t childIndex = 0;
      uint32_t numChildren = theNode->GetChildCount();

      if (aHint == CaretAssociationHint::Before) {
        if (aOffset > 0) {
          childIndex = aOffset - 1;
        } else {
          childIndex = aOffset;
        }
      } else {
        MOZ_ASSERT(aHint == CaretAssociationHint::After);
        if (aOffset >= numChildren) {
          if (numChildren > 0) {
            childIndex = numChildren - 1;
          } else {
            childIndex = 0;
          }
        } else {
          childIndex = aOffset;
        }
      }

      if (childIndex > 0 || numChildren > 0) {
        nsCOMPtr<nsIContent> childNode =
            theNode->GetChildAt_Deprecated(childIndex);

        if (!childNode) {
          break;
        }

        theNode = childNode;
      }

      // Now that we have the child node, check if it too
      // can contain children. If so, descend into child.
      if (theNode->IsElement() && theNode->GetChildCount() &&
          !theNode->HasIndependentSelection()) {
        aNode = theNode;
        aOffset = aOffset > childIndex ? theNode->GetChildCount() : 0;
        continue;
      }
      // Check to see if theNode is a text node. If it is, translate
      // aOffset into an offset into the text node.

      RefPtr<Text> textNode = theNode->GetAsText();
      if (textNode) {
        if (theNode->GetPrimaryFrame()) {
          if (aOffset > childIndex) {
            uint32_t textLength = textNode->Length();

            offsetInFrameContent = textLength;
          } else {
            offsetInFrameContent = 0;
          }
        } else {
          uint32_t numChildren = aNode->GetChildCount();
          uint32_t newChildIndex = aHint == CaretAssociationHint::Before
                                       ? childIndex - 1
                                       : childIndex + 1;

          if (newChildIndex < numChildren) {
            nsCOMPtr<nsIContent> newChildNode =
                aNode->GetChildAt_Deprecated(newChildIndex);
            if (!newChildNode) {
              return nullptr;
            }

            aNode = newChildNode;
            aOffset = aHint == CaretAssociationHint::Before
                          ? aNode->GetChildCount()
                          : 0;
            continue;
          }  // newChildIndex is illegal which means we're at first or last
          // child. Just use original node to get the frame.
          theNode = aNode;
        }
      }
    }

    // If the node is a ShadowRoot, the frame needs to be adjusted,
    // because a ShadowRoot does not get a frame. Its children are rendered
    // as children of the host.
    if (ShadowRoot* shadow = ShadowRoot::FromNode(theNode)) {
      theNode = shadow->GetHost();
    }

    returnFrame = theNode->GetPrimaryFrame();
    if (!returnFrame) {
      if (aHint == CaretAssociationHint::Before) {
        if (aOffset > 0) {
          --aOffset;
          continue;
        }
        break;
      }
      if (aOffset < theNode->GetChildCount()) {
        ++aOffset;
        continue;
      }
      break;
    }

    break;
  }  // end while

  if (!returnFrame) {
    return nullptr;
  }

  // If we ended up here and were asked to position the caret after a visible
  // break, let's return the frame on the next line instead if it exists.
  if (aOffset > 0 && (uint32_t)aOffset >= aNode->Length() &&
      theNode == aNode->GetLastChild()) {
    nsIFrame* newFrame;
    nsLayoutUtils::IsInvisibleBreak(theNode, &newFrame);
    if (newFrame) {
      returnFrame = newFrame;
      offsetInFrameContent = 0;
    }
  }

  // find the child frame containing the offset we want
  int32_t unused = 0;
  returnFrame->GetChildFrameContainingOffset(
      static_cast<int32_t>(offsetInFrameContent),
      aHint == CaretAssociationHint::After, &unused, &returnFrame);
  if (aReturnOffset) {
    *aReturnOffset = offsetInFrameContent;
  }
  return returnFrame;
}

/**
 * Find the first frame in an in-order traversal of the frame subtree rooted
 * at aFrame which is either a text frame logically at the end of a line,
 * or which is aStopAtFrame. Return null if no such frame is found. We don't
 * descend into the children of non-eLineParticipant frames.
 */
static nsIFrame* CheckForTrailingTextFrameRecursive(nsIFrame* aFrame,
                                                    nsIFrame* aStopAtFrame) {
  if (aFrame == aStopAtFrame ||
      ((aFrame->IsTextFrame() &&
        (static_cast<nsTextFrame*>(aFrame))->IsAtEndOfLine()))) {
    return aFrame;
  }
  if (!aFrame->IsLineParticipant()) {
    return nullptr;
  }

  for (nsIFrame* f : aFrame->PrincipalChildList()) {
    if (nsIFrame* r = CheckForTrailingTextFrameRecursive(f, aStopAtFrame)) {
      return r;
    }
  }
  return nullptr;
}

static nsLineBox* FindContainingLine(nsIFrame* aFrame) {
  while (aFrame && aFrame->IsLineParticipant()) {
    nsIFrame* parent = aFrame->GetParent();
    nsBlockFrame* blockParent = do_QueryFrame(parent);
    if (blockParent) {
      bool isValid;
      nsBlockInFlowLineIterator iter(blockParent, aFrame, &isValid);
      return isValid ? iter.GetLine().get() : nullptr;
    }
    aFrame = parent;
  }
  return nullptr;
}

static void AdjustCaretFrameForLineEnd(nsIFrame** aFrame, uint32_t* aOffset,
                                       bool aEditableOnly) {
  nsLineBox* line = FindContainingLine(*aFrame);
  if (!line) {
    return;
  }
  uint32_t count = line->GetChildCount();
  for (nsIFrame* f = line->mFirstChild; count > 0;
       --count, f = f->GetNextSibling()) {
    nsIFrame* r = CheckForTrailingTextFrameRecursive(f, *aFrame);
    if (r == *aFrame) {
      return;
    }
    if (!r) {
      continue;
    }
    // If found text frame is non-editable but the start frame content is
    // editable, we don't want to put caret into the non-editable text node.
    // We should return the given frame as-is in this case.
    if (aEditableOnly && !r->GetContent()->IsEditable()) {
      return;
    }
    // We found our frame, but we may not be able to properly paint the caret
    // if -moz-user-modify differs from our actual frame.
    MOZ_ASSERT(r->IsTextFrame(), "Expected text frame");
    *aFrame = r;
    *aOffset = (static_cast<nsTextFrame*>(r))->GetContentEnd();
    return;
    // FYI: Setting the caret association hint was done during a call of
    // GetPrimaryFrameForCaretAtFocusNode.  Therefore, this may not be intended
    // by the original author.
  }
}

CaretFrameData SelectionMovementUtils::GetCaretFrameForNodeOffset(
    const nsFrameSelection* aFrameSelection, nsIContent* aContentNode,
    uint32_t aOffset, CaretAssociationHint aFrameHint,
    intl::BidiEmbeddingLevel aBidiLevel,
    ForceEditableRegion aForceEditableRegion) {
  if (!aContentNode || !aContentNode->IsInComposedDoc()) {
    return {};
  }

  CaretFrameData result;
  result.mHint = aFrameHint;
  if (aFrameSelection) {
    PresShell* presShell = aFrameSelection->GetPresShell();
    if (!presShell) {
      return {};
    }

    if (!aContentNode || !aContentNode->IsInComposedDoc() ||
        presShell->GetDocument() != aContentNode->GetComposedDoc()) {
      return {};
    }

    result.mHint = aFrameSelection->GetHint();
  }

  MOZ_ASSERT_IF(aForceEditableRegion == ForceEditableRegion::Yes,
                aContentNode->IsEditable());

  result.mFrame = result.mUnadjustedFrame =
      SelectionMovementUtils::GetFrameForNodeOffset(
          aContentNode, aOffset, aFrameHint, &result.mOffsetInFrameContent);
  if (!result.mFrame) {
    return {};
  }

  if (SelectionMovementUtils::AdjustFrameForLineStart(
          result.mFrame, result.mOffsetInFrameContent)) {
    result.mHint = CaretAssociationHint::After;
  } else {
    // if the frame is after a text frame that's logically at the end of the
    // line (e.g. if the frame is a <br> frame), then put the caret at the end
    // of that text frame instead. This way, the caret will be positioned as if
    // trailing whitespace was not trimmed.
    AdjustCaretFrameForLineEnd(
        &result.mFrame, &result.mOffsetInFrameContent,
        aForceEditableRegion == ForceEditableRegion::Yes);
  }

  // Mamdouh : modification of the caret to work at rtl and ltr with Bidi
  //
  // Direction Style from visibility->mDirection
  // ------------------
  if (!result.mFrame->PresContext()->BidiEnabled()) {
    return result;
  }

  // If there has been a reflow, take the caret Bidi level to be the level of
  // the current frame
  if (aBidiLevel & BIDI_LEVEL_UNDEFINED) {
    aBidiLevel = result.mFrame->GetEmbeddingLevel();
  }

  nsIFrame* frameBefore;
  nsIFrame* frameAfter;
  intl::BidiEmbeddingLevel
      levelBefore;  // Bidi level of the character before the caret
  intl::BidiEmbeddingLevel
      levelAfter;  // Bidi level of the character after the caret

  auto [start, end] = result.mFrame->GetOffsets();
  if (start == 0 || end == 0 ||
      static_cast<uint32_t>(start) == result.mOffsetInFrameContent ||
      static_cast<uint32_t>(end) == result.mOffsetInFrameContent) {
    nsPrevNextBidiLevels levels = SelectionMovementUtils::GetPrevNextBidiLevels(
        aContentNode, aOffset, result.mHint, false);

    /* Boundary condition, we need to know the Bidi levels of the characters
     * before and after the caret */
    if (levels.mFrameBefore || levels.mFrameAfter) {
      frameBefore = levels.mFrameBefore;
      frameAfter = levels.mFrameAfter;
      levelBefore = levels.mLevelBefore;
      levelAfter = levels.mLevelAfter;

      if ((levelBefore != levelAfter) || (aBidiLevel != levelBefore)) {
        aBidiLevel =
            std::max(aBidiLevel, std::min(levelBefore, levelAfter));  // rule c3
        aBidiLevel =
            std::min(aBidiLevel, std::max(levelBefore, levelAfter));  // rule c4
        if (aBidiLevel == levelBefore ||                              // rule c1
            (aBidiLevel > levelBefore && aBidiLevel < levelAfter &&
             aBidiLevel.IsSameDirection(levelBefore)) ||  // rule c5
            (aBidiLevel < levelBefore && aBidiLevel > levelAfter &&
             aBidiLevel.IsSameDirection(levelBefore)))  // rule c9
        {
          if (result.mFrame != frameBefore) {
            if (frameBefore) {  // if there is a frameBefore, move into it
              result.mFrame = frameBefore;
              std::tie(start, end) = result.mFrame->GetOffsets();
              result.mOffsetInFrameContent = end;
            } else {
              // if there is no frameBefore, we must be at the beginning of
              // the line so we stay with the current frame. Exception: when
              // the first frame on the line has a different Bidi level from
              // the paragraph level, there is no real frame for the caret to
              // be in. We have to find the visually first frame on the line.
              intl::BidiEmbeddingLevel baseLevel = frameAfter->GetBaseLevel();
              if (baseLevel != levelAfter) {
                PeekOffsetStruct pos(eSelectBeginLine, eDirPrevious, 0,
                                     nsPoint(0, 0),
                                     {PeekOffsetOption::StopAtScroller,
                                      PeekOffsetOption::Visual});
                if (NS_SUCCEEDED(frameAfter->PeekOffset(&pos))) {
                  result.mFrame = pos.mResultFrame;
                  result.mOffsetInFrameContent = pos.mContentOffset;
                }
              }
            }
          }
        } else if (aBidiLevel == levelAfter ||  // rule c2
                   (aBidiLevel > levelBefore && aBidiLevel < levelAfter &&
                    aBidiLevel.IsSameDirection(levelAfter)) ||  // rule c6
                   (aBidiLevel < levelBefore && aBidiLevel > levelAfter &&
                    aBidiLevel.IsSameDirection(levelAfter)))  // rule c10
        {
          if (result.mFrame != frameAfter) {
            if (frameAfter) {
              // if there is a frameAfter, move into it
              result.mFrame = frameAfter;
              std::tie(start, end) = result.mFrame->GetOffsets();
              result.mOffsetInFrameContent = start;
            } else {
              // if there is no frameAfter, we must be at the end of the line
              // so we stay with the current frame.
              // Exception: when the last frame on the line has a different
              // Bidi level from the paragraph level, there is no real frame
              // for the caret to be in. We have to find the visually last
              // frame on the line.
              intl::BidiEmbeddingLevel baseLevel = frameBefore->GetBaseLevel();
              if (baseLevel != levelBefore) {
                PeekOffsetStruct pos(eSelectEndLine, eDirNext, 0, nsPoint(0, 0),
                                     {PeekOffsetOption::StopAtScroller,
                                      PeekOffsetOption::Visual});
                if (NS_SUCCEEDED(frameBefore->PeekOffset(&pos))) {
                  result.mFrame = pos.mResultFrame;
                  result.mOffsetInFrameContent = pos.mContentOffset;
                }
              }
            }
          }
        } else if (aBidiLevel > levelBefore &&
                   aBidiLevel < levelAfter &&  // rule c7/8
                   // before and after have the same parity
                   levelBefore.IsSameDirection(levelAfter) &&
                   // caret has different parity
                   !aBidiLevel.IsSameDirection(levelAfter)) {
          MOZ_ASSERT_IF(aFrameSelection && aFrameSelection->GetPresShell(),
                        aFrameSelection->GetPresShell()->GetPresContext() ==
                            frameAfter->PresContext());
          Result<nsIFrame*, nsresult> frameOrError =
              SelectionMovementUtils::GetFrameFromLevel(frameAfter, eDirNext,
                                                        aBidiLevel);
          if (MOZ_LIKELY(frameOrError.isOk())) {
            result.mFrame = frameOrError.unwrap();
            std::tie(start, end) = result.mFrame->GetOffsets();
            levelAfter = result.mFrame->GetEmbeddingLevel();
            if (aBidiLevel.IsRTL()) {
              // c8: caret to the right of the rightmost character
              result.mOffsetInFrameContent = levelAfter.IsRTL() ? start : end;
            } else {
              // c7: caret to the left of the leftmost character
              result.mOffsetInFrameContent = levelAfter.IsRTL() ? end : start;
            }
          }
        } else if (aBidiLevel < levelBefore &&
                   aBidiLevel > levelAfter &&  // rule c11/12
                   // before and after have the same parity
                   levelBefore.IsSameDirection(levelAfter) &&
                   // caret has different parity
                   !aBidiLevel.IsSameDirection(levelAfter)) {
          MOZ_ASSERT_IF(aFrameSelection && aFrameSelection->GetPresShell(),
                        aFrameSelection->GetPresShell()->GetPresContext() ==
                            frameBefore->PresContext());
          Result<nsIFrame*, nsresult> frameOrError =
              SelectionMovementUtils::GetFrameFromLevel(
                  frameBefore, eDirPrevious, aBidiLevel);
          if (MOZ_LIKELY(frameOrError.isOk())) {
            result.mFrame = frameOrError.unwrap();
            std::tie(start, end) = result.mFrame->GetOffsets();
            levelBefore = result.mFrame->GetEmbeddingLevel();
            if (aBidiLevel.IsRTL()) {
              // c12: caret to the left of the leftmost character
              result.mOffsetInFrameContent = levelBefore.IsRTL() ? end : start;
            } else {
              // c11: caret to the right of the rightmost character
              result.mOffsetInFrameContent = levelBefore.IsRTL() ? start : end;
            }
          }
        }
      }
    }
  }

  return result;
}

// static
PrimaryFrameData SelectionMovementUtils::GetPrimaryFrameForCaret(
    nsIContent* aContent, uint32_t aOffset, bool aVisual,
    CaretAssociationHint aHint, intl::BidiEmbeddingLevel aCaretBidiLevel) {
  MOZ_ASSERT(aContent);

  {
    const PrimaryFrameData result =
        SelectionMovementUtils::GetPrimaryOrCaretFrameForNodeOffset(
            aContent, aOffset, aVisual, aHint, aCaretBidiLevel);
    if (result.mFrame) {
      return result;
    }
  }

  // If aContent is whitespace only, we promote focus node to parent because
  // whitespace only node might have no frame.

  if (!aContent->TextIsOnlyWhitespace()) {
    return {};
  }

  nsIContent* parent = aContent->GetParent();
  if (NS_WARN_IF(!parent)) {
    return {};
  }
  const Maybe<uint32_t> offset = parent->ComputeIndexOf(aContent);
  if (NS_WARN_IF(offset.isNothing())) {
    return {};
  }
  return SelectionMovementUtils::GetPrimaryOrCaretFrameForNodeOffset(
      parent, *offset, aVisual, aHint, aCaretBidiLevel);
}

// static
PrimaryFrameData SelectionMovementUtils::GetPrimaryOrCaretFrameForNodeOffset(
    nsIContent* aContent, uint32_t aOffset, bool aVisual,
    CaretAssociationHint aHint, intl::BidiEmbeddingLevel aCaretBidiLevel) {
  if (aVisual) {
    const CaretFrameData result =
        SelectionMovementUtils::GetCaretFrameForNodeOffset(
            nullptr, aContent, aOffset, aHint, aCaretBidiLevel,
            aContent && aContent->IsEditable() ? ForceEditableRegion::Yes
                                               : ForceEditableRegion::No);
    return {result.mFrame, result.mOffsetInFrameContent, result.mHint};
  }

  uint32_t offset = 0;
  nsIFrame* theFrame = SelectionMovementUtils::GetFrameForNodeOffset(
      aContent, aOffset, aHint, &offset);
  return {theFrame, offset, aHint};
}

}  // namespace mozilla

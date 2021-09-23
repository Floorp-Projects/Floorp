/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextLeafRange.h"

#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/a11y/LocalAccessible.h"
#include "nsAccUtils.h"
#include "nsContentUtils.h"
#include "nsIAccessiblePivot.h"
#include "nsILineIterator.h"
#include "nsTArray.h"
#include "nsTextFrame.h"
#include "Pivot.h"

namespace mozilla::a11y {

/*** Helpers ***/

/**
 * These two functions convert between rendered and content text offsets.
 * When text DOM nodes are rendered, the rendered text often does not contain
 * all the whitespace from the source. For example, by default, the text
 * "a   b" will be rendered as "a b"; i.e. multiple spaces are compressed to
 * one. TextLeafAccessibles contain rendered text, but when we query layout, we
 * need to provide offsets into the original content text. Similarly, layout
 * returns content offsets, but we need to convert them to rendered offsets to
 * map them to TextLeafAccessibles.
 */

static int32_t RenderedToContentOffset(LocalAccessible* aAcc,
                                       uint32_t aRenderedOffset) {
  if (aAcc->LocalParent() && aAcc->LocalParent()->IsTextField()) {
    return static_cast<int32_t>(aRenderedOffset);
  }

  nsIFrame* frame = aAcc->GetFrame();
  MOZ_ASSERT(frame && frame->IsTextFrame());

  nsIFrame::RenderedText text =
      frame->GetRenderedText(aRenderedOffset, aRenderedOffset + 1,
                             nsIFrame::TextOffsetType::OffsetsInRenderedText,
                             nsIFrame::TrailingWhitespace::DontTrim);
  return text.mOffsetWithinNodeText;
}

static uint32_t ContentToRenderedOffset(LocalAccessible* aAcc,
                                        int32_t aContentOffset) {
  if (aAcc->LocalParent() && aAcc->LocalParent()->IsTextField()) {
    return aContentOffset;
  }

  nsIFrame* frame = aAcc->GetFrame();
  MOZ_ASSERT(frame && frame->IsTextFrame());

  nsIFrame::RenderedText text =
      frame->GetRenderedText(aContentOffset, aContentOffset + 1,
                             nsIFrame::TextOffsetType::OffsetsInContentText,
                             nsIFrame::TrailingWhitespace::DontTrim);
  return text.mOffsetWithinNodeRenderedText;
}

class LeafRule : public PivotRule {
 public:
  virtual uint16_t Match(Accessible* aAcc) override {
    if (aAcc->IsOuterDoc()) {
      return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }
    // We deliberately include Accessibles such as empty input elements and
    // empty containers, as these can be at the start of a line.
    if (!aAcc->HasChildren()) {
      return nsIAccessibleTraversalRule::FILTER_MATCH;
    }
    return nsIAccessibleTraversalRule::FILTER_IGNORE;
  }
};

static Accessible* NextLeaf(Accessible* aOrigin) {
  DocAccessible* doc = aOrigin->AsLocal()->Document();
  Pivot pivot(doc);
  auto rule = LeafRule();
  return pivot.Next(aOrigin, rule);
}

static Accessible* PrevLeaf(Accessible* aOrigin) {
  DocAccessible* doc = aOrigin->AsLocal()->Document();
  Pivot pivot(doc);
  auto rule = LeafRule();
  return pivot.Prev(aOrigin, rule);
}

static bool IsLocalAccAtLineStart(LocalAccessible* aAcc) {
  if (aAcc->NativeRole() == roles::LISTITEM_MARKER) {
    // A bullet always starts a line.
    return true;
  }
  // Splitting of content across lines is handled by layout.
  // nsIFrame::IsLogicallyAtLineEdge queries whether a frame is the first frame
  // on its line. However, we can't use that because the first frame on a line
  // might not be included in the a11y tree; e.g. an empty span, or space
  // in the DOM after a line break which is stripped when rendered. Instead, we
  // get the line number for this Accessible's frame and the line number for the
  // previous leaf Accessible's frame and compare them.
  Accessible* prev = PrevLeaf(aAcc);
  LocalAccessible* prevLocal = prev ? prev->AsLocal() : nullptr;
  if (!prevLocal) {
    // There's nothing before us, so this is the start of the first line.
    return true;
  }
  if (prevLocal->NativeRole() == roles::LISTITEM_MARKER) {
    // If there is  a bullet immediately before us and we're inside the same
    // list item, this is not the start of a line.
    LocalAccessible* listItem = prevLocal->LocalParent();
    MOZ_ASSERT(listItem);
    LocalAccessible* doc = listItem->Document();
    MOZ_ASSERT(doc);
    for (LocalAccessible* parent = aAcc->LocalParent(); parent && parent != doc;
         parent = parent->LocalParent()) {
      if (parent == listItem) {
        return false;
      }
    }
  }
  nsIFrame* thisFrame = aAcc->GetFrame();
  if (!thisFrame) {
    return false;
  }
  // Even though we have a leaf Accessible, there might be a child frame; e.g.
  // an empty input element is a leaf Accessible but has an empty text frame
  // child. To get a line number, we need a leaf frame.
  nsIFrame::GetFirstLeaf(&thisFrame);
  nsIFrame* prevFrame = prevLocal->GetFrame();
  if (!prevFrame) {
    return false;
  }
  nsIFrame::GetLastLeaf(&prevFrame);
  nsIFrame* thisLineFrame = nullptr;
  nsIFrame* thisBlock = thisFrame->GetContainingBlockForLine(
      /* aLockScroll */ false, thisLineFrame);
  if (!thisBlock) {
    // We couldn't get the containing block for this frame. In that case, we
    // play it safe and assume this is the beginning of a new line.
    return true;
  }
  nsIFrame* prevLineFrame = nullptr;
  nsIFrame* prevBlock = prevFrame->GetContainingBlockForLine(
      /* aLockScroll */ false, prevLineFrame);
  if (thisBlock != prevBlock) {
    // If the blocks are different, that means there's nothing before us on the
    // same line, so we're at the start.
    return true;
  }
  nsAutoLineIterator it = thisBlock->GetLineIterator();
  if (!it) {
    // Error; play it safe.
    return true;
  }
  int32_t thisLineNum = it->FindLineContaining(thisLineFrame);
  if (thisLineNum < 0) {
    // Error; play it safe.
    return true;
  }
  int32_t prevLineNum = it->FindLineContaining(prevLineFrame);
  // if the blocks and line numbers are different, that means there's nothing
  // before us on the same line, so we're at the start.
  return thisLineNum != prevLineNum;
}

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

bool TextLeafPoint::IsEmptyLastLine() const {
  if (mAcc->IsHTMLBr() && mOffset == 1) {
    return true;
  }
  if (!mAcc->IsTextLeaf()) {
    return false;
  }
  LocalAccessible* localAcc = mAcc->AsLocal();
  MOZ_ASSERT(localAcc);
  if (mOffset < static_cast<int32_t>(nsAccUtils::TextLength(localAcc))) {
    return false;
  }
  nsAutoString text;
  mAcc->AsLocal()->AppendTextTo(text, mOffset - 1, 1);
  return text.CharAt(0) == '\n';
}

TextLeafPoint TextLeafPoint::FindPrevLineStartSameLocalAcc(
    bool aIncludeOrigin) const {
  LocalAccessible* acc = mAcc->AsLocal();
  MOZ_ASSERT(acc);
  if (mOffset == 0) {
    if (aIncludeOrigin && IsLocalAccAtLineStart(acc)) {
      return *this;
    }
    return TextLeafPoint();
  }
  nsIFrame* frame = acc->GetFrame();
  if (!frame) {
    MOZ_ASSERT_UNREACHABLE("No frame");
    return TextLeafPoint();
  }
  if (!frame->IsTextFrame()) {
    if (IsLocalAccAtLineStart(acc)) {
      return TextLeafPoint(acc, 0);
    }
    return TextLeafPoint();
  }
  // Each line of a text node is rendered as a continuation frame. Get the
  // continuation containing the origin.
  int32_t origOffset = mOffset;
  origOffset = RenderedToContentOffset(acc, origOffset);
  nsTextFrame* continuation = nullptr;
  int32_t unusedOffsetInContinuation = 0;
  frame->GetChildFrameContainingOffset(
      origOffset, true, &unusedOffsetInContinuation, (nsIFrame**)&continuation);
  MOZ_ASSERT(continuation);
  int32_t lineStart = continuation->GetContentOffset();
  if (!aIncludeOrigin && lineStart > 0 && lineStart == origOffset) {
    // A line starts at the origin, but the caller doesn't want this included.
    // Go back one more.
    continuation = continuation->GetPrevContinuation();
    MOZ_ASSERT(continuation);
    lineStart = continuation->GetContentOffset();
  }
  MOZ_ASSERT(lineStart >= 0);
  if (lineStart == 0 && !IsLocalAccAtLineStart(acc)) {
    // This is the first line of this text node, but there is something else
    // on the same line before this text node, so don't return this as a line
    // start.
    return TextLeafPoint();
  }
  lineStart = static_cast<int32_t>(ContentToRenderedOffset(acc, lineStart));
  return TextLeafPoint(acc, lineStart);
}

TextLeafPoint TextLeafPoint::FindNextLineStartSameLocalAcc(
    bool aIncludeOrigin) const {
  LocalAccessible* acc = mAcc->AsLocal();
  MOZ_ASSERT(acc);
  if (aIncludeOrigin && mOffset == 0 && IsLocalAccAtLineStart(acc)) {
    return *this;
  }
  nsIFrame* frame = acc->GetFrame();
  if (!frame) {
    MOZ_ASSERT_UNREACHABLE("No frame");
    return TextLeafPoint();
  }
  if (!frame->IsTextFrame()) {
    // There can't be multiple lines in a non-text leaf.
    return TextLeafPoint();
  }
  // Each line of a text node is rendered as a continuation frame. Get the
  // continuation containing the origin.
  int32_t origOffset = mOffset;
  origOffset = RenderedToContentOffset(acc, origOffset);
  nsTextFrame* continuation = nullptr;
  int32_t unusedOffsetInContinuation = 0;
  frame->GetChildFrameContainingOffset(
      origOffset, true, &unusedOffsetInContinuation, (nsIFrame**)&continuation);
  MOZ_ASSERT(continuation);
  if (
      // A line starts at the origin and the caller wants this included.
      aIncludeOrigin && continuation->GetContentOffset() == origOffset &&
      // If this is the first line of this text node (offset 0), don't treat it
      // as a line start if there's something else on the line before this text
      // node.
      !(origOffset == 0 && !IsLocalAccAtLineStart(acc))) {
    return *this;
  }
  continuation = continuation->GetNextContinuation();
  if (!continuation) {
    return TextLeafPoint();
  }
  int32_t lineStart = continuation->GetContentOffset();
  lineStart = static_cast<int32_t>(ContentToRenderedOffset(acc, lineStart));
  return TextLeafPoint(acc, lineStart);
}

TextLeafPoint TextLeafPoint::FindBoundary(AccessibleTextBoundary aBoundaryType,
                                          nsDirection aDirection,
                                          bool aIncludeOrigin) const {
  // XXX Add handling for caret at end of wrapped line.
  if (aBoundaryType == nsIAccessibleText::BOUNDARY_LINE_START &&
      aIncludeOrigin && aDirection == eDirPrevious && IsEmptyLastLine()) {
    // If we're at an empty line at the end of an Accessible,  we don't want to
    // walk into the previous line. For example, this can happen if the caret
    // is positioned on an empty line at the end of a textarea.
    return *this;
  }
  TextLeafPoint searchFrom = *this;
  bool includeOrigin = aIncludeOrigin;
  for (;;) {
    TextLeafPoint boundary;
    // Search for the boundary within the current Accessible.
    switch (aBoundaryType) {
      case nsIAccessibleText::BOUNDARY_LINE_START:
        if (aDirection == eDirPrevious) {
          boundary = searchFrom.FindPrevLineStartSameLocalAcc(includeOrigin);
        } else {
          boundary = searchFrom.FindNextLineStartSameLocalAcc(includeOrigin);
        }
        break;
      default:
        MOZ_ASSERT_UNREACHABLE();
        break;
    }
    if (boundary) {
      return boundary;
    }
    // We didn't find it in this Accessible, so try the previous/next leaf.
    Accessible* acc = aDirection == eDirPrevious ? PrevLeaf(searchFrom.mAcc)
                                                 : NextLeaf(searchFrom.mAcc);
    if (!acc) {
      // No further leaf was found. Use the start/end of the first/last leaf.
      return TextLeafPoint(searchFrom.mAcc,
                           aDirection == eDirPrevious
                               ? 0
                               : static_cast<int32_t>(nsAccUtils::TextLength(
                                     searchFrom.mAcc->AsLocal())));
    }
    searchFrom.mAcc = acc;
    // When searching backward, search from the end of the text in the
    // Accessible. When searching forward, search from the start of the text.
    searchFrom.mOffset =
        aDirection == eDirPrevious
            ? static_cast<int32_t>(nsAccUtils::TextLength(acc->AsLocal()))
            : 0;
    // The start/end of the Accessible might be a boundary. If so, we must stop
    // on it.
    includeOrigin = true;
  }
  MOZ_ASSERT_UNREACHABLE();
  return TextLeafPoint();
}

}  // namespace mozilla::a11y

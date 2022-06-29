/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessible-inl.h"

#include "nsAccessibilityService.h"
#include "nsAccessiblePivot.h"
#include "nsIAccessibleTypes.h"
#include "AccAttributes.h"
#include "DocAccessible.h"
#include "HTMLListAccessible.h"
#include "LocalAccessible-inl.h"
#include "Pivot.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "TextAttrs.h"
#include "TextLeafRange.h"
#include "TextRange.h"
#include "TreeWalker.h"

#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsFocusManager.h"
#include "nsIEditingSession.h"
#include "nsContainerFrame.h"
#include "nsFrameSelection.h"
#include "nsILineIterator.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIMathMLFrame.h"
#include "nsRange.h"
#include "nsTextFragment.h"
#include "mozilla/Assertions.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/EditorBase.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/HTMLHeadingElement.h"
#include "mozilla/dom/Selection.h"
#include "gfxSkipChars.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::a11y;

/**
 * This class is used in HyperTextAccessible to search for paragraph
 * boundaries.
 */
class ParagraphBoundaryRule : public PivotRule {
 public:
  explicit ParagraphBoundaryRule(LocalAccessible* aAnchor,
                                 uint32_t aAnchorTextoffset,
                                 nsDirection aDirection,
                                 bool aSkipAnchorSubtree = false)
      : mAnchor(aAnchor),
        mAnchorTextOffset(aAnchorTextoffset),
        mDirection(aDirection),
        mSkipAnchorSubtree(aSkipAnchorSubtree),
        mLastMatchTextOffset(0) {}

  virtual uint16_t Match(Accessible* aAcc) override {
    MOZ_ASSERT(aAcc && aAcc->IsLocal());
    LocalAccessible* acc = aAcc->AsLocal();
    if (acc->IsOuterDoc()) {
      // The child document might be remote and we can't (and don't want to)
      // handle remote documents. Also, iframes are inline anyway and thus
      // can't be paragraph boundaries. Therefore, skip this unconditionally.
      return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }

    uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;
    if (mSkipAnchorSubtree && acc == mAnchor) {
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }

    // First, deal with the case that we encountered a line break, for example,
    // a br in a paragraph.
    if (acc->Role() == roles::WHITESPACE) {
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
      return result;
    }

    // Now, deal with the case that we encounter a new block level accessible.
    // This also means a new paragraph boundary start.
    nsIFrame* frame = acc->GetFrame();
    if (frame && frame->IsBlockFrame()) {
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
      return result;
    }

    // A text leaf can contain a line break if it's pre-formatted text.
    if (acc->IsTextLeaf()) {
      nsAutoString name;
      acc->Name(name);
      int32_t offset;
      if (mDirection == eDirPrevious) {
        if (acc == mAnchor && mAnchorTextOffset == 0) {
          // We're already at the start of this node, so there can be no line
          // break before.
          return result;
        }
        // If we began on a line break, we don't want to match it, so search
        // from 1 before our anchor offset.
        offset =
            name.RFindChar('\n', acc == mAnchor ? mAnchorTextOffset - 1 : -1);
      } else {
        offset = name.FindChar('\n', acc == mAnchor ? mAnchorTextOffset : 0);
      }
      if (offset != -1) {
        // Line ebreak!
        mLastMatchTextOffset = offset;
        result |= nsIAccessibleTraversalRule::FILTER_MATCH;
      }
    }

    return result;
  }

  // This is only valid if the last match was a text leaf. It returns the
  // offset of the line break character in that text leaf.
  uint32_t GetLastMatchTextOffset() { return mLastMatchTextOffset; }

 private:
  LocalAccessible* mAnchor;
  uint32_t mAnchorTextOffset;
  nsDirection mDirection;
  bool mSkipAnchorSubtree;
  uint32_t mLastMatchTextOffset;
};

/**
 * This class is used in HyperTextAccessible::FindParagraphStartOffset to
 * search forward exactly one step from a match found by the above.
 * It should only be initialized with a boundary, and it will skip that
 * boundary's sub tree if it is a block element boundary.
 */
class SkipParagraphBoundaryRule : public PivotRule {
 public:
  explicit SkipParagraphBoundaryRule(Accessible* aBoundary)
      : mBoundary(aBoundary) {}

  virtual uint16_t Match(Accessible* aAcc) override {
    MOZ_ASSERT(aAcc && aAcc->IsLocal());
    // If matching the boundary, skip its sub tree.
    if (aAcc == mBoundary) {
      return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }
    return nsIAccessibleTraversalRule::FILTER_MATCH;
  }

 private:
  Accessible* mBoundary;
};

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible
////////////////////////////////////////////////////////////////////////////////

HyperTextAccessible::HyperTextAccessible(nsIContent* aNode, DocAccessible* aDoc)
    : AccessibleWrap(aNode, aDoc) {
  mType = eHyperTextType;
  mGenericTypes |= eHyperText;
}

role HyperTextAccessible::NativeRole() const {
  a11y::role r = GetAccService()->MarkupRole(mContent);
  if (r != roles::NOTHING) return r;

  nsIFrame* frame = GetFrame();
  if (frame && frame->IsInlineFrame()) return roles::TEXT;

  return roles::TEXT_CONTAINER;
}

uint64_t HyperTextAccessible::NativeState() const {
  uint64_t states = AccessibleWrap::NativeState();

  if (mContent->AsElement()->State().HasState(dom::ElementState::READWRITE)) {
    states |= states::EDITABLE;

  } else if (mContent->IsHTMLElement(nsGkAtoms::article)) {
    // We want <article> to behave like a document in terms of readonly state.
    states |= states::READONLY;
  }

  nsIFrame* frame = GetFrame();
  if ((states & states::EDITABLE) || (frame && frame->IsSelectable(nullptr))) {
    // If the accessible is editable the layout selectable state only disables
    // mouse selection, but keyboard (shift+arrow) selection is still possible.
    states |= states::SELECTABLE_TEXT;
  }

  return states;
}

LayoutDeviceIntRect HyperTextAccessible::GetBoundsInFrame(
    nsIFrame* aFrame, uint32_t aStartRenderedOffset,
    uint32_t aEndRenderedOffset) {
  nsPresContext* presContext = mDoc->PresContext();
  if (!aFrame->IsTextFrame()) {
    return LayoutDeviceIntRect::FromAppUnitsToNearest(
        aFrame->GetScreenRectInAppUnits(), presContext->AppUnitsPerDevPixel());
  }

  // Substring must be entirely within the same text node.
  int32_t startContentOffset, endContentOffset;
  nsresult rv = RenderedToContentOffset(aFrame, aStartRenderedOffset,
                                        &startContentOffset);
  NS_ENSURE_SUCCESS(rv, LayoutDeviceIntRect());
  rv = RenderedToContentOffset(aFrame, aEndRenderedOffset, &endContentOffset);
  NS_ENSURE_SUCCESS(rv, LayoutDeviceIntRect());

  nsIFrame* frame;
  int32_t startContentOffsetInFrame;
  // Get the right frame continuation -- not really a child, but a sibling of
  // the primary frame passed in
  rv = aFrame->GetChildFrameContainingOffset(
      startContentOffset, false, &startContentOffsetInFrame, &frame);
  NS_ENSURE_SUCCESS(rv, LayoutDeviceIntRect());

  nsRect screenRect;
  while (frame && startContentOffset < endContentOffset) {
    // Start with this frame's screen rect, which we will shrink based on
    // the substring we care about within it. We will then add that frame to
    // the total screenRect we are returning.
    nsRect frameScreenRect = frame->GetScreenRectInAppUnits();

    // Get the length of the substring in this frame that we want the bounds for
    auto [startFrameTextOffset, endFrameTextOffset] = frame->GetOffsets();
    int32_t frameTotalTextLength = endFrameTextOffset - startFrameTextOffset;
    int32_t seekLength = endContentOffset - startContentOffset;
    int32_t frameSubStringLength =
        std::min(frameTotalTextLength - startContentOffsetInFrame, seekLength);

    // Add the point where the string starts to the frameScreenRect
    nsPoint frameTextStartPoint;
    rv = frame->GetPointFromOffset(startContentOffset, &frameTextStartPoint);
    NS_ENSURE_SUCCESS(rv, LayoutDeviceIntRect());

    // Use the point for the end offset to calculate the width
    nsPoint frameTextEndPoint;
    rv = frame->GetPointFromOffset(startContentOffset + frameSubStringLength,
                                   &frameTextEndPoint);
    NS_ENSURE_SUCCESS(rv, LayoutDeviceIntRect());

    frameScreenRect.SetRectX(
        frameScreenRect.X() +
            std::min(frameTextStartPoint.x, frameTextEndPoint.x),
        mozilla::Abs(frameTextStartPoint.x - frameTextEndPoint.x));

    screenRect.UnionRect(frameScreenRect, screenRect);

    // Get ready to loop back for next frame continuation
    startContentOffset += frameSubStringLength;
    startContentOffsetInFrame = 0;
    frame = frame->GetNextContinuation();
  }

  return LayoutDeviceIntRect::FromAppUnitsToNearest(
      screenRect, presContext->AppUnitsPerDevPixel());
}

uint32_t HyperTextAccessible::DOMPointToOffset(nsINode* aNode,
                                               int32_t aNodeOffset,
                                               bool aIsEndOffset) const {
  if (!aNode) return 0;

  uint32_t offset = 0;
  nsINode* findNode = nullptr;

  if (aNodeOffset == -1) {
    findNode = aNode;

  } else if (aNode->IsText()) {
    // For text nodes, aNodeOffset comes in as a character offset
    // Text offset will be added at the end, if we find the offset in this
    // hypertext We want the "skipped" offset into the text (rendered text
    // without the extra whitespace)
    nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, 0);

    nsresult rv = ContentToRenderedOffset(frame, aNodeOffset, &offset);
    NS_ENSURE_SUCCESS(rv, 0);

    findNode = aNode;

  } else {
    // findNode could be null if aNodeOffset == # of child nodes, which means
    // one of two things:
    // 1) there are no children, and the passed-in node is not mContent -- use
    //    parentContent for the node to find
    // 2) there are no children and the passed-in node is mContent, which means
    //    we're an empty nsIAccessibleText
    // 3) there are children and we're at the end of the children

    findNode = aNode->GetChildAt_Deprecated(aNodeOffset);
    if (!findNode) {
      if (aNodeOffset == 0) {
        if (aNode == GetNode()) {
          // Case #1: this accessible has no children and thus has empty text,
          // we can only be at hypertext offset 0.
          return 0;
        }

        // Case #2: there are no children, we're at this node.
        findNode = aNode;
      } else if (aNodeOffset == static_cast<int32_t>(aNode->GetChildCount())) {
        // Case #3: we're after the last child, get next node to this one.
        for (nsINode* tmpNode = aNode;
             !findNode && tmpNode && tmpNode != mContent;
             tmpNode = tmpNode->GetParent()) {
          findNode = tmpNode->GetNextSibling();
        }
      }
    }
  }

  // Get accessible for this findNode, or if that node isn't accessible, use the
  // accessible for the next DOM node which has one (based on forward depth
  // first search)
  LocalAccessible* descendant = nullptr;
  if (findNode) {
    dom::HTMLBRElement* brElement = dom::HTMLBRElement::FromNode(findNode);
    if (brElement && brElement->IsPaddingForEmptyEditor()) {
      // This <br> is the hacky "padding <br> element" used when there is no
      // text in the editor.
      return 0;
    }

    descendant = mDoc->GetAccessible(findNode);
    if (!descendant && findNode->IsContent()) {
      LocalAccessible* container = mDoc->GetContainerAccessible(findNode);
      if (container) {
        TreeWalker walker(container, findNode->AsContent(),
                          TreeWalker::eWalkContextTree);
        descendant = walker.Next();
        if (!descendant) descendant = container;
      }
    }
  }

  return TransformOffset(descendant, offset, aIsEndOffset);
}

uint32_t HyperTextAccessible::TransformOffset(LocalAccessible* aDescendant,
                                              uint32_t aOffset,
                                              bool aIsEndOffset) const {
  // From the descendant, go up and get the immediate child of this hypertext.
  uint32_t offset = aOffset;
  LocalAccessible* descendant = aDescendant;
  while (descendant) {
    LocalAccessible* parent = descendant->LocalParent();
    if (parent == this) return GetChildOffset(descendant) + offset;

    // This offset no longer applies because the passed-in text object is not
    // a child of the hypertext. This happens when there are nested hypertexts,
    // e.g. <div>abc<h1>def</h1>ghi</div>. Thus we need to adjust the offset
    // to make it relative the hypertext.
    // If the end offset is not supposed to be inclusive and the original point
    // is not at 0 offset then the returned offset should be after an embedded
    // character the original point belongs to.
    if (aIsEndOffset) {
      // Similar to our special casing in FindOffset, we add handling for
      // bulleted lists here because PeekOffset returns the inner text node
      // for a list when it should return the list bullet.
      // We manually set the offset so the error doesn't propagate up.
      if (offset == 0 && parent && parent->IsHTMLListItem() &&
          descendant->LocalPrevSibling() &&
          descendant->LocalPrevSibling() ==
              parent->AsHTMLListItem()->Bullet()) {
        offset = 0;
      } else {
        offset = (offset > 0 || descendant->IndexInParent() > 0) ? 1 : 0;
      }
    } else {
      offset = 0;
    }

    descendant = parent;
  }

  // If the given a11y point cannot be mapped into offset relative this
  // hypertext offset then return length as fallback value.
  return CharacterCount();
}

DOMPoint HyperTextAccessible::OffsetToDOMPoint(int32_t aOffset) const {
  // 0 offset is valid even if no children. In this case the associated editor
  // is empty so return a DOM point for editor root element.
  if (aOffset == 0) {
    RefPtr<EditorBase> editorBase = GetEditor();
    if (editorBase) {
      if (editorBase->IsEmpty()) {
        return DOMPoint(editorBase->GetRoot(), 0);
      }
    }
  }

  int32_t childIdx = GetChildIndexAtOffset(aOffset);
  if (childIdx == -1) return DOMPoint();

  LocalAccessible* child = LocalChildAt(childIdx);
  int32_t innerOffset = aOffset - GetChildOffset(childIdx);

  // A text leaf case.
  if (child->IsTextLeaf()) {
    // The point is inside the text node. This is always true for any text leaf
    // except a last child one. See assertion below.
    if (aOffset < GetChildOffset(childIdx + 1)) {
      nsIContent* content = child->GetContent();
      int32_t idx = 0;
      if (NS_FAILED(RenderedToContentOffset(content->GetPrimaryFrame(),
                                            innerOffset, &idx))) {
        return DOMPoint();
      }

      return DOMPoint(content, idx);
    }

    // Set the DOM point right after the text node.
    MOZ_ASSERT(static_cast<uint32_t>(aOffset) == CharacterCount());
    innerOffset = 1;
  }

  // Case of embedded object. The point is either before or after the element.
  NS_ASSERTION(innerOffset == 0 || innerOffset == 1, "A wrong inner offset!");
  nsINode* node = child->GetNode();
  nsINode* parentNode = node->GetParentNode();
  return parentNode ? DOMPoint(parentNode,
                               parentNode->ComputeIndexOf_Deprecated(node) +
                                   innerOffset)
                    : DOMPoint();
}

uint32_t HyperTextAccessible::FindOffset(uint32_t aOffset,
                                         nsDirection aDirection,
                                         nsSelectionAmount aAmount,
                                         EWordMovementType aWordMovementType) {
  NS_ASSERTION(aDirection == eDirPrevious || aAmount != eSelectBeginLine,
               "eSelectBeginLine should only be used with eDirPrevious");

  // Find a leaf accessible frame to start with. PeekOffset wants this.
  HyperTextAccessible* text = this;
  LocalAccessible* child = nullptr;
  int32_t innerOffset = aOffset;

  do {
    int32_t childIdx = text->GetChildIndexAtOffset(innerOffset);

    // We can have an empty text leaf as our only child. Since empty text
    // leaves are not accessible we then have no children, but 0 is a valid
    // innerOffset.
    if (childIdx == -1) {
      NS_ASSERTION(innerOffset == 0 && !text->ChildCount(), "No childIdx?");
      return DOMPointToOffset(text->GetNode(), 0, aDirection == eDirNext);
    }

    child = text->LocalChildAt(childIdx);

    // HTML list items may need special processing because PeekOffset doesn't
    // work with list bullets.
    if (text->IsHTMLListItem()) {
      HTMLLIAccessible* li = text->AsHTMLListItem();
      if (child == li->Bullet()) {
        // XXX: the logic is broken for multichar bullets in moving by
        // char/cluster/word cases.
        if (text != this) {
          return aDirection == eDirPrevious ? TransformOffset(text, 0, false)
                                            : TransformOffset(text, 1, true);
        }
        if (aDirection == eDirPrevious) return 0;

        uint32_t nextOffset = GetChildOffset(1);
        if (nextOffset == 0) return 0;

        switch (aAmount) {
          case eSelectLine:
          case eSelectEndLine:
            // Ask a text leaf next (if not empty) to the bullet for an offset
            // since list item may be multiline.
            return nextOffset < CharacterCount()
                       ? FindOffset(nextOffset, aDirection, aAmount,
                                    aWordMovementType)
                       : nextOffset;

          default:
            return nextOffset;
        }
      }
    }

    innerOffset -= text->GetChildOffset(childIdx);

    text = child->AsHyperText();
  } while (text);

  nsIFrame* childFrame = child->GetFrame();
  if (!childFrame) {
    NS_ERROR("No child frame");
    return 0;
  }

  int32_t innerContentOffset = innerOffset;
  if (child->IsTextLeaf()) {
    NS_ASSERTION(childFrame->IsTextFrame(), "Wrong frame!");
    RenderedToContentOffset(childFrame, innerOffset, &innerContentOffset);
  }

  nsIFrame* frameAtOffset = childFrame;
  int32_t unusedOffsetInFrame = 0;
  childFrame->GetChildFrameContainingOffset(
      innerContentOffset, true, &unusedOffsetInFrame, &frameAtOffset);

  const bool kIsJumpLinesOk = true;       // okay to jump lines
  const bool kIsScrollViewAStop = false;  // do not stop at scroll views
  const bool kIsKeyboardSelect = true;    // is keyboard selection
  const bool kIsVisualBidi = false;       // use visual order for bidi text
  nsPeekOffsetStruct pos(
      aAmount, aDirection, innerContentOffset, nsPoint(0, 0), kIsJumpLinesOk,
      kIsScrollViewAStop, kIsKeyboardSelect, kIsVisualBidi, false,
      nsPeekOffsetStruct::ForceEditableRegion::No, aWordMovementType, false);
  nsresult rv = frameAtOffset->PeekOffset(&pos);

  // PeekOffset fails on last/first lines of the text in certain cases.
  bool fallBackToSelectEndLine = false;
  if (NS_FAILED(rv) && aAmount == eSelectLine) {
    fallBackToSelectEndLine = aDirection == eDirNext;
    pos.mAmount = fallBackToSelectEndLine ? eSelectEndLine : eSelectBeginLine;
    frameAtOffset->PeekOffset(&pos);
  }
  if (!pos.mResultContent) {
    NS_ERROR("No result content!");
    return 0;
  }

  // Turn the resulting DOM point into an offset.
  uint32_t hyperTextOffset = DOMPointToOffset(
      pos.mResultContent, pos.mContentOffset, aDirection == eDirNext);

  if (fallBackToSelectEndLine && IsLineEndCharAt(hyperTextOffset)) {
    // We used eSelectEndLine, but the caller requested eSelectLine.
    // If there's a '\n' at the end of the line, eSelectEndLine will stop on
    // it rather than after it. This is not what we want, since the caller
    // wants the next line, not the same line.
    ++hyperTextOffset;
  }

  if (aDirection == eDirPrevious) {
    // If we reached the end during search, this means we didn't find the DOM
    // point and we're actually at the start of the paragraph
    if (hyperTextOffset == CharacterCount()) return 0;

    // PeekOffset stops right before bullet so return 0 to workaround it.
    if (IsHTMLListItem() && aAmount == eSelectBeginLine &&
        hyperTextOffset > 0) {
      LocalAccessible* prevOffsetChild = GetChildAtOffset(hyperTextOffset - 1);
      if (prevOffsetChild == AsHTMLListItem()->Bullet()) return 0;
    }
  }

  return hyperTextOffset;
}

uint32_t HyperTextAccessible::FindWordBoundary(
    uint32_t aOffset, nsDirection aDirection,
    EWordMovementType aWordMovementType) {
  uint32_t orig =
      FindOffset(aOffset, aDirection, eSelectWord, aWordMovementType);
  if (aWordMovementType != eStartWord) {
    return orig;
  }
  if (aDirection == eDirPrevious) {
    // When layout.word_select.stop_at_punctuation is true (the default),
    // for a word beginning with punctuation, layout treats the punctuation
    // as the start of the word when moving next. However, when moving
    // previous, layout stops *after* the punctuation. We want to be
    // consistent regardless of movement direction and always treat punctuation
    // as the start of a word.
    if (!StaticPrefs::layout_word_select_stop_at_punctuation()) {
      return orig;
    }
    // Case 1: Example: "a @"
    // If aOffset is 2 or 3, orig will be 0, but it should be 2. That is,
    // previous word moved back too far.
    LocalAccessible* child = GetChildAtOffset(orig);
    if (child && child->IsHyperText()) {
      // For a multi-word embedded object, previous word correctly goes back
      // to the start of the word (the embedded object). Next word (below)
      // incorrectly stops after the embedded object in this case, so return
      // the already correct result.
      // Example: "a x y b", where "x y" is an embedded link
      // If aOffset is 4, orig will be 2, which is correct.
      // If we get the next word (below), we'll end up returning 3 instead.
      return orig;
    }
    uint32_t next = FindOffset(orig, eDirNext, eSelectWord, eStartWord);
    if (next < aOffset) {
      // Next word stopped on punctuation.
      return next;
    }
    // case 2: example: "a @@b"
    // If aOffset is 2, 3 or 4, orig will be 4, but it should be 2. That is,
    // previous word didn't go back far enough.
    if (orig == 0) {
      return orig;
    }
    // Walk backwards by offset, getting the next word.
    // In the loop, o is unsigned, so o >= 0 will always be true and won't
    // prevent us from decrementing at 0. Instead, we check that o doesn't
    // wrap around.
    for (uint32_t o = orig - 1; o < orig; --o) {
      next = FindOffset(o, eDirNext, eSelectWord, eStartWord);
      if (next == orig) {
        // Next word and previous word were consistent. This
        // punctuation problem isn't applicable here.
        break;
      }
      if (next < orig) {
        // Next word stopped on punctuation.
        return next;
      }
    }
  } else {
    // When layout.word_select.stop_at_punctuation is true (the default),
    // when positioned on punctuation in the middle of a word, next word skips
    // the rest of the word. However, when positioned before the punctuation,
    // next word moves just after the punctuation. We want to be consistent
    // regardless of starting position and always stop just after the
    // punctuation.
    // Next word can move too far when positioned on white space too.
    // Example: "a b@c"
    // If aOffset is 3, orig will be 5, but it should be 4. That is, next word
    // moved too far.
    if (aOffset == 0) {
      return orig;
    }
    uint32_t prev = FindOffset(orig, eDirPrevious, eSelectWord, eStartWord);
    if (prev <= aOffset) {
      // orig definitely isn't too far forward.
      return orig;
    }
    // Walk backwards by offset, getting the next word.
    // In the loop, o is unsigned, so o >= 0 will always be true and won't
    // prevent us from decrementing at 0. Instead, we check that o doesn't
    // wrap around.
    for (uint32_t o = aOffset - 1; o < aOffset; --o) {
      uint32_t next = FindOffset(o, eDirNext, eSelectWord, eStartWord);
      if (next > aOffset && next < orig) {
        return next;
      }
      if (next <= aOffset) {
        break;
      }
    }
  }
  return orig;
}

uint32_t HyperTextAccessible::FindLineBoundary(
    uint32_t aOffset, EWhichLineBoundary aWhichLineBoundary) {
  // Note: empty last line doesn't have own frame (a previous line contains '\n'
  // character instead) thus when it makes a difference we need to process this
  // case separately (otherwise operations are performed on previous line).
  switch (aWhichLineBoundary) {
    case ePrevLineBegin: {
      // Fetch a previous line and move to its start (as arrow up and home keys
      // were pressed).
      if (IsEmptyLastLineOffset(aOffset)) {
        return FindOffset(aOffset, eDirPrevious, eSelectBeginLine);
      }

      uint32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      return FindOffset(tmpOffset, eDirPrevious, eSelectBeginLine);
    }

    case ePrevLineEnd: {
      if (IsEmptyLastLineOffset(aOffset)) return aOffset - 1;

      // If offset is at first line then return 0 (first line start).
      uint32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectBeginLine);
      if (tmpOffset == 0) return 0;

      // Otherwise move to end of previous line (as arrow up and end keys were
      // pressed).
      tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      return FindOffset(tmpOffset, eDirNext, eSelectEndLine);
    }

    case eThisLineBegin: {
      if (IsEmptyLastLineOffset(aOffset)) return aOffset;

      // Move to begin of the current line (as home key was pressed).
      uint32_t thisLineBeginOffset =
          FindOffset(aOffset, eDirPrevious, eSelectBeginLine);
      if (IsCharAt(thisLineBeginOffset, kEmbeddedObjectChar)) {
        // We landed on an embedded character, don't mess with possible embedded
        // line breaks, and assume the offset is correct.
        return thisLineBeginOffset;
      }

      // Sometimes, there is the possibility layout returned an
      // offset smaller than it should. Sanity-check by moving to the end of the
      // previous line and see if that has a greater offset.
      uint32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      tmpOffset = FindOffset(tmpOffset, eDirNext, eSelectEndLine);
      if (tmpOffset > thisLineBeginOffset && tmpOffset < aOffset) {
        // We found a previous line offset. Return the next character after it
        // as our start offset if it points to a line end char.
        return IsLineEndCharAt(tmpOffset) ? tmpOffset + 1 : tmpOffset;
      }
      return thisLineBeginOffset;
    }

    case eThisLineEnd:
      if (IsEmptyLastLineOffset(aOffset)) return aOffset;

      // Move to end of the current line (as end key was pressed).
      return FindOffset(aOffset, eDirNext, eSelectEndLine);

    case eNextLineBegin: {
      if (IsEmptyLastLineOffset(aOffset)) return aOffset;

      // Move to begin of the next line if any (arrow down and home keys),
      // otherwise end of the current line (arrow down only).
      uint32_t tmpOffset = FindOffset(aOffset, eDirNext, eSelectLine);
      uint32_t characterCount = CharacterCount();
      if (tmpOffset == characterCount) {
        return tmpOffset;
      }

      // Now, simulate the Home key on the next line to get its real offset.
      uint32_t nextLineBeginOffset =
          FindOffset(tmpOffset, eDirPrevious, eSelectBeginLine);
      // Sometimes, there are line breaks inside embedded characters. If this
      // is the case, the cursor is after the line break, but the offset will
      // be that of the embedded character, which points to before the line
      // break. We definitely want the line break included.
      if (IsCharAt(nextLineBeginOffset, kEmbeddedObjectChar)) {
        // We can determine if there is a line break by pressing End from
        // the queried offset. If there is a line break, the offset will be 1
        // greater, since this line ends with the embed. If there is not, the
        // value will be different even if a line break follows right after the
        // embed.
        uint32_t thisLineEndOffset =
            FindOffset(aOffset, eDirNext, eSelectEndLine);
        if (thisLineEndOffset == nextLineBeginOffset + 1) {
          // If we're querying the offset of the embedded character, we want
          // the end offset of the parent line instead. Press End
          // once more from the current position, which is after the embed.
          if (nextLineBeginOffset == aOffset) {
            uint32_t thisLineEndOffset2 =
                FindOffset(thisLineEndOffset, eDirNext, eSelectEndLine);
            // The above returns an offset exclusive the final line break, so we
            // need to add 1 to it to return an inclusive end offset. Make sure
            // we don't overshoot if we've started from another embedded
            // character that has a line break, or landed on another embedded
            // character, or if the result is the very end.
            return (thisLineEndOffset2 == characterCount ||
                    (IsCharAt(thisLineEndOffset, kEmbeddedObjectChar) &&
                     thisLineEndOffset2 == thisLineEndOffset + 1) ||
                    IsCharAt(thisLineEndOffset2, kEmbeddedObjectChar))
                       ? thisLineEndOffset2
                       : thisLineEndOffset2 + 1;
          }

          return thisLineEndOffset;
        }
        return nextLineBeginOffset;
      }

      // If the resulting offset is not greater than the offset we started from,
      // layout could not find the offset for us. This can happen with certain
      // inline-block elements.
      if (nextLineBeginOffset <= aOffset) {
        // Walk forward from the offset we started from up to tmpOffset,
        // stopping after a line end character.
        nextLineBeginOffset = aOffset;
        while (nextLineBeginOffset < tmpOffset) {
          if (IsLineEndCharAt(nextLineBeginOffset)) {
            return nextLineBeginOffset + 1;
          }
          nextLineBeginOffset++;
        }
      }

      return nextLineBeginOffset;
    }

    case eNextLineEnd: {
      if (IsEmptyLastLineOffset(aOffset)) return aOffset;

      // Move to next line end (as down arrow and end key were pressed).
      uint32_t tmpOffset = FindOffset(aOffset, eDirNext, eSelectLine);
      if (tmpOffset == CharacterCount()) return tmpOffset;

      return FindOffset(tmpOffset, eDirNext, eSelectEndLine);
    }
  }

  return 0;
}

int32_t HyperTextAccessible::FindParagraphStartOffset(uint32_t aOffset) {
  // Because layout often gives us offsets that are incompatible with
  // accessibility API requirements, for example when a paragraph contains
  // presentational line breaks as found in Google Docs, use the accessibility
  // tree to find the start offset instead.
  LocalAccessible* child = GetChildAtOffset(aOffset);
  if (!child) {
    return -1;  // Invalid offset
  }

  // Use the pivot class to search for the start  offset.
  Pivot p = Pivot(this);
  ParagraphBoundaryRule boundaryRule = ParagraphBoundaryRule(
      child, child->IsTextLeaf() ? aOffset - GetChildOffset(child) : 0,
      eDirPrevious);
  Accessible* match = p.Prev(child, boundaryRule, true);
  if (!match || match->AsLocal() == this) {
    // Found nothing, or pivot found the root of the search, startOffset is 0.
    // This will include all relevant text nodes.
    return 0;
  }

  if (match == child) {
    // We started out on a boundary.
    if (match->Role() == roles::WHITESPACE) {
      // We are on a line break boundary, so force pivot to find the previous
      // boundary. What we want is any text before this, if any.
      match = p.Prev(match, boundaryRule);
      if (!match || match->AsLocal() == this) {
        // Same as before, we landed on the root, so offset is definitely 0.
        return 0;
      }
    } else if (!match->AsLocal()->IsTextLeaf()) {
      // The match is a block element, which is always a starting point, so
      // just return its offset.
      return TransformOffset(match->AsLocal(), 0, false);
    }
  }

  if (match->AsLocal()->IsTextLeaf()) {
    // ParagraphBoundaryRule only returns a text leaf if it contains a line
    // break. We want to stop after that.
    return TransformOffset(match->AsLocal(),
                           boundaryRule.GetLastMatchTextOffset() + 1, false);
  }

  // This is a previous boundary, we don't want to include it itself.
  // So, walk forward one accessible, excluding the descendants of this
  // boundary if it is a block element. The below call to Next should always be
  // initialized with a boundary.
  SkipParagraphBoundaryRule goForwardOneRule = SkipParagraphBoundaryRule(match);
  match = p.Next(match, goForwardOneRule);
  // We already know that the search skipped over at least one accessible,
  // so match can't be null. Get its transformed offset.
  MOZ_ASSERT(match);
  return TransformOffset(match->AsLocal(), 0, false);
}

int32_t HyperTextAccessible::FindParagraphEndOffset(uint32_t aOffset) {
  // Because layout often gives us offsets that are incompatible with
  // accessibility API requirements, for example when a paragraph contains
  // presentational line breaks as found in Google Docs, use the accessibility
  // tree to find the end offset instead.
  LocalAccessible* child = GetChildAtOffset(aOffset);
  if (!child) {
    return -1;  // invalid offset
  }

  // Use the pivot class to search for the end offset.
  Pivot p = Pivot(this);
  ParagraphBoundaryRule boundaryRule = ParagraphBoundaryRule(
      child, child->IsTextLeaf() ? aOffset - GetChildOffset(child) : 0,
      eDirNext,
      // In order to encompass all paragraphs inside embedded objects, not just
      // the first, we want to skip the anchor's subtree.
      /* aSkipAnchorSubtree */ true);
  // Search forward for the end offset, including child. We don't want
  // to go beyond this point if this offset indicates a paragraph boundary.
  Accessible* match = p.Next(child, boundaryRule, true);
  if (match) {
    // Found something of relevance, adjust end offset.
    LocalAccessible* matchAcc = match->AsLocal();
    uint32_t matchOffset;
    if (matchAcc->IsTextLeaf()) {
      // ParagraphBoundaryRule only returns a text leaf if it contains a line
      // break.
      matchOffset = boundaryRule.GetLastMatchTextOffset() + 1;
    } else if (matchAcc->Role() != roles::WHITESPACE && matchAcc != child) {
      // We found a block boundary that wasn't our origin. We want to stop
      // right on it, not after it, since we don't want to include the content
      // of the block.
      matchOffset = 0;
    } else {
      matchOffset = nsAccUtils::TextLength(matchAcc);
    }
    return TransformOffset(matchAcc, matchOffset, true);
  }

  // Didn't find anything, end offset is character count.
  return CharacterCount();
}

void HyperTextAccessible::TextBeforeOffset(int32_t aOffset,
                                           AccessibleTextBoundary aBoundaryType,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset,
                                           nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // This isn't strictly related to caching, but this new text implementation
    // is being developed to make caching feasible. We put it behind this pref
    // to make it easy to test while it's still under development.
    return HyperTextAccessibleBase::TextBeforeOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }

  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  if (aBoundaryType == nsIAccessibleText::BOUNDARY_PARAGRAPH) {
    // Not supported, bail out with empty text.
    return;
  }

  index_t convertedOffset = ConvertMagicOffset(aOffset);
  if (!convertedOffset.IsValid() || convertedOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return;
  }

  uint32_t adjustedOffset = convertedOffset;
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
    adjustedOffset = AdjustCaretOffset(adjustedOffset);
  }

  switch (aBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      if (convertedOffset != 0) {
        CharAt(convertedOffset - 1, aText, aStartOffset, aEndOffset);
      }
      break;

    case nsIAccessibleText::BOUNDARY_WORD_START: {
      // If the offset is a word start (except text length offset) then move
      // backward to find a start offset (end offset is the given offset).
      // Otherwise move backward twice to find both start and end offsets.
      if (adjustedOffset == CharacterCount()) {
        *aEndOffset =
            FindWordBoundary(adjustedOffset, eDirPrevious, eStartWord);
        *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
      } else {
        *aStartOffset =
            FindWordBoundary(adjustedOffset, eDirPrevious, eStartWord);
        *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eStartWord);
        if (*aEndOffset != static_cast<int32_t>(adjustedOffset)) {
          *aEndOffset = *aStartOffset;
          *aStartOffset =
              FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
        }
      }
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
    }

    case nsIAccessibleText::BOUNDARY_WORD_END: {
      // Move word backward twice to find start and end offsets.
      *aEndOffset = FindWordBoundary(convertedOffset, eDirPrevious, eEndWord);
      *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eEndWord);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
    }

    case nsIAccessibleText::BOUNDARY_LINE_START:
      *aStartOffset = FindLineBoundary(adjustedOffset, ePrevLineBegin);
      *aEndOffset = FindLineBoundary(adjustedOffset, eThisLineBegin);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_LINE_END: {
      *aEndOffset = FindLineBoundary(adjustedOffset, ePrevLineEnd);
      int32_t tmpOffset = *aEndOffset;
      // Adjust offset if line is wrapped.
      if (*aEndOffset != 0 && !IsLineEndCharAt(*aEndOffset)) tmpOffset--;

      *aStartOffset = FindLineBoundary(tmpOffset, ePrevLineEnd);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
    }
  }
}

void HyperTextAccessible::TextAtOffset(int32_t aOffset,
                                       AccessibleTextBoundary aBoundaryType,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset, nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // This isn't strictly related to caching, but this new text implementation
    // is being developed to make caching feasible. We put it behind this pref
    // to make it easy to test while it's still under development.
    return HyperTextAccessibleBase::TextAtOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }

  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  uint32_t adjustedOffset = ConvertMagicOffset(aOffset);
  if (adjustedOffset == std::numeric_limits<uint32_t>::max()) {
    NS_ERROR("Wrong given offset!");
    return;
  }

  switch (aBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      // Return no char if caret is at the end of wrapped line (case of no line
      // end character). Returning a next line char is confusing for AT.
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET &&
          IsCaretAtEndOfLine()) {
        *aStartOffset = *aEndOffset = adjustedOffset;
      } else {
        CharAt(adjustedOffset, aText, aStartOffset, aEndOffset);
      }
      break;

    case nsIAccessibleText::BOUNDARY_WORD_START:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
        adjustedOffset = AdjustCaretOffset(adjustedOffset);
      }

      *aEndOffset = FindWordBoundary(adjustedOffset, eDirNext, eStartWord);
      *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_WORD_END:
      // Ignore the spec and follow what WebKitGtk does because Orca expects it,
      // i.e. return a next word at word end offset of the current word
      // (WebKitGtk behavior) instead the current word (AKT spec).
      *aEndOffset = FindWordBoundary(adjustedOffset, eDirNext, eEndWord);
      *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eEndWord);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_LINE_START:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
        adjustedOffset = AdjustCaretOffset(adjustedOffset);
      }

      *aStartOffset = FindLineBoundary(adjustedOffset, eThisLineBegin);
      *aEndOffset = FindLineBoundary(adjustedOffset, eNextLineBegin);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_LINE_END:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
        adjustedOffset = AdjustCaretOffset(adjustedOffset);
      }

      // In contrast to word end boundary we follow the spec here.
      *aStartOffset = FindLineBoundary(adjustedOffset, ePrevLineEnd);
      *aEndOffset = FindLineBoundary(adjustedOffset, eThisLineEnd);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_PARAGRAPH: {
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
        adjustedOffset = AdjustCaretOffset(adjustedOffset);
      }

      if (IsEmptyLastLineOffset(adjustedOffset)) {
        // We are on the last line of a paragraph where there is no text.
        // For example, in a textarea where a new line has just been inserted.
        // In this case, return offsets for an empty line without text content.
        *aStartOffset = *aEndOffset = adjustedOffset;
        break;
      }

      *aStartOffset = FindParagraphStartOffset(adjustedOffset);
      *aEndOffset = FindParagraphEndOffset(adjustedOffset);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
    }
  }
}

void HyperTextAccessible::TextAfterOffset(int32_t aOffset,
                                          AccessibleTextBoundary aBoundaryType,
                                          int32_t* aStartOffset,
                                          int32_t* aEndOffset,
                                          nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // This isn't strictly related to caching, but this new text implementation
    // is being developed to make caching feasible. We put it behind this pref
    // to make it easy to test while it's still under development.
    return HyperTextAccessibleBase::TextAfterOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }

  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  if (aBoundaryType == nsIAccessibleText::BOUNDARY_PARAGRAPH) {
    // Not supported, bail out with empty text.
    return;
  }

  index_t convertedOffset = ConvertMagicOffset(aOffset);
  if (!convertedOffset.IsValid() || convertedOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return;
  }

  uint32_t adjustedOffset = convertedOffset;
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
    adjustedOffset = AdjustCaretOffset(adjustedOffset);
  }

  switch (aBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      // If caret is at the end of wrapped line (case of no line end character)
      // then char after the offset is a first char at next line.
      if (adjustedOffset >= CharacterCount()) {
        *aStartOffset = *aEndOffset = CharacterCount();
      } else {
        CharAt(adjustedOffset + 1, aText, aStartOffset, aEndOffset);
      }
      break;

    case nsIAccessibleText::BOUNDARY_WORD_START:
      // Move word forward twice to find start and end offsets.
      *aStartOffset = FindWordBoundary(adjustedOffset, eDirNext, eStartWord);
      *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eStartWord);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_WORD_END:
      // If the offset is a word end (except 0 offset) then move forward to find
      // end offset (start offset is the given offset). Otherwise move forward
      // twice to find both start and end offsets.
      if (convertedOffset == 0) {
        *aStartOffset = FindWordBoundary(convertedOffset, eDirNext, eEndWord);
        *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eEndWord);
      } else {
        *aEndOffset = FindWordBoundary(convertedOffset, eDirNext, eEndWord);
        *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eEndWord);
        if (*aStartOffset != static_cast<int32_t>(convertedOffset)) {
          *aStartOffset = *aEndOffset;
          *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eEndWord);
        }
      }
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_LINE_START:
      *aStartOffset = FindLineBoundary(adjustedOffset, eNextLineBegin);
      *aEndOffset = FindLineBoundary(*aStartOffset, eNextLineBegin);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_LINE_END:
      *aStartOffset = FindLineBoundary(adjustedOffset, eThisLineEnd);
      *aEndOffset = FindLineBoundary(adjustedOffset, eNextLineEnd);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
  }
}

already_AddRefed<AccAttributes> HyperTextAccessible::TextAttributes(
    bool aIncludeDefAttrs, int32_t aOffset, int32_t* aStartOffset,
    int32_t* aEndOffset) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // This isn't strictly related to caching, but this new text implementation
    // is being developed to make caching feasible. We put it behind this pref
    // to make it easy to test while it's still under development.
    return HyperTextAccessibleBase::TextAttributes(aIncludeDefAttrs, aOffset,
                                                   aStartOffset, aEndOffset);
  }

  // 1. Get each attribute and its ranges one after another.
  // 2. As we get each new attribute, we pass the current start and end offsets
  //    as in/out parameters. In other words, as attributes are collected,
  //    the attribute range itself can only stay the same or get smaller.

  RefPtr<AccAttributes> attributes = new AccAttributes();
  *aStartOffset = *aEndOffset = 0;
  index_t offset = ConvertMagicOffset(aOffset);
  if (!offset.IsValid() || offset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return attributes.forget();
  }

  LocalAccessible* accAtOffset = GetChildAtOffset(offset);
  if (!accAtOffset) {
    // Offset 0 is correct offset when accessible has empty text. Include
    // default attributes if they were requested, otherwise return empty set.
    if (offset == 0) {
      if (aIncludeDefAttrs) {
        TextAttrsMgr textAttrsMgr(this);
        textAttrsMgr.GetAttributes(attributes);
      }
    }
    return attributes.forget();
  }

  int32_t accAtOffsetIdx = accAtOffset->IndexInParent();
  uint32_t startOffset = GetChildOffset(accAtOffsetIdx);
  uint32_t endOffset = GetChildOffset(accAtOffsetIdx + 1);
  int32_t offsetInAcc = offset - startOffset;

  TextAttrsMgr textAttrsMgr(this, aIncludeDefAttrs, accAtOffset,
                            accAtOffsetIdx);
  textAttrsMgr.GetAttributes(attributes, &startOffset, &endOffset);

  // Compute spelling attributes on text accessible only.
  nsIFrame* offsetFrame = accAtOffset->GetFrame();
  if (offsetFrame && offsetFrame->IsTextFrame()) {
    int32_t nodeOffset = 0;
    RenderedToContentOffset(offsetFrame, offsetInAcc, &nodeOffset);

    // Set 'misspelled' text attribute.
    // FYI: Max length of text in a text node is less than INT32_MAX (see
    //      NS_MAX_TEXT_FRAGMENT_LENGTH) so that nodeOffset should always
    //      be 0 or greater.
    MOZ_DIAGNOSTIC_ASSERT(accAtOffset->GetNode()->IsText());
    MOZ_DIAGNOSTIC_ASSERT(nodeOffset >= 0);
    GetSpellTextAttr(accAtOffset->GetNode(), static_cast<uint32_t>(nodeOffset),
                     &startOffset, &endOffset, attributes);
  }

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;
  return attributes.forget();
}

already_AddRefed<AccAttributes> HyperTextAccessible::DefaultTextAttributes() {
  RefPtr<AccAttributes> attributes = new AccAttributes();

  TextAttrsMgr textAttrsMgr(this);
  textAttrsMgr.GetAttributes(attributes);
  return attributes.forget();
}

void HyperTextAccessible::SetMathMLXMLRoles(AccAttributes* aAttributes) {
  // Add MathML xmlroles based on the position inside the parent.
  LocalAccessible* parent = LocalParent();
  if (parent) {
    switch (parent->Role()) {
      case roles::MATHML_CELL:
      case roles::MATHML_ENCLOSED:
      case roles::MATHML_ERROR:
      case roles::MATHML_MATH:
      case roles::MATHML_ROW:
      case roles::MATHML_SQUARE_ROOT:
      case roles::MATHML_STYLE:
        if (Role() == roles::MATHML_OPERATOR) {
          // This is an operator inside an <mrow> (or an inferred <mrow>).
          // See http://www.w3.org/TR/MathML3/chapter3.html#presm.inferredmrow
          // XXX We should probably do something similar for MATHML_FENCED, but
          // operators do not appear in the accessible tree. See bug 1175747.
          nsIMathMLFrame* mathMLFrame = do_QueryFrame(GetFrame());
          if (mathMLFrame) {
            nsEmbellishData embellishData;
            mathMLFrame->GetEmbellishData(embellishData);
            if (NS_MATHML_EMBELLISH_IS_FENCE(embellishData.flags)) {
              if (!LocalPrevSibling()) {
                aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                          nsGkAtoms::open_fence);
              } else if (!LocalNextSibling()) {
                aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                          nsGkAtoms::close_fence);
              }
            }
            if (NS_MATHML_EMBELLISH_IS_SEPARATOR(embellishData.flags)) {
              aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                        nsGkAtoms::separator_);
            }
          }
        }
        break;
      case roles::MATHML_FRACTION:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles, IndexInParent() == 0 ? nsGkAtoms::numerator
                                                      : nsGkAtoms::denominator);
        break;
      case roles::MATHML_ROOT:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::root_index);
        break;
      case roles::MATHML_SUB:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::subscript);
        break;
      case roles::MATHML_SUP:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::superscript);
        break;
      case roles::MATHML_SUB_SUP: {
        int32_t index = IndexInParent();
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            index == 0
                ? nsGkAtoms::base
                : (index == 1 ? nsGkAtoms::subscript : nsGkAtoms::superscript));
      } break;
      case roles::MATHML_UNDER:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::underscript);
        break;
      case roles::MATHML_OVER:
        aAttributes->SetAttribute(
            nsGkAtoms::xmlroles,
            IndexInParent() == 0 ? nsGkAtoms::base : nsGkAtoms::overscript);
        break;
      case roles::MATHML_UNDER_OVER: {
        int32_t index = IndexInParent();
        aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                  index == 0
                                      ? nsGkAtoms::base
                                      : (index == 1 ? nsGkAtoms::underscript
                                                    : nsGkAtoms::overscript));
      } break;
      case roles::MATHML_MULTISCRIPTS: {
        // Get the <multiscripts> base.
        nsIContent* child;
        bool baseFound = false;
        for (child = parent->GetContent()->GetFirstChild(); child;
             child = child->GetNextSibling()) {
          if (child->IsMathMLElement()) {
            baseFound = true;
            break;
          }
        }
        if (baseFound) {
          nsIContent* content = GetContent();
          if (child == content) {
            // We are the base.
            aAttributes->SetAttribute(nsGkAtoms::xmlroles, nsGkAtoms::base);
          } else {
            // Browse the list of scripts to find us and determine our type.
            bool postscript = true;
            bool subscript = true;
            for (child = child->GetNextSibling(); child;
                 child = child->GetNextSibling()) {
              if (!child->IsMathMLElement()) continue;
              if (child->IsMathMLElement(nsGkAtoms::mprescripts_)) {
                postscript = false;
                subscript = true;
                continue;
              }
              if (child == content) {
                if (postscript) {
                  aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                            subscript ? nsGkAtoms::subscript
                                                      : nsGkAtoms::superscript);
                } else {
                  aAttributes->SetAttribute(nsGkAtoms::xmlroles,
                                            subscript
                                                ? nsGkAtoms::presubscript
                                                : nsGkAtoms::presuperscript);
                }
                break;
              }
              subscript = !subscript;
            }
          }
        }
      } break;
      default:
        break;
    }
  }
}

already_AddRefed<AccAttributes> HyperTextAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = AccessibleWrap::NativeAttributes();

  // 'formatting' attribute is deprecated, 'display' attribute should be
  // instead.
  nsIFrame* frame = GetFrame();
  if (frame && frame->IsBlockFrame()) {
    attributes->SetAttribute(nsGkAtoms::formatting, nsGkAtoms::block);
  }

  if (FocusMgr()->IsFocused(this)) {
    int32_t lineNumber = CaretLineNumber();
    if (lineNumber >= 1) {
      attributes->SetAttribute(nsGkAtoms::lineNumber, lineNumber);
    }
  }

  if (HasOwnContent()) {
    GetAccService()->MarkupAttributes(mContent, attributes);
    if (mContent->IsMathMLElement()) SetMathMLXMLRoles(attributes);
  }

  return attributes.forget();
}

int32_t HyperTextAccessible::OffsetAtPoint(int32_t aX, int32_t aY,
                                           uint32_t aCoordType) {
  nsIFrame* hyperFrame = GetFrame();
  if (!hyperFrame) return -1;

  LayoutDeviceIntPoint coords =
      nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordType, this);

  nsPresContext* presContext = mDoc->PresContext();
  nsPoint coordsInAppUnits = LayoutDeviceIntPoint::ToAppUnits(
      coords, presContext->AppUnitsPerDevPixel());

  nsRect frameScreenRect = hyperFrame->GetScreenRectInAppUnits();
  if (!frameScreenRect.Contains(coordsInAppUnits.x, coordsInAppUnits.y)) {
    return -1;  // Not found
  }

  nsPoint pointInHyperText(coordsInAppUnits.x - frameScreenRect.X(),
                           coordsInAppUnits.y - frameScreenRect.Y());

  // Go through the frames to check if each one has the point.
  // When one does, add up the character offsets until we have a match

  // We have an point in an accessible child of this, now we need to add up the
  // offsets before it to what we already have
  int32_t offset = 0;
  uint32_t childCount = ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    LocalAccessible* childAcc = mChildren[childIdx];

    nsIFrame* primaryFrame = childAcc->GetFrame();
    NS_ENSURE_TRUE(primaryFrame, -1);

    nsIFrame* frame = primaryFrame;
    while (frame) {
      nsIContent* content = frame->GetContent();
      NS_ENSURE_TRUE(content, -1);
      nsPoint pointInFrame = pointInHyperText - frame->GetOffsetTo(hyperFrame);
      nsSize frameSize = frame->GetSize();
      if (pointInFrame.x < frameSize.width &&
          pointInFrame.y < frameSize.height) {
        // Finished
        if (frame->IsTextFrame()) {
          nsIFrame::ContentOffsets contentOffsets =
              frame->GetContentOffsetsFromPointExternal(
                  pointInFrame, nsIFrame::IGNORE_SELECTION_STYLE);
          if (contentOffsets.IsNull() || contentOffsets.content != content) {
            return -1;  // Not found
          }
          uint32_t addToOffset;
          nsresult rv = ContentToRenderedOffset(
              primaryFrame, contentOffsets.offset, &addToOffset);
          NS_ENSURE_SUCCESS(rv, -1);
          offset += addToOffset;
        }
        return offset;
      }
      frame = frame->GetNextContinuation();
    }

    offset += nsAccUtils::TextLength(childAcc);
  }

  return -1;  // Not found
}

LayoutDeviceIntRect HyperTextAccessible::TextBounds(int32_t aStartOffset,
                                                    int32_t aEndOffset,
                                                    uint32_t aCoordType) {
  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  if (!startOffset.IsValid() || !endOffset.IsValid() ||
      startOffset > endOffset || endOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset");
    return LayoutDeviceIntRect();
  }

  if (CharacterCount() == 0) {
    nsPresContext* presContext = mDoc->PresContext();
    // Empty content, use our own bound to at least get x,y coordinates
    nsIFrame* frame = GetFrame();
    if (!frame) {
      return LayoutDeviceIntRect();
    }
    return LayoutDeviceIntRect::FromAppUnitsToNearest(
        frame->GetScreenRectInAppUnits(), presContext->AppUnitsPerDevPixel());
  }

  int32_t childIdx = GetChildIndexAtOffset(startOffset);
  if (childIdx == -1) return LayoutDeviceIntRect();

  LayoutDeviceIntRect bounds;
  int32_t prevOffset = GetChildOffset(childIdx);
  int32_t offset1 = startOffset - prevOffset;

  while (childIdx < static_cast<int32_t>(ChildCount())) {
    nsIFrame* frame = LocalChildAt(childIdx++)->GetFrame();
    if (!frame) {
      MOZ_ASSERT_UNREACHABLE("No frame for a child!");
      continue;
    }

    int32_t nextOffset = GetChildOffset(childIdx);
    if (nextOffset >= static_cast<int32_t>(endOffset)) {
      bounds.UnionRect(
          bounds, GetBoundsInFrame(frame, offset1, endOffset - prevOffset));
      break;
    }

    bounds.UnionRect(bounds,
                     GetBoundsInFrame(frame, offset1, nextOffset - prevOffset));

    prevOffset = nextOffset;
    offset1 = 0;
  }

  // This document may have a resolution set, we will need to multiply
  // the document-relative coordinates by that value and re-apply the doc's
  // screen coordinates.
  nsPresContext* presContext = mDoc->PresContext();
  nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
  LayoutDeviceIntRect orgRectPixels =
      LayoutDeviceIntRect::FromAppUnitsToNearest(
          rootFrame->GetScreenRectInAppUnits(),
          presContext->AppUnitsPerDevPixel());
  bounds.MoveBy(-orgRectPixels.X(), -orgRectPixels.Y());
  bounds.ScaleRoundOut(presContext->PresShell()->GetResolution());
  bounds.MoveBy(orgRectPixels.X(), orgRectPixels.Y());

  auto boundsX = bounds.X();
  auto boundsY = bounds.Y();
  nsAccUtils::ConvertScreenCoordsTo(&boundsX, &boundsY, aCoordType, this);
  bounds.MoveTo(boundsX, boundsY);
  return bounds;
}

already_AddRefed<EditorBase> HyperTextAccessible::GetEditor() const {
  if (!mContent->HasFlag(NODE_IS_EDITABLE)) {
    // If we're inside an editable container, then return that container's
    // editor
    LocalAccessible* ancestor = LocalParent();
    while (ancestor) {
      HyperTextAccessible* hyperText = ancestor->AsHyperText();
      if (hyperText) {
        // Recursion will stop at container doc because it has its own impl
        // of GetEditor()
        return hyperText->GetEditor();
      }

      ancestor = ancestor->LocalParent();
    }

    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mContent);
  nsCOMPtr<nsIEditingSession> editingSession;
  docShell->GetEditingSession(getter_AddRefs(editingSession));
  if (!editingSession) return nullptr;  // No editing session interface

  dom::Document* docNode = mDoc->DocumentNode();
  RefPtr<HTMLEditor> htmlEditor =
      editingSession->GetHTMLEditorForWindow(docNode->GetWindow());
  return htmlEditor.forget();
}

/**
 * =================== Caret & Selection ======================
 */

nsresult HyperTextAccessible::SetSelectionRange(int32_t aStartPos,
                                                int32_t aEndPos) {
  // Before setting the selection range, we need to ensure that the editor
  // is initialized. (See bug 804927.)
  // Otherwise, it's possible that lazy editor initialization will override
  // the selection we set here and leave the caret at the end of the text.
  // By calling GetEditor here, we ensure that editor initialization is
  // completed before we set the selection.
  RefPtr<EditorBase> editorBase = GetEditor();

  bool isFocusable = InteractiveState() & states::FOCUSABLE;

  // If accessible is focusable then focus it before setting the selection to
  // neglect control's selection changes on focus if any (for example, inputs
  // that do select all on focus).
  // some input controls
  if (isFocusable) TakeFocus();

  RefPtr<dom::Selection> domSel = DOMSelection();
  NS_ENSURE_STATE(domSel);

  // Set up the selection.
  for (const uint32_t idx : Reversed(IntegerRange(1u, domSel->RangeCount()))) {
    MOZ_ASSERT(domSel->RangeCount() == idx + 1);
    RefPtr<nsRange> range{domSel->GetRangeAt(idx)};
    if (!range) {
      break;  // The range count has been changed by somebody else.
    }
    domSel->RemoveRangeAndUnselectFramesAndNotifyListeners(*range,
                                                           IgnoreErrors());
  }
  SetSelectionBoundsAt(0, aStartPos, aEndPos);

  // Make sure it is visible
  domSel->ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                         ScrollAxis(), ScrollAxis(),
                         dom::Selection::SCROLL_FOR_CARET_MOVE |
                             dom::Selection::SCROLL_OVERFLOW_HIDDEN);

  // When selection is done, move the focus to the selection if accessible is
  // not focusable. That happens when selection is set within hypertext
  // accessible.
  if (isFocusable) return NS_OK;

  nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
  if (DOMFocusManager) {
    NS_ENSURE_TRUE(mDoc, NS_ERROR_FAILURE);
    dom::Document* docNode = mDoc->DocumentNode();
    NS_ENSURE_TRUE(docNode, NS_ERROR_FAILURE);
    nsCOMPtr<nsPIDOMWindowOuter> window = docNode->GetWindow();
    RefPtr<dom::Element> result;
    DOMFocusManager->MoveFocus(
        window, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
        nsIFocusManager::FLAG_BYMOVEFOCUS, getter_AddRefs(result));
  }

  return NS_OK;
}

int32_t HyperTextAccessible::CaretOffset() const {
  // Not focused focusable accessible except document accessible doesn't have
  // a caret.
  if (!IsDoc() && !FocusMgr()->IsFocused(this) &&
      (InteractiveState() & states::FOCUSABLE)) {
    return -1;
  }

  // Check cached value.
  int32_t caretOffset = -1;
  HyperTextAccessible* text = SelectionMgr()->AccessibleWithCaret(&caretOffset);

  // Use cached value if it corresponds to this accessible.
  if (caretOffset != -1) {
    if (text == this) return caretOffset;

    nsINode* textNode = text->GetNode();
    // Ignore offset if cached accessible isn't a text leaf.
    if (nsCoreUtils::IsAncestorOf(GetNode(), textNode)) {
      return TransformOffset(text, textNode->IsText() ? caretOffset : 0, false);
    }
  }

  // No caret if the focused node is not inside this DOM node and this DOM node
  // is not inside of focused node.
  FocusManager::FocusDisposition focusDisp =
      FocusMgr()->IsInOrContainsFocus(this);
  if (focusDisp == FocusManager::eNone) return -1;

  // Turn the focus node and offset of the selection into caret hypretext
  // offset.
  dom::Selection* domSel = DOMSelection();
  NS_ENSURE_TRUE(domSel, -1);

  nsINode* focusNode = domSel->GetFocusNode();
  uint32_t focusOffset = domSel->FocusOffset();

  // No caret if this DOM node is inside of focused node but the selection's
  // focus point is not inside of this DOM node.
  if (focusDisp == FocusManager::eContainedByFocus) {
    nsINode* resultNode =
        nsCoreUtils::GetDOMNodeFromDOMPoint(focusNode, focusOffset);

    nsINode* thisNode = GetNode();
    if (resultNode != thisNode &&
        !nsCoreUtils::IsAncestorOf(thisNode, resultNode)) {
      return -1;
    }
  }

  return DOMPointToOffset(focusNode, focusOffset);
}

int32_t HyperTextAccessible::CaretLineNumber() {
  // Provide the line number for the caret, relative to the
  // currently focused node. Use a 1-based index
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection) return -1;

  dom::Selection* domSel = frameSelection->GetSelection(SelectionType::eNormal);
  if (!domSel) return -1;

  nsINode* caretNode = domSel->GetFocusNode();
  if (!caretNode || !caretNode->IsContent()) return -1;

  nsIContent* caretContent = caretNode->AsContent();
  if (!nsCoreUtils::IsAncestorOf(GetNode(), caretContent)) return -1;

  int32_t returnOffsetUnused;
  uint32_t caretOffset = domSel->FocusOffset();
  CaretAssociationHint hint = frameSelection->GetHint();
  nsIFrame* caretFrame = frameSelection->GetFrameForNodeOffset(
      caretContent, caretOffset, hint, &returnOffsetUnused);
  NS_ENSURE_TRUE(caretFrame, -1);

  AutoAssertNoDomMutations guard;  // The nsILineIterators below will break if
                                   // the DOM is modified while they're in use!
  int32_t lineNumber = 1;
  nsILineIterator* lineIterForCaret = nullptr;
  nsIContent* hyperTextContent = IsContent() ? mContent.get() : nullptr;
  while (caretFrame) {
    if (hyperTextContent == caretFrame->GetContent()) {
      return lineNumber;  // Must be in a single line hyper text, there is no
                          // line iterator
    }
    nsContainerFrame* parentFrame = caretFrame->GetParent();
    if (!parentFrame) break;

    // Add lines for the sibling frames before the caret
    nsIFrame* sibling = parentFrame->PrincipalChildList().FirstChild();
    while (sibling && sibling != caretFrame) {
      nsILineIterator* lineIterForSibling = sibling->GetLineIterator();
      if (lineIterForSibling) {
        // For the frames before that grab all the lines
        int32_t addLines = lineIterForSibling->GetNumLines();
        lineNumber += addLines;
      }
      sibling = sibling->GetNextSibling();
    }

    // Get the line number relative to the container with lines
    if (!lineIterForCaret) {  // Add the caret line just once
      lineIterForCaret = parentFrame->GetLineIterator();
      if (lineIterForCaret) {
        // Ancestor of caret
        int32_t addLines = lineIterForCaret->FindLineContaining(caretFrame);
        lineNumber += addLines;
      }
    }

    caretFrame = parentFrame;
  }

  MOZ_ASSERT_UNREACHABLE(
      "DOM ancestry had this hypertext but frame ancestry didn't");
  return lineNumber;
}

LayoutDeviceIntRect HyperTextAccessible::GetCaretRect(nsIWidget** aWidget) {
  *aWidget = nullptr;

  RefPtr<nsCaret> caret = mDoc->PresShellPtr()->GetCaret();
  NS_ENSURE_TRUE(caret, LayoutDeviceIntRect());

  bool isVisible = caret->IsVisible();
  if (!isVisible) return LayoutDeviceIntRect();

  nsRect rect;
  nsIFrame* frame = caret->GetGeometry(&rect);
  if (!frame || rect.IsEmpty()) return LayoutDeviceIntRect();

  nsPoint offset;
  // Offset from widget origin to the frame origin, which includes chrome
  // on the widget.
  *aWidget = frame->GetNearestWidget(offset);
  NS_ENSURE_TRUE(*aWidget, LayoutDeviceIntRect());
  rect.MoveBy(offset);

  LayoutDeviceIntRect caretRect = LayoutDeviceIntRect::FromUnknownRect(
      rect.ToOutsidePixels(frame->PresContext()->AppUnitsPerDevPixel()));
  // clang-format off
  // ((content screen origin) - (content offset in the widget)) = widget origin on the screen
  // clang-format on
  caretRect.MoveBy((*aWidget)->WidgetToScreenOffset() -
                   (*aWidget)->GetClientOffset());

  // Correct for character size, so that caret always matches the size of
  // the character. This is important for font size transitions, and is
  // necessary because the Gecko caret uses the previous character's size as
  // the user moves forward in the text by character.
  int32_t caretOffset = CaretOffset();
  if (NS_WARN_IF(caretOffset == -1)) {
    // The caret offset will be -1 if this Accessible isn't focused. Note that
    // the DOM node contaning the caret might be focused, but the Accessible
    // might not be; e.g. due to an autocomplete popup suggestion having a11y
    // focus.
    return LayoutDeviceIntRect();
  }
  LayoutDeviceIntRect charRect = CharBounds(
      caretOffset, nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE);
  if (!charRect.IsEmpty()) {
    caretRect.SetTopEdge(charRect.Y());
  }
  return caretRect;
}

void HyperTextAccessible::GetSelectionDOMRanges(SelectionType aSelectionType,
                                                nsTArray<nsRange*>* aRanges) {
  // Ignore selection if it is not visible.
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection || frameSelection->GetDisplaySelection() <=
                             nsISelectionController::SELECTION_HIDDEN) {
    return;
  }

  dom::Selection* domSel = frameSelection->GetSelection(aSelectionType);
  if (!domSel) return;

  nsINode* startNode = GetNode();

  RefPtr<EditorBase> editorBase = GetEditor();
  if (editorBase) {
    startNode = editorBase->GetRoot();
  }

  if (!startNode) return;

  uint32_t childCount = startNode->GetChildCount();
  nsresult rv = domSel->GetRangesForIntervalArray(startNode, 0, startNode,
                                                  childCount, true, aRanges);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Remove collapsed ranges
  aRanges->RemoveElementsBy(
      [](const auto& range) { return range->Collapsed(); });
}

int32_t HyperTextAccessible::SelectionCount() {
  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(SelectionType::eNormal, &ranges);
  return ranges.Length();
}

bool HyperTextAccessible::SelectionBoundsAt(int32_t aSelectionNum,
                                            int32_t* aStartOffset,
                                            int32_t* aEndOffset) {
  *aStartOffset = *aEndOffset = 0;

  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(SelectionType::eNormal, &ranges);

  uint32_t rangeCount = ranges.Length();
  if (aSelectionNum < 0 || aSelectionNum >= static_cast<int32_t>(rangeCount)) {
    return false;
  }

  nsRange* range = ranges[aSelectionNum];

  // Get start and end points.
  nsINode* startNode = range->GetStartContainer();
  nsINode* endNode = range->GetEndContainer();
  uint32_t startOffset = range->StartOffset();
  uint32_t endOffset = range->EndOffset();

  // Make sure start is before end, by swapping DOM points.  This occurs when
  // the user selects backwards in the text.
  const Maybe<int32_t> order =
      nsContentUtils::ComparePoints(endNode, endOffset, startNode, startOffset);

  if (!order) {
    MOZ_ASSERT_UNREACHABLE();
    return false;
  }

  if (*order < 0) {
    std::swap(startNode, endNode);
    std::swap(startOffset, endOffset);
  }

  if (!startNode->IsInclusiveDescendantOf(mContent)) {
    *aStartOffset = 0;
  } else {
    *aStartOffset =
        DOMPointToOffset(startNode, AssertedCast<int32_t>(startOffset));
  }

  if (!endNode->IsInclusiveDescendantOf(mContent)) {
    *aEndOffset = CharacterCount();
  } else {
    *aEndOffset =
        DOMPointToOffset(endNode, AssertedCast<int32_t>(endOffset), true);
  }
  return true;
}

bool HyperTextAccessible::SetSelectionBoundsAt(int32_t aSelectionNum,
                                               int32_t aStartOffset,
                                               int32_t aEndOffset) {
  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  if (!startOffset.IsValid() || !endOffset.IsValid() ||
      std::max(startOffset, endOffset) > CharacterCount()) {
    NS_ERROR("Wrong in offset");
    return false;
  }

  TextRange range(this, this, startOffset, this, endOffset);
  return range.SetSelectionAt(aSelectionNum);
}

bool HyperTextAccessible::RemoveFromSelection(int32_t aSelectionNum) {
  RefPtr<dom::Selection> domSel = DOMSelection();
  if (!domSel) return false;

  if (aSelectionNum < 0 ||
      aSelectionNum >= static_cast<int32_t>(domSel->RangeCount())) {
    return false;
  }

  const RefPtr<nsRange> range{
      domSel->GetRangeAt(static_cast<uint32_t>(aSelectionNum))};
  domSel->RemoveRangeAndUnselectFramesAndNotifyListeners(*range,
                                                         IgnoreErrors());
  return true;
}

void HyperTextAccessible::ScrollSubstringTo(int32_t aStartOffset,
                                            int32_t aEndOffset,
                                            uint32_t aScrollType) {
  TextRange range(this, this, aStartOffset, this, aEndOffset);
  range.ScrollIntoView(aScrollType);
}

void HyperTextAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                                 int32_t aEndOffset,
                                                 uint32_t aCoordinateType,
                                                 int32_t aX, int32_t aY) {
  nsIFrame* frame = GetFrame();
  if (!frame) return;

  LayoutDeviceIntPoint coords =
      nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType, this);

  RefPtr<nsRange> domRange = nsRange::Create(mContent);
  TextRange range(this, this, aStartOffset, this, aEndOffset);
  if (!range.AssignDOMRange(domRange)) {
    return;
  }

  nsPresContext* presContext = frame->PresContext();
  nsPoint coordsInAppUnits = LayoutDeviceIntPoint::ToAppUnits(
      coords, presContext->AppUnitsPerDevPixel());

  bool initialScrolled = false;
  nsIFrame* parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent())) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(parentFrame);
    if (scrollableFrame) {
      if (!initialScrolled) {
        // Scroll substring to the given point. Turn the point into percents
        // relative scrollable area to use nsCoreUtils::ScrollSubstringTo.
        nsRect frameRect = parentFrame->GetScreenRectInAppUnits();
        nscoord offsetPointX = coordsInAppUnits.x - frameRect.X();
        nscoord offsetPointY = coordsInAppUnits.y - frameRect.Y();

        nsSize size(parentFrame->GetSize());

        // avoid divide by zero
        size.width = size.width ? size.width : 1;
        size.height = size.height ? size.height : 1;

        int16_t hPercent = offsetPointX * 100 / size.width;
        int16_t vPercent = offsetPointY * 100 / size.height;

        nsresult rv = nsCoreUtils::ScrollSubstringTo(
            frame, domRange, ScrollAxis(vPercent, WhenToScroll::Always),
            ScrollAxis(hPercent, WhenToScroll::Always));
        if (NS_FAILED(rv)) return;

        initialScrolled = true;
      } else {
        // Substring was scrolled to the given point already inside its closest
        // scrollable area. If there are nested scrollable areas then make
        // sure we scroll lower areas to the given point inside currently
        // traversed scrollable area.
        nsCoreUtils::ScrollFrameToPoint(parentFrame, frame, coords);
      }
    }
    frame = parentFrame;
  }
}

void HyperTextAccessible::EnclosingRange(a11y::TextRange& aRange) const {
  if (IsTextField()) {
    aRange.Set(mDoc, const_cast<HyperTextAccessible*>(this), 0,
               const_cast<HyperTextAccessible*>(this), CharacterCount());
  } else {
    aRange.Set(mDoc, mDoc, 0, mDoc, mDoc->CharacterCount());
  }
}

void HyperTextAccessible::SelectionRanges(
    nsTArray<a11y::TextRange>* aRanges) const {
  dom::Selection* sel = DOMSelection();
  if (!sel) {
    return;
  }

  TextRange::TextRangesFromSelection(sel, aRanges);
}

void HyperTextAccessible::VisibleRanges(
    nsTArray<a11y::TextRange>* aRanges) const {}

void HyperTextAccessible::RangeByChild(LocalAccessible* aChild,
                                       a11y::TextRange& aRange) const {
  HyperTextAccessible* ht = aChild->AsHyperText();
  if (ht) {
    aRange.Set(mDoc, ht, 0, ht, ht->CharacterCount());
    return;
  }

  LocalAccessible* child = aChild;
  LocalAccessible* parent = nullptr;
  while ((parent = child->LocalParent()) && !(ht = parent->AsHyperText())) {
    child = parent;
  }

  // If no text then return collapsed text range, otherwise return a range
  // containing the text enclosed by the given child.
  if (ht) {
    int32_t childIdx = child->IndexInParent();
    int32_t startOffset = ht->GetChildOffset(childIdx);
    int32_t endOffset =
        child->IsTextLeaf() ? ht->GetChildOffset(childIdx + 1) : startOffset;
    aRange.Set(mDoc, ht, startOffset, ht, endOffset);
  }
}

void HyperTextAccessible::RangeAtPoint(int32_t aX, int32_t aY,
                                       a11y::TextRange& aRange) const {
  LocalAccessible* child =
      mDoc->LocalChildAtPoint(aX, aY, EWhichChildAtPoint::DeepestChild);
  if (!child) return;

  LocalAccessible* parent = nullptr;
  while ((parent = child->LocalParent()) && !parent->IsHyperText()) {
    child = parent;
  }

  // Return collapsed text range for the point.
  if (parent) {
    HyperTextAccessible* ht = parent->AsHyperText();
    int32_t offset = ht->GetChildOffset(child);
    aRange.Set(mDoc, ht, offset, ht, offset);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public

// LocalAccessible protected
ENameValueFlag HyperTextAccessible::NativeName(nsString& aName) const {
  // Check @alt attribute for invalid img elements.
  bool hasImgAlt = false;
  if (mContent->IsHTMLElement(nsGkAtoms::img)) {
    hasImgAlt = mContent->AsElement()->GetAttr(kNameSpaceID_None,
                                               nsGkAtoms::alt, aName);
    if (!aName.IsEmpty()) return eNameOK;
  }

  ENameValueFlag nameFlag = AccessibleWrap::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  // Get name from title attribute for HTML abbr and acronym elements making it
  // a valid name from markup. Otherwise their name isn't picked up by recursive
  // name computation algorithm. See NS_OK_NAME_FROM_TOOLTIP.
  if (IsAbbreviation() && mContent->AsElement()->GetAttr(
                              kNameSpaceID_None, nsGkAtoms::title, aName)) {
    aName.CompressWhitespace();
  }

  return hasImgAlt ? eNoNameOnPurpose : eNameOK;
}

void HyperTextAccessible::Shutdown() {
  mOffsets.Clear();
  AccessibleWrap::Shutdown();
}

bool HyperTextAccessible::RemoveChild(LocalAccessible* aAccessible) {
  InvalidateCachedHyperTextOffsets();
  return AccessibleWrap::RemoveChild(aAccessible);
}

bool HyperTextAccessible::InsertChildAt(uint32_t aIndex,
                                        LocalAccessible* aChild) {
  InvalidateCachedHyperTextOffsets();
  return AccessibleWrap::InsertChildAt(aIndex, aChild);
}

Relation HyperTextAccessible::RelationByType(RelationType aType) const {
  Relation rel = LocalAccessible::RelationByType(aType);

  switch (aType) {
    case RelationType::NODE_CHILD_OF:
      if (HasOwnContent() && mContent->IsMathMLElement()) {
        LocalAccessible* parent = LocalParent();
        if (parent) {
          nsIContent* parentContent = parent->GetContent();
          if (parentContent &&
              parentContent->IsMathMLElement(nsGkAtoms::mroot_)) {
            // Add a relation pointing to the parent <mroot>.
            rel.AppendTarget(parent);
          }
        }
      }
      break;
    case RelationType::NODE_PARENT_OF:
      if (HasOwnContent() && mContent->IsMathMLElement(nsGkAtoms::mroot_)) {
        LocalAccessible* base = LocalChildAt(0);
        LocalAccessible* index = LocalChildAt(1);
        if (base && index) {
          // Append the <mroot> children in the order index, base.
          rel.AppendTarget(index);
          rel.AppendTarget(base);
        }
      }
      break;
    default:
      break;
  }

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible public static

nsresult HyperTextAccessible::ContentToRenderedOffset(
    nsIFrame* aFrame, int32_t aContentOffset, uint32_t* aRenderedOffset) const {
  if (!aFrame) {
    // Current frame not rendered -- this can happen if text is set on
    // something with display: none
    *aRenderedOffset = 0;
    return NS_OK;
  }

  if (IsTextField()) {
    *aRenderedOffset = aContentOffset;
    return NS_OK;
  }

  NS_ASSERTION(aFrame->IsTextFrame(), "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  nsIFrame::RenderedText text =
      aFrame->GetRenderedText(aContentOffset, aContentOffset + 1,
                              nsIFrame::TextOffsetType::OffsetsInContentText,
                              nsIFrame::TrailingWhitespace::DontTrim);
  *aRenderedOffset = text.mOffsetWithinNodeRenderedText;

  return NS_OK;
}

nsresult HyperTextAccessible::RenderedToContentOffset(
    nsIFrame* aFrame, uint32_t aRenderedOffset, int32_t* aContentOffset) const {
  if (IsTextField()) {
    *aContentOffset = aRenderedOffset;
    return NS_OK;
  }

  *aContentOffset = 0;
  NS_ENSURE_TRUE(aFrame, NS_ERROR_FAILURE);

  NS_ASSERTION(aFrame->IsTextFrame(), "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  nsIFrame::RenderedText text =
      aFrame->GetRenderedText(aRenderedOffset, aRenderedOffset + 1,
                              nsIFrame::TextOffsetType::OffsetsInRenderedText,
                              nsIFrame::TrailingWhitespace::DontTrim);
  *aContentOffset = text.mOffsetWithinNodeText;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible protected

nsresult HyperTextAccessible::GetDOMPointByFrameOffset(
    nsIFrame* aFrame, int32_t aOffset, LocalAccessible* aAccessible,
    DOMPoint* aPoint) {
  NS_ENSURE_ARG(aAccessible);

  if (!aFrame) {
    // If the given frame is null then set offset after the DOM node of the
    // given accessible.
    NS_ASSERTION(!aAccessible->IsDoc(),
                 "Shouldn't be called on document accessible!");

    nsIContent* content = aAccessible->GetContent();
    NS_ASSERTION(content, "Shouldn't operate on defunct accessible!");

    nsIContent* parent = content->GetParent();

    aPoint->idx = parent->ComputeIndexOf_Deprecated(content) + 1;
    aPoint->node = parent;

  } else if (aFrame->IsTextFrame()) {
    nsIContent* content = aFrame->GetContent();
    NS_ENSURE_STATE(content);

    nsIFrame* primaryFrame = content->GetPrimaryFrame();
    nsresult rv =
        RenderedToContentOffset(primaryFrame, aOffset, &(aPoint->idx));
    NS_ENSURE_SUCCESS(rv, rv);

    aPoint->node = content;

  } else {
    nsIContent* content = aFrame->GetContent();
    NS_ENSURE_STATE(content);

    nsIContent* parent = content->GetParent();
    NS_ENSURE_STATE(parent);

    aPoint->idx = parent->ComputeIndexOf_Deprecated(content);
    aPoint->node = parent;
  }

  return NS_OK;
}

// HyperTextAccessible
void HyperTextAccessible::GetSpellTextAttr(nsINode* aNode, uint32_t aNodeOffset,
                                           uint32_t* aStartOffset,
                                           uint32_t* aEndOffset,
                                           AccAttributes* aAttributes) {
  RefPtr<nsFrameSelection> fs = FrameSelection();
  if (!fs) return;

  dom::Selection* domSel = fs->GetSelection(SelectionType::eSpellCheck);
  if (!domSel) return;

  const uint32_t rangeCount = domSel->RangeCount();
  if (!rangeCount) {
    return;
  }

  uint32_t startOffset = 0, endOffset = 0;
  for (const uint32_t idx : IntegerRange(rangeCount)) {
    MOZ_ASSERT(domSel->RangeCount() == rangeCount);
    const nsRange* range = domSel->GetRangeAt(idx);
    MOZ_ASSERT(range);
    if (range->Collapsed()) continue;

    // See if the point comes after the range in which case we must continue in
    // case there is another range after this one.
    nsINode* endNode = range->GetEndContainer();
    uint32_t endNodeOffset = range->EndOffset();
    Maybe<int32_t> order = nsContentUtils::ComparePoints(
        aNode, aNodeOffset, endNode, endNodeOffset);
    if (NS_WARN_IF(!order)) {
      continue;
    }

    if (*order >= 0) {
      continue;
    }

    // At this point our point is either in this range or before it but after
    // the previous range.  So we check to see if the range starts before the
    // point in which case the point is in the missspelled range, otherwise it
    // must be before the range and after the previous one if any.
    nsINode* startNode = range->GetStartContainer();
    int32_t startNodeOffset = range->StartOffset();
    order = nsContentUtils::ComparePoints(startNode, startNodeOffset, aNode,
                                          aNodeOffset);
    if (!order) {
      // As (`aNode`, `aNodeOffset`) is comparable to the end of the range, it
      // should also be comparable to the range's start. Returning here
      // prevents crashes in release builds.
      MOZ_ASSERT_UNREACHABLE();
      return;
    }

    if (*order <= 0) {
      startOffset = DOMPointToOffset(startNode, startNodeOffset);

      endOffset = DOMPointToOffset(endNode, endNodeOffset);

      if (startOffset > *aStartOffset) *aStartOffset = startOffset;

      if (endOffset < *aEndOffset) *aEndOffset = endOffset;

      aAttributes->SetAttribute(nsGkAtoms::invalid, nsGkAtoms::spelling);

      return;
    }

    // This range came after the point.
    endOffset = DOMPointToOffset(startNode, startNodeOffset);

    if (idx > 0) {
      const nsRange* prevRange = domSel->GetRangeAt(idx - 1);
      startOffset = DOMPointToOffset(prevRange->GetEndContainer(),
                                     prevRange->EndOffset());
    }

    // The previous range might not be within this accessible. In that case,
    // DOMPointToOffset returns length as a fallback. We don't want to use
    // that offset if so, hence the startOffset < *aEndOffset check.
    if (startOffset > *aStartOffset && startOffset < *aEndOffset) {
      *aStartOffset = startOffset;
    }

    if (endOffset < *aEndOffset) *aEndOffset = endOffset;

    return;
  }

  // We never found a range that ended after the point, therefore we know that
  // the point is not in a range, that we do not need to compute an end offset,
  // and that we should use the end offset of the last range to compute the
  // start offset of the text attribute range.
  const nsRange* prevRange = domSel->GetRangeAt(rangeCount - 1);
  startOffset =
      DOMPointToOffset(prevRange->GetEndContainer(), prevRange->EndOffset());

  // The previous range might not be within this accessible. In that case,
  // DOMPointToOffset returns length as a fallback. We don't want to use
  // that offset if so, hence the startOffset < *aEndOffset check.
  if (startOffset > *aStartOffset && startOffset < *aEndOffset) {
    *aStartOffset = startOffset;
  }
}

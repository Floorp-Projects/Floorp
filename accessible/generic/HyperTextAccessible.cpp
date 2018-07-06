/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessible-inl.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsIAccessibleTypes.h"
#include "DocAccessible.h"
#include "HTMLListAccessible.h"
#include "Role.h"
#include "States.h"
#include "TextAttrs.h"
#include "TextRange.h"
#include "TreeWalker.h"

#include "nsCaret.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIEditingSession.h"
#include "nsContainerFrame.h"
#include "nsFrameSelection.h"
#include "nsILineIterator.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPersistentProperties2.h"
#include "nsIScrollableFrame.h"
#include "nsIServiceManager.h"
#include "nsITextControlElement.h"
#include "nsIMathMLFrame.h"
#include "nsRange.h"
#include "nsTextFragment.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/dom/Element.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/TextEditor.h"
#include "gfxSkipChars.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible
////////////////////////////////////////////////////////////////////////////////

HyperTextAccessible::
  HyperTextAccessible(nsIContent* aNode, DocAccessible* aDoc) :
  AccessibleWrap(aNode, aDoc)
{
  mType = eHyperTextType;
  mGenericTypes |= eHyperText;
}

role
HyperTextAccessible::NativeRole() const
{
  a11y::role r = GetAccService()->MarkupRole(mContent);
  if (r != roles::NOTHING)
    return r;

  nsIFrame* frame = GetFrame();
  if (frame && frame->IsInlineFrame())
    return roles::TEXT;

  return roles::TEXT_CONTAINER;
}

uint64_t
HyperTextAccessible::NativeState() const
{
  uint64_t states = AccessibleWrap::NativeState();

  if (mContent->AsElement()->State().HasState(NS_EVENT_STATE_MOZ_READWRITE)) {
    states |= states::EDITABLE;

  } else if (mContent->IsHTMLElement(nsGkAtoms::article)) {
    // We want <article> to behave like a document in terms of readonly state.
    states |= states::READONLY;
  }

  if (HasChildren())
    states |= states::SELECTABLE_TEXT;

  return states;
}

nsIntRect
HyperTextAccessible::GetBoundsInFrame(nsIFrame* aFrame,
                                      uint32_t aStartRenderedOffset,
                                      uint32_t aEndRenderedOffset)
{
  nsPresContext* presContext = mDoc->PresContext();
  if (!aFrame->IsTextFrame()) {
    return aFrame->GetScreenRectInAppUnits().
      ToNearestPixels(presContext->AppUnitsPerDevPixel());
  }

  // Substring must be entirely within the same text node.
  int32_t startContentOffset, endContentOffset;
  nsresult rv = RenderedToContentOffset(aFrame, aStartRenderedOffset, &startContentOffset);
  NS_ENSURE_SUCCESS(rv, nsIntRect());
  rv = RenderedToContentOffset(aFrame, aEndRenderedOffset, &endContentOffset);
  NS_ENSURE_SUCCESS(rv, nsIntRect());

  nsIFrame *frame;
  int32_t startContentOffsetInFrame;
  // Get the right frame continuation -- not really a child, but a sibling of
  // the primary frame passed in
  rv = aFrame->GetChildFrameContainingOffset(startContentOffset, false,
                                             &startContentOffsetInFrame, &frame);
  NS_ENSURE_SUCCESS(rv, nsIntRect());

  nsRect screenRect;
  while (frame && startContentOffset < endContentOffset) {
    // Start with this frame's screen rect, which we will shrink based on
    // the substring we care about within it. We will then add that frame to
    // the total screenRect we are returning.
    nsRect frameScreenRect = frame->GetScreenRectInAppUnits();

    // Get the length of the substring in this frame that we want the bounds for
    int32_t startFrameTextOffset, endFrameTextOffset;
    frame->GetOffsets(startFrameTextOffset, endFrameTextOffset);
    int32_t frameTotalTextLength = endFrameTextOffset - startFrameTextOffset;
    int32_t seekLength = endContentOffset - startContentOffset;
    int32_t frameSubStringLength = std::min(frameTotalTextLength - startContentOffsetInFrame, seekLength);

    // Add the point where the string starts to the frameScreenRect
    nsPoint frameTextStartPoint;
    rv = frame->GetPointFromOffset(startContentOffset, &frameTextStartPoint);
    NS_ENSURE_SUCCESS(rv, nsIntRect());

    // Use the point for the end offset to calculate the width
    nsPoint frameTextEndPoint;
    rv = frame->GetPointFromOffset(startContentOffset + frameSubStringLength, &frameTextEndPoint);
    NS_ENSURE_SUCCESS(rv, nsIntRect());

    frameScreenRect.SetRectX(frameScreenRect.X() + std::min(frameTextStartPoint.x, frameTextEndPoint.x),
                             mozilla::Abs(frameTextStartPoint.x - frameTextEndPoint.x));

    screenRect.UnionRect(frameScreenRect, screenRect);

    // Get ready to loop back for next frame continuation
    startContentOffset += frameSubStringLength;
    startContentOffsetInFrame = 0;
    frame = frame->GetNextContinuation();
  }

  return screenRect.ToNearestPixels(presContext->AppUnitsPerDevPixel());
}

void
HyperTextAccessible::TextSubstring(int32_t aStartOffset, int32_t aEndOffset,
                                   nsAString& aText)
{
  aText.Truncate();

  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  if (!startOffset.IsValid() || !endOffset.IsValid() ||
      startOffset > endOffset || endOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset");
    return;
  }

  int32_t startChildIdx = GetChildIndexAtOffset(startOffset);
  if (startChildIdx == -1)
    return;

  int32_t endChildIdx = GetChildIndexAtOffset(endOffset);
  if (endChildIdx == -1)
    return;

  if (startChildIdx == endChildIdx) {
    int32_t childOffset =  GetChildOffset(startChildIdx);
    if (childOffset == -1)
      return;

    Accessible* child = GetChildAt(startChildIdx);
    child->AppendTextTo(aText, startOffset - childOffset,
                        endOffset - startOffset);
    return;
  }

  int32_t startChildOffset =  GetChildOffset(startChildIdx);
  if (startChildOffset == -1)
    return;

  Accessible* startChild = GetChildAt(startChildIdx);
  startChild->AppendTextTo(aText, startOffset - startChildOffset);

  for (int32_t childIdx = startChildIdx + 1; childIdx < endChildIdx; childIdx++) {
    Accessible* child = GetChildAt(childIdx);
    child->AppendTextTo(aText);
  }

  int32_t endChildOffset =  GetChildOffset(endChildIdx);
  if (endChildOffset == -1)
    return;

  Accessible* endChild = GetChildAt(endChildIdx);
  endChild->AppendTextTo(aText, 0, endOffset - endChildOffset);
}

uint32_t
HyperTextAccessible::DOMPointToOffset(nsINode* aNode, int32_t aNodeOffset,
                                      bool aIsEndOffset) const
{
  if (!aNode)
    return 0;

  uint32_t offset = 0;
  nsINode* findNode = nullptr;

  if (aNodeOffset == -1) {
    findNode = aNode;

  } else if (aNode->IsText()) {
    // For text nodes, aNodeOffset comes in as a character offset
    // Text offset will be added at the end, if we find the offset in this hypertext
    // We want the "skipped" offset into the text (rendered text without the extra whitespace)
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
  // accessible for the next DOM node which has one (based on forward depth first search)
  Accessible* descendant = nullptr;
  if (findNode) {
    nsCOMPtr<nsIContent> findContent(do_QueryInterface(findNode));
    if (findContent && findContent->IsHTMLElement(nsGkAtoms::br) &&
        findContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                              nsGkAtoms::mozeditorbogusnode,
                                              nsGkAtoms::_true,
                                              eIgnoreCase)) {
      // This <br> is the hacky "bogus node" used when there is no text in a control
      return 0;
    }

    descendant = mDoc->GetAccessible(findNode);
    if (!descendant && findNode->IsContent()) {
      Accessible* container = mDoc->GetContainerAccessible(findNode);
      if (container) {
        TreeWalker walker(container, findNode->AsContent(),
                          TreeWalker::eWalkContextTree);
        descendant = walker.Next();
        if (!descendant)
          descendant = container;
      }
    }
  }

  return TransformOffset(descendant, offset, aIsEndOffset);
}

uint32_t
HyperTextAccessible::TransformOffset(Accessible* aDescendant,
                                     uint32_t aOffset, bool aIsEndOffset) const
{
  // From the descendant, go up and get the immediate child of this hypertext.
  uint32_t offset = aOffset;
  Accessible* descendant = aDescendant;
  while (descendant) {
    Accessible* parent = descendant->Parent();
    if (parent == this)
      return GetChildOffset(descendant) + offset;

    // This offset no longer applies because the passed-in text object is not
    // a child of the hypertext. This happens when there are nested hypertexts,
    // e.g. <div>abc<h1>def</h1>ghi</div>. Thus we need to adjust the offset
    // to make it relative the hypertext.
    // If the end offset is not supposed to be inclusive and the original point
    // is not at 0 offset then the returned offset should be after an embedded
    // character the original point belongs to.
    if (aIsEndOffset)
      offset = (offset > 0 || descendant->IndexInParent() > 0) ? 1 : 0;
    else
      offset = 0;

    descendant = parent;
  }

  // If the given a11y point cannot be mapped into offset relative this hypertext
  // offset then return length as fallback value.
  return CharacterCount();
}

/**
 * GetElementAsContentOf() returns a content representing an element which is
 * or includes aNode.
 *
 * XXX This method is enough to retrieve ::before or ::after pseudo element.
 *     So, if you want to use this for other purpose, you might need to check
 *     ancestors too.
 */
static nsIContent* GetElementAsContentOf(nsINode* aNode)
{
  if (Element* element = Element::FromNode(aNode)) {
    return element;
  }
  return aNode->GetParentElement();
}

bool
HyperTextAccessible::OffsetsToDOMRange(int32_t aStartOffset, int32_t aEndOffset,
                                       nsRange* aRange)
{
  DOMPoint startPoint = OffsetToDOMPoint(aStartOffset);
  if (!startPoint.node)
    return false;

  // HyperTextAccessible manages pseudo elements generated by ::before or
  // ::after.  However, contents of them are not in the DOM tree normally.
  // Therefore, they are not selectable and editable.  So, when this creates
  // a DOM range, it should not start from nor end in any pseudo contents.

  nsIContent* container = GetElementAsContentOf(startPoint.node);
  DOMPoint startPointForDOMRange =
    ClosestNotGeneratedDOMPoint(startPoint, container);
  aRange->SetStart(startPointForDOMRange.node, startPointForDOMRange.idx);

  // If the caller wants collapsed range, let's collapse the range to its start.
  if (aStartOffset == aEndOffset) {
    aRange->Collapse(true);
    return true;
  }

  DOMPoint endPoint = OffsetToDOMPoint(aEndOffset);
  if (!endPoint.node)
    return false;

  if (startPoint.node != endPoint.node) {
    container = GetElementAsContentOf(endPoint.node);
  }

  DOMPoint endPointForDOMRange =
    ClosestNotGeneratedDOMPoint(endPoint, container);
  aRange->SetEnd(endPointForDOMRange.node, endPointForDOMRange.idx);
  return true;
}

DOMPoint
HyperTextAccessible::OffsetToDOMPoint(int32_t aOffset)
{
  // 0 offset is valid even if no children. In this case the associated editor
  // is empty so return a DOM point for editor root element.
  if (aOffset == 0) {
    RefPtr<TextEditor> textEditor = GetEditor();
    if (textEditor) {
      bool isEmpty = false;
      textEditor->GetDocumentIsEmpty(&isEmpty);
      if (isEmpty) {
        return DOMPoint(textEditor->GetRoot(), 0);
      }
    }
  }

  int32_t childIdx = GetChildIndexAtOffset(aOffset);
  if (childIdx == -1)
    return DOMPoint();

  Accessible* child = GetChildAt(childIdx);
  int32_t innerOffset = aOffset - GetChildOffset(childIdx);

  // A text leaf case.
  if (child->IsTextLeaf()) {
    // The point is inside the text node. This is always true for any text leaf
    // except a last child one. See assertion below.
    if (aOffset < GetChildOffset(childIdx + 1)) {
      nsIContent* content = child->GetContent();
      int32_t idx = 0;
      if (NS_FAILED(RenderedToContentOffset(content->GetPrimaryFrame(),
                                            innerOffset, &idx)))
        return DOMPoint();

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
  return parentNode ?
    DOMPoint(parentNode, parentNode->ComputeIndexOf(node) + innerOffset) :
    DOMPoint();
}

DOMPoint
HyperTextAccessible::ClosestNotGeneratedDOMPoint(const DOMPoint& aDOMPoint,
                                                 nsIContent* aElementContent)
{
  MOZ_ASSERT(aDOMPoint.node, "The node must not be null");

  // ::before pseudo element
  if (aElementContent &&
      aElementContent->IsGeneratedContentContainerForBefore()) {
    MOZ_ASSERT(aElementContent->GetParent(),
               "::before must have parent element");
    // The first child of its parent (i.e., immediately after the ::before) is
    // good point for a DOM range.
    return DOMPoint(aElementContent->GetParent(), 0);
  }

  // ::after pseudo element
  if (aElementContent &&
      aElementContent->IsGeneratedContentContainerForAfter()) {
    MOZ_ASSERT(aElementContent->GetParent(),
               "::after must have parent element");
    // The end of its parent (i.e., immediately before the ::after) is good
    // point for a DOM range.
    return DOMPoint(aElementContent->GetParent(),
                    aElementContent->GetParent()->GetChildCount());
  }

  return aDOMPoint;
}

uint32_t
HyperTextAccessible::FindOffset(uint32_t aOffset, nsDirection aDirection,
                                nsSelectionAmount aAmount,
                                EWordMovementType aWordMovementType)
{
  NS_ASSERTION(aDirection == eDirPrevious || aAmount != eSelectBeginLine,
               "eSelectBeginLine should only be used with eDirPrevious");

  // Find a leaf accessible frame to start with. PeekOffset wants this.
  HyperTextAccessible* text = this;
  Accessible* child = nullptr;
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

    child = text->GetChildAt(childIdx);

    // HTML list items may need special processing because PeekOffset doesn't
    // work with list bullets.
    if (text->IsHTMLListItem()) {
      HTMLLIAccessible* li = text->AsHTMLListItem();
      if (child == li->Bullet()) {
        // XXX: the logic is broken for multichar bullets in moving by
        // char/cluster/word cases.
        if (text != this) {
          return aDirection == eDirPrevious ?
            TransformOffset(text, 0, false) :
            TransformOffset(text, 1, true);
        }
        if (aDirection == eDirPrevious)
          return 0;

        uint32_t nextOffset = GetChildOffset(1);
        if (nextOffset == 0)
          return 0;

        switch (aAmount) {
          case eSelectLine:
          case eSelectEndLine:
            // Ask a text leaf next (if not empty) to the bullet for an offset
            // since list item may be multiline.
            return nextOffset < CharacterCount() ?
              FindOffset(nextOffset, aDirection, aAmount, aWordMovementType) :
              nextOffset;

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
  childFrame->GetChildFrameContainingOffset(innerContentOffset, true,
                                            &unusedOffsetInFrame,
                                            &frameAtOffset);

  const bool kIsJumpLinesOk = true; // okay to jump lines
  const bool kIsScrollViewAStop = false; // do not stop at scroll views
  const bool kIsKeyboardSelect = true; // is keyboard selection
  const bool kIsVisualBidi = false; // use visual order for bidi text
  nsPeekOffsetStruct pos(aAmount, aDirection, innerContentOffset,
                         nsPoint(0, 0), kIsJumpLinesOk, kIsScrollViewAStop,
                         kIsKeyboardSelect, kIsVisualBidi,
                         false, aWordMovementType);
  nsresult rv = frameAtOffset->PeekOffset(&pos);

  // PeekOffset fails on last/first lines of the text in certain cases.
  if (NS_FAILED(rv) && aAmount == eSelectLine) {
    pos.mAmount = (aDirection == eDirNext) ? eSelectEndLine : eSelectBeginLine;
    frameAtOffset->PeekOffset(&pos);
  }
  if (!pos.mResultContent) {
    NS_ERROR("No result content!");
    return 0;
  }

  // Turn the resulting DOM point into an offset.
  uint32_t hyperTextOffset = DOMPointToOffset(pos.mResultContent,
                                              pos.mContentOffset,
                                              aDirection == eDirNext);

  if (aDirection == eDirPrevious) {
    // If we reached the end during search, this means we didn't find the DOM point
    // and we're actually at the start of the paragraph
    if (hyperTextOffset == CharacterCount())
      return 0;

    // PeekOffset stops right before bullet so return 0 to workaround it.
    if (IsHTMLListItem() && aAmount == eSelectBeginLine &&
        hyperTextOffset > 0) {
      Accessible* prevOffsetChild = GetChildAtOffset(hyperTextOffset - 1);
      if (prevOffsetChild == AsHTMLListItem()->Bullet())
        return 0;
    }
  }

  return hyperTextOffset;
}

uint32_t
HyperTextAccessible::FindLineBoundary(uint32_t aOffset,
                                      EWhichLineBoundary aWhichLineBoundary)
{
  // Note: empty last line doesn't have own frame (a previous line contains '\n'
  // character instead) thus when it makes a difference we need to process this
  // case separately (otherwise operations are performed on previous line).
  switch (aWhichLineBoundary) {
    case ePrevLineBegin: {
      // Fetch a previous line and move to its start (as arrow up and home keys
      // were pressed).
      if (IsEmptyLastLineOffset(aOffset))
        return FindOffset(aOffset, eDirPrevious, eSelectBeginLine);

      uint32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      return FindOffset(tmpOffset, eDirPrevious, eSelectBeginLine);
    }

    case ePrevLineEnd: {
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset - 1;

      // If offset is at first line then return 0 (first line start).
      uint32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectBeginLine);
      if (tmpOffset == 0)
        return 0;

      // Otherwise move to end of previous line (as arrow up and end keys were
      // pressed).
      tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      return FindOffset(tmpOffset, eDirNext, eSelectEndLine);
    }

    case eThisLineBegin:
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to begin of the current line (as home key was pressed).
      return FindOffset(aOffset, eDirPrevious, eSelectBeginLine);

    case eThisLineEnd:
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to end of the current line (as end key was pressed).
      return FindOffset(aOffset, eDirNext, eSelectEndLine);

    case eNextLineBegin: {
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to begin of the next line if any (arrow down and home keys),
      // otherwise end of the current line (arrow down only).
      uint32_t tmpOffset = FindOffset(aOffset, eDirNext, eSelectLine);
      if (tmpOffset == CharacterCount())
        return tmpOffset;

      return FindOffset(tmpOffset, eDirPrevious, eSelectBeginLine);
    }

    case eNextLineEnd: {
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to next line end (as down arrow and end key were pressed).
      uint32_t tmpOffset = FindOffset(aOffset, eDirNext, eSelectLine);
      if (tmpOffset == CharacterCount())
        return tmpOffset;

      return FindOffset(tmpOffset, eDirNext, eSelectEndLine);
    }
  }

  return 0;
}

void
HyperTextAccessible::TextBeforeOffset(int32_t aOffset,
                                      AccessibleTextBoundary aBoundaryType,
                                      int32_t* aStartOffset, int32_t* aEndOffset,
                                      nsAString& aText)
{
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  index_t convertedOffset = ConvertMagicOffset(aOffset);
  if (!convertedOffset.IsValid() || convertedOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return;
  }

  uint32_t adjustedOffset = convertedOffset;
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
    adjustedOffset = AdjustCaretOffset(adjustedOffset);

  switch (aBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      if (convertedOffset != 0)
        CharAt(convertedOffset - 1, aText, aStartOffset, aEndOffset);
      break;

    case nsIAccessibleText::BOUNDARY_WORD_START: {
      // If the offset is a word start (except text length offset) then move
      // backward to find a start offset (end offset is the given offset).
      // Otherwise move backward twice to find both start and end offsets.
      if (adjustedOffset == CharacterCount()) {
        *aEndOffset = FindWordBoundary(adjustedOffset, eDirPrevious, eStartWord);
        *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
      } else {
        *aStartOffset = FindWordBoundary(adjustedOffset, eDirPrevious, eStartWord);
        *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eStartWord);
        if (*aEndOffset != static_cast<int32_t>(adjustedOffset)) {
          *aEndOffset = *aStartOffset;
          *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
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
      if (*aEndOffset != 0 && !IsLineEndCharAt(*aEndOffset))
        tmpOffset--;

      *aStartOffset = FindLineBoundary(tmpOffset, ePrevLineEnd);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
    }
  }
}

void
HyperTextAccessible::TextAtOffset(int32_t aOffset,
                                  AccessibleTextBoundary aBoundaryType,
                                  int32_t* aStartOffset, int32_t* aEndOffset,
                                  nsAString& aText)
{
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
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET && IsCaretAtEndOfLine())
        *aStartOffset = *aEndOffset = adjustedOffset;
      else
        CharAt(adjustedOffset, aText, aStartOffset, aEndOffset);
      break;

    case nsIAccessibleText::BOUNDARY_WORD_START:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
        adjustedOffset = AdjustCaretOffset(adjustedOffset);

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
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
        adjustedOffset = AdjustCaretOffset(adjustedOffset);

      *aStartOffset = FindLineBoundary(adjustedOffset, eThisLineBegin);
      *aEndOffset = FindLineBoundary(adjustedOffset, eNextLineBegin);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;

    case nsIAccessibleText::BOUNDARY_LINE_END:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
        adjustedOffset = AdjustCaretOffset(adjustedOffset);

      // In contrast to word end boundary we follow the spec here.
      *aStartOffset = FindLineBoundary(adjustedOffset, ePrevLineEnd);
      *aEndOffset = FindLineBoundary(adjustedOffset, eThisLineEnd);
      TextSubstring(*aStartOffset, *aEndOffset, aText);
      break;
  }
}

void
HyperTextAccessible::TextAfterOffset(int32_t aOffset,
                                     AccessibleTextBoundary aBoundaryType,
                                     int32_t* aStartOffset, int32_t* aEndOffset,
                                     nsAString& aText)
{
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  index_t convertedOffset = ConvertMagicOffset(aOffset);
  if (!convertedOffset.IsValid() || convertedOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return;
  }

  uint32_t adjustedOffset = convertedOffset;
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
    adjustedOffset = AdjustCaretOffset(adjustedOffset);

  switch (aBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      // If caret is at the end of wrapped line (case of no line end character)
      // then char after the offset is a first char at next line.
      if (adjustedOffset >= CharacterCount())
        *aStartOffset = *aEndOffset = CharacterCount();
      else
        CharAt(adjustedOffset + 1, aText, aStartOffset, aEndOffset);
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

already_AddRefed<nsIPersistentProperties>
HyperTextAccessible::TextAttributes(bool aIncludeDefAttrs, int32_t aOffset,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset)
{
  // 1. Get each attribute and its ranges one after another.
  // 2. As we get each new attribute, we pass the current start and end offsets
  //    as in/out parameters. In other words, as attributes are collected,
  //    the attribute range itself can only stay the same or get smaller.

  *aStartOffset = *aEndOffset = 0;
  index_t offset = ConvertMagicOffset(aOffset);
  if (!offset.IsValid() || offset > CharacterCount()) {
    NS_ERROR("Wrong in offset!");
    return nullptr;
  }

  nsCOMPtr<nsIPersistentProperties> attributes =
    do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);

  Accessible* accAtOffset = GetChildAtOffset(offset);
  if (!accAtOffset) {
    // Offset 0 is correct offset when accessible has empty text. Include
    // default attributes if they were requested, otherwise return empty set.
    if (offset == 0) {
      if (aIncludeDefAttrs) {
        TextAttrsMgr textAttrsMgr(this);
        textAttrsMgr.GetAttributes(attributes);
      }
      return attributes.forget();
    }
    return nullptr;
  }

  int32_t accAtOffsetIdx = accAtOffset->IndexInParent();
  uint32_t startOffset = GetChildOffset(accAtOffsetIdx);
  uint32_t endOffset = GetChildOffset(accAtOffsetIdx + 1);
  int32_t offsetInAcc = offset - startOffset;

  TextAttrsMgr textAttrsMgr(this, aIncludeDefAttrs, accAtOffset,
                            accAtOffsetIdx);
  textAttrsMgr.GetAttributes(attributes, &startOffset, &endOffset);

  // Compute spelling attributes on text accessible only.
  nsIFrame *offsetFrame = accAtOffset->GetFrame();
  if (offsetFrame && offsetFrame->IsTextFrame()) {
    int32_t nodeOffset = 0;
    RenderedToContentOffset(offsetFrame, offsetInAcc, &nodeOffset);

    // Set 'misspelled' text attribute.
    GetSpellTextAttr(accAtOffset->GetNode(), nodeOffset,
                     &startOffset, &endOffset, attributes);
  }

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;
  return attributes.forget();
}

already_AddRefed<nsIPersistentProperties>
HyperTextAccessible::DefaultTextAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);

  TextAttrsMgr textAttrsMgr(this);
  textAttrsMgr.GetAttributes(attributes);
  return attributes.forget();
}

int32_t
HyperTextAccessible::GetLevelInternal()
{
  if (mContent->IsHTMLElement(nsGkAtoms::h1))
    return 1;
  if (mContent->IsHTMLElement(nsGkAtoms::h2))
    return 2;
  if (mContent->IsHTMLElement(nsGkAtoms::h3))
    return 3;
  if (mContent->IsHTMLElement(nsGkAtoms::h4))
    return 4;
  if (mContent->IsHTMLElement(nsGkAtoms::h5))
    return 5;
  if (mContent->IsHTMLElement(nsGkAtoms::h6))
    return 6;

  return AccessibleWrap::GetLevelInternal();
}

void
HyperTextAccessible::SetMathMLXMLRoles(nsIPersistentProperties* aAttributes)
{
  // Add MathML xmlroles based on the position inside the parent.
  Accessible* parent = Parent();
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
            if (!PrevSibling()) {
              nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                                     nsGkAtoms::open_fence);
            } else if (!NextSibling()) {
              nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                                     nsGkAtoms::close_fence);
            }
          }
          if (NS_MATHML_EMBELLISH_IS_SEPARATOR(embellishData.flags)) {
            nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                                   nsGkAtoms::separator_);
          }
        }
      }
    break;
    case roles::MATHML_FRACTION:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             IndexInParent() == 0 ?
                             nsGkAtoms::numerator :
                             nsGkAtoms::denominator);
      break;
    case roles::MATHML_ROOT:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             IndexInParent() == 0 ? nsGkAtoms::base :
                             nsGkAtoms::root_index);
      break;
    case roles::MATHML_SUB:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             IndexInParent() == 0 ? nsGkAtoms::base :
                             nsGkAtoms::subscript);
      break;
    case roles::MATHML_SUP:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             IndexInParent() == 0 ? nsGkAtoms::base :
                             nsGkAtoms::superscript);
      break;
    case roles::MATHML_SUB_SUP: {
      int32_t index = IndexInParent();
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             index == 0 ? nsGkAtoms::base :
                             (index == 1 ? nsGkAtoms::subscript :
                              nsGkAtoms::superscript));
    } break;
    case roles::MATHML_UNDER:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             IndexInParent() == 0 ? nsGkAtoms::base :
                             nsGkAtoms::underscript);
      break;
    case roles::MATHML_OVER:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             IndexInParent() == 0 ? nsGkAtoms::base :
                             nsGkAtoms::overscript);
      break;
    case roles::MATHML_UNDER_OVER: {
      int32_t index = IndexInParent();
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                             index == 0 ? nsGkAtoms::base :
                             (index == 1 ? nsGkAtoms::underscript :
                              nsGkAtoms::overscript));
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
          nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                                 nsGkAtoms::base);
        } else {
          // Browse the list of scripts to find us and determine our type.
          bool postscript = true;
          bool subscript = true;
          for (child = child->GetNextSibling(); child;
               child = child->GetNextSibling()) {
            if (!child->IsMathMLElement())
              continue;
            if (child->IsMathMLElement(nsGkAtoms::mprescripts_)) {
              postscript = false;
              subscript = true;
              continue;
            }
            if (child == content) {
              if (postscript) {
                nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                                       subscript ?
                                       nsGkAtoms::subscript :
                                       nsGkAtoms::superscript);
              } else {
                nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::xmlroles,
                                       subscript ?
                                       nsGkAtoms::presubscript :
                                       nsGkAtoms::presuperscript);
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

already_AddRefed<nsIPersistentProperties>
HyperTextAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    AccessibleWrap::NativeAttributes();

  // 'formatting' attribute is deprecated, 'display' attribute should be
  // instead.
  nsIFrame *frame = GetFrame();
  if (frame && frame->IsBlockFrame()) {
    nsAutoString unused;
    attributes->SetStringProperty(NS_LITERAL_CSTRING("formatting"),
                                  NS_LITERAL_STRING("block"), unused);
  }

  if (FocusMgr()->IsFocused(this)) {
    int32_t lineNumber = CaretLineNumber();
    if (lineNumber >= 1) {
      nsAutoString strLineNumber;
      strLineNumber.AppendInt(lineNumber);
      nsAccUtils::SetAccAttr(attributes, nsGkAtoms::lineNumber, strLineNumber);
    }
  }

  if (HasOwnContent()) {
    GetAccService()->MarkupAttributes(mContent, attributes);
    if (mContent->IsMathMLElement())
      SetMathMLXMLRoles(attributes);
  }

  return attributes.forget();
}

nsAtom*
HyperTextAccessible::LandmarkRole() const
{
  if (!HasOwnContent())
    return nullptr;

  // For the html landmark elements we expose them like we do ARIA landmarks to
  // make AT navigation schemes "just work".
  if (mContent->IsHTMLElement(nsGkAtoms::nav)) {
    return nsGkAtoms::navigation;
  }

  if (mContent->IsHTMLElement(nsGkAtoms::aside)) {
    return nsGkAtoms::complementary;
  }

  if (mContent->IsHTMLElement(nsGkAtoms::main)) {
    return nsGkAtoms::main;
  }

  // Only return xml-roles "region" if the section has an accessible name.
  if (mContent->IsHTMLElement(nsGkAtoms::section)) {
    nsAutoString name;
    const_cast<HyperTextAccessible*>(this)->Name(name);
    return name.IsEmpty() ? nullptr : nsGkAtoms::region;
  }

  // Only return xml-roles "form" if the form has an accessible name.
  if (mContent->IsHTMLElement(nsGkAtoms::form)) {
    nsAutoString name;
    const_cast<HyperTextAccessible*>(this)->Name(name);
    return name.IsEmpty() ? nullptr : nsGkAtoms::form;
  }

  return nullptr;
}

int32_t
HyperTextAccessible::OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType)
{
  nsIFrame* hyperFrame = GetFrame();
  if (!hyperFrame)
    return -1;

  nsIntPoint coords = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordType,
                                                        this);

  nsPresContext* presContext = mDoc->PresContext();
  nsPoint coordsInAppUnits =
    ToAppUnits(coords, presContext->AppUnitsPerDevPixel());

  nsRect frameScreenRect = hyperFrame->GetScreenRectInAppUnits();
  if (!frameScreenRect.Contains(coordsInAppUnits.x, coordsInAppUnits.y))
    return -1; // Not found

  nsPoint pointInHyperText(coordsInAppUnits.x - frameScreenRect.X(),
                           coordsInAppUnits.y - frameScreenRect.Y());

  // Go through the frames to check if each one has the point.
  // When one does, add up the character offsets until we have a match

  // We have an point in an accessible child of this, now we need to add up the
  // offsets before it to what we already have
  int32_t offset = 0;
  uint32_t childCount = ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* childAcc = mChildren[childIdx];

    nsIFrame *primaryFrame = childAcc->GetFrame();
    NS_ENSURE_TRUE(primaryFrame, -1);

    nsIFrame *frame = primaryFrame;
    while (frame) {
      nsIContent *content = frame->GetContent();
      NS_ENSURE_TRUE(content, -1);
      nsPoint pointInFrame = pointInHyperText - frame->GetOffsetTo(hyperFrame);
      nsSize frameSize = frame->GetSize();
      if (pointInFrame.x < frameSize.width && pointInFrame.y < frameSize.height) {
        // Finished
        if (frame->IsTextFrame()) {
          nsIFrame::ContentOffsets contentOffsets =
            frame->GetContentOffsetsFromPointExternal(pointInFrame, nsIFrame::IGNORE_SELECTION_STYLE);
          if (contentOffsets.IsNull() || contentOffsets.content != content) {
            return -1; // Not found
          }
          uint32_t addToOffset;
          nsresult rv = ContentToRenderedOffset(primaryFrame,
                                                contentOffsets.offset,
                                                &addToOffset);
          NS_ENSURE_SUCCESS(rv, -1);
          offset += addToOffset;
        }
        return offset;
      }
      frame = frame->GetNextContinuation();
    }

    offset += nsAccUtils::TextLength(childAcc);
  }

  return -1; // Not found
}

nsIntRect
HyperTextAccessible::TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                                uint32_t aCoordType)
{
  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  if (!startOffset.IsValid() || !endOffset.IsValid() ||
      startOffset > endOffset || endOffset > CharacterCount()) {
    NS_ERROR("Wrong in offset");
    return nsIntRect();
  }

  if (CharacterCount() == 0) {
    nsPresContext* presContext = mDoc->PresContext();
    // Empty content, use our own bound to at least get x,y coordinates
    return GetFrame()->GetScreenRectInAppUnits().
      ToNearestPixels(presContext->AppUnitsPerDevPixel());
  }

  int32_t childIdx = GetChildIndexAtOffset(startOffset);
  if (childIdx == -1)
    return nsIntRect();

  nsIntRect bounds;
  int32_t prevOffset = GetChildOffset(childIdx);
  int32_t offset1 = startOffset - prevOffset;

  while (childIdx < static_cast<int32_t>(ChildCount())) {
    nsIFrame* frame = GetChildAt(childIdx++)->GetFrame();
    if (!frame) {
      MOZ_ASSERT_UNREACHABLE("No frame for a child!");
      continue;
    }

    int32_t nextOffset = GetChildOffset(childIdx);
    if (nextOffset >= static_cast<int32_t>(endOffset)) {
      bounds.UnionRect(bounds, GetBoundsInFrame(frame, offset1,
                                                endOffset - prevOffset));
      break;
    }

    bounds.UnionRect(bounds, GetBoundsInFrame(frame, offset1,
                                              nextOffset - prevOffset));

    prevOffset = nextOffset;
    offset1 = 0;
  }

  // This document may have a resolution set, we will need to multiply
  // the document-relative coordinates by that value and re-apply the doc's
  // screen coordinates.
  nsPresContext* presContext = mDoc->PresContext();
  nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
  nsIntRect orgRectPixels = rootFrame->GetScreenRectInAppUnits().ToNearestPixels(presContext->AppUnitsPerDevPixel());
  bounds.MoveBy(-orgRectPixels.X(), -orgRectPixels.Y());
  bounds.ScaleRoundOut(presContext->PresShell()->GetResolution());
  bounds.MoveBy(orgRectPixels.X(), orgRectPixels.Y());

  auto boundsX = bounds.X();
  auto boundsY = bounds.Y();
  nsAccUtils::ConvertScreenCoordsTo(&boundsX, &boundsY, aCoordType, this);
  bounds.MoveTo(boundsX, boundsY);
  return bounds;
}

already_AddRefed<TextEditor>
HyperTextAccessible::GetEditor() const
{
  if (!mContent->HasFlag(NODE_IS_EDITABLE)) {
    // If we're inside an editable container, then return that container's editor
    Accessible* ancestor = Parent();
    while (ancestor) {
      HyperTextAccessible* hyperText = ancestor->AsHyperText();
      if (hyperText) {
        // Recursion will stop at container doc because it has its own impl
        // of GetEditor()
        return hyperText->GetEditor();
      }

      ancestor = ancestor->Parent();
    }

    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mContent);
  nsCOMPtr<nsIEditingSession> editingSession;
  docShell->GetEditingSession(getter_AddRefs(editingSession));
  if (!editingSession)
    return nullptr; // No editing session interface

  nsIDocument* docNode = mDoc->DocumentNode();
  RefPtr<HTMLEditor> htmlEditor =
    editingSession->GetHTMLEditorForWindow(docNode->GetWindow());
  return htmlEditor.forget();
}

/**
  * =================== Caret & Selection ======================
  */

nsresult
HyperTextAccessible::SetSelectionRange(int32_t aStartPos, int32_t aEndPos)
{
  // Before setting the selection range, we need to ensure that the editor
  // is initialized. (See bug 804927.)
  // Otherwise, it's possible that lazy editor initialization will override
  // the selection we set here and leave the caret at the end of the text.
  // By calling GetEditor here, we ensure that editor initialization is
  // completed before we set the selection.
  RefPtr<TextEditor> textEditor = GetEditor();

  bool isFocusable = InteractiveState() & states::FOCUSABLE;

  // If accessible is focusable then focus it before setting the selection to
  // neglect control's selection changes on focus if any (for example, inputs
  // that do select all on focus).
  // some input controls
  if (isFocusable)
    TakeFocus();

  dom::Selection* domSel = DOMSelection();
  NS_ENSURE_STATE(domSel);

  // Set up the selection.
  for (int32_t idx = domSel->RangeCount() - 1; idx > 0; idx--)
    domSel->RemoveRange(*domSel->GetRangeAt(idx), IgnoreErrors());
  SetSelectionBoundsAt(0, aStartPos, aEndPos);

  // Make sure it is visible
  domSel->ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                         nsIPresShell::ScrollAxis(),
                         nsIPresShell::ScrollAxis(),
                         dom::Selection::SCROLL_FOR_CARET_MOVE |
                             dom::Selection::SCROLL_OVERFLOW_HIDDEN);

  // When selection is done, move the focus to the selection if accessible is
  // not focusable. That happens when selection is set within hypertext
  // accessible.
  if (isFocusable)
    return NS_OK;

  nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
  if (DOMFocusManager) {
    NS_ENSURE_TRUE(mDoc, NS_ERROR_FAILURE);
    nsIDocument* docNode = mDoc->DocumentNode();
    NS_ENSURE_TRUE(docNode, NS_ERROR_FAILURE);
    nsCOMPtr<nsPIDOMWindowOuter> window = docNode->GetWindow();
    RefPtr<dom::Element> result;
    DOMFocusManager->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
                               nsIFocusManager::FLAG_BYMOVEFOCUS, getter_AddRefs(result));
  }

  return NS_OK;
}

int32_t
HyperTextAccessible::CaretOffset() const
{
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
    if (text == this)
      return caretOffset;

    nsINode* textNode = text->GetNode();
    // Ignore offset if cached accessible isn't a text leaf.
    if (nsCoreUtils::IsAncestorOf(GetNode(), textNode))
      return TransformOffset(text,
        textNode->IsText() ? caretOffset : 0, false);
  }

  // No caret if the focused node is not inside this DOM node and this DOM node
  // is not inside of focused node.
  FocusManager::FocusDisposition focusDisp =
    FocusMgr()->IsInOrContainsFocus(this);
  if (focusDisp == FocusManager::eNone)
    return -1;

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
        !nsCoreUtils::IsAncestorOf(thisNode, resultNode))
      return -1;
  }

  return DOMPointToOffset(focusNode, focusOffset);
}

int32_t
HyperTextAccessible::CaretLineNumber()
{
  // Provide the line number for the caret, relative to the
  // currently focused node. Use a 1-based index
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection)
    return -1;

  dom::Selection* domSel = frameSelection->GetSelection(SelectionType::eNormal);
  if (!domSel)
    return - 1;

  nsINode* caretNode = domSel->GetFocusNode();
  if (!caretNode || !caretNode->IsContent())
    return -1;

  nsIContent* caretContent = caretNode->AsContent();
  if (!nsCoreUtils::IsAncestorOf(GetNode(), caretContent))
    return -1;

  int32_t returnOffsetUnused;
  uint32_t caretOffset = domSel->FocusOffset();
  CaretAssociationHint hint = frameSelection->GetHint();
  nsIFrame *caretFrame = frameSelection->GetFrameForNodeOffset(caretContent, caretOffset,
                                                               hint, &returnOffsetUnused);
  NS_ENSURE_TRUE(caretFrame, -1);

  int32_t lineNumber = 1;
  nsAutoLineIterator lineIterForCaret;
  nsIContent *hyperTextContent = IsContent() ? mContent.get() : nullptr;
  while (caretFrame) {
    if (hyperTextContent == caretFrame->GetContent()) {
      return lineNumber; // Must be in a single line hyper text, there is no line iterator
    }
    nsContainerFrame *parentFrame = caretFrame->GetParent();
    if (!parentFrame)
      break;

    // Add lines for the sibling frames before the caret
    nsIFrame *sibling = parentFrame->PrincipalChildList().FirstChild();
    while (sibling && sibling != caretFrame) {
      nsAutoLineIterator lineIterForSibling = sibling->GetLineIterator();
      if (lineIterForSibling) {
        // For the frames before that grab all the lines
        int32_t addLines = lineIterForSibling->GetNumLines();
        lineNumber += addLines;
      }
      sibling = sibling->GetNextSibling();
    }

    // Get the line number relative to the container with lines
    if (!lineIterForCaret) {   // Add the caret line just once
      lineIterForCaret = parentFrame->GetLineIterator();
      if (lineIterForCaret) {
        // Ancestor of caret
        int32_t addLines = lineIterForCaret->FindLineContaining(caretFrame);
        lineNumber += addLines;
      }
    }

    caretFrame = parentFrame;
  }

  MOZ_ASSERT_UNREACHABLE("DOM ancestry had this hypertext but frame ancestry didn't");
  return lineNumber;
}

LayoutDeviceIntRect
HyperTextAccessible::GetCaretRect(nsIWidget** aWidget)
{
  *aWidget = nullptr;

  RefPtr<nsCaret> caret = mDoc->PresShell()->GetCaret();
  NS_ENSURE_TRUE(caret, LayoutDeviceIntRect());

  bool isVisible = caret->IsVisible();
  if (!isVisible)
    return LayoutDeviceIntRect();

  nsRect rect;
  nsIFrame* frame = caret->GetGeometry(&rect);
  if (!frame || rect.IsEmpty())
    return LayoutDeviceIntRect();

  nsPoint offset;
  // Offset from widget origin to the frame origin, which includes chrome
  // on the widget.
  *aWidget = frame->GetNearestWidget(offset);
  NS_ENSURE_TRUE(*aWidget, LayoutDeviceIntRect());
  rect.MoveBy(offset);

  LayoutDeviceIntRect caretRect = LayoutDeviceIntRect::FromUnknownRect(
    rect.ToOutsidePixels(frame->PresContext()->AppUnitsPerDevPixel()));
  // ((content screen origin) - (content offset in the widget)) = widget origin on the screen
  caretRect.MoveBy((*aWidget)->WidgetToScreenOffset() - (*aWidget)->GetClientOffset());

  // Correct for character size, so that caret always matches the size of
  // the character. This is important for font size transitions, and is
  // necessary because the Gecko caret uses the previous character's size as
  // the user moves forward in the text by character.
  nsIntRect charRect = CharBounds(CaretOffset(),
                                  nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE);
  if (!charRect.IsEmpty()) {
    caretRect.SetTopEdge(charRect.Y());
  }
  return caretRect;
}

void
HyperTextAccessible::GetSelectionDOMRanges(SelectionType aSelectionType,
                                           nsTArray<nsRange*>* aRanges)
{
  // Ignore selection if it is not visible.
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection ||
      frameSelection->GetDisplaySelection() <= nsISelectionController::SELECTION_HIDDEN)
    return;

  dom::Selection* domSel = frameSelection->GetSelection(aSelectionType);
  if (!domSel)
    return;

  nsINode* startNode = GetNode();

  RefPtr<TextEditor> textEditor = GetEditor();
  if (textEditor) {
    startNode = textEditor->GetRoot();
  }

  if (!startNode)
    return;

  uint32_t childCount = startNode->GetChildCount();
  nsresult rv = domSel->
    GetRangesForIntervalArray(startNode, 0, startNode, childCount, true, aRanges);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Remove collapsed ranges
  uint32_t numRanges = aRanges->Length();
  for (uint32_t idx = 0; idx < numRanges; idx ++) {
    if ((*aRanges)[idx]->Collapsed()) {
      aRanges->RemoveElementAt(idx);
      --numRanges;
      --idx;
    }
  }
}

int32_t
HyperTextAccessible::SelectionCount()
{
  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(SelectionType::eNormal, &ranges);
  return ranges.Length();
}

bool
HyperTextAccessible::SelectionBoundsAt(int32_t aSelectionNum,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset)
{
  *aStartOffset = *aEndOffset = 0;

  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(SelectionType::eNormal, &ranges);

  uint32_t rangeCount = ranges.Length();
  if (aSelectionNum < 0 || aSelectionNum >= static_cast<int32_t>(rangeCount))
    return false;

  nsRange* range = ranges[aSelectionNum];

  // Get start and end points.
  nsINode* startNode = range->GetStartContainer();
  nsINode* endNode = range->GetEndContainer();
  int32_t startOffset = range->StartOffset(), endOffset = range->EndOffset();

  // Make sure start is before end, by swapping DOM points.  This occurs when
  // the user selects backwards in the text.
  int32_t rangeCompare = nsContentUtils::ComparePoints(endNode, endOffset,
                                                       startNode, startOffset);
  if (rangeCompare < 0) {
    nsINode* tempNode = startNode;
    startNode = endNode;
    endNode = tempNode;
    int32_t tempOffset = startOffset;
    startOffset = endOffset;
    endOffset = tempOffset;
  }

  if (!nsContentUtils::ContentIsDescendantOf(startNode, mContent))
    *aStartOffset = 0;
  else
    *aStartOffset = DOMPointToOffset(startNode, startOffset);

  if (!nsContentUtils::ContentIsDescendantOf(endNode, mContent))
    *aEndOffset = CharacterCount();
  else
    *aEndOffset = DOMPointToOffset(endNode, endOffset, true);
  return true;
}

bool
HyperTextAccessible::SetSelectionBoundsAt(int32_t aSelectionNum,
                                          int32_t aStartOffset,
                                          int32_t aEndOffset)
{
  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  if (!startOffset.IsValid() || !endOffset.IsValid() ||
      std::max(startOffset, endOffset) > CharacterCount()) {
    NS_ERROR("Wrong in offset");
    return false;
  }

  dom::Selection* domSel = DOMSelection();
  if (!domSel)
    return false;

  RefPtr<nsRange> range;
  uint32_t rangeCount = domSel->RangeCount();
  if (aSelectionNum == static_cast<int32_t>(rangeCount))
    range = new nsRange(mContent);
  else
    range = domSel->GetRangeAt(aSelectionNum);

  if (!range)
    return false;

  if (!OffsetsToDOMRange(std::min(startOffset, endOffset),
                         std::max(startOffset, endOffset), range))
    return false;

  // If this is not a new range, notify selection listeners that the existing
  // selection range has changed. Otherwise, just add the new range.
  if (aSelectionNum != static_cast<int32_t>(rangeCount)) {
    domSel->RemoveRange(*range, IgnoreErrors());
  }

  IgnoredErrorResult err;
  domSel->AddRange(*range, err);

  if (!err.Failed()) {
    // Changing the direction of the selection assures that the caret
    // will be at the logical end of the selection.
    domSel->SetDirection(startOffset < endOffset ? eDirNext : eDirPrevious);
    return true;
  }

  return false;
}

bool
HyperTextAccessible::RemoveFromSelection(int32_t aSelectionNum)
{
  dom::Selection* domSel = DOMSelection();
  if (!domSel)
    return false;

  if (aSelectionNum < 0 || aSelectionNum >= static_cast<int32_t>(domSel->RangeCount()))
    return false;

  domSel->RemoveRange(*domSel->GetRangeAt(aSelectionNum), IgnoreErrors());
  return true;
}

void
HyperTextAccessible::ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                                       uint32_t aScrollType)
{
  RefPtr<nsRange> range = new nsRange(mContent);
  if (OffsetsToDOMRange(aStartOffset, aEndOffset, range))
    nsCoreUtils::ScrollSubstringTo(GetFrame(), range, aScrollType);
}

void
HyperTextAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                            int32_t aEndOffset,
                                            uint32_t aCoordinateType,
                                            int32_t aX, int32_t aY)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return;

  nsIntPoint coords = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType,
                                                        this);

  RefPtr<nsRange> range = new nsRange(mContent);
  if (!OffsetsToDOMRange(aStartOffset, aEndOffset, range))
    return;

  nsPresContext* presContext = frame->PresContext();
  nsPoint coordsInAppUnits =
    ToAppUnits(coords, presContext->AppUnitsPerDevPixel());

  bool initialScrolled = false;
  nsIFrame *parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent())) {
    nsIScrollableFrame *scrollableFrame = do_QueryFrame(parentFrame);
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

        nsresult rv = nsCoreUtils::ScrollSubstringTo(frame, range,
                                                     nsIPresShell::ScrollAxis(vPercent),
                                                     nsIPresShell::ScrollAxis(hPercent));
        if (NS_FAILED(rv))
          return;

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

void
HyperTextAccessible::EnclosingRange(a11y::TextRange& aRange) const
{
  if (IsTextField()) {
    aRange.Set(mDoc, const_cast<HyperTextAccessible*>(this), 0,
               const_cast<HyperTextAccessible*>(this), CharacterCount());
  } else {
    aRange.Set(mDoc, mDoc, 0, mDoc, mDoc->CharacterCount());
  }
}

void
HyperTextAccessible::SelectionRanges(nsTArray<a11y::TextRange>* aRanges) const
{
  MOZ_ASSERT(aRanges->Length() == 0, "TextRange array supposed to be empty");

  dom::Selection* sel = DOMSelection();
  if (!sel)
    return;

  aRanges->SetCapacity(sel->RangeCount());

  for (uint32_t idx = 0; idx < sel->RangeCount(); idx++) {
    nsRange* DOMRange = sel->GetRangeAt(idx);
    HyperTextAccessible* startContainer =
      nsAccUtils::GetTextContainer(DOMRange->GetStartContainer());
    HyperTextAccessible* endContainer =
      nsAccUtils::GetTextContainer(DOMRange->GetEndContainer());
    if (!startContainer || !endContainer) {
      continue;
    }

    int32_t startOffset =
      startContainer->DOMPointToOffset(DOMRange->GetStartContainer(),
                                       DOMRange->StartOffset(), false);
    int32_t endOffset =
      endContainer->DOMPointToOffset(DOMRange->GetEndContainer(),
                                     DOMRange->EndOffset(), true);

    TextRange tr(IsTextField() ? const_cast<HyperTextAccessible*>(this) : mDoc,
                    startContainer, startOffset, endContainer, endOffset);
    *(aRanges->AppendElement()) = std::move(tr);
  }
}

void
HyperTextAccessible::VisibleRanges(nsTArray<a11y::TextRange>* aRanges) const
{
}

void
HyperTextAccessible::RangeByChild(Accessible* aChild,
                                  a11y::TextRange& aRange) const
{
  HyperTextAccessible* ht = aChild->AsHyperText();
  if (ht) {
    aRange.Set(mDoc, ht, 0, ht, ht->CharacterCount());
    return;
  }

  Accessible* child = aChild;
  Accessible* parent = nullptr;
  while ((parent = child->Parent()) && !(ht = parent->AsHyperText()))
    child = parent;

  // If no text then return collapsed text range, otherwise return a range
  // containing the text enclosed by the given child.
  if (ht) {
    int32_t childIdx = child->IndexInParent();
    int32_t startOffset = ht->GetChildOffset(childIdx);
    int32_t endOffset = child->IsTextLeaf() ?
      ht->GetChildOffset(childIdx + 1) : startOffset;
    aRange.Set(mDoc, ht, startOffset, ht, endOffset);
  }
}

void
HyperTextAccessible::RangeAtPoint(int32_t aX, int32_t aY,
                                  a11y::TextRange& aRange) const
{
  Accessible* child = mDoc->ChildAtPoint(aX, aY, eDeepestChild);
  if (!child)
    return;

  Accessible* parent = nullptr;
  while ((parent = child->Parent()) && !parent->IsHyperText())
    child = parent;

  // Return collapsed text range for the point.
  if (parent) {
    HyperTextAccessible* ht = parent->AsHyperText();
    int32_t offset = ht->GetChildOffset(child);
    aRange.Set(mDoc, ht, offset, ht, offset);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Accessible public

// Accessible protected
ENameValueFlag
HyperTextAccessible::NativeName(nsString& aName) const
{
  // Check @alt attribute for invalid img elements.
  bool hasImgAlt = false;
  if (mContent->IsHTMLElement(nsGkAtoms::img)) {
    hasImgAlt =
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName);
    if (!aName.IsEmpty())
      return eNameOK;
  }

  ENameValueFlag nameFlag = AccessibleWrap::NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

  // Get name from title attribute for HTML abbr and acronym elements making it
  // a valid name from markup. Otherwise their name isn't picked up by recursive
  // name computation algorithm. See NS_OK_NAME_FROM_TOOLTIP.
  if (IsAbbreviation() &&
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::title, aName))
    aName.CompressWhitespace();

  return hasImgAlt ? eNoNameOnPurpose : eNameOK;
}

void
HyperTextAccessible::Shutdown()
{
  mOffsets.Clear();
  AccessibleWrap::Shutdown();
}

bool
HyperTextAccessible::RemoveChild(Accessible* aAccessible)
{
  int32_t childIndex = aAccessible->IndexInParent();
  int32_t count = mOffsets.Length() - childIndex;
  if (count > 0)
    mOffsets.RemoveElementsAt(childIndex, count);

  return AccessibleWrap::RemoveChild(aAccessible);
}

bool
HyperTextAccessible::InsertChildAt(uint32_t aIndex, Accessible* aChild)
{
  int32_t count = mOffsets.Length() - aIndex;
  if (count > 0 ) {
    mOffsets.RemoveElementsAt(aIndex, count);
  }
  return AccessibleWrap::InsertChildAt(aIndex, aChild);
}

Relation
HyperTextAccessible::RelationByType(RelationType aType) const
{
  Relation rel = Accessible::RelationByType(aType);

  switch (aType) {
    case RelationType::NODE_CHILD_OF:
      if (HasOwnContent() && mContent->IsMathMLElement()) {
        Accessible* parent = Parent();
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
        Accessible* base = GetChildAt(0);
        Accessible* index = GetChildAt(1);
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

nsresult
HyperTextAccessible::ContentToRenderedOffset(nsIFrame* aFrame, int32_t aContentOffset,
                                             uint32_t* aRenderedOffset) const
{
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

  nsIFrame::RenderedText text = aFrame->GetRenderedText(aContentOffset,
      aContentOffset + 1, nsIFrame::TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
      nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);
  *aRenderedOffset = text.mOffsetWithinNodeRenderedText;

  return NS_OK;
}

nsresult
HyperTextAccessible::RenderedToContentOffset(nsIFrame* aFrame, uint32_t aRenderedOffset,
                                             int32_t* aContentOffset) const
{
  if (IsTextField()) {
    *aContentOffset = aRenderedOffset;
    return NS_OK;
  }

  *aContentOffset = 0;
  NS_ENSURE_TRUE(aFrame, NS_ERROR_FAILURE);

  NS_ASSERTION(aFrame->IsTextFrame(), "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  nsIFrame::RenderedText text = aFrame->GetRenderedText(aRenderedOffset,
      aRenderedOffset + 1, nsIFrame::TextOffsetType::OFFSETS_IN_RENDERED_TEXT,
      nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);
  *aContentOffset = text.mOffsetWithinNodeText;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible public

int32_t
HyperTextAccessible::GetChildOffset(uint32_t aChildIndex,
                                    bool aInvalidateAfter) const
{
  if (aChildIndex == 0) {
    if (aInvalidateAfter)
      mOffsets.Clear();

    return aChildIndex;
  }

  int32_t count = mOffsets.Length() - aChildIndex;
  if (count > 0) {
    if (aInvalidateAfter)
      mOffsets.RemoveElementsAt(aChildIndex, count);

    return mOffsets[aChildIndex - 1];
  }

  uint32_t lastOffset = mOffsets.IsEmpty() ?
    0 : mOffsets[mOffsets.Length() - 1];

  while (mOffsets.Length() < aChildIndex) {
    Accessible* child = mChildren[mOffsets.Length()];
    lastOffset += nsAccUtils::TextLength(child);
    mOffsets.AppendElement(lastOffset);
  }

  return mOffsets[aChildIndex - 1];
}

int32_t
HyperTextAccessible::GetChildIndexAtOffset(uint32_t aOffset) const
{
  uint32_t lastOffset = 0;
  const uint32_t offsetCount = mOffsets.Length();

  if (offsetCount > 0) {
    lastOffset = mOffsets[offsetCount - 1];
    if (aOffset < lastOffset) {
      size_t index;
      if (BinarySearch(mOffsets, 0, offsetCount, aOffset, &index)) {
        return (index < (offsetCount - 1)) ? index + 1 : index;
      }

      return (index == offsetCount) ? -1 : index;
    }
  }

  uint32_t childCount = ChildCount();
  while (mOffsets.Length() < childCount) {
    Accessible* child = GetChildAt(mOffsets.Length());
    lastOffset += nsAccUtils::TextLength(child);
    mOffsets.AppendElement(lastOffset);
    if (aOffset < lastOffset)
      return mOffsets.Length() - 1;
  }

  if (aOffset == lastOffset)
    return mOffsets.Length() - 1;

  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible protected

nsresult
HyperTextAccessible::GetDOMPointByFrameOffset(nsIFrame* aFrame, int32_t aOffset,
                                              Accessible* aAccessible,
                                              DOMPoint* aPoint)
{
  NS_ENSURE_ARG(aAccessible);

  if (!aFrame) {
    // If the given frame is null then set offset after the DOM node of the
    // given accessible.
    NS_ASSERTION(!aAccessible->IsDoc(),
                 "Shouldn't be called on document accessible!");

    nsIContent* content = aAccessible->GetContent();
    NS_ASSERTION(content, "Shouldn't operate on defunct accessible!");

    nsIContent* parent = content->GetParent();

    aPoint->idx = parent->ComputeIndexOf(content) + 1;
    aPoint->node = parent;

  } else if (aFrame->IsTextFrame()) {
    nsIContent* content = aFrame->GetContent();
    NS_ENSURE_STATE(content);

    nsIFrame *primaryFrame = content->GetPrimaryFrame();
    nsresult rv = RenderedToContentOffset(primaryFrame, aOffset, &(aPoint->idx));
    NS_ENSURE_SUCCESS(rv, rv);

    aPoint->node = content;

  } else {
    nsIContent* content = aFrame->GetContent();
    NS_ENSURE_STATE(content);

    nsIContent* parent = content->GetParent();
    NS_ENSURE_STATE(parent);

    aPoint->idx = parent->ComputeIndexOf(content);
    aPoint->node = parent;
  }

  return NS_OK;
}

// HyperTextAccessible
void
HyperTextAccessible::GetSpellTextAttr(nsINode* aNode,
                                      int32_t aNodeOffset,
                                      uint32_t* aStartOffset,
                                      uint32_t* aEndOffset,
                                      nsIPersistentProperties* aAttributes)
{
  RefPtr<nsFrameSelection> fs = FrameSelection();
  if (!fs)
    return;

  dom::Selection* domSel = fs->GetSelection(SelectionType::eSpellCheck);
  if (!domSel)
    return;

  int32_t rangeCount = domSel->RangeCount();
  if (rangeCount <= 0)
    return;

  uint32_t startOffset = 0, endOffset = 0;
  for (int32_t idx = 0; idx < rangeCount; idx++) {
    nsRange* range = domSel->GetRangeAt(idx);
    if (range->Collapsed())
      continue;

    // See if the point comes after the range in which case we must continue in
    // case there is another range after this one.
    nsINode* endNode = range->GetEndContainer();
    int32_t endNodeOffset = range->EndOffset();
    if (nsContentUtils::ComparePoints(aNode, aNodeOffset,
                                      endNode, endNodeOffset) >= 0)
      continue;

    // At this point our point is either in this range or before it but after
    // the previous range.  So we check to see if the range starts before the
    // point in which case the point is in the missspelled range, otherwise it
    // must be before the range and after the previous one if any.
    nsINode* startNode = range->GetStartContainer();
    int32_t startNodeOffset = range->StartOffset();
    if (nsContentUtils::ComparePoints(startNode, startNodeOffset, aNode,
                                      aNodeOffset) <= 0) {
      startOffset = DOMPointToOffset(startNode, startNodeOffset);

      endOffset = DOMPointToOffset(endNode, endNodeOffset);

      if (startOffset > *aStartOffset)
        *aStartOffset = startOffset;

      if (endOffset < *aEndOffset)
        *aEndOffset = endOffset;

      if (aAttributes) {
        nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::invalid,
                               NS_LITERAL_STRING("spelling"));
      }

      return;
    }

    // This range came after the point.
    endOffset = DOMPointToOffset(startNode, startNodeOffset);

    if (idx > 0) {
      nsRange* prevRange = domSel->GetRangeAt(idx - 1);
      startOffset = DOMPointToOffset(prevRange->GetEndContainer(),
                                     prevRange->EndOffset());
    }

    if (startOffset > *aStartOffset)
      *aStartOffset = startOffset;

    if (endOffset < *aEndOffset)
      *aEndOffset = endOffset;

    return;
  }

  // We never found a range that ended after the point, therefore we know that
  // the point is not in a range, that we do not need to compute an end offset,
  // and that we should use the end offset of the last range to compute the
  // start offset of the text attribute range.
  nsRange* prevRange = domSel->GetRangeAt(rangeCount - 1);
  startOffset = DOMPointToOffset(prevRange->GetEndContainer(),
                                 prevRange->EndOffset());

  if (startOffset > *aStartOffset)
    *aStartOffset = startOffset;
}

bool
HyperTextAccessible::IsTextRole()
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry &&
      (roleMapEntry->role == roles::GRAPHIC ||
       roleMapEntry->role == roles::IMAGE_MAP ||
       roleMapEntry->role == roles::SLIDER ||
       roleMapEntry->role == roles::PROGRESSBAR ||
       roleMapEntry->role == roles::SEPARATOR))
    return false;

  return true;
}

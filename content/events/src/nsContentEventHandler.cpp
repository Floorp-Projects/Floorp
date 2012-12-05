/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentEventHandler.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsRange.h"
#include "nsGUIEvent.h"
#include "nsCaret.h"
#include "nsCopySupport.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIContentIterator.h"
#include "nsTextFragment.h"
#include "nsTextFrame.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIMEStateManager.h"
#include "nsIObjectFrame.h"

/******************************************************************/
/* nsContentEventHandler                                          */
/******************************************************************/

nsContentEventHandler::nsContentEventHandler(
                              nsPresContext* aPresContext) :
  mPresContext(aPresContext),
  mPresShell(aPresContext->GetPresShell()), mSelection(nullptr),
  mFirstSelectedRange(nullptr), mRootContent(nullptr)
{
}

nsresult
nsContentEventHandler::InitCommon()
{
  if (mSelection)
    return NS_OK;

  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);

  // If text frame which has overflowing selection underline is dirty,
  // we need to flush the pending reflow here.
  mPresShell->FlushPendingNotifications(Flush_Layout);

  // Flushing notifications can cause mPresShell to be destroyed (bug 577963).
  NS_ENSURE_TRUE(!mPresShell->IsDestroying(), NS_ERROR_FAILURE);

  nsCopySupport::GetSelectionForCopy(mPresShell->GetDocument(),
                                     getter_AddRefs(mSelection));

  nsCOMPtr<nsIDOMRange> firstRange;
  nsresult rv = mSelection->GetRangeAt(0, getter_AddRefs(firstRange));
  // This shell doesn't support selection.
  if (NS_FAILED(rv))
    return NS_ERROR_NOT_AVAILABLE;
  mFirstSelectedRange = static_cast<nsRange*>(firstRange.get());

  nsINode* startNode = mFirstSelectedRange->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  nsINode* endNode = mFirstSelectedRange->GetEndParent();
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  // See bug 537041 comment 5, the range could have removed node.
  NS_ENSURE_TRUE(startNode->GetCurrentDoc() == mPresShell->GetDocument(),
                 NS_ERROR_NOT_AVAILABLE);
  NS_ASSERTION(startNode->GetCurrentDoc() == endNode->GetCurrentDoc(),
               "mFirstSelectedRange crosses the document boundary");

  mRootContent = startNode->GetSelectionRootContent(mPresShell);
  NS_ENSURE_TRUE(mRootContent, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
nsContentEventHandler::Init(nsQueryContentEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent must not be null");

  nsresult rv = InitCommon();
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = false;

  aEvent->mReply.mContentsRoot = mRootContent.get();

  bool isCollapsed;
  rv = mSelection->GetIsCollapsed(&isCollapsed);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_NOT_AVAILABLE);
  aEvent->mReply.mHasSelection = !isCollapsed;

  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  NS_ASSERTION(caret, "GetCaret returned null");

  nsRect r;
  nsIFrame* frame = caret->GetGeometry(mSelection, &r);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  aEvent->mReply.mFocusedWidget = frame->GetNearestWidget();

  return NS_OK;
}

nsresult
nsContentEventHandler::Init(nsSelectionEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent must not be null");

  nsresult rv = InitCommon();
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = false;

  return NS_OK;
}

// Editor places a bogus BR node under its root content if the editor doesn't
// have any text. This happens even for single line editors.
// When we get text content and when we change the selection,
// we don't want to include the bogus BRs at the end.
static bool IsContentBR(nsIContent* aContent)
{
  return aContent->IsHTML() &&
         aContent->Tag() == nsGkAtoms::br &&
         !aContent->AttrValueIs(kNameSpaceID_None,
                                nsGkAtoms::type,
                                nsGkAtoms::moz,
                                eIgnoreCase) &&
         !aContent->AttrValueIs(kNameSpaceID_None,
                                nsGkAtoms::mozeditorbogusnode,
                                nsGkAtoms::_true,
                                eIgnoreCase);
}

static void ConvertToNativeNewlines(nsAFlatString& aString)
{
#if defined(XP_MACOSX)
  aString.ReplaceSubstring(NS_LITERAL_STRING("\n"), NS_LITERAL_STRING("\r"));
#elif defined(XP_WIN)
  aString.ReplaceSubstring(NS_LITERAL_STRING("\n"), NS_LITERAL_STRING("\r\n"));
#endif
}

static void AppendString(nsAString& aString, nsIContent* aContent)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text)
    return;
  text->AppendTo(aString);
}

static void AppendSubString(nsAString& aString, nsIContent* aContent,
                            uint32_t aXPOffset, uint32_t aXPLength)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text)
    return;
  text->AppendTo(aString, int32_t(aXPOffset), int32_t(aXPLength));
}

#if defined(XP_WIN)
static uint32_t CountNewlinesInXPLength(nsIContent* aContent,
                                        uint32_t aXPLength)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text)
    return 0;
  // For automated tests, we should abort on debug build.
  NS_ABORT_IF_FALSE(
    (aXPLength == UINT32_MAX || aXPLength <= text->GetLength()),
    "aXPLength is out-of-bounds");
  const uint32_t length = NS_MIN(aXPLength, text->GetLength());
  uint32_t newlines = 0;
  for (uint32_t i = 0; i < length; ++i) {
    if (text->CharAt(i) == '\n') {
      ++newlines;
    }
  }
  return newlines;
}

static uint32_t CountNewlinesInNativeLength(nsIContent* aContent,
                                            uint32_t aNativeLength)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text) {
    return 0;
  }
  // For automated tests, we should abort on debug build.
  NS_ABORT_IF_FALSE(
    (aNativeLength == UINT32_MAX || aNativeLength <= text->GetLength() * 2),
    "aNativeLength is unexpected value");
  const uint32_t xpLength = text->GetLength();
  uint32_t newlines = 0;
  for (uint32_t i = 0, nativeOffset = 0;
       i < xpLength && nativeOffset < aNativeLength;
       ++i, ++nativeOffset) {
    // For automated tests, we should abort on debug build.
    NS_ABORT_IF_FALSE(i < text->GetLength(), "i is out-of-bounds");
    if (text->CharAt(i) == '\n') {
      ++newlines;
      ++nativeOffset;
    }
  }
  return newlines;
}
#endif

/* static */ uint32_t
nsContentEventHandler::GetNativeTextLength(nsIContent* aContent, uint32_t aMaxLength)
{
  if (aContent->IsNodeOfType(nsINode::eTEXT)) {
    uint32_t textLengthDifference =
#if defined(XP_MACOSX)
      // On Mac, the length of a native newline ("\r") is equal to the length of
      // the XP newline ("\n"), so the native length is the same as the XP length.
      0;
#elif defined(XP_WIN)
      // On Windows, the length of a native newline ("\r\n") is twice the length of
      // the XP newline ("\n"), so XP length is equal to the length of the native
      // offset plus the number of newlines encountered in the string.
      CountNewlinesInXPLength(aContent, aMaxLength);
#else
      // On other platforms, the native and XP newlines are the same.
      0;
#endif

    const nsTextFragment* text = aContent->GetText();
    if (!text)
      return 0;
    uint32_t length = NS_MIN(text->GetLength(), aMaxLength);
    return length + textLengthDifference;
  } else if (IsContentBR(aContent)) {
#if defined(XP_WIN)
    // Length of \r\n
    return 2;
#else
    return 1;
#endif
  }
  return 0;
}

static uint32_t ConvertToXPOffset(nsIContent* aContent, uint32_t aNativeOffset)
{
#if defined(XP_MACOSX)
  // On Mac, the length of a native newline ("\r") is equal to the length of
  // the XP newline ("\n"), so the native offset is the same as the XP offset.
  return aNativeOffset;
#elif defined(XP_WIN)
  // On Windows, the length of a native newline ("\r\n") is twice the length of
  // the XP newline ("\n"), so XP offset is equal to the length of the native
  // offset minus the number of newlines encountered in the string.
  return aNativeOffset - CountNewlinesInNativeLength(aContent, aNativeOffset);
#else
  // On other platforms, the native and XP newlines are the same.
  return aNativeOffset;
#endif
}

static nsresult GenerateFlatTextContent(nsRange* aRange,
                                        nsAFlatString& aString)
{
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  iter->Init(aRange);

  NS_ASSERTION(aString.IsEmpty(), "aString must be empty string");

  nsINode* startNode = aRange->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  nsINode* endNode = aRange->GetEndParent();
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  if (startNode == endNode && startNode->IsNodeOfType(nsINode::eTEXT)) {
    nsIContent* content = static_cast<nsIContent*>(startNode);
    AppendSubString(aString, content, aRange->StartOffset(),
                    aRange->EndOffset() - aRange->StartOffset());
    ConvertToNativeNewlines(aString);
    return NS_OK;
  }

  nsAutoString tmpStr;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node)
      break;
    if (!node->IsNodeOfType(nsINode::eCONTENT))
      continue;
    nsIContent* content = static_cast<nsIContent*>(node);

    if (content->IsNodeOfType(nsINode::eTEXT)) {
      if (content == startNode)
        AppendSubString(aString, content, aRange->StartOffset(),
                        content->TextLength() - aRange->StartOffset());
      else if (content == endNode)
        AppendSubString(aString, content, 0, aRange->EndOffset());
      else
        AppendString(aString, content);
    } else if (IsContentBR(content))
        aString.Append(PRUnichar('\n'));
  }
  ConvertToNativeNewlines(aString);
  return NS_OK;
}

nsresult
nsContentEventHandler::ExpandToClusterBoundary(nsIContent* aContent,
                                               bool aForward,
                                               uint32_t* aXPOffset)
{
  // XXX This method assumes that the frame boundaries must be cluster
  // boundaries. It's false, but no problem now, maybe.
  if (!aContent->IsNodeOfType(nsINode::eTEXT) ||
      *aXPOffset == 0 || *aXPOffset == aContent->TextLength())
    return NS_OK;

  NS_ASSERTION(*aXPOffset <= aContent->TextLength(),
               "offset is out of range.");

  nsRefPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
  int32_t offsetInFrame;
  nsFrameSelection::HINT hint =
    aForward ? nsFrameSelection::HINTLEFT : nsFrameSelection::HINTRIGHT;
  nsIFrame* frame = fs->GetFrameForNodeOffset(aContent, int32_t(*aXPOffset),
                                              hint, &offsetInFrame);
  if (!frame) {
    // This content doesn't have any frames, we only can check surrogate pair...
    const nsTextFragment* text = aContent->GetText();
    NS_ENSURE_TRUE(text, NS_ERROR_FAILURE);
    if (NS_IS_LOW_SURROGATE(text->CharAt(*aXPOffset)) &&
        NS_IS_HIGH_SURROGATE(text->CharAt(*aXPOffset - 1)))
      *aXPOffset += aForward ? 1 : -1;
    return NS_OK;
  }
  int32_t startOffset, endOffset;
  nsresult rv = frame->GetOffsets(startOffset, endOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aXPOffset == uint32_t(startOffset) || *aXPOffset == uint32_t(endOffset))
    return NS_OK;
  if (frame->GetType() != nsGkAtoms::textFrame)
    return NS_ERROR_FAILURE;
  nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
  int32_t newOffsetInFrame = *aXPOffset - startOffset;
  newOffsetInFrame += aForward ? -1 : 1;
  textFrame->PeekOffsetCharacter(aForward, &newOffsetInFrame);
  *aXPOffset = startOffset + newOffsetInFrame;
  return NS_OK;
}

nsresult
nsContentEventHandler::SetRangeFromFlatTextOffset(
                              nsRange* aRange,
                              uint32_t aNativeOffset,
                              uint32_t aNativeLength,
                              bool aExpandToClusterBoundaries)
{
  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv = iter->Init(mRootContent);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t nativeOffset = 0;
  uint32_t nativeEndOffset = aNativeOffset + aNativeLength;
  bool startSet = false;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node)
      break;
    if (!node->IsNodeOfType(nsINode::eCONTENT))
      continue;
    nsIContent* content = static_cast<nsIContent*>(node);

    uint32_t nativeTextLength;
    nativeTextLength = GetNativeTextLength(content);
    if (nativeTextLength == 0)
      continue;

    if (nativeOffset <= aNativeOffset &&
        aNativeOffset < nativeOffset + nativeTextLength) {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(content));
      NS_ASSERTION(domNode, "aContent doesn't have nsIDOMNode!");

      uint32_t xpOffset =
        content->IsNodeOfType(nsINode::eTEXT) ?
          ConvertToXPOffset(content, aNativeOffset - nativeOffset) : 0;

      if (aExpandToClusterBoundaries) {
        rv = ExpandToClusterBoundary(content, false, &xpOffset);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = aRange->SetStart(domNode, int32_t(xpOffset));
      NS_ENSURE_SUCCESS(rv, rv);
      startSet = true;
      if (aNativeLength == 0) {
        // Ensure that the end offset and the start offset are same.
        rv = aRange->SetEnd(domNode, int32_t(xpOffset));
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    if (nativeEndOffset <= nativeOffset + nativeTextLength) {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(content));
      NS_ASSERTION(domNode, "aContent doesn't have nsIDOMNode!");

      uint32_t xpOffset;
      if (content->IsNodeOfType(nsINode::eTEXT)) {
        xpOffset = ConvertToXPOffset(content, nativeEndOffset - nativeOffset);
        if (aExpandToClusterBoundaries) {
          rv = ExpandToClusterBoundary(content, true, &xpOffset);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else {
        // Use first position of next node, because the end node is ignored
        // by ContentIterator when the offset is zero.
        xpOffset = 0;
        iter->Next();
        if (iter->IsDone())
          break;
        domNode = do_QueryInterface(iter->GetCurrentNode());
      }

      rv = aRange->SetEnd(domNode, int32_t(xpOffset));
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    nativeOffset += nativeTextLength;
  }

  if (nativeOffset < aNativeOffset)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mRootContent));
  NS_ASSERTION(domNode, "lastContent doesn't have nsIDOMNode!");
  if (!startSet) {
    MOZ_ASSERT(!mRootContent->IsNodeOfType(nsINode::eTEXT));
    rv = aRange->SetStart(domNode, int32_t(mRootContent->GetChildCount()));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = aRange->SetEnd(domNode, int32_t(mRootContent->GetChildCount()));
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsIDOMRange::SetEnd failed");
  return rv;
}

nsresult
nsContentEventHandler::OnQuerySelectedText(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  NS_ASSERTION(aEvent->mReply.mString.IsEmpty(),
               "The reply string must be empty");

  rv = GetFlatTextOffsetOfRange(mRootContent,
                                mFirstSelectedRange, &aEvent->mReply.mOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> anchorDomNode, focusDomNode;
  rv = mSelection->GetAnchorNode(getter_AddRefs(anchorDomNode));
  NS_ENSURE_TRUE(anchorDomNode, NS_ERROR_FAILURE);
  rv = mSelection->GetFocusNode(getter_AddRefs(focusDomNode));
  NS_ENSURE_TRUE(focusDomNode, NS_ERROR_FAILURE);

  int32_t anchorOffset, focusOffset;
  rv = mSelection->GetAnchorOffset(&anchorOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mSelection->GetFocusOffset(&focusOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> anchorNode(do_QueryInterface(anchorDomNode));
  nsCOMPtr<nsINode> focusNode(do_QueryInterface(focusDomNode));
  NS_ENSURE_TRUE(anchorNode && focusNode, NS_ERROR_UNEXPECTED);

  int16_t compare = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  focusNode, focusOffset);
  aEvent->mReply.mReversed = compare > 0;

  if (compare) {
    rv = GenerateFlatTextContent(mFirstSelectedRange, aEvent->mReply.mString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryTextContent(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  NS_ASSERTION(aEvent->mReply.mString.IsEmpty(),
               "The reply string must be empty");

  nsRefPtr<nsRange> range = new nsRange();
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset,
                                  aEvent->mInput.mLength, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GenerateFlatTextContent(range, aEvent->mReply.mString);
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = true;

  return NS_OK;
}

// Adjust to use a child node if possible
// to make the returned rect more accurate
static nsINode* AdjustTextRectNode(nsINode* aNode,
                                   int32_t& aOffset)
{
  int32_t childCount = int32_t(aNode->GetChildCount());
  nsINode* node = aNode;
  if (childCount) {
    if (aOffset < childCount) {
      node = aNode->GetChildAt(aOffset);
      aOffset = 0;
    } else if (aOffset == childCount) {
      node = aNode->GetChildAt(childCount - 1);
      aOffset = node->IsNodeOfType(nsINode::eTEXT) ?
          static_cast<nsIContent*>(node)->TextLength() : 1;
    }
  }
  return node;
}

// Similar to nsFrameSelection::GetFrameForNodeOffset,
// but this is more flexible for OnQueryTextRect to use
static nsresult GetFrameForTextRect(nsINode* aNode,
                                    int32_t aOffset,
                                    bool aHint,
                                    nsIFrame** aReturnFrame)
{
  NS_ENSURE_TRUE(aNode && aNode->IsNodeOfType(nsINode::eCONTENT),
                 NS_ERROR_UNEXPECTED);
  nsIContent* content = static_cast<nsIContent*>(aNode);
  nsIFrame* frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  int32_t childOffset = 0;
  return frame->GetChildFrameContainingOffset(aOffset, aHint, &childOffset,
                                              aReturnFrame);
}

nsresult
nsContentEventHandler::OnQueryTextRect(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsRange> range = new nsRange();
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset,
                                  aEvent->mInput.mLength, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // used to iterate over all contents and their frames
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  iter->Init(range);

  // get the starting frame
  int32_t offset = range->StartOffset();
  nsINode* node = iter->GetCurrentNode();
  if (!node) {
    node = AdjustTextRectNode(range->GetStartParent(), offset);
  }
  nsIFrame* firstFrame = nullptr;
  rv = GetFrameForTextRect(node, offset, true, &firstFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the starting frame rect
  nsRect rect(nsPoint(0, 0), firstFrame->GetRect().Size());
  rv = ConvertToRootViewRelativeOffset(firstFrame, rect);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect frameRect = rect;
  nsPoint ptOffset;
  firstFrame->GetPointFromOffset(offset, &ptOffset);
  // minus 1 to avoid creating an empty rect
  rect.x += ptOffset.x - 1;
  rect.width -= ptOffset.x - 1;

  // get the ending frame
  offset = range->EndOffset();
  node = AdjustTextRectNode(range->GetEndParent(), offset);
  nsIFrame* lastFrame = nullptr;
  rv = GetFrameForTextRect(node, offset, range->Collapsed(), &lastFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  // iterate over all covered frames
  for (nsIFrame* frame = firstFrame; frame != lastFrame;) {
    frame = frame->GetNextContinuation();
    if (!frame) {
      do {
        iter->Next();
        node = iter->GetCurrentNode();
        if (!node)
          break;
        if (!node->IsNodeOfType(nsINode::eCONTENT))
          continue;
        frame = static_cast<nsIContent*>(node)->GetPrimaryFrame();
      } while (!frame && !iter->IsDone());
      if (!frame) {
        // this can happen when the end offset of the range is 0.
        frame = lastFrame;
      }
    }
    frameRect.SetRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootViewRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    if (frame != lastFrame) {
      // not last frame, so just add rect to previous result
      rect.UnionRect(rect, frameRect);
    }
  }

  // get the ending frame rect
  lastFrame->GetPointFromOffset(offset, &ptOffset);
  // minus 1 to avoid creating an empty rect
  frameRect.width -= lastFrame->GetRect().width - ptOffset.x - 1;

  if (firstFrame == lastFrame) {
    rect.IntersectRect(rect, frameRect);
  } else {
    rect.UnionRect(rect, frameRect);
  }
  aEvent->mReply.mRect =
      rect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel());
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryEditorRect(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  nsIFrame* frame = mRootContent->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // get rect for first frame
  nsRect resultRect(nsPoint(0, 0), frame->GetRect().Size());
  rv = ConvertToRootViewRelativeOffset(frame, resultRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // account for any additional frames
  while ((frame = frame->GetNextContinuation()) != nullptr) {
    nsRect frameRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootViewRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    resultRect.UnionRect(resultRect, frameRect);
  }

  aEvent->mReply.mRect =
      resultRect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel());
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryCaretRect(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsCaret> caret = mPresShell->GetCaret();
  NS_ASSERTION(caret, "GetCaret returned null");

  // When the selection is collapsed and the queried offset is current caret
  // position, we should return the "real" caret rect.
  bool selectionIsCollapsed;
  rv = mSelection->GetIsCollapsed(&selectionIsCollapsed);
  NS_ENSURE_SUCCESS(rv, rv);

  if (selectionIsCollapsed) {
    uint32_t offset;
    rv = GetFlatTextOffsetOfRange(mRootContent, mFirstSelectedRange, &offset);
    NS_ENSURE_SUCCESS(rv, rv);
    if (offset == aEvent->mInput.mOffset) {
      nsRect rect;
      nsIFrame* caretFrame = caret->GetGeometry(mSelection, &rect);
      if (!caretFrame)
        return NS_ERROR_FAILURE;
      rv = ConvertToRootViewRelativeOffset(caretFrame, rect);
      NS_ENSURE_SUCCESS(rv, rv);
      aEvent->mReply.mRect =
        rect.ToOutsidePixels(caretFrame->PresContext()->AppUnitsPerDevPixel());
      aEvent->mSucceeded = true;
      return NS_OK;
    }
  }

  // Otherwise, we should set the guessed caret rect.
  nsRefPtr<nsRange> range = new nsRange();
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset, 0, true);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t offsetInFrame;
  nsIFrame* frame;
  rv = GetStartFrameAndOffset(range, &frame, &offsetInFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPoint posInFrame;
  rv = frame->GetPointFromOffset(range->StartOffset(), &posInFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect rect;
  rect.x = posInFrame.x;
  rect.y = posInFrame.y;
  rect.width = caret->GetCaretRect().width;
  rect.height = frame->GetSize().height;

  rv = ConvertToRootViewRelativeOffset(frame, rect);
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mReply.mRect =
      rect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel());
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryContentState(nsQueryContentEvent * aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;
  
  aEvent->mSucceeded = true;

  return NS_OK;
}

nsresult
nsContentEventHandler::OnQuerySelectionAsTransferable(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  if (!aEvent->mReply.mHasSelection) {
    aEvent->mSucceeded = true;
    aEvent->mReply.mTransferable = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = mPresShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  rv = nsCopySupport::GetTransferableForSelection(mSelection, doc, getter_AddRefs(aEvent->mReply.mTransferable));
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryCharacterAtPoint(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, NS_ERROR_FAILURE);
  nsIWidget* rootWidget = rootFrame->GetNearestWidget();
  NS_ENSURE_TRUE(rootWidget, NS_ERROR_FAILURE);

  // The root frame's widget might be different, e.g., the event was fired on
  // a popup but the rootFrame is the document root.
  if (rootWidget != aEvent->widget) {
    NS_PRECONDITION(aEvent->widget, "The event must have the widget");
    nsIView* view = nsIView::GetViewFor(aEvent->widget);
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
    rootFrame = view->GetFrame();
    NS_ENSURE_TRUE(rootFrame, NS_ERROR_FAILURE);
    rootWidget = rootFrame->GetNearestWidget();
    NS_ENSURE_TRUE(rootWidget, NS_ERROR_FAILURE);
  }

  nsQueryContentEvent eventOnRoot(true, NS_QUERY_CHARACTER_AT_POINT,
                                  rootWidget);
  eventOnRoot.refPoint = aEvent->refPoint;
  if (rootWidget != aEvent->widget) {
    eventOnRoot.refPoint += aEvent->widget->WidgetToScreenOffset();
    eventOnRoot.refPoint -= rootWidget->WidgetToScreenOffset();
  }
  nsPoint ptInRoot =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(&eventOnRoot, rootFrame);

  nsIFrame* targetFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, ptInRoot);
  if (!targetFrame || targetFrame->GetType() != nsGkAtoms::textFrame ||
      !targetFrame->GetContent() ||
      !nsContentUtils::ContentIsDescendantOf(targetFrame->GetContent(),
                                             mRootContent)) {
    // there is no character at the point.
    aEvent->mReply.mOffset = nsQueryContentEvent::NOT_FOUND;
    aEvent->mSucceeded = true;
    return NS_OK;
  }
  nsPoint ptInTarget = ptInRoot + rootFrame->GetOffsetToCrossDoc(targetFrame);
  int32_t rootAPD = rootFrame->PresContext()->AppUnitsPerDevPixel();
  int32_t targetAPD = targetFrame->PresContext()->AppUnitsPerDevPixel();
  ptInTarget = ptInTarget.ConvertAppUnits(rootAPD, targetAPD);

  nsTextFrame* textframe = static_cast<nsTextFrame*>(targetFrame);
  nsIFrame::ContentOffsets offsets =
    textframe->GetCharacterOffsetAtFramePoint(ptInTarget);
  NS_ENSURE_TRUE(offsets.content, NS_ERROR_FAILURE);
  uint32_t nativeOffset;
  rv = GetFlatTextOffsetOfRange(mRootContent, offsets.content, offsets.offset,
                                &nativeOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsQueryContentEvent textRect(true, NS_QUERY_TEXT_RECT, aEvent->widget);
  textRect.InitForQueryTextRect(nativeOffset, 1);
  rv = OnQueryTextRect(&textRect);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(textRect.mSucceeded, NS_ERROR_FAILURE);

  // currently, we don't need to get the actual text.
  aEvent->mReply.mOffset = nativeOffset;
  aEvent->mReply.mRect = textRect.mReply.mRect;
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryDOMWidgetHittest(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  aEvent->mReply.mWidgetIsHit = false;

  NS_ENSURE_TRUE(aEvent->widget, NS_ERROR_FAILURE);

  nsIDocument* doc = mPresShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  nsIFrame* docFrame = mPresShell->GetRootFrame();
  NS_ENSURE_TRUE(docFrame, NS_ERROR_FAILURE);

  nsIntPoint eventLoc =
    aEvent->refPoint + aEvent->widget->WidgetToScreenOffset();
  nsIntRect docFrameRect = docFrame->GetScreenRect(); // Returns CSS pixels
  eventLoc.x = mPresContext->DevPixelsToIntCSSPixels(eventLoc.x);
  eventLoc.y = mPresContext->DevPixelsToIntCSSPixels(eventLoc.y);
  eventLoc.x -= docFrameRect.x;
  eventLoc.y -= docFrameRect.y;

  nsCOMPtr<nsIDOMElement> elementUnderMouse;
  doc->ElementFromPointHelper(eventLoc.x, eventLoc.y, false, false,
                              getter_AddRefs(elementUnderMouse));
  nsCOMPtr<nsIContent> contentUnderMouse = do_QueryInterface(elementUnderMouse);
  if (contentUnderMouse) {
    nsIWidget* targetWidget = nullptr;
    nsIFrame* targetFrame = contentUnderMouse->GetPrimaryFrame();
    nsIObjectFrame* pluginFrame = do_QueryFrame(targetFrame);
    if (pluginFrame) {
      targetWidget = pluginFrame->GetWidget();
    } else if (targetFrame) {
      targetWidget = targetFrame->GetNearestWidget();
    }
    if (aEvent->widget == targetWidget)
      aEvent->mReply.mWidgetIsHit = true;
  }

  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsContentEventHandler::GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                                nsINode* aNode,
                                                int32_t aNodeOffset,
                                                uint32_t* aNativeOffset)
{
  NS_ASSERTION(aNativeOffset, "param is invalid");

  nsRefPtr<nsRange> prev = new nsRange();
  nsCOMPtr<nsIDOMNode> rootDOMNode(do_QueryInterface(aRootContent));
  prev->SetStart(rootDOMNode, 0);

  nsCOMPtr<nsIDOMNode> startDOMNode(do_QueryInterface(aNode));
  NS_ASSERTION(startDOMNode, "startNode doesn't have nsIDOMNode");
  prev->SetEnd(startDOMNode, aNodeOffset);

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  iter->Init(prev);

  nsCOMPtr<nsINode> startNode = do_QueryInterface(startDOMNode);
  nsINode* endNode = aNode;

  *aNativeOffset = 0;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node)
      break;
    if (!node->IsNodeOfType(nsINode::eCONTENT))
      continue;
    nsIContent* content = static_cast<nsIContent*>(node);

    if (node->IsNodeOfType(nsINode::eTEXT)) {
      // Note: our range always starts from offset 0
      if (node == endNode)
        *aNativeOffset += GetNativeTextLength(content, aNodeOffset);
      else
        *aNativeOffset += GetNativeTextLength(content);
    } else if (IsContentBR(content)) {
#if defined(XP_WIN)
      // On Windows, the length of the newline is 2.
      *aNativeOffset += 2;
#else
      // On other platforms, the length of the newline is 1.
      *aNativeOffset += 1;
#endif
    }
  }
  return NS_OK;
}

nsresult
nsContentEventHandler::GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                                nsRange* aRange,
                                                uint32_t* aNativeOffset)
{
  nsINode* startNode = aRange->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  int32_t startOffset = aRange->StartOffset();
  return GetFlatTextOffsetOfRange(aRootContent, startNode, startOffset,
                                  aNativeOffset);
}

nsresult
nsContentEventHandler::GetStartFrameAndOffset(nsRange* aRange,
                                              nsIFrame** aFrame,
                                              int32_t* aOffsetInFrame)
{
  NS_ASSERTION(aRange && aFrame && aOffsetInFrame, "params are invalid");

  nsIContent* content = nullptr;
  nsINode* node = aRange->GetStartParent();
  if (node && node->IsNodeOfType(nsINode::eCONTENT))
    content = static_cast<nsIContent*>(node);
  NS_ASSERTION(content, "the start node doesn't have nsIContent!");

  nsRefPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
  *aFrame = fs->GetFrameForNodeOffset(content, aRange->StartOffset(),
                                      fs->GetHint(), aOffsetInFrame);
  NS_ENSURE_TRUE((*aFrame), NS_ERROR_FAILURE);
  NS_ASSERTION((*aFrame)->GetType() == nsGkAtoms::textFrame,
               "The frame is not textframe");
  return NS_OK;
}

nsresult
nsContentEventHandler::ConvertToRootViewRelativeOffset(nsIFrame* aFrame,
                                                       nsRect& aRect)
{
  NS_ASSERTION(aFrame, "aFrame must not be null");

  nsIView* view = nullptr;
  nsPoint posInView;
  aFrame->GetOffsetFromView(posInView, &view);
  if (!view)
    return NS_ERROR_FAILURE;
  aRect += posInView + view->GetOffsetTo(nullptr);
  return NS_OK;
}

static void AdjustRangeForSelection(nsIContent* aRoot,
                                    nsINode** aNode,
                                    int32_t* aOffset)
{
  nsINode* node = *aNode;
  int32_t offset = *aOffset;
  if (aRoot != node && node->GetParent() &&
      !node->IsNodeOfType(nsINode::eTEXT)) {
    node = node->GetParent();
    offset = node->IndexOf(*aNode) + (offset ? 1 : 0);
  }
  
  nsIContent* brContent = node->GetChildAt(offset - 1);
  while (brContent && brContent->IsHTML()) {
    if (brContent->Tag() != nsGkAtoms::br || IsContentBR(brContent))
      break;
    brContent = node->GetChildAt(--offset - 1);
  }
  *aNode = node;
  *aOffset = NS_MAX(offset, 0);
}

nsresult
nsContentEventHandler::OnSelectionEvent(nsSelectionEvent* aEvent)
{
  aEvent->mSucceeded = false;

  // Get selection to manipulate
  // XXX why do we need to get them from ISM? This method should work fine
  //     without ISM.
  nsresult rv = nsIMEStateManager::
      GetFocusSelectionAndRoot(getter_AddRefs(mSelection),
                               getter_AddRefs(mRootContent));
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = Init(aEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get range from offset and length
  nsRefPtr<nsRange> range = new nsRange();
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mOffset, aEvent->mLength,
                                  aEvent->mExpandToClusterBoundary);
  NS_ENSURE_SUCCESS(rv, rv);

  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
  int32_t startOffset = range->StartOffset();
  int32_t endOffset = range->EndOffset();
  AdjustRangeForSelection(mRootContent, &startNode, &startOffset);
  AdjustRangeForSelection(mRootContent, &endNode, &endOffset);

  nsCOMPtr<nsIDOMNode> startDomNode(do_QueryInterface(startNode));
  nsCOMPtr<nsIDOMNode> endDomNode(do_QueryInterface(endNode));
  NS_ENSURE_TRUE(startDomNode && endDomNode, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSelection));
  selPrivate->StartBatchChanges();

  // Clear selection first before setting
  rv = mSelection->RemoveAllRanges();
  // Need to call EndBatchChanges at the end even if call failed
  if (NS_SUCCEEDED(rv)) {
    if (aEvent->mReversed) {
      rv = mSelection->Collapse(endDomNode, endOffset);
    } else {
      rv = mSelection->Collapse(startDomNode, startOffset);
    }
    if (NS_SUCCEEDED(rv) &&
        (startDomNode != endDomNode || startOffset != endOffset)) {
      if (aEvent->mReversed) {
        rv = mSelection->Extend(startDomNode, startOffset);
      } else {
        rv = mSelection->Extend(endDomNode, endOffset);
      }
    }
  }
  selPrivate->EndBatchChanges();
  NS_ENSURE_SUCCESS(rv, rv);

  selPrivate->ScrollIntoViewInternal(
    nsISelectionController::SELECTION_FOCUS_REGION,
    false, nsIPresShell::ScrollAxis(), nsIPresShell::ScrollAxis());
  aEvent->mSucceeded = true;
  return NS_OK;
}

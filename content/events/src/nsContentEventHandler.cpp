/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Ningjie Chen <chenn@email.uc.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsContentEventHandler.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsIDOMText.h"
#include "nsIDOMRange.h"
#include "nsRange.h"
#include "nsGUIEvent.h"
#include "nsCaret.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIContentIterator.h"
#include "nsTextFragment.h"
#include "nsTextFrame.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsContentUtils.h"
#include "nsISelection2.h"
#include "nsIMEStateManager.h"

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);

/******************************************************************/
/* nsContentEventHandler                                          */
/******************************************************************/

nsContentEventHandler::nsContentEventHandler(
                              nsPresContext* aPresContext) :
  mPresContext(aPresContext),
  mPresShell(aPresContext->GetPresShell()), mSelection(nsnull),
  mFirstSelectedRange(nsnull), mRootContent(nsnull)
{
}

nsresult
nsContentEventHandler::Init(nsQueryContentEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent must not be null");

  if (mSelection)
    return NS_OK;

  aEvent->mSucceeded = PR_FALSE;

  if (!mPresShell)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = mPresShell->GetSelectionForCopy(getter_AddRefs(mSelection));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mSelection,
               "GetSelectionForCopy succeeded, but the result is null");

  nsCOMPtr<nsIDOMRange> firstRange;
  rv = mSelection->GetRangeAt(0, getter_AddRefs(firstRange));
  // This shell doesn't support selection.
  if (NS_FAILED(rv))
    return NS_ERROR_NOT_AVAILABLE;
  mFirstSelectedRange = do_QueryInterface(firstRange);
  NS_ENSURE_TRUE(mFirstSelectedRange, NS_ERROR_FAILURE);

  nsINode* startNode = mFirstSelectedRange->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  mRootContent = startNode->GetSelectionRootContent(mPresShell);
  NS_ENSURE_TRUE(mRootContent, NS_ERROR_FAILURE);

  aEvent->mReply.mContentsRoot = mRootContent.get();

  nsRefPtr<nsCaret> caret;
  rv = mPresShell->GetCaret(getter_AddRefs(caret));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(caret, "GetCaret succeeded, but the result is null");
  PRBool isCollapsed;
  nsRect r;
  nsIView* view = nsnull;
  rv = caret->GetCaretCoordinates(nsCaret::eRenderingViewCoordinates,
                                  mSelection, &r, &isCollapsed, &view);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  aEvent->mReply.mFocusedWidget = view->GetWidget();

  return NS_OK;
}

// Editor places a bogus BR node under its root content if the editor doesn't
// have any text. This happens even for single line editors.
// When we get text content and when we change the selection,
// we don't want to include the bogus BRs at the end.
static PRBool IsContentBR(nsIContent* aContent)
{
  return aContent->IsNodeOfType(nsINode::eHTML) &&
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

static void ConvertToXPNewlines(nsAFlatString& aString)
{
#if defined(XP_MACOSX)
  aString.ReplaceSubstring(NS_LITERAL_STRING("\r"), NS_LITERAL_STRING("\n"));
#elif defined(XP_WIN)
  aString.ReplaceSubstring(NS_LITERAL_STRING("\r\n"), NS_LITERAL_STRING("\n"));
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
                            PRUint32 aXPOffset, PRUint32 aXPLength)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text)
    return;
  text->AppendTo(aString, PRInt32(aXPOffset), PRInt32(aXPLength));
}

static PRUint32 GetNativeTextLength(nsIContent* aContent)
{
  nsAutoString str;
  if (aContent->IsNodeOfType(nsINode::eTEXT))
    AppendString(str, aContent);
  else if (IsContentBR(aContent))
    str.Assign(PRUnichar('\n'));
  ConvertToNativeNewlines(str);
  return str.Length();
}

static PRUint32 ConvertToXPOffset(nsIContent* aContent, PRUint32 aNativeOffset)
{

  nsAutoString str;
  AppendString(str, aContent);
  ConvertToNativeNewlines(str);
  NS_ASSERTION(aNativeOffset <= str.Length(),
               "aOffsetForNativeLF is too large!");
  str.Truncate(aNativeOffset);
  ConvertToXPNewlines(str);
  return str.Length();
}

static nsresult GenerateFlatTextContent(nsIRange* aRange,
                                        nsAFlatString& aString)
{
  nsCOMPtr<nsIContentIterator> iter;
  nsresult rv = NS_NewContentIterator(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(iter, "NS_NewContentIterator succeeded, but the result is null");
  nsCOMPtr<nsIDOMRange> domRange(do_QueryInterface(aRange));
  NS_ASSERTION(domRange, "aRange doesn't have nsIDOMRange!");
  iter->Init(domRange);

  NS_ASSERTION(aString.IsEmpty(), "aString must be empty string");

  nsINode* startNode = aRange->GetStartParent();
  nsINode* endNode = aRange->GetEndParent();

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
    if (!node || !node->IsNodeOfType(nsINode::eCONTENT))
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
                                                    PRBool aForward,
                                                    PRUint32* aXPOffset)
{
  // XXX This method assumes that the frame boundaries must be cluster
  // boundaries. It's false, but no problem now, maybe.
  if (!aContent->IsNodeOfType(nsINode::eTEXT) ||
      *aXPOffset == 0 || *aXPOffset == aContent->TextLength())
    return NS_OK;

  NS_ASSERTION(*aXPOffset >= 0 && *aXPOffset <= aContent->TextLength(),
               "offset is out of range.");

  nsCOMPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
  PRInt32 offsetInFrame;
  nsFrameSelection::HINT hint =
    aForward ? nsFrameSelection::HINTLEFT : nsFrameSelection::HINTRIGHT;
  nsIFrame* frame = fs->GetFrameForNodeOffset(aContent, PRInt32(*aXPOffset),
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
  PRInt32 startOffset, endOffset;
  nsresult rv = frame->GetOffsets(startOffset, endOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aXPOffset == PRUint32(startOffset) || *aXPOffset == PRUint32(endOffset))
    return NS_OK;
  if (frame->GetType() != nsGkAtoms::textFrame)
    return NS_ERROR_FAILURE;
  nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
  PRInt32 newOffsetInFrame = *aXPOffset - startOffset;
  newOffsetInFrame += aForward ? -1 : 1;
  textFrame->PeekOffsetCharacter(aForward, &newOffsetInFrame);
  *aXPOffset = startOffset + newOffsetInFrame;
  return NS_OK;
}

nsresult
nsContentEventHandler::SetRangeFromFlatTextOffset(
                              nsIRange* aRange,
                              PRUint32 aNativeOffset,
                              PRUint32 aNativeLength,
                              PRBool aExpandToClusterBoundaries)
{
  nsCOMPtr<nsIContentIterator> iter;
  nsresult rv = NS_NewContentIterator(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(iter, "NS_NewContentIterator succeeded, but the result is null");
  rv = iter->Init(mRootContent);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMRange> domRange(do_QueryInterface(aRange));
  NS_ASSERTION(domRange, "aRange doesn't have nsIDOMRange!");

  PRUint32 nativeOffset = 0;
  PRUint32 nativeEndOffset = aNativeOffset + aNativeLength;
  nsCOMPtr<nsIContent> content;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node || !node->IsNodeOfType(nsINode::eCONTENT))
      continue;
    nsIContent* content = static_cast<nsIContent*>(node);

    PRUint32 nativeTextLength;
    nativeTextLength = GetNativeTextLength(content);
    if (nativeTextLength == 0)
      continue;

    if (nativeOffset <= aNativeOffset &&
        aNativeOffset < nativeOffset + nativeTextLength) {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(content));
      NS_ASSERTION(domNode, "aContent doesn't have nsIDOMNode!");

      PRUint32 xpOffset =
        content->IsNodeOfType(nsINode::eTEXT) ?
          ConvertToXPOffset(content, aNativeOffset - nativeOffset) : 0;

      if (aExpandToClusterBoundaries) {
        rv = ExpandToClusterBoundary(content, PR_FALSE, &xpOffset);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = domRange->SetStart(domNode, PRInt32(xpOffset));
      NS_ENSURE_SUCCESS(rv, rv);
      if (aNativeLength == 0) {
        // Ensure that the end offset and the start offset are same.
        rv = domRange->SetEnd(domNode, PRInt32(xpOffset));
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    if (nativeEndOffset <= nativeOffset + nativeTextLength) {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(content));
      NS_ASSERTION(domNode, "aContent doesn't have nsIDOMNode!");

      PRUint32 xpOffset;
      if (content->IsNodeOfType(nsINode::eTEXT)) {
        xpOffset = ConvertToXPOffset(content, nativeEndOffset - nativeOffset);
        if (aExpandToClusterBoundaries) {
          rv = ExpandToClusterBoundary(content, PR_TRUE, &xpOffset);
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

      rv = domRange->SetEnd(domNode, PRInt32(xpOffset));
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    nativeOffset += nativeTextLength;
  }

  if (nativeOffset < aNativeOffset)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mRootContent));
  NS_ASSERTION(domNode, "lastContent doesn't have nsIDOMNode!");
  if (!content) {
    rv = domRange->SetStart(domNode, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = domRange->SetEnd(domNode, PRInt32(mRootContent->GetChildCount()));
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

  PRInt32 anchorOffset, focusOffset;
  rv = mSelection->GetAnchorOffset(&anchorOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mSelection->GetFocusOffset(&focusOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> anchorNode(do_QueryInterface(anchorDomNode));
  nsCOMPtr<nsINode> focusNode(do_QueryInterface(focusDomNode));
  NS_ENSURE_TRUE(anchorNode && focusNode, NS_ERROR_UNEXPECTED);

  PRInt16 compare = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  focusNode, focusOffset);
  aEvent->mReply.mReversed = compare > 0;

  if (compare) {
    nsCOMPtr<nsIRange> range = mFirstSelectedRange;
    rv = GenerateFlatTextContent(range, aEvent->mReply.mString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aEvent->mSucceeded = PR_TRUE;
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

  nsCOMPtr<nsIRange> range = new nsRange();
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset,
                                  aEvent->mInput.mLength, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GenerateFlatTextContent(range, aEvent->mReply.mString);
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = PR_TRUE;

  return NS_OK;
}

// Adjust to use a child node if possible
// to make the returned rect more accurate
static nsINode* AdjustTextRectNode(nsINode* aNode,
                                   PRInt32& aOffset)
{
  PRInt32 childCount = PRInt32(aNode->GetChildCount());
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
static nsresult GetFrameForTextRect(nsIPresShell* aPresShell,
                                    nsINode* aNode,
                                    PRInt32 aOffset,
                                    PRBool aHint,
                                    nsIFrame** aReturnFrame)
{
  NS_ENSURE_TRUE(aNode && aNode->IsNodeOfType(nsINode::eCONTENT),
                 NS_ERROR_UNEXPECTED);
  nsIContent* content = static_cast<nsIContent*>(aNode);
  nsIFrame* frame = aPresShell->GetPrimaryFrameFor(content);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  PRInt32 childOffset = 0;
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
  if (!range) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset,
                                  aEvent->mInput.mLength, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // used to iterate over all contents and their frames
  nsCOMPtr<nsIContentIterator> iter;
  rv = NS_NewContentIterator(getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);
  iter->Init(range);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the starting frame
  PRInt32 offset = range->StartOffset();
  nsINode* node = iter->GetCurrentNode();
  if (!node) {
    node = AdjustTextRectNode(range->GetStartParent(), offset);
  }
  nsIFrame* firstFrame = nsnull;
  rv = GetFrameForTextRect(mPresShell, node, offset,
                           PR_TRUE, &firstFrame);
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
  nsIFrame* lastFrame = nsnull;
  rv = GetFrameForTextRect(mPresShell, node, offset,
                           range->Collapsed(), &lastFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  // iterate over all covered frames
  for (nsIFrame* frame = firstFrame; frame != lastFrame;) {
    frame = frame->GetNextContinuation();
    if (!frame) {
      do {
        iter->Next();
        node = iter->GetCurrentNode();
        if (!node || !node->IsNodeOfType(nsINode::eCONTENT))
          continue;
        frame = mPresShell->GetPrimaryFrameFor(static_cast<nsIContent*>(node));
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
    nsRect::ToOutsidePixels(rect, mPresContext->AppUnitsPerDevPixel());
  aEvent->mSucceeded = PR_TRUE;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryEditorRect(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  nsIFrame* frame = mPresShell->GetPrimaryFrameFor(mRootContent);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // get rect for first frame
  nsRect resultRect(nsPoint(0, 0), frame->GetRect().Size());
  rv = ConvertToRootViewRelativeOffset(frame, resultRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // account for any additional frames
  while ((frame = frame->GetNextContinuation()) != nsnull) {
    nsRect frameRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootViewRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    resultRect.UnionRect(resultRect, frameRect);
  }

  aEvent->mReply.mRect =
    nsRect::ToOutsidePixels(resultRect, mPresContext->AppUnitsPerDevPixel());
  aEvent->mSucceeded = PR_TRUE;
  return NS_OK;
}

nsresult
nsContentEventHandler::OnQueryCaretRect(nsQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsCaret> caret;
  rv = mPresShell->GetCaret(getter_AddRefs(caret));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(caret, "GetCaret succeeded, but the result is null");

  // When the selection is collapsed and the queried offset is current caret
  // position, we should return the "real" caret rect.
  PRBool selectionIsCollapsed;
  rv = mSelection->GetIsCollapsed(&selectionIsCollapsed);
  NS_ENSURE_SUCCESS(rv, rv);

  if (selectionIsCollapsed) {
    PRUint32 offset;
    rv = GetFlatTextOffsetOfRange(mRootContent, mFirstSelectedRange, &offset);
    NS_ENSURE_SUCCESS(rv, rv);
    if (offset == aEvent->mInput.mOffset) {
      PRBool isCollapsed;
      nsRect rect;
      rv = caret->GetCaretCoordinates(nsCaret::eTopLevelWindowCoordinates,
                                      mSelection, &rect,
                                      &isCollapsed, nsnull);
      aEvent->mReply.mRect =
        nsRect::ToOutsidePixels(rect, mPresContext->AppUnitsPerDevPixel());
      NS_ENSURE_SUCCESS(rv, rv);
      aEvent->mSucceeded = PR_TRUE;
      return NS_OK;
    }
  }

  // Otherwise, we should set the guessed caret rect.
  nsCOMPtr<nsIRange> range = new nsRange();
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset, 0, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 offsetInFrame;
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
    nsRect::ToOutsidePixels(rect, mPresContext->AppUnitsPerDevPixel());

  aEvent->mSucceeded = PR_TRUE;
  return NS_OK;
}

nsresult
nsContentEventHandler::GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                                nsINode* aNode,
                                                PRInt32 aNodeOffset,
                                                PRUint32* aNativeOffset)
{
  NS_ASSERTION(aNativeOffset, "param is invalid");

  nsCOMPtr<nsIRange> prev = new nsRange();
  NS_ENSURE_TRUE(prev, NS_ERROR_OUT_OF_MEMORY);
  nsCOMPtr<nsIDOMRange> domPrev(do_QueryInterface(prev));
  NS_ASSERTION(domPrev, "nsRange doesn't have nsIDOMRange??");
  nsCOMPtr<nsIDOMNode> rootDOMNode(do_QueryInterface(aRootContent));
  domPrev->SetStart(rootDOMNode, 0);

  nsCOMPtr<nsIDOMNode> startDOMNode(do_QueryInterface(aNode));
  NS_ASSERTION(startDOMNode, "startNode doesn't have nsIDOMNode");
  domPrev->SetEnd(startDOMNode, aNodeOffset);

  nsAutoString prevStr;
  nsresult rv = GenerateFlatTextContent(prev, prevStr);
  NS_ENSURE_SUCCESS(rv, rv);
  *aNativeOffset = prevStr.Length();
  return NS_OK;
}

nsresult
nsContentEventHandler::GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                                nsIRange* aRange,
                                                PRUint32* aNativeOffset)
{
  nsINode* startNode = aRange->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  PRInt32 startOffset = aRange->StartOffset();
  return GetFlatTextOffsetOfRange(aRootContent, startNode, startOffset,
                                  aNativeOffset);
}

nsresult
nsContentEventHandler::GetStartFrameAndOffset(nsIRange* aRange,
                                              nsIFrame** aFrame,
                                              PRInt32* aOffsetInFrame)
{
  NS_ASSERTION(aRange && aFrame && aOffsetInFrame, "params are invalid");

  nsIContent* content = nsnull;
  nsINode* node = aRange->GetStartParent();
  if (node && node->IsNodeOfType(nsINode::eCONTENT))
    content = static_cast<nsIContent*>(node);
  NS_ASSERTION(content, "the start node doesn't have nsIContent!");

  nsCOMPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
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

  nsIView* view = nsnull;
  nsPoint posInView;
  aFrame->GetOffsetFromView(posInView, &view);
  if (!view)
    return NS_ERROR_FAILURE;
  aRect += posInView + view->GetOffsetTo(nsnull);
  return NS_OK;
}

static void AdjustRangeForSelection(nsIContent* aRoot,
                                    nsINode** aNode,
                                    PRInt32* aOffset)
{
  nsINode* node = *aNode;
  PRInt32 offset = *aOffset;
  if (aRoot != node && node->GetParent() &&
      !node->IsNodeOfType(nsINode::eTEXT)) {
    node = node->GetParent();
    offset = node->IndexOf(*aNode) + (offset ? 1 : 0);
  }
  nsINode* brNode = node->GetChildAt(offset - 1);
  while (brNode && brNode->IsNodeOfType(nsINode::eHTML)) {
    nsIContent* brContent = static_cast<nsIContent*>(brNode);
    if (brContent->Tag() != nsGkAtoms::br || IsContentBR(brContent))
      break;
    brNode = node->GetChildAt(--offset - 1);
  }
  *aNode = node;
  *aOffset = PR_MAX(offset, 0);
}

nsresult
nsContentEventHandler::OnSelectionEvent(nsSelectionEvent* aEvent)
{
  aEvent->mSucceeded = PR_FALSE;

  // Get selection to manipulate
  nsCOMPtr<nsISelection> sel;
  nsresult rv = nsIMEStateManager::
      GetFocusSelectionAndRoot(getter_AddRefs(sel),
                               getter_AddRefs(mRootContent));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get range from offset and length
  nsRefPtr<nsRange> range = new nsRange();
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mOffset,
                                  aEvent->mLength, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
  PRInt32 startOffset = range->StartOffset();
  PRInt32 endOffset = range->EndOffset();
  AdjustRangeForSelection(mRootContent, &startNode, &startOffset);
  AdjustRangeForSelection(mRootContent, &endNode, &endOffset);

  nsCOMPtr<nsIDOMNode> startDomNode(do_QueryInterface(startNode));
  nsCOMPtr<nsIDOMNode> endDomNode(do_QueryInterface(endNode));
  NS_ENSURE_TRUE(startDomNode && endDomNode, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(sel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_UNEXPECTED);
  selPrivate->StartBatchChanges();

  // Clear selection first before setting
  rv = sel->RemoveAllRanges();
  // Need to call EndBatchChanges at the end even if call failed
  if (NS_SUCCEEDED(rv)) {
    if (aEvent->mReversed) {
      rv = sel->Collapse(endDomNode, endOffset);
    } else {
      rv = sel->Collapse(startDomNode, startOffset);
    }
    if (NS_SUCCEEDED(rv) &&
        (startDomNode != endDomNode || startOffset != endOffset)) {
      if (aEvent->mReversed) {
        rv = sel->Extend(startDomNode, startOffset);
      } else {
        rv = sel->Extend(endDomNode, endOffset);
      }
    }
  }
  selPrivate->EndBatchChanges();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelection2>(do_QueryInterface(sel))->ScrollIntoView(
      nsISelectionController::SELECTION_FOCUS_REGION, PR_FALSE, -1, -1);
  aEvent->mSucceeded = PR_TRUE;
  return NS_OK;
}

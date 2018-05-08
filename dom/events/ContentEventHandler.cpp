/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentEventHandler.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/Selection.h"
#include "nsCaret.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCopySupport.h"
#include "nsElementTable.h"
#include "nsFocusManager.h"
#include "nsFontMetrics.h"
#include "nsFrameSelection.h"
#include "nsIContentIterator.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIObjectFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsQueryObject.h"
#include "nsRange.h"
#include "nsTextFragment.h"
#include "nsTextFrame.h"
#include "nsView.h"

#include <algorithm>

namespace mozilla {

using namespace dom;
using namespace widget;

/******************************************************************/
/* ContentEventHandler::RawRange                                  */
/******************************************************************/

void
ContentEventHandler::RawRange::AssertStartIsBeforeOrEqualToEnd()
{
  MOZ_ASSERT(
    nsContentUtils::ComparePoints(mStart.Container(),
                                  static_cast<int32_t>(mStart.Offset()),
                                  mEnd.Container(),
                                  static_cast<int32_t>(mEnd.Offset())) <= 0);
}

nsresult
ContentEventHandler::RawRange::SetStart(const RawRangeBoundary& aStart)
{
  nsINode* newRoot = nsRange::ComputeRootNode(aStart.Container());
  if (!newRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }

  if (!aStart.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, or if positioned in another document.
  if (!IsPositioned() || newRoot != mRoot) {
    mRoot = newRoot;
    mStart = mEnd = aStart;
    return NS_OK;
  }

  mStart = aStart;
  AssertStartIsBeforeOrEqualToEnd();
  return NS_OK;
}

nsresult
ContentEventHandler::RawRange::SetEnd(const RawRangeBoundary& aEnd)
{
  nsINode* newRoot = nsRange::ComputeRootNode(aEnd.Container());
  if (!newRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }

  if (!aEnd.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, or if positioned in another document.
  if (!IsPositioned() || newRoot != mRoot) {
    mRoot = newRoot;
    mStart = mEnd = aEnd;
    return NS_OK;
  }

  mEnd = aEnd;
  AssertStartIsBeforeOrEqualToEnd();
  return NS_OK;
}

nsresult
ContentEventHandler::RawRange::SetEndAfter(nsINode* aEndContainer)
{
  uint32_t offset = 0;
  nsINode* container =
    nsRange::GetContainerAndOffsetAfter(aEndContainer, &offset);
  return SetEnd(container, offset);
}

void
ContentEventHandler::RawRange::SetStartAndEnd(const nsRange* aRange)
{
  DebugOnly<nsresult> rv = SetStartAndEnd(aRange->StartRef().AsRaw(),
                                          aRange->EndRef().AsRaw());
  MOZ_ASSERT(!aRange->IsPositioned() || NS_SUCCEEDED(rv));
}

nsresult
ContentEventHandler::RawRange::SetStartAndEnd(const RawRangeBoundary& aStart,
                                              const RawRangeBoundary& aEnd)
{
  nsINode* newStartRoot = nsRange::ComputeRootNode(aStart.Container());
  if (!newStartRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!aStart.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (aStart.Container() == aEnd.Container()) {
    if (!aEnd.IsSetAndValid()) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }
    MOZ_ASSERT(aStart.Offset() <= aEnd.Offset());
    mRoot = newStartRoot;
    mStart = aStart;
    mEnd = aEnd;
    return NS_OK;
  }

  nsINode* newEndRoot = nsRange::ComputeRootNode(aEnd.Container());
  if (!newEndRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!aEnd.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // If they have different root, this should be collapsed at the end point.
  if (newStartRoot != newEndRoot) {
    mRoot = newEndRoot;
    mStart = mEnd = aEnd;
    return NS_OK;
  }

  // Otherwise, set the range as specified.
  mRoot = newStartRoot;
  mStart = aStart;
  mEnd = aEnd;
  AssertStartIsBeforeOrEqualToEnd();
  return NS_OK;
}

nsresult
ContentEventHandler::RawRange::SelectNodeContents(
                                 nsINode* aNodeToSelectContents)
{
  nsINode* newRoot = nsRange::ComputeRootNode(aNodeToSelectContents);
  if (!newRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  mRoot = newRoot;
  mStart = RawRangeBoundary(aNodeToSelectContents, nullptr);
  mEnd = RawRangeBoundary(aNodeToSelectContents,
                          aNodeToSelectContents->GetLastChild());
  return NS_OK;
}

/******************************************************************/
/* ContentEventHandler                                            */
/******************************************************************/

// NOTE
//
// ContentEventHandler *creates* ranges as following rules:
// 1. Start of range:
//   1.1. Cases: [textNode or text[Node or textNode[
//        When text node is start of a range, start node is the text node and
//        start offset is any number between 0 and the length of the text.
//   1.2. Case: [<element>:
//        When start of an element node is start of a range, start node is
//        parent of the element and start offset is the element's index in the
//        parent.
//   1.3. Case: <element/>[
//        When after an empty element node is start of a range, start node is
//        parent of the element and start offset is the element's index in the
//        parent + 1.
//   1.4. Case: <element>[
//        When start of a non-empty element is start of a range, start node is
//        the element and start offset is 0.
//   1.5. Case: <root>[
//        When start of a range is 0 and there are no nodes causing text,
//        start node is the root node and start offset is 0.
//   1.6. Case: [</root>
//        When start of a range is out of bounds, start node is the root node
//        and start offset is number of the children.
// 2. End of range:
//   2.1. Cases: ]textNode or text]Node or textNode]
//        When a text node is end of a range, end node is the text node and
//        end offset is any number between 0 and the length of the text.
//   2.2. Case: ]<element>
//        When before an element node (meaning before the open tag of the
//        element) is end of a range, end node is previous node causing text.
//        Note that this case shouldn't be handled directly.  If rule 2.1 and
//        2.3 are handled correctly, the loop with nsContentIterator shouldn't
//        reach the element node since the loop should've finished already at
//        handling the last node which caused some text.
//   2.3. Case: <element>]
//        When a line break is caused before a non-empty element node and it's
//        end of a range, end node is the element and end offset is 0.
//        (i.e., including open tag of the element)
//   2.4. Cases: <element/>]
//        When after an empty element node is end of a range, end node is
//        parent of the element node and end offset is the element's index in
//        the parent + 1.  (i.e., including close tag of the element or empty
//        element)
//   2.5. Case: ]</root>
//        When end of a range is out of bounds, end node is the root node and
//        end offset is number of the children.
//
// ContentEventHandler *treats* ranges as following additional rules:
// 1. When the start node is an element node which doesn't have children,
//    it includes a line break caused before itself (i.e., includes its open
//    tag).  For example, if start position is { <br>, 0 }, the line break
//    caused by <br> should be included into the flatten text.
// 2. When the end node is an element node which doesn't have children,
//    it includes the end (i.e., includes its close tag except empty element).
//    Although, currently, any close tags don't cause line break, this also
//    includes its open tag.  For example, if end position is { <br>, 0 }, the
//    line break caused by the <br> should be included into the flatten text.

ContentEventHandler::ContentEventHandler(nsPresContext* aPresContext)
  : mDocument(aPresContext->Document())
{
}

nsresult
ContentEventHandler::InitBasic()
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  // If text frame which has overflowing selection underline is dirty,
  // we need to flush the pending reflow here.
  mDocument->FlushPendingNotifications(FlushType::Layout);
  return NS_OK;
}

nsresult
ContentEventHandler::InitRootContent(Selection* aNormalSelection)
{
  MOZ_ASSERT(aNormalSelection);

  // Root content should be computed with normal selection because normal
  // selection is typically has at least one range but the other selections
  // not so.  If there is a range, computing its root is easy, but if
  // there are no ranges, we need to use ancestor limit instead.
  MOZ_ASSERT(aNormalSelection->Type() == SelectionType::eNormal);

  if (!aNormalSelection->RangeCount()) {
    // If there is no selection range, we should compute the selection root
    // from ancestor limiter or root content of the document.
    mRootContent = aNormalSelection->GetAncestorLimiter();
    if (!mRootContent) {
      mRootContent = mDocument->GetRootElement();
      if (NS_WARN_IF(!mRootContent)) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    return NS_OK;
  }

  RefPtr<nsRange> range(aNormalSelection->GetRangeAt(0));
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_UNEXPECTED;
  }

  // If there is a selection, we should retrieve the selection root from
  // the range since when the window is inactivated, the ancestor limiter
  // of selection was cleared by blur event handler of EditorBase but the
  // selection range still keeps storing the nodes.  If the active element of
  // the deactive window is <input> or <textarea>, we can compute the
  // selection root from them.
  nsINode* startNode = range->GetStartContainer();
  nsINode* endNode = range->GetEndContainer();
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }

  // See bug 537041 comment 5, the range could have removed node.
  if (NS_WARN_IF(startNode->GetComposedDoc() != mDocument)) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(startNode->GetComposedDoc() == endNode->GetComposedDoc(),
               "firstNormalSelectionRange crosses the document boundary");

  mRootContent = startNode->GetSelectionRootContent(mDocument->GetShell());
  if (NS_WARN_IF(!mRootContent)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
ContentEventHandler::InitCommon(SelectionType aSelectionType)
{
  if (mSelection && mSelection->Type() == aSelectionType) {
    return NS_OK;
  }

  mSelection = nullptr;
  mRootContent = nullptr;
  mFirstSelectedRawRange.Clear();

  nsresult rv = InitBasic();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionController> selectionController;
  if (nsIPresShell* shell = mDocument->GetShell()) {
    selectionController = shell->GetSelectionControllerForFocusedContent();
  }
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mSelection =
    selectionController->GetSelection(ToRawSelectionType(aSelectionType));
  if (NS_WARN_IF(!mSelection)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<Selection> normalSelection;
  if (mSelection->Type() == SelectionType::eNormal) {
    normalSelection = mSelection;
  } else {
    normalSelection = 
      selectionController->GetSelection(nsISelectionController::SELECTION_NORMAL);
    if (NS_WARN_IF(!normalSelection)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  rv = InitRootContent(normalSelection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mSelection->RangeCount()) {
    mFirstSelectedRawRange.SetStartAndEnd(mSelection->GetRangeAt(0));
    return NS_OK;
  }

  // Even if there are no selection ranges, it's usual case if aSelectionType
  // is a special selection.
  if (aSelectionType != SelectionType::eNormal) {
    MOZ_ASSERT(!mFirstSelectedRawRange.IsPositioned());
    return NS_OK;
  }

  // But otherwise, we need to assume that there is a selection range at the
  // beginning of the root content if aSelectionType is eNormal.
  rv = mFirstSelectedRawRange.CollapseTo(RawRangeBoundary(mRootContent, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult
ContentEventHandler::Init(WidgetQueryContentEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent must not be null");
  MOZ_ASSERT(aEvent->mMessage == eQuerySelectedText ||
             aEvent->mInput.mSelectionType == SelectionType::eNormal);

  if (NS_WARN_IF(!aEvent->mInput.IsValidOffset()) ||
      NS_WARN_IF(!aEvent->mInput.IsValidEventMessage(aEvent->mMessage))) {
    return NS_ERROR_FAILURE;
  }

  // Note that we should ignore WidgetQueryContentEvent::Input::mSelectionType
  // if the event isn't eQuerySelectedText.
  SelectionType selectionType =
    aEvent->mMessage == eQuerySelectedText ? aEvent->mInput.mSelectionType :
                                             SelectionType::eNormal;
  if (NS_WARN_IF(selectionType == SelectionType::eNone)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = InitCommon(selectionType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Be aware, WidgetQueryContentEvent::mInput::mOffset should be made absolute
  // offset before sending it to ContentEventHandler because querying selection
  // every time may be expensive.  So, if the caller caches selection, it
  // should initialize the event with the cached value.
  if (aEvent->mInput.mRelativeToInsertionPoint) {
    MOZ_ASSERT(selectionType == SelectionType::eNormal);
    RefPtr<TextComposition> composition =
      IMEStateManager::GetTextCompositionFor(aEvent->mWidget);
    if (composition) {
      uint32_t compositionStart = composition->NativeOffsetOfStartComposition();
      if (NS_WARN_IF(!aEvent->mInput.MakeOffsetAbsolute(compositionStart))) {
        return NS_ERROR_FAILURE;
      }
    } else {
      LineBreakType lineBreakType = GetLineBreakType(aEvent);
      uint32_t selectionStart = 0;
      rv = GetStartOffset(mFirstSelectedRawRange, &selectionStart,
                          lineBreakType);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_ERROR_FAILURE;
      }
      if (NS_WARN_IF(!aEvent->mInput.MakeOffsetAbsolute(selectionStart))) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  aEvent->mSucceeded = false;

  aEvent->mReply.mContentsRoot = mRootContent.get();

  aEvent->mReply.mHasSelection = !mSelection->IsCollapsed();

  nsRect r;
  nsIFrame* frame = nsCaret::GetGeometry(mSelection, &r);
  if (!frame) {
    frame = mRootContent->GetPrimaryFrame();
    if (NS_WARN_IF(!frame)) {
      return NS_ERROR_FAILURE;
    }
  }
  aEvent->mReply.mFocusedWidget = frame->GetNearestWidget();

  return NS_OK;
}

nsresult
ContentEventHandler::Init(WidgetSelectionEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent must not be null");

  nsresult rv = InitCommon();
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = false;

  return NS_OK;
}

nsIContent*
ContentEventHandler::GetFocusedContent()
{
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  return nsFocusManager::GetFocusedDescendant(
                           window,
                           nsFocusManager::eIncludeAllDescendants,
                           getter_AddRefs(focusedWindow));
}

bool
ContentEventHandler::IsPlugin(nsIContent* aContent)
{
  return aContent &&
         aContent->GetDesiredIMEState().mEnabled == IMEState::PLUGIN;
}

nsresult
ContentEventHandler::QueryContentRect(nsIContent* aContent,
                                      WidgetQueryContentEvent* aEvent)
{
  MOZ_ASSERT(aContent, "aContent must not be null");

  nsIFrame* frame = aContent->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // get rect for first frame
  nsRect resultRect(nsPoint(0, 0), frame->GetRect().Size());
  nsresult rv = ConvertToRootRelativeOffset(frame, resultRect);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPresContext* presContext = frame->PresContext();

  // account for any additional frames
  while ((frame = frame->GetNextContinuation())) {
    nsRect frameRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    resultRect.UnionRect(resultRect, frameRect);
  }

  aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
      resultRect.ToOutsidePixels(presContext->AppUnitsPerDevPixel()));
  // Returning empty rect may cause native IME confused, let's make sure to
  // return non-empty rect.
  EnsureNonEmptyRect(aEvent->mReply.mRect);
  aEvent->mSucceeded = true;

  return NS_OK;
}

// Editor places a bogus BR node under its root content if the editor doesn't
// have any text. This happens even for single line editors.
// When we get text content and when we change the selection,
// we don't want to include the bogus BRs at the end.
static bool IsContentBR(nsIContent* aContent)
{
  return aContent->IsHTMLElement(nsGkAtoms::br) &&
         !aContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::type,
                                             nsGkAtoms::moz,
                                             eIgnoreCase) &&
         !aContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::mozeditorbogusnode,
                                             nsGkAtoms::_true,
                                             eIgnoreCase);
}

static bool IsMozBR(nsIContent* aContent)
{
  return aContent->IsHTMLElement(nsGkAtoms::br) && !IsContentBR(aContent);
}

static void ConvertToNativeNewlines(nsString& aString)
{
#if defined(XP_WIN)
  aString.ReplaceSubstring(NS_LITERAL_STRING("\n"), NS_LITERAL_STRING("\r\n"));
#endif
}

static void AppendString(nsAString& aString, nsIContent* aContent)
{
  NS_ASSERTION(aContent->IsText(),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text) {
    return;
  }
  text->AppendTo(aString);
}

static void AppendSubString(nsAString& aString, nsIContent* aContent,
                            uint32_t aXPOffset, uint32_t aXPLength)
{
  NS_ASSERTION(aContent->IsText(),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text) {
    return;
  }
  text->AppendTo(aString, int32_t(aXPOffset), int32_t(aXPLength));
}

#if defined(XP_WIN)
static uint32_t CountNewlinesInXPLength(nsIContent* aContent,
                                        uint32_t aXPLength)
{
  NS_ASSERTION(aContent->IsText(),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text) {
    return 0;
  }
  // For automated tests, we should abort on debug build.
  MOZ_ASSERT(aXPLength == UINT32_MAX || aXPLength <= text->GetLength(),
             "aXPLength is out-of-bounds");
  const uint32_t length = std::min(aXPLength, text->GetLength());
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
  NS_ASSERTION(aContent->IsText(),
               "aContent is not a text node!");
  const nsTextFragment* text = aContent->GetText();
  if (!text) {
    return 0;
  }
  // For automated tests, we should abort on debug build.
  MOZ_ASSERT(
    (aNativeLength == UINT32_MAX || aNativeLength <= text->GetLength() * 2),
    "aNativeLength is unexpected value");
  const uint32_t xpLength = text->GetLength();
  uint32_t newlines = 0;
  for (uint32_t i = 0, nativeOffset = 0;
       i < xpLength && nativeOffset < aNativeLength;
       ++i, ++nativeOffset) {
    // For automated tests, we should abort on debug build.
    MOZ_ASSERT(i < text->GetLength(), "i is out-of-bounds");
    if (text->CharAt(i) == '\n') {
      ++newlines;
      ++nativeOffset;
    }
  }
  return newlines;
}
#endif

/* static */ uint32_t
ContentEventHandler::GetNativeTextLength(nsIContent* aContent,
                                         uint32_t aStartOffset,
                                         uint32_t aEndOffset)
{
  MOZ_ASSERT(aEndOffset >= aStartOffset,
             "aEndOffset must be equals or larger than aStartOffset");
  if (NS_WARN_IF(!aContent->IsText())) {
    return 0;
  }
  if (aStartOffset == aEndOffset) {
    return 0;
  }
  return GetTextLength(aContent, LINE_BREAK_TYPE_NATIVE, aEndOffset) -
           GetTextLength(aContent, LINE_BREAK_TYPE_NATIVE, aStartOffset);
}

/* static */ uint32_t
ContentEventHandler::GetNativeTextLength(nsIContent* aContent,
                                         uint32_t aMaxLength)
{
  if (NS_WARN_IF(!aContent->IsText())) {
    return 0;
  }
  return GetTextLength(aContent, LINE_BREAK_TYPE_NATIVE, aMaxLength);
}

/* static */ uint32_t
ContentEventHandler::GetNativeTextLengthBefore(nsIContent* aContent,
                                               nsINode* aRootNode)
{
  if (NS_WARN_IF(aContent->IsText())) {
    return 0;
  }
  return ShouldBreakLineBefore(aContent, aRootNode) ?
           GetBRLength(LINE_BREAK_TYPE_NATIVE) : 0;
}

/* static inline */ uint32_t
ContentEventHandler::GetBRLength(LineBreakType aLineBreakType)
{
#if defined(XP_WIN)
  // Length of \r\n
  return (aLineBreakType == LINE_BREAK_TYPE_NATIVE) ? 2 : 1;
#else
  return 1;
#endif
}

/* static */ uint32_t
ContentEventHandler::GetTextLength(nsIContent* aContent,
                                   LineBreakType aLineBreakType,
                                   uint32_t aMaxLength)
{
  MOZ_ASSERT(aContent->IsText());

  uint32_t textLengthDifference =
#if defined(XP_WIN)
    // On Windows, the length of a native newline ("\r\n") is twice the length
    // of the XP newline ("\n"), so XP length is equal to the length of the
    // native offset plus the number of newlines encountered in the string.
    (aLineBreakType == LINE_BREAK_TYPE_NATIVE) ?
      CountNewlinesInXPLength(aContent, aMaxLength) : 0;
#else
    // On other platforms, the native and XP newlines are the same.
    0;
#endif

  const nsTextFragment* text = aContent->GetText();
  if (!text) {
    return 0;
  }
  uint32_t length = std::min(text->GetLength(), aMaxLength);
  return length + textLengthDifference;
}

static uint32_t ConvertToXPOffset(nsIContent* aContent, uint32_t aNativeOffset)
{
#if defined(XP_WIN)
  // On Windows, the length of a native newline ("\r\n") is twice the length of
  // the XP newline ("\n"), so XP offset is equal to the length of the native
  // offset minus the number of newlines encountered in the string.
  return aNativeOffset - CountNewlinesInNativeLength(aContent, aNativeOffset);
#else
  // On other platforms, the native and XP newlines are the same.
  return aNativeOffset;
#endif
}

/* static */ bool
ContentEventHandler::ShouldBreakLineBefore(nsIContent* aContent,
                                           nsINode* aRootNode)
{
  // We don't need to append linebreak at the start of the root element.
  if (aContent == aRootNode) {
    return false;
  }

  // If it's not an HTML element (including other markup language's elements),
  // we shouldn't insert like break before that for now.  Becoming this is a
  // problem must be edge case.  E.g., when ContentEventHandler is used with
  // MathML or SVG elements.
  if (!aContent->IsHTMLElement()) {
    return false;
  }

  // If the element is <br>, we need to check if the <br> is caused by web
  // content.  Otherwise, i.e., it's caused by internal reason of Gecko,
  // it shouldn't be exposed as a line break to flatten text.
  if (aContent->IsHTMLElement(nsGkAtoms::br)) {
    return IsContentBR(aContent);
  }

  // Note that ideally, we should refer the style of the primary frame of
  // aContent for deciding if it's an inline.  However, it's difficult
  // IMEContentObserver to notify IME of text change caused by style change.
  // Therefore, currently, we should check only from the tag for now.
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::a,
                                    nsGkAtoms::abbr,
                                    nsGkAtoms::acronym,
                                    nsGkAtoms::b,
                                    nsGkAtoms::bdi,
                                    nsGkAtoms::bdo,
                                    nsGkAtoms::big,
                                    nsGkAtoms::cite,
                                    nsGkAtoms::code,
                                    nsGkAtoms::data,
                                    nsGkAtoms::del,
                                    nsGkAtoms::dfn,
                                    nsGkAtoms::em,
                                    nsGkAtoms::font,
                                    nsGkAtoms::i,
                                    nsGkAtoms::ins,
                                    nsGkAtoms::kbd,
                                    nsGkAtoms::mark,
                                    nsGkAtoms::s,
                                    nsGkAtoms::samp,
                                    nsGkAtoms::small,
                                    nsGkAtoms::span,
                                    nsGkAtoms::strike,
                                    nsGkAtoms::strong,
                                    nsGkAtoms::sub,
                                    nsGkAtoms::sup,
                                    nsGkAtoms::time,
                                    nsGkAtoms::tt,
                                    nsGkAtoms::u,
                                    nsGkAtoms::var)) {
    return false;
  }

  // If the element is unknown element, we shouldn't insert line breaks before
  // it since unknown elements should be ignored.
  RefPtr<HTMLUnknownElement> unknownHTMLElement = do_QueryObject(aContent);
  return !unknownHTMLElement;
}

nsresult
ContentEventHandler::GenerateFlatTextContent(nsIContent* aContent,
                                             nsString& aString,
                                             LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aString.IsEmpty());

  RawRange rawRange;
  nsresult rv = rawRange.SelectNodeContents(aContent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return GenerateFlatTextContent(rawRange, aString, aLineBreakType);
}

nsresult
ContentEventHandler::GenerateFlatTextContent(const RawRange& aRawRange,
                                             nsString& aString,
                                             LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aString.IsEmpty());

  if (aRawRange.Collapsed()) {
    return NS_OK;
  }

  nsINode* startNode = aRawRange.GetStartContainer();
  nsINode* endNode = aRawRange.GetEndContainer();
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }

  if (startNode == endNode && startNode->IsText()) {
    nsIContent* content = startNode->AsContent();
    AppendSubString(aString, content, aRawRange.StartOffset(),
                    aRawRange.EndOffset() - aRawRange.StartOffset());
    ConvertToNativeNewlines(aString);
    return NS_OK;
  }

  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv =
    iter->Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    if (!node->IsContent()) {
      continue;
    }
    nsIContent* content = node->AsContent();

    if (content->IsText()) {
      if (content == startNode) {
        AppendSubString(aString, content, aRawRange.StartOffset(),
                        content->TextLength() - aRawRange.StartOffset());
      } else if (content == endNode) {
        AppendSubString(aString, content, 0, aRawRange.EndOffset());
      } else {
        AppendString(aString, content);
      }
    } else if (ShouldBreakLineBefore(content, mRootContent)) {
      aString.Append(char16_t('\n'));
    }
  }
  if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
    ConvertToNativeNewlines(aString);
  }
  return NS_OK;
}

static FontRange*
AppendFontRange(nsTArray<FontRange>& aFontRanges, uint32_t aBaseOffset)
{
  FontRange* fontRange = aFontRanges.AppendElement();
  fontRange->mStartOffset = aBaseOffset;
  return fontRange;
}

/* static */ uint32_t
ContentEventHandler::GetTextLengthInRange(nsIContent* aContent,
                                          uint32_t aXPStartOffset,
                                          uint32_t aXPEndOffset,
                                          LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aContent->IsText());

  return aLineBreakType == LINE_BREAK_TYPE_NATIVE ?
    GetNativeTextLength(aContent, aXPStartOffset, aXPEndOffset) :
    aXPEndOffset - aXPStartOffset;
}

/* static */ void
ContentEventHandler::AppendFontRanges(FontRangeArray& aFontRanges,
                                      nsIContent* aContent,
                                      uint32_t aBaseOffset,
                                      uint32_t aXPStartOffset,
                                      uint32_t aXPEndOffset,
                                      LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aContent->IsText());

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    // It is a non-rendered content, create an empty range for it.
    AppendFontRange(aFontRanges, aBaseOffset);
    return;
  }

  uint32_t baseOffset = aBaseOffset;
#ifdef DEBUG
  {
    nsTextFrame* text = do_QueryFrame(frame);
    MOZ_ASSERT(text, "Not a text frame");
  }
#endif
  auto* curr = static_cast<nsTextFrame*>(frame);
  while (curr) {
    uint32_t frameXPStart =
      std::max(static_cast<uint32_t>(curr->GetContentOffset()), aXPStartOffset);
    uint32_t frameXPEnd =
      std::min(static_cast<uint32_t>(curr->GetContentEnd()), aXPEndOffset);
    if (frameXPStart >= frameXPEnd) {
      curr = curr->GetNextContinuation();
      continue;
    }

    gfxSkipCharsIterator iter = curr->EnsureTextRun(nsTextFrame::eInflated);
    gfxTextRun* textRun = curr->GetTextRun(nsTextFrame::eInflated);

    nsTextFrame* next = nullptr;
    if (frameXPEnd < aXPEndOffset) {
      next = curr->GetNextContinuation();
      while (next && next->GetTextRun(nsTextFrame::eInflated) == textRun) {
        frameXPEnd =
          std::min(static_cast<uint32_t>(next->GetContentEnd()), aXPEndOffset);
        next = frameXPEnd < aXPEndOffset ?
          next->GetNextContinuation() : nullptr;
      }
    }

    gfxTextRun::Range skipRange(iter.ConvertOriginalToSkipped(frameXPStart),
                                iter.ConvertOriginalToSkipped(frameXPEnd));
    gfxTextRun::GlyphRunIterator runIter(textRun, skipRange);
    uint32_t lastXPEndOffset = frameXPStart;
    while (runIter.NextRun()) {
      gfxFont* font = runIter.GetGlyphRun()->mFont.get();
      uint32_t startXPOffset =
        iter.ConvertSkippedToOriginal(runIter.GetStringStart());
      // It is possible that the first glyph run has exceeded the frame,
      // because the whole frame is filled by skipped chars.
      if (startXPOffset >= frameXPEnd) {
        break;
      }

      if (startXPOffset > lastXPEndOffset) {
        // Create range for skipped leading chars.
        AppendFontRange(aFontRanges, baseOffset);
        baseOffset += GetTextLengthInRange(
          aContent, lastXPEndOffset, startXPOffset, aLineBreakType);
        lastXPEndOffset = startXPOffset;
      }

      FontRange* fontRange = AppendFontRange(aFontRanges, baseOffset);
      fontRange->mFontName = font->GetName();
      fontRange->mFontSize = font->GetAdjustedSize();

      // The converted original offset may exceed the range,
      // hence we need to clamp it.
      uint32_t endXPOffset =
        iter.ConvertSkippedToOriginal(runIter.GetStringEnd());
      endXPOffset = std::min(frameXPEnd, endXPOffset);
      baseOffset += GetTextLengthInRange(aContent, startXPOffset, endXPOffset,
                                         aLineBreakType);
      lastXPEndOffset = endXPOffset;
    }
    if (lastXPEndOffset < frameXPEnd) {
      // Create range for skipped trailing chars. It also handles case
      // that the whole frame contains only skipped chars.
      AppendFontRange(aFontRanges, baseOffset);
      baseOffset += GetTextLengthInRange(
        aContent, lastXPEndOffset, frameXPEnd, aLineBreakType);
    }

    curr = next;
  }
}

nsresult
ContentEventHandler::GenerateFlatFontRanges(const RawRange& aRawRange,
                                            FontRangeArray& aFontRanges,
                                            uint32_t& aLength,
                                            LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aFontRanges.IsEmpty(), "aRanges must be empty array");

  if (aRawRange.Collapsed()) {
    return NS_OK;
  }

  nsINode* startNode = aRawRange.GetStartContainer();
  nsINode* endNode = aRawRange.GetEndContainer();
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }

  // baseOffset is the flattened offset of each content node.
  int32_t baseOffset = 0;
  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv =
    iter->Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    if (!node->IsContent()) {
      continue;
    }
    nsIContent* content = node->AsContent();

    if (content->IsText()) {
      uint32_t startOffset = content != startNode ? 0 : aRawRange.StartOffset();
      uint32_t endOffset = content != endNode ?
        content->TextLength() : aRawRange.EndOffset();
      AppendFontRanges(aFontRanges, content, baseOffset,
                       startOffset, endOffset, aLineBreakType);
      baseOffset += GetTextLengthInRange(content, startOffset, endOffset,
                                         aLineBreakType);
    } else if (ShouldBreakLineBefore(content, mRootContent)) {
      if (aFontRanges.IsEmpty()) {
        MOZ_ASSERT(baseOffset == 0);
        FontRange* fontRange = AppendFontRange(aFontRanges, baseOffset);
        nsIFrame* frame = content->GetPrimaryFrame();
        if (frame) {
          const nsFont& font = frame->GetParent()->StyleFont()->mFont;
          const FontFamilyList& fontList = font.fontlist;
          const FontFamilyName& fontName = fontList.IsEmpty() ?
            FontFamilyName(fontList.GetDefaultFontType()) :
            fontList.GetFontlist()->mNames[0];
          fontName.AppendToString(fontRange->mFontName, false);
          fontRange->mFontSize =
            frame->PresContext()->AppUnitsToDevPixels(font.size);
        }
      }
      baseOffset += GetBRLength(aLineBreakType);
    }
  }

  aLength = baseOffset;
  return NS_OK;
}

nsresult
ContentEventHandler::ExpandToClusterBoundary(nsIContent* aContent,
                                             bool aForward,
                                             uint32_t* aXPOffset)
{
  // XXX This method assumes that the frame boundaries must be cluster
  // boundaries. It's false, but no problem now, maybe.
  if (!aContent->IsText() ||
      *aXPOffset == 0 || *aXPOffset == aContent->TextLength()) {
    return NS_OK;
  }

  NS_ASSERTION(*aXPOffset <= aContent->TextLength(),
               "offset is out of range.");

  MOZ_DIAGNOSTIC_ASSERT(mDocument->GetShell());
  RefPtr<nsFrameSelection> fs = mDocument->GetShell()->FrameSelection();
  int32_t offsetInFrame;
  CaretAssociationHint hint =
    aForward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* frame = fs->GetFrameForNodeOffset(aContent, int32_t(*aXPOffset),
                                              hint, &offsetInFrame);
  if (frame) {
    int32_t startOffset, endOffset;
    nsresult rv = frame->GetOffsets(startOffset, endOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aXPOffset == static_cast<uint32_t>(startOffset) ||
        *aXPOffset == static_cast<uint32_t>(endOffset)) {
      return NS_OK;
    }
    if (!frame->IsTextFrame()) {
      return NS_ERROR_FAILURE;
    }
    nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
    int32_t newOffsetInFrame = *aXPOffset - startOffset;
    newOffsetInFrame += aForward ? -1 : 1;
    // PeekOffsetCharacter() should respect cluster but ignore user-select
    // style.  If it returns "FOUND", we should use the result.  Otherwise,
    // we shouldn't use the result because the offset was moved to reversed
    // direction.
    nsTextFrame::PeekOffsetCharacterOptions options;
    options.mRespectClusters = true;
    options.mIgnoreUserStyleAll = true;
    if (textFrame->PeekOffsetCharacter(aForward, &newOffsetInFrame,
                                       options) == nsIFrame::FOUND) {
      *aXPOffset = startOffset + newOffsetInFrame;
      return NS_OK;
    }
  }

  // If the frame isn't available, we only can check surrogate pair...
  const nsTextFragment* text = aContent->GetText();
  NS_ENSURE_TRUE(text, NS_ERROR_FAILURE);
  if (NS_IS_LOW_SURROGATE(text->CharAt(*aXPOffset)) &&
      NS_IS_HIGH_SURROGATE(text->CharAt(*aXPOffset - 1))) {
    *aXPOffset += aForward ? 1 : -1;
  }
  return NS_OK;
}

nsresult
ContentEventHandler::SetRawRangeFromFlatTextOffset(
                       RawRange* aRawRange,
                       uint32_t aOffset,
                       uint32_t aLength,
                       LineBreakType aLineBreakType,
                       bool aExpandToClusterBoundaries,
                       uint32_t* aNewOffset,
                       nsIContent** aLastTextNode)
{
  if (aNewOffset) {
    *aNewOffset = aOffset;
  }
  if (aLastTextNode) {
    *aLastTextNode = nullptr;
  }

  // Special case like <br contenteditable>
  if (!mRootContent->HasChildren()) {
    nsresult rv = aRawRange->CollapseTo(RawRangeBoundary(mRootContent, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv = iter->Init(mRootContent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t offset = 0;
  uint32_t endOffset = aOffset + aLength;
  bool startSet = false;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    // FYI: mRootContent shouldn't cause any text. So, we can skip it simply.
    if (node == mRootContent || !node->IsContent()) {
      continue;
    }
    nsIContent* content = node->AsContent();

    if (aLastTextNode && content->IsText()) {
      NS_IF_RELEASE(*aLastTextNode);
      NS_ADDREF(*aLastTextNode = content);
    }

    uint32_t textLength =
      content->IsText() ?
        GetTextLength(content, aLineBreakType) :
        (ShouldBreakLineBefore(content, mRootContent) ?
           GetBRLength(aLineBreakType) : 0);
    if (!textLength) {
      continue;
    }

    // When the start offset is in between accumulated offset and the last
    // offset of the node, the node is the start node of the range.
    if (!startSet && aOffset <= offset + textLength) {
      nsINode* startNode = nullptr;
      int32_t startNodeOffset = -1;
      if (content->IsText()) {
        // Rule #1.1: [textNode or text[Node or textNode[
        uint32_t xpOffset = aOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          xpOffset = ConvertToXPOffset(content, xpOffset);
        }

        if (aExpandToClusterBoundaries) {
          uint32_t oldXPOffset = xpOffset;
          rv = ExpandToClusterBoundary(content, false, &xpOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          if (aNewOffset) {
            // This is correct since a cluster shouldn't include line break.
            *aNewOffset -= (oldXPOffset - xpOffset);
          }
        }
        startNode = content;
        startNodeOffset = static_cast<int32_t>(xpOffset);
      } else if (aOffset < offset + textLength) {
        // Rule #1.2 [<element>
        startNode = content->GetParent();
        if (NS_WARN_IF(!startNode)) {
          return NS_ERROR_FAILURE;
        }
        startNodeOffset = startNode->ComputeIndexOf(content);
        if (NS_WARN_IF(startNodeOffset == -1)) {
          // The content is being removed from the parent!
          return NS_ERROR_FAILURE;
        }
      } else if (!content->HasChildren()) {
        // Rule #1.3: <element/>[
        startNode = content->GetParent();
        if (NS_WARN_IF(!startNode)) {
          return NS_ERROR_FAILURE;
        }
        startNodeOffset = startNode->ComputeIndexOf(content) + 1;
        if (NS_WARN_IF(startNodeOffset == 0)) {
          // The content is being removed from the parent!
          return NS_ERROR_FAILURE;
        }
      } else {
        // Rule #1.4: <element>[
        startNode = content;
        startNodeOffset = 0;
      }
      NS_ASSERTION(startNode, "startNode must not be nullptr");
      NS_ASSERTION(startNodeOffset >= 0,
                   "startNodeOffset must not be negative");
      rv = aRawRange->SetStart(startNode,
                               static_cast<uint32_t>(startNodeOffset));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      startSet = true;

      if (!aLength) {
        rv = aRawRange->SetEnd(startNode,
                               static_cast<uint32_t>(startNodeOffset));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
    }

    // When the end offset is in the content, the node is the end node of the
    // range.
    if (endOffset <= offset + textLength) {
      MOZ_ASSERT(startSet,
        "The start of the range should've been set already");
      if (content->IsText()) {
        // Rule #2.1: ]textNode or text]Node or textNode]
        uint32_t xpOffset = endOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          uint32_t xpOffsetCurrent = ConvertToXPOffset(content, xpOffset);
          if (xpOffset && GetBRLength(aLineBreakType) > 1) {
            MOZ_ASSERT(GetBRLength(aLineBreakType) == 2);
            uint32_t xpOffsetPre = ConvertToXPOffset(content, xpOffset - 1);
            // If previous character's XP offset is same as current character's,
            // it means that the end offset is between \r and \n.  So, the
            // range end should be after the \n.
            if (xpOffsetPre == xpOffsetCurrent) {
              xpOffset = xpOffsetCurrent + 1;
            } else {
              xpOffset = xpOffsetCurrent;
            }
          }
        }
        if (aExpandToClusterBoundaries) {
          rv = ExpandToClusterBoundary(content, true, &xpOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        NS_ASSERTION(xpOffset <= INT32_MAX,
          "The end node offset is too large");
        rv = aRawRange->SetEnd(content, xpOffset);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }

      if (endOffset == offset) {
        // Rule #2.2: ]<element>
        // NOTE: Please don't crash on release builds because it must be
        //       overreaction but we shouldn't allow this bug when some
        //       automated tests find this.
        MOZ_ASSERT(false, "This case should've already been handled at "
                          "the last node which caused some text");
        return NS_ERROR_FAILURE;
      }

      if (content->HasChildren() &&
          ShouldBreakLineBefore(content, mRootContent)) {
        // Rule #2.3: </element>]
        rv = aRawRange->SetEnd(content, 0);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }

      // Rule #2.4: <element/>]
      nsINode* endNode = content->GetParent();
      if (NS_WARN_IF(!endNode)) {
        return NS_ERROR_FAILURE;
      }
      int32_t indexInParent = endNode->ComputeIndexOf(content);
      if (NS_WARN_IF(indexInParent == -1)) {
        // The content is being removed from the parent!
        return NS_ERROR_FAILURE;
      }
      rv = aRawRange->SetEnd(endNode, indexInParent + 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    offset += textLength;
  }

  if (!startSet) {
    MOZ_ASSERT(!mRootContent->IsText());
    if (!offset) {
      // Rule #1.5: <root>[</root>
      // When there are no nodes causing text, the start of the DOM range
      // should be start of the root node since clicking on such editor (e.g.,
      // <div contenteditable><span></span></div>) sets caret to the start of
      // the editor (i.e., before <span> in the example).
      rv = aRawRange->SetStart(mRootContent, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (!aLength) {
        rv = aRawRange->SetEnd(mRootContent, 0);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
    } else {
      // Rule #1.5: [</root>
      rv = aRawRange->SetStart(mRootContent, mRootContent->GetChildCount());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    if (aNewOffset) {
      *aNewOffset = offset;
    }
  }
  // Rule #2.5: ]</root>
  rv = aRawRange->SetEnd(mRootContent, mRootContent->GetChildCount());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/* static */ LineBreakType
ContentEventHandler::GetLineBreakType(WidgetQueryContentEvent* aEvent)
{
  return GetLineBreakType(aEvent->mUseNativeLineBreak);
}

/* static */ LineBreakType
ContentEventHandler::GetLineBreakType(WidgetSelectionEvent* aEvent)
{
  return GetLineBreakType(aEvent->mUseNativeLineBreak);
}

/* static */ LineBreakType
ContentEventHandler::GetLineBreakType(bool aUseNativeLineBreak)
{
  return aUseNativeLineBreak ?
    LINE_BREAK_TYPE_NATIVE : LINE_BREAK_TYPE_XP;
}

nsresult
ContentEventHandler::HandleQueryContentEvent(WidgetQueryContentEvent* aEvent)
{
  switch (aEvent->mMessage) {
    case eQuerySelectedText:
      return OnQuerySelectedText(aEvent);
    case eQueryTextContent:
      return OnQueryTextContent(aEvent);
    case eQueryCaretRect:
      return OnQueryCaretRect(aEvent);
    case eQueryTextRect:
      return OnQueryTextRect(aEvent);
    case eQueryTextRectArray:
      return OnQueryTextRectArray(aEvent);
    case eQueryEditorRect:
      return OnQueryEditorRect(aEvent);
    case eQueryContentState:
      return OnQueryContentState(aEvent);
    case eQuerySelectionAsTransferable:
      return OnQuerySelectionAsTransferable(aEvent);
    case eQueryCharacterAtPoint:
      return OnQueryCharacterAtPoint(aEvent);
    case eQueryDOMWidgetHittest:
      return OnQueryDOMWidgetHittest(aEvent);
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_OK;
}

// Similar to nsFrameSelection::GetFrameForNodeOffset,
// but this is more flexible for OnQueryTextRect to use
static nsresult GetFrameForTextRect(nsINode* aNode,
                                    int32_t aNodeOffset,
                                    bool aHint,
                                    nsIFrame** aReturnFrame)
{
  NS_ENSURE_TRUE(aNode && aNode->IsContent(), NS_ERROR_UNEXPECTED);
  nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  int32_t childNodeOffset = 0;
  return frame->GetChildFrameContainingOffset(aNodeOffset, aHint,
                                              &childNodeOffset, aReturnFrame);
}

nsresult
ContentEventHandler::OnQuerySelectedText(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mFirstSelectedRawRange.IsPositioned()) {
    MOZ_ASSERT(aEvent->mInput.mSelectionType != SelectionType::eNormal);
    MOZ_ASSERT(aEvent->mReply.mOffset == WidgetQueryContentEvent::NOT_FOUND);
    MOZ_ASSERT(aEvent->mReply.mString.IsEmpty());
    MOZ_ASSERT(!aEvent->mReply.mHasSelection);
    aEvent->mSucceeded = true;
    return NS_OK;
  }

  nsINode* const startNode = mFirstSelectedRawRange.GetStartContainer();
  nsINode* const endNode = mFirstSelectedRawRange.GetEndContainer();

  // Make sure the selection is within the root content range.
  if (!nsContentUtils::ContentIsDescendantOf(startNode, mRootContent) ||
      !nsContentUtils::ContentIsDescendantOf(endNode, mRootContent)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(aEvent->mReply.mString.IsEmpty(),
               "The reply string must be empty");

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  rv = GetStartOffset(mFirstSelectedRawRange,
                      &aEvent->mReply.mOffset, lineBreakType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> anchorNode, focusNode;
  int32_t anchorOffset = 0, focusOffset = 0;
  if (mSelection->RangeCount()) {
    // If there is only one selection range, the anchor/focus node and offset
    // are the information of the range.  Therefore, we have the direction
    // information.
    if (mSelection->RangeCount() == 1) {
      anchorNode = mSelection->GetAnchorNode();
      focusNode = mSelection->GetFocusNode();
      if (NS_WARN_IF(!anchorNode) || NS_WARN_IF(!focusNode)) {
        return NS_ERROR_FAILURE;
      }
      anchorOffset = static_cast<int32_t>(mSelection->AnchorOffset());
      focusOffset = static_cast<int32_t>(mSelection->FocusOffset());
      if (NS_WARN_IF(anchorOffset < 0) || NS_WARN_IF(focusOffset < 0)) {
        return NS_ERROR_FAILURE;
      }

      int16_t compare = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                      focusNode, focusOffset);
      aEvent->mReply.mReversed = compare > 0;
    }
    // However, if there are 2 or more selection ranges, we have no information
    // of that.
    else {
      aEvent->mReply.mReversed = false;
    }

    if (!mFirstSelectedRawRange.Collapsed()) {
      rv = GenerateFlatTextContent(mFirstSelectedRawRange,
                                   aEvent->mReply.mString, lineBreakType);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      aEvent->mReply.mString.Truncate();
    }
  } else {
    NS_ASSERTION(mFirstSelectedRawRange.Collapsed(),
      "When mSelection doesn't have selection, mFirstSelectedRawRange must be "
      "collapsed");
    anchorNode = focusNode = mFirstSelectedRawRange.GetStartContainer();
    if (NS_WARN_IF(!anchorNode)) {
      return NS_ERROR_FAILURE;
    }
    anchorOffset = focusOffset =
      static_cast<int32_t>(mFirstSelectedRawRange.StartOffset());
    if (NS_WARN_IF(anchorOffset < 0)) {
      return NS_ERROR_FAILURE;
    }

    aEvent->mReply.mReversed = false;
    aEvent->mReply.mString.Truncate();
  }


  nsIFrame* frame = nullptr;
  rv = GetFrameForTextRect(focusNode, focusOffset, true, &frame);
  if (NS_SUCCEEDED(rv) && frame) {
    aEvent->mReply.mWritingMode = frame->GetWritingMode();
  } else {
    aEvent->mReply.mWritingMode = WritingMode();
  }

  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryTextContent(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(aEvent->mReply.mString.IsEmpty(),
               "The reply string must be empty");

  LineBreakType lineBreakType = GetLineBreakType(aEvent);

  RawRange rawRange;
  rv = SetRawRangeFromFlatTextOffset(&rawRange, aEvent->mInput.mOffset,
                                     aEvent->mInput.mLength, lineBreakType,
                                     false, &aEvent->mReply.mOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GenerateFlatTextContent(rawRange, aEvent->mReply.mString, lineBreakType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aEvent->mWithFontRanges) {
    uint32_t fontRangeLength;
    rv = GenerateFlatFontRanges(rawRange, aEvent->mReply.mFontRanges,
                                fontRangeLength, lineBreakType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(fontRangeLength == aEvent->mReply.mString.Length(),
               "Font ranges doesn't match the string");
  }

  aEvent->mSucceeded = true;

  return NS_OK;
}

void
ContentEventHandler::EnsureNonEmptyRect(nsRect& aRect) const
{
  // See the comment in ContentEventHandler.h why this doesn't set them to
  // one device pixel.
  aRect.height = std::max(1, aRect.height);
  aRect.width = std::max(1, aRect.width);
}

void
ContentEventHandler::EnsureNonEmptyRect(LayoutDeviceIntRect& aRect) const
{
  aRect.height = std::max(1, aRect.height);
  aRect.width = std::max(1, aRect.width);
}

ContentEventHandler::NodePosition
ContentEventHandler::GetNodePositionHavingFlatText(
                       const NodePosition& aNodePosition)
{
  return GetNodePositionHavingFlatText(aNodePosition.Container(),
                                       aNodePosition.Offset());
}

ContentEventHandler::NodePosition
ContentEventHandler::GetNodePositionHavingFlatText(nsINode* aNode,
                                                   int32_t aNodeOffset)
{
  if (aNode->IsText()) {
    return NodePosition(aNode, aNodeOffset);
  }

  int32_t childCount = static_cast<int32_t>(aNode->GetChildCount());

  // If it's a empty element node, returns itself.
  if (!childCount) {
    MOZ_ASSERT(!aNodeOffset || aNodeOffset == 1);
    return NodePosition(aNode, aNodeOffset);
  }

  // If there is a node at given position, return the start of it.
  if (aNodeOffset < childCount) {
    return NodePosition(aNode->GetChildAt_Deprecated(aNodeOffset), 0);
  }

  // If the offset represents "after" the node, we need to return the last
  // child of it.  For example, if a range is |<p>[<br>]</p>|, then, the
  // end point is {<p>, 1}.  In such case, callers need the <br> node.
  if (aNodeOffset == childCount) {
    nsINode* node = aNode->GetChildAt_Deprecated(childCount - 1);
    return NodePosition(node,
      node->IsText()
        ? static_cast<int32_t>(node->AsContent()->TextLength())
        : 1);
  }

  NS_WARNING("aNodeOffset is invalid value");
  return NodePosition();
}

ContentEventHandler::FrameAndNodeOffset
ContentEventHandler::GetFirstFrameInRangeForTextRect(const RawRange& aRawRange)
{
  NodePosition nodePosition;
  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv =
    iter->Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FrameAndNodeOffset();
  }
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }

    if (!node->IsContent()) {
      continue;
    }

    if (node->IsText()) {
      // If the range starts at the end of a text node, we need to find
      // next node which causes text.
      int32_t offsetInNode =
        node == aRawRange.GetStartContainer() ? aRawRange.StartOffset() : 0;
      if (static_cast<uint32_t>(offsetInNode) < node->Length()) {
        nodePosition.Set(node, offsetInNode);
        break;
      }
      continue;
    }

    // If the element node causes a line break before it, it's the first
    // node causing text.
    if (ShouldBreakLineBefore(node->AsContent(), mRootContent) ||
        IsMozBR(node->AsContent())) {
      nodePosition.Set(node, 0);
    }
  }

  if (!nodePosition.IsSet()) {
    return FrameAndNodeOffset();
  }

  nsIFrame* firstFrame = nullptr;
  GetFrameForTextRect(nodePosition.Container(), nodePosition.Offset(),
                      true, &firstFrame);
  return FrameAndNodeOffset(firstFrame, nodePosition.Offset());
}

ContentEventHandler::FrameAndNodeOffset
ContentEventHandler::GetLastFrameInRangeForTextRect(const RawRange& aRawRange)
{
  NodePosition nodePosition;
  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv =
    iter->Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FrameAndNodeOffset();
  }

  nsINode* endNode = aRawRange.GetEndContainer();
  uint32_t endOffset = aRawRange.EndOffset();
  // If the end point is start of a text node or specified by its parent and
  // index, the node shouldn't be included into the range.  For example,
  // with this case, |<p>abc[<br>]def</p>|, the range ends at 3rd children of
  // <p> (see the range creation rules, "2.4. Cases: <element/>]"). This causes
  // following frames:
  // +----+-----+
  // | abc|[<br>|
  // +----+-----+
  // +----+
  // |]def|
  // +----+
  // So, if this method includes the 2nd text frame's rect to its result, the
  // caller will return too tall rect which includes 2 lines in this case isn't
  // expected by native IME  (e.g., popup of IME will be positioned at bottom
  // of "d" instead of right-bottom of "c").  Therefore, this method shouldn't
  // include the last frame when its content isn't really in aRawRange.
  nsINode* nextNodeOfRangeEnd = nullptr;
  if (endNode->IsText()) {
    // Don't set nextNodeOfRangeEnd to the start node of aRawRange because if
    // endNode is same as start node of the range, the text node shouldn't be
    // next of range end even if the offset is 0.  This could occur with empty
    // text node.
    if (!endOffset && aRawRange.GetStartContainer() != endNode) {
      nextNodeOfRangeEnd = endNode;
    }
  } else if (endOffset < endNode->GetChildCount()) {
    nextNodeOfRangeEnd = endNode->GetChildAt_Deprecated(endOffset);
  }

  for (iter->Last(); !iter->IsDone(); iter->Prev()) {
    nsINode* node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }

    if (!node->IsContent() || node == nextNodeOfRangeEnd) {
      continue;
    }

    if (node->IsText()) {
      uint32_t offset;
      if (node == aRawRange.GetEndContainer()) {
        offset = aRawRange.EndOffset();
      } else {
        offset = node->Length();
      }
      nodePosition.Set(node, offset);

      // If the text node is empty or the last node of the range but the index
      // is 0, we should store current position but continue looking for
      // previous node (If there are no nodes before it, we should use current
      // node position for returning its frame).
      if (!nodePosition.Offset()) {
        continue;
      }
      break;
    }

    if (ShouldBreakLineBefore(node->AsContent(), mRootContent) ||
        IsMozBR(node->AsContent())) {
      nodePosition.Set(node, 0);
      break;
    }
  }

  if (!nodePosition.IsSet()) {
    return FrameAndNodeOffset();
  }

  nsIFrame* lastFrame = nullptr;
  GetFrameForTextRect(nodePosition.Container(),
                      nodePosition.Offset(),
                      true, &lastFrame);
  if (!lastFrame) {
    return FrameAndNodeOffset();
  }

  // If the last frame is a text frame, we need to check if the range actually
  // includes at least one character in the range.  Therefore, if it's not a
  // text frame, we need to do nothing anymore.
  if (!lastFrame->IsTextFrame()) {
    return FrameAndNodeOffset(lastFrame, nodePosition.Offset());
  }

  int32_t start, end;
  if (NS_WARN_IF(NS_FAILED(lastFrame->GetOffsets(start, end)))) {
    return FrameAndNodeOffset();
  }

  // If the start offset in the node is same as the computed offset in the
  // node and it's not 0, the frame shouldn't be added to the text rect.  So,
  // this should return previous text frame and its last offset if there is
  // at least one text frame.
  if (nodePosition.Offset() && nodePosition.Offset() == static_cast<uint32_t>(start)) {
    nodePosition.Set(nodePosition.Container(), nodePosition.Offset() - 1);
    GetFrameForTextRect(nodePosition.Container(), nodePosition.Offset(), true,
                        &lastFrame);
    if (NS_WARN_IF(!lastFrame)) {
      return FrameAndNodeOffset();
    }
  }

  return FrameAndNodeOffset(lastFrame, nodePosition.Offset());
}

ContentEventHandler::FrameRelativeRect
ContentEventHandler::GetLineBreakerRectBefore(nsIFrame* aFrame)
{
  // Note that this method should be called only with an element's frame whose
  // open tag causes a line break or moz-<br> for computing empty last line's
  // rect.
  MOZ_ASSERT(ShouldBreakLineBefore(aFrame->GetContent(), mRootContent) ||
             IsMozBR(aFrame->GetContent()));

  nsIFrame* frameForFontMetrics = aFrame;

  // If it's not a <br> frame, this method computes the line breaker's rect
  // outside the frame.  Therefore, we need to compute with parent frame's
  // font metrics in such case.
  if (!aFrame->IsBrFrame() && aFrame->GetParent()) {
    frameForFontMetrics = aFrame->GetParent();
  }

  // Note that <br> element's rect is decided with line-height but we need
  // a rect only with font height.  Additionally, <br> frame's width and
  // height are 0 in quirks mode if it's not an empty line.  So, we cannot
  // use frame rect information even if it's a <br> frame.

  FrameRelativeRect result(aFrame);

  RefPtr<nsFontMetrics> fontMetrics =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(frameForFontMetrics);
  if (NS_WARN_IF(!fontMetrics)) {
    return FrameRelativeRect();
  }

  const WritingMode kWritingMode = frameForFontMetrics->GetWritingMode();
  nscoord baseline = aFrame->GetCaretBaseline();
  if (kWritingMode.IsVertical()) {
    if (kWritingMode.IsLineInverted()) {
      result.mRect.x = baseline - fontMetrics->MaxDescent();
    } else {
      result.mRect.x = baseline - fontMetrics->MaxAscent();
    }
    result.mRect.width = fontMetrics->MaxHeight();
  } else {
    result.mRect.y = baseline - fontMetrics->MaxAscent();
    result.mRect.height = fontMetrics->MaxHeight();
  }

  // If aFrame isn't a <br> frame, caret should be at outside of it because
  // the line break is before its open tag.  For example, case of
  // |<div><p>some text</p></div>|, caret is before <p> element and in <div>
  // element, the caret should be left of top-left corner of <p> element like:
  //
  // +-<div>-------------------  <div>'s border box
  // | I +-<p>-----------------  <p>'s border box
  // | I |
  // | I |
  // |   |
  //   ^- caret
  //
  // However, this is a hack for unusual scenario.  This hack shouldn't be
  // used as far as possible.
  if (!aFrame->IsBrFrame()) {
    if (kWritingMode.IsVertical()) {
      if (kWritingMode.IsLineInverted()) {
        // above of top-left corner of aFrame.
        result.mRect.x = 0;
      } else {
        // above of top-right corner of aFrame.
        result.mRect.x = aFrame->GetRect().XMost() - result.mRect.width;
      }
      result.mRect.y = -aFrame->PresContext()->AppUnitsPerDevPixel();
    } else {
      // left of top-left corner of aFrame.
      result.mRect.x = -aFrame->PresContext()->AppUnitsPerDevPixel();
      result.mRect.y = 0;
    }
  }
  return result;
}

ContentEventHandler::FrameRelativeRect
ContentEventHandler::GuessLineBreakerRectAfter(nsIContent* aTextContent)
{
  // aTextContent should be a text node.
  MOZ_ASSERT(aTextContent->IsText());

  FrameRelativeRect result;
  int32_t length = static_cast<int32_t>(aTextContent->Length());
  if (NS_WARN_IF(length < 0)) {
    return result;
  }
  // Get the last nsTextFrame which is caused by aTextContent.  Note that
  // a text node can cause multiple text frames, e.g., the text is too long
  // and wrapped by its parent block or the text has line breakers and its
  // white-space property respects the line breakers (e.g., |pre|).
  nsIFrame* lastTextFrame = nullptr;
  nsresult rv = GetFrameForTextRect(aTextContent, length, true, &lastTextFrame);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!lastTextFrame)) {
    return result;
  }
  const nsRect kLastTextFrameRect = lastTextFrame->GetRect();
  if (lastTextFrame->GetWritingMode().IsVertical()) {
    // Below of the last text frame.
    result.mRect.SetRect(0, kLastTextFrameRect.height,
                         kLastTextFrameRect.width, 0);
  } else {
    // Right of the last text frame (not bidi-aware).
    result.mRect.SetRect(kLastTextFrameRect.width, 0,
                         0, kLastTextFrameRect.height);
  }
  result.mBaseFrame = lastTextFrame;
  return result;
}

ContentEventHandler::FrameRelativeRect
ContentEventHandler::GuessFirstCaretRectIn(nsIFrame* aFrame)
{
  const WritingMode kWritingMode = aFrame->GetWritingMode();
  nsPresContext* presContext = aFrame->PresContext();

  // Computes the font height, but if it's not available, we should use
  // default font size of Firefox.  The default font size in default settings
  // is 16px.
  RefPtr<nsFontMetrics> fontMetrics =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);
  const nscoord kMaxHeight =
    fontMetrics ? fontMetrics->MaxHeight() :
                  16 * presContext->AppUnitsPerDevPixel();

  nsRect caretRect;
  const nsRect kContentRect = aFrame->GetContentRect() - aFrame->GetPosition();
  caretRect.y = kContentRect.y;
  if (!kWritingMode.IsVertical()) {
    if (kWritingMode.IsBidiLTR()) {
      caretRect.x = kContentRect.x;
    } else {
      // Move 1px left for the space of caret itself.
      const nscoord kOnePixel = presContext->AppUnitsPerDevPixel();
      caretRect.x = kContentRect.XMost() - kOnePixel;
    }
    caretRect.height = kMaxHeight;
    // However, don't add kOnePixel here because it may cause 2px width at
    // aligning the edge to device pixels.
    caretRect.width = 1;
  } else {
    if (kWritingMode.IsVerticalLR()) {
      caretRect.x = kContentRect.x;
    } else {
      caretRect.x = kContentRect.XMost() - kMaxHeight;
    }
    caretRect.width = kMaxHeight;
    // Don't add app units for a device pixel because it may cause 2px height
    // at aligning the edge to device pixels.
    caretRect.height = 1;
  }
  return FrameRelativeRect(caretRect, aFrame);
}

nsresult
ContentEventHandler::OnQueryTextRectArray(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  const uint32_t kBRLength = GetBRLength(lineBreakType);

  bool isVertical = false;
  LayoutDeviceIntRect rect;
  uint32_t offset = aEvent->mInput.mOffset;
  const uint32_t kEndOffset = offset + aEvent->mInput.mLength;
  bool wasLineBreaker = false;
  // lastCharRect stores the last charRect value (see below for the detail of
  // charRect).
  nsRect lastCharRect;
  // lastFrame is base frame of lastCharRect.
  nsIFrame* lastFrame = nullptr;
  while (offset < kEndOffset) {
    nsCOMPtr<nsIContent> lastTextContent;
    RawRange rawRange;
    rv = SetRawRangeFromFlatTextOffset(&rawRange, offset, 1, lineBreakType,
                                       true, nullptr,
                                       getter_AddRefs(lastTextContent));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // If the range is collapsed, offset has already reached the end of the
    // contents.
    if (rawRange.Collapsed()) {
      break;
    }

    // Get the first frame which causes some text after the offset.
    FrameAndNodeOffset firstFrame = GetFirstFrameInRangeForTextRect(rawRange);

    // If GetFirstFrameInRangeForTextRect() does not return valid frame, that
    // means that there are no visible frames having text or the offset reached
    // the end of contents.
    if (!firstFrame.IsValid()) {
      nsAutoString allText;
      rv = GenerateFlatTextContent(mRootContent, allText, lineBreakType);
      // If the offset doesn't reach the end of contents yet but there is no
      // frames for the node, that means that current offset's node is hidden
      // by CSS or something.  Ideally, we should handle it with the last
      // visible text node's last character's rect, but it's not usual cases
      // in actual web services.  Therefore, currently, we should make this
      // case fail.
      if (NS_WARN_IF(NS_FAILED(rv)) || offset < allText.Length()) {
        return NS_ERROR_FAILURE;
      }
      // Otherwise, we should append caret rect at the end of the contents
      // later.
      break;
    }

    nsIContent* firstContent = firstFrame.mFrame->GetContent();
    if (NS_WARN_IF(!firstContent)) {
      return NS_ERROR_FAILURE;
    }

    bool startsBetweenLineBreaker = false;
    nsAutoString chars;
    // XXX not bidi-aware this class...
    isVertical = firstFrame->GetWritingMode().IsVertical();

    nsIFrame* baseFrame = firstFrame;
    // charRect should have each character rect or line breaker rect relative
    // to the base frame.
    AutoTArray<nsRect, 16> charRects;

    // If the first frame is a text frame, the result should be computed with
    // the frame's API.
    if (firstFrame->IsTextFrame()) {
      rv = firstFrame->GetCharacterRectsInRange(firstFrame.mOffsetInNode,
                                                kEndOffset - offset, charRects);
      if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(charRects.IsEmpty())) {
        return rv;
      }
      // Assign the characters whose rects are computed by the call of
      // nsTextFrame::GetCharacterRectsInRange().
      AppendSubString(chars, firstContent, firstFrame.mOffsetInNode,
                      charRects.Length());
      if (NS_WARN_IF(chars.Length() != charRects.Length())) {
        return NS_ERROR_UNEXPECTED;
      }
      if (kBRLength > 1 && chars[0] == '\n' &&
          offset == aEvent->mInput.mOffset && offset) {
        // If start of range starting from previous offset of query range is
        // same as the start of query range, the query range starts from
        // between a line breaker (i.e., the range starts between "\r" and
        // "\n").
        RawRange rawRangeToPrevOffset;
        rv = SetRawRangeFromFlatTextOffset(&rawRangeToPrevOffset,
                                           aEvent->mInput.mOffset - 1, 1,
                                           lineBreakType, true, nullptr);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        startsBetweenLineBreaker =
          rawRange.GetStartContainer() ==
            rawRangeToPrevOffset.GetStartContainer() &&
          rawRange.StartOffset() == rawRangeToPrevOffset.StartOffset();
      }
    }
    // Other contents should cause a line breaker rect before it.
    // Note that moz-<br> element does not cause any text, however,
    // it represents empty line at the last of current block.  Therefore,
    // we need to compute its rect too.
    else if (ShouldBreakLineBefore(firstContent, mRootContent) ||
             IsMozBR(firstContent)) {
      nsRect brRect;
      // If the frame is not a <br> frame, we need to compute the caret rect
      // with last character's rect before firstContent if there is.
      // For example, if caret is after "c" of |<p>abc</p><p>def</p>|, IME may
      // query a line breaker's rect after "c".  Then, if we compute it only
      // with the 2nd <p>'s block frame, the result will be:
      //  +-<p>--------------------------------+
      //  |abc                                 |
      //  +------------------------------------+
      //
      // I+-<p>--------------------------------+
      //  |def                                 |
      //  +------------------------------------+
      // However, users expect popup windows of IME should be positioned at
      // right-bottom of "c" like this:
      //  +-<p>--------------------------------+
      //  |abcI                                |
      //  +------------------------------------+
      //
      //  +-<p>--------------------------------+
      //  |def                                 |
      //  +------------------------------------+
      // Therefore, if the first frame isn't a <br> frame and there is a text
      // node before the first node in the queried range, we should compute the
      // first rect with the previous character's rect.
      // If we already compute a character's rect in the queried range, we can
      // compute it with the cached last character's rect.  (However, don't
      // use this path if it's a <br> frame because trusting <br> frame's rect
      // is better than guessing the rect from the previous character.)
      if (!firstFrame->IsBrFrame() && aEvent->mInput.mOffset != offset) {
        baseFrame = lastFrame;
        brRect = lastCharRect;
        if (!wasLineBreaker) {
          if (isVertical) {
            // Right of the last character.
            brRect.y = brRect.YMost() + 1;
            brRect.height = 1;
          } else {
            // Under the last character.
            brRect.x = brRect.XMost() + 1;
            brRect.width = 1;
          }
        }
      }
      // If it's not a <br> frame and it's the first character rect at the
      // queried range, we need to the previous character of the start of
      // the queried range if there is a text node.
      else if (!firstFrame->IsBrFrame() && lastTextContent) {
        FrameRelativeRect brRectRelativeToLastTextFrame =
          GuessLineBreakerRectAfter(lastTextContent);
        if (NS_WARN_IF(!brRectRelativeToLastTextFrame.IsValid())) {
          return NS_ERROR_FAILURE;
        }
        // Look for the last text frame for lastTextContent.
        nsIFrame* primaryFrame = lastTextContent->GetPrimaryFrame();
        if (NS_WARN_IF(!primaryFrame)) {
          return NS_ERROR_FAILURE;
        }
        baseFrame = primaryFrame->LastContinuation();
        if (NS_WARN_IF(!baseFrame)) {
          return NS_ERROR_FAILURE;
        }
        brRect = brRectRelativeToLastTextFrame.RectRelativeTo(baseFrame);
      }
      // Otherwise, we need to compute the line breaker's rect only with the
      // first frame's rect.  But this may be unexpected.  For example,
      // |<div contenteditable>[<p>]abc</p></div>|.  In this case, caret is
      // before "a", therefore, users expect the rect left of "a".  However,
      // we don't have enough information about the next character here and
      // this isn't usual case (e.g., IME typically tries to query the rect
      // of "a" or caret rect for computing its popup position).  Therefore,
      // we shouldn't do more complicated hack here unless we'll get some bug
      // reports actually.
      else {
        FrameRelativeRect relativeBRRect = GetLineBreakerRectBefore(firstFrame);
        brRect = relativeBRRect.RectRelativeTo(firstFrame);
      }
      charRects.AppendElement(brRect);
      chars.AssignLiteral("\n");
      if (kBRLength > 1 && offset == aEvent->mInput.mOffset && offset) {
        // If the first frame for the previous offset of the query range and
        // the first frame for the start of query range are same, that means
        // the start offset is between the first line breaker (i.e., the range
        // starts between "\r" and "\n").
        rv = SetRawRangeFromFlatTextOffset(&rawRange,
                                           aEvent->mInput.mOffset - 1, 1,
                                           lineBreakType, true, nullptr);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NS_ERROR_UNEXPECTED;
        }
        FrameAndNodeOffset frameForPrevious =
          GetFirstFrameInRangeForTextRect(rawRange);
        startsBetweenLineBreaker = frameForPrevious.mFrame == firstFrame.mFrame;
      }
    } else {
      NS_WARNING("The frame is neither a text frame nor a frame whose content "
                 "causes a line break");
      return NS_ERROR_FAILURE;
    }

    for (size_t i = 0; i < charRects.Length() && offset < kEndOffset; i++) {
      nsRect charRect = charRects[i];
      // Store lastCharRect before applying CSS transform because it may be
      // used for computing a line breaker rect.  Then, the computed line
      // breaker rect will be applied CSS transform again.  Therefore,
      // the value of lastCharRect should be raw rect value relative to the
      // base frame.
      lastCharRect = charRect;
      lastFrame = baseFrame;
      rv = ConvertToRootRelativeOffset(baseFrame, charRect);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rect = LayoutDeviceIntRect::FromUnknownRect(charRect.ToOutsidePixels(
        baseFrame->PresContext()->AppUnitsPerDevPixel()));
      // Returning empty rect may cause native IME confused, let's make sure to
      // return non-empty rect.
      EnsureNonEmptyRect(rect);

      aEvent->mReply.mRectArray.AppendElement(rect);
      offset++;

      // If it's not a line breaker or the line breaker length is same as
      // XP line breaker's, we need to do nothing for current character.
      wasLineBreaker = chars[i] == '\n';
      if (!wasLineBreaker || kBRLength == 1) {
        continue;
      }

      MOZ_ASSERT(kBRLength == 2);

      // If it's already reached the end of query range, we don't need to do
      // anymore.
      if (offset == kEndOffset) {
        break;
      }

      // If the query range starts from between a line breaker, i.e., it starts
      // between "\r" and "\n", the appended rect was for the "\n".  Therefore,
      // we don't need to append same rect anymore for current "\r\n".
      if (startsBetweenLineBreaker) {
        continue;
      }

      // The appended rect was for "\r" of "\r\n".  Therefore, we need to
      // append same rect for "\n" too because querying rect of "\r" and "\n"
      // should return same rect.  E.g., IME may query previous character's
      // rect of first character of a line.
      aEvent->mReply.mRectArray.AppendElement(rect);
      offset++;
    }
  }

  // If the query range is longer than actual content length, we should append
  // caret rect at the end of the content as the last character rect because
  // native IME may want to query character rect at the end of contents for
  // deciding the position of a popup window (e.g., suggest window for next
  // word).  Note that when this method hasn't appended character rects, it
  // means that the offset is too large or the query range is collapsed.
  if (offset < kEndOffset || aEvent->mReply.mRectArray.IsEmpty()) {
    // If we've already retrieved some character rects before current offset,
    // we can guess the last rect from the last character's rect unless it's a
    // line breaker.  (If it's a line breaker, the caret rect is in next line.)
    if (!aEvent->mReply.mRectArray.IsEmpty() && !wasLineBreaker) {
      rect = aEvent->mReply.mRectArray.LastElement();
      if (isVertical) {
        rect.y = rect.YMost() + 1;
        rect.height = 1;
        MOZ_ASSERT(rect.width);
      } else {
        rect.x = rect.XMost() + 1;
        rect.width = 1;
        MOZ_ASSERT(rect.height);
      }
      aEvent->mReply.mRectArray.AppendElement(rect);
    } else {
      // Note that don't use eQueryCaretRect here because if caret is at the
      // end of the content, it returns actual caret rect instead of computing
      // the rect itself.  It means that the result depends on caret position.
      // So, we shouldn't use it for consistency result in automated tests.
      WidgetQueryContentEvent queryTextRect(eQueryTextRect, *aEvent);
      WidgetQueryContentEvent::Options options(*aEvent);
      queryTextRect.InitForQueryTextRect(offset, 1, options);
      rv = OnQueryTextRect(&queryTextRect);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (NS_WARN_IF(!queryTextRect.mSucceeded)) {
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(!queryTextRect.mReply.mRect.IsEmpty());
      if (queryTextRect.mReply.mWritingMode.IsVertical()) {
        queryTextRect.mReply.mRect.height = 1;
      } else {
        queryTextRect.mReply.mRect.width = 1;
      }
      aEvent->mReply.mRectArray.AppendElement(queryTextRect.mReply.mRect);
    }
  }

  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryTextRect(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // If mLength is 0 (this may be caused by bug of native IME), we should
  // redirect this event to OnQueryCaretRect().
  if (!aEvent->mInput.mLength) {
    return OnQueryCaretRect(aEvent);
  }

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  RawRange rawRange;
  nsCOMPtr<nsIContent> lastTextContent;
  rv = SetRawRangeFromFlatTextOffset(&rawRange, aEvent->mInput.mOffset,
                                     aEvent->mInput.mLength, lineBreakType,
                                     true, &aEvent->mReply.mOffset,
                                     getter_AddRefs(lastTextContent));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GenerateFlatTextContent(rawRange, aEvent->mReply.mString, lineBreakType);
  NS_ENSURE_SUCCESS(rv, rv);

  // used to iterate over all contents and their frames
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  rv = iter->Init(rawRange.Start().AsRaw(), rawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  // Get the first frame which causes some text after the offset.
  FrameAndNodeOffset firstFrame = GetFirstFrameInRangeForTextRect(rawRange);

  // If GetFirstFrameInRangeForTextRect() does not return valid frame, that
  // means that there are no visible frames having text or the offset reached
  // the end of contents.
  if (!firstFrame.IsValid()) {
    nsAutoString allText;
    rv = GenerateFlatTextContent(mRootContent, allText, lineBreakType);
    // If the offset doesn't reach the end of contents but there is no frames
    // for the node, that means that current offset's node is hidden by CSS or
    // something.  Ideally, we should handle it with the last visible text
    // node's last character's rect, but it's not usual cases in actual web
    // services.  Therefore, currently, we should make this case fail.
    if (NS_WARN_IF(NS_FAILED(rv)) ||
        static_cast<uint32_t>(aEvent->mInput.mOffset) < allText.Length()) {
      return NS_ERROR_FAILURE;
    }

    // Look for the last frame which should be included text rects.
    rv = rawRange.SelectNodeContents(mRootContent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_UNEXPECTED;
    }
    nsRect rect;
    FrameAndNodeOffset lastFrame = GetLastFrameInRangeForTextRect(rawRange);
    // If there is at least one frame which can be used for computing a rect
    // for a character or a line breaker, we should use it for guessing the
    // caret rect at the end of the contents.
    nsPresContext* presContext;
    if (lastFrame) {
      presContext = lastFrame->PresContext();
      if (NS_WARN_IF(!lastFrame->GetContent())) {
        return NS_ERROR_FAILURE;
      }
      FrameRelativeRect relativeRect;
      // If there is a <br> frame at the end, it represents an empty line at
      // the end with moz-<br> or content <br> in a block level element.
      if (lastFrame->IsBrFrame()) {
        relativeRect = GetLineBreakerRectBefore(lastFrame);
      }
      // If there is a text frame at the end, use its information.
      else if (lastFrame->IsTextFrame()) {
        relativeRect = GuessLineBreakerRectAfter(lastFrame->GetContent());
      }
      // If there is an empty frame which is neither a text frame nor a <br>
      // frame at the end, guess caret rect in it.
      else {
        relativeRect = GuessFirstCaretRectIn(lastFrame);
      }
      if (NS_WARN_IF(!relativeRect.IsValid())) {
        return NS_ERROR_FAILURE;
      }
      rect = relativeRect.RectRelativeTo(lastFrame);
      rv = ConvertToRootRelativeOffset(lastFrame, rect);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      aEvent->mReply.mWritingMode = lastFrame->GetWritingMode();
    }
    // Otherwise, if there are no contents in mRootContent, guess caret rect in
    // its frame (with its font height and content box).
    else {
      nsIFrame* rootContentFrame = mRootContent->GetPrimaryFrame();
      if (NS_WARN_IF(!rootContentFrame)) {
        return NS_ERROR_FAILURE;
      }
      presContext = rootContentFrame->PresContext();
      FrameRelativeRect relativeRect = GuessFirstCaretRectIn(rootContentFrame);
      if (NS_WARN_IF(!relativeRect.IsValid())) {
        return NS_ERROR_FAILURE;
      }
      rect = relativeRect.RectRelativeTo(rootContentFrame);
      rv = ConvertToRootRelativeOffset(rootContentFrame, rect);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      aEvent->mReply.mWritingMode = rootContentFrame->GetWritingMode();
    }
    aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
      rect.ToOutsidePixels(presContext->AppUnitsPerDevPixel()));
    EnsureNonEmptyRect(aEvent->mReply.mRect);
    aEvent->mSucceeded = true;
    return NS_OK;
  }

  nsRect rect, frameRect;
  nsPoint ptOffset;

  // If the first frame is a text frame, the result should be computed with
  // the frame's rect but not including the rect before start point of the
  // queried range.
  if (firstFrame->IsTextFrame()) {
    rect.SetRect(nsPoint(0, 0), firstFrame->GetRect().Size());
    rv = ConvertToRootRelativeOffset(firstFrame, rect);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    frameRect = rect;
    // Exclude the rect before start point of the queried range.
    firstFrame->GetPointFromOffset(firstFrame.mOffsetInNode, &ptOffset);
    if (firstFrame->GetWritingMode().IsVertical()) {
      rect.y += ptOffset.y;
      rect.height -= ptOffset.y;
    } else {
      rect.x += ptOffset.x;
      rect.width -= ptOffset.x;
    }
  }
  // If first frame causes a line breaker but it's not a <br> frame, we cannot
  // compute proper rect only with the frame because typically caret is at
  // right of the last character of it.  For example, if caret is after "c" of
  // |<p>abc</p><p>def</p>|, IME may query a line breaker's rect after "c".
  // Then, if we compute it only with the 2nd <p>'s block frame, the result
  // will be:
  //  +-<p>--------------------------------+
  //  |abc                                 |
  //  +------------------------------------+
  //
  // I+-<p>--------------------------------+
  //  |def                                 |
  //  +------------------------------------+
  // However, users expect popup windows of IME should be positioned at
  // right-bottom of "c" like this:
  //  +-<p>--------------------------------+
  //  |abcI                                |
  //  +------------------------------------+
  //
  //  +-<p>--------------------------------+
  //  |def                                 |
  //  +------------------------------------+
  // Therefore, if the first frame isn't a <br> frame and there is a text
  // node before the first node in the queried range, we should compute the
  // first rect with the previous character's rect.
  else if (!firstFrame->IsBrFrame() && lastTextContent) {
    FrameRelativeRect brRectAfterLastChar =
      GuessLineBreakerRectAfter(lastTextContent);
    if (NS_WARN_IF(!brRectAfterLastChar.IsValid())) {
      return NS_ERROR_FAILURE;
    }
    rect = brRectAfterLastChar.mRect;
    rv = ConvertToRootRelativeOffset(brRectAfterLastChar.mBaseFrame, rect);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    frameRect = rect;
  }
  // Otherwise, we need to compute the line breaker's rect only with the
  // first frame's rect.  But this may be unexpected.  For example,
  // |<div contenteditable>[<p>]abc</p></div>|.  In this case, caret is before
  // "a", therefore, users expect the rect left of "a".  However, we don't
  // have enough information about the next character here and this isn't
  // usual case (e.g., IME typically tries to query the rect of "a" or caret
  // rect for computing its popup position).  Therefore, we shouldn't do
  // more complicated hack here unless we'll get some bug reports actually.
  else {
    FrameRelativeRect relativeRect = GetLineBreakerRectBefore(firstFrame);
    if (NS_WARN_IF(!relativeRect.IsValid())) {
      return NS_ERROR_FAILURE;
    }
    rect = relativeRect.RectRelativeTo(firstFrame);
    rv = ConvertToRootRelativeOffset(firstFrame, rect);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    frameRect = rect;
  }
  // UnionRect() requires non-empty rect.  So, let's make sure to get non-emtpy
  // rect from the first frame.
  EnsureNonEmptyRect(rect);

  // Get the last frame which causes some text in the range.
  FrameAndNodeOffset lastFrame = GetLastFrameInRangeForTextRect(rawRange);
  if (NS_WARN_IF(!lastFrame.IsValid())) {
    return NS_ERROR_FAILURE;
  }

  // iterate over all covered frames
  for (nsIFrame* frame = firstFrame; frame != lastFrame;) {
    frame = frame->GetNextContinuation();
    if (!frame) {
      do {
        iter->Next();
        nsINode* node = iter->GetCurrentNode();
        if (!node) {
          break;
        }
        if (!node->IsContent()) {
          continue;
        }
        nsIFrame* primaryFrame = node->AsContent()->GetPrimaryFrame();
        // The node may be hidden by CSS.
        if (!primaryFrame) {
          continue;
        }
        // We should take only text frame's rect and br frame's rect.  We can
        // always use frame rect of text frame and GetLineBreakerRectBefore()
        // can return exactly correct rect only for <br> frame for now.  On the
        // other hand, GetLineBreakRectBefore() returns guessed caret rect for
        // the other frames.  We shouldn't include such odd rect to the result.
        if (primaryFrame->IsTextFrame() || primaryFrame->IsBrFrame()) {
          frame = primaryFrame;
        }
      } while (!frame && !iter->IsDone());
      if (!frame) {
        break;
      }
    }
    if (frame->IsTextFrame()) {
      frameRect.SetRect(nsPoint(0, 0), frame->GetRect().Size());
    } else {
      MOZ_ASSERT(frame->IsBrFrame());
      FrameRelativeRect relativeRect = GetLineBreakerRectBefore(frame);
      if (NS_WARN_IF(!relativeRect.IsValid())) {
        return NS_ERROR_FAILURE;
      }
      frameRect = relativeRect.RectRelativeTo(frame);
    }
    rv = ConvertToRootRelativeOffset(frame, frameRect);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // UnionRect() requires non-empty rect.  So, let's make sure to get
    // non-emtpy rect from the frame.
    EnsureNonEmptyRect(frameRect);
    if (frame != lastFrame) {
      // not last frame, so just add rect to previous result
      rect.UnionRect(rect, frameRect);
    }
  }

  // Get the ending frame rect.
  // FYI: If first frame and last frame are same, frameRect is already set
  //      to the rect excluding the text before the query range.
  if (firstFrame.mFrame != lastFrame.mFrame) {
    frameRect.SetRect(nsPoint(0, 0), lastFrame->GetRect().Size());
    rv = ConvertToRootRelativeOffset(lastFrame, frameRect);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Shrink the last frame for cutting off the text after the query range.
  if (lastFrame->IsTextFrame()) {
    lastFrame->GetPointFromOffset(lastFrame.mOffsetInNode, &ptOffset);
    if (lastFrame->GetWritingMode().IsVertical()) {
      frameRect.height -= lastFrame->GetRect().height - ptOffset.y;
    } else {
      frameRect.width -= lastFrame->GetRect().width - ptOffset.x;
    }
    // UnionRect() requires non-empty rect.  So, let's make sure to get
    // non-empty rect from the last frame.
    EnsureNonEmptyRect(frameRect);

    if (firstFrame.mFrame == lastFrame.mFrame) {
      rect.IntersectRect(rect, frameRect);
    } else {
      rect.UnionRect(rect, frameRect);
    }
  }

  aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
      rect.ToOutsidePixels(lastFrame->PresContext()->AppUnitsPerDevPixel()));
  // Returning empty rect may cause native IME confused, let's make sure to
  // return non-empty rect.
  EnsureNonEmptyRect(aEvent->mReply.mRect);
  aEvent->mReply.mWritingMode = lastFrame->GetWritingMode();
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryEditorRect(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIContent* focusedContent = GetFocusedContent();
  rv = QueryContentRect(IsPlugin(focusedContent) ?
                          focusedContent : mRootContent.get(), aEvent);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryCaretRect(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // When the selection is collapsed and the queried offset is current caret
  // position, we should return the "real" caret rect.
  if (mSelection->IsCollapsed()) {
    nsRect caretRect;
    nsIFrame* caretFrame = nsCaret::GetGeometry(mSelection, &caretRect);
    if (caretFrame) {
      uint32_t offset;
      rv = GetStartOffset(mFirstSelectedRawRange,
                          &offset, GetLineBreakType(aEvent));
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == aEvent->mInput.mOffset) {
        rv = ConvertToRootRelativeOffset(caretFrame, caretRect);
        NS_ENSURE_SUCCESS(rv, rv);
        nscoord appUnitsPerDevPixel =
          caretFrame->PresContext()->AppUnitsPerDevPixel();
        aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
          caretRect.ToOutsidePixels(appUnitsPerDevPixel));
        // Returning empty rect may cause native IME confused, let's make sure
        // to return non-empty rect.
        EnsureNonEmptyRect(aEvent->mReply.mRect);
        aEvent->mReply.mWritingMode = caretFrame->GetWritingMode();
        aEvent->mReply.mOffset = aEvent->mInput.mOffset;
        aEvent->mSucceeded = true;
        return NS_OK;
      }
    }
  }

  // Otherwise, we should guess the caret rect from the character's rect.
  WidgetQueryContentEvent queryTextRectEvent(eQueryTextRect, *aEvent);
  WidgetQueryContentEvent::Options options(*aEvent);
  queryTextRectEvent.InitForQueryTextRect(aEvent->mInput.mOffset, 1, options);
  rv = OnQueryTextRect(&queryTextRectEvent);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!queryTextRectEvent.mSucceeded)) {
    return NS_ERROR_FAILURE;
  }
  queryTextRectEvent.mReply.mString.Truncate();
  aEvent->mReply = queryTextRectEvent.mReply;
  if (aEvent->GetWritingMode().IsVertical()) {
    aEvent->mReply.mRect.height = 1;
  } else {
    aEvent->mReply.mRect.width = 1;
  }
  // Returning empty rect may cause native IME confused, let's make sure to
  // return non-empty rect.
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryContentState(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQuerySelectionAsTransferable(
                       WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!aEvent->mReply.mHasSelection) {
    aEvent->mSucceeded = true;
    aEvent->mReply.mTransferable = nullptr;
    return NS_OK;
  }

  rv = nsCopySupport::GetTransferableForSelection(
         mSelection, mDocument, getter_AddRefs(aEvent->mReply.mTransferable));
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryCharacterAtPoint(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aEvent->mReply.mOffset = aEvent->mReply.mTentativeCaretOffset =
    WidgetQueryContentEvent::NOT_FOUND;

  nsIPresShell* shell = mDocument->GetShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);
  nsIFrame* rootFrame = shell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, NS_ERROR_FAILURE);
  nsIWidget* rootWidget = rootFrame->GetNearestWidget();
  NS_ENSURE_TRUE(rootWidget, NS_ERROR_FAILURE);

  // The root frame's widget might be different, e.g., the event was fired on
  // a popup but the rootFrame is the document root.
  if (rootWidget != aEvent->mWidget) {
    MOZ_ASSERT(aEvent->mWidget, "The event must have the widget");
    nsView* view = nsView::GetViewFor(aEvent->mWidget);
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
    rootFrame = view->GetFrame();
    NS_ENSURE_TRUE(rootFrame, NS_ERROR_FAILURE);
    rootWidget = rootFrame->GetNearestWidget();
    NS_ENSURE_TRUE(rootWidget, NS_ERROR_FAILURE);
  }

  WidgetQueryContentEvent eventOnRoot(true, eQueryCharacterAtPoint,
                                      rootWidget);
  eventOnRoot.mUseNativeLineBreak = aEvent->mUseNativeLineBreak;
  eventOnRoot.mRefPoint = aEvent->mRefPoint;
  if (rootWidget != aEvent->mWidget) {
    eventOnRoot.mRefPoint += aEvent->mWidget->WidgetToScreenOffset() -
      rootWidget->WidgetToScreenOffset();
  }
  nsPoint ptInRoot =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(&eventOnRoot, rootFrame);

  nsIFrame* targetFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, ptInRoot);
  if (!targetFrame || !targetFrame->GetContent() ||
      !nsContentUtils::ContentIsDescendantOf(targetFrame->GetContent(),
                                             mRootContent)) {
    // There is no character at the point.
    aEvent->mSucceeded = true;
    return NS_OK;
  }
  nsPoint ptInTarget = ptInRoot + rootFrame->GetOffsetToCrossDoc(targetFrame);
  int32_t rootAPD = rootFrame->PresContext()->AppUnitsPerDevPixel();
  int32_t targetAPD = targetFrame->PresContext()->AppUnitsPerDevPixel();
  ptInTarget = ptInTarget.ScaleToOtherAppUnits(rootAPD, targetAPD);

  nsIFrame::ContentOffsets tentativeCaretOffsets =
    targetFrame->GetContentOffsetsFromPoint(ptInTarget);
  if (!tentativeCaretOffsets.content ||
      !nsContentUtils::ContentIsDescendantOf(tentativeCaretOffsets.content,
                                             mRootContent)) {
    // There is no character nor tentative caret point at the point.
    aEvent->mSucceeded = true;
    return NS_OK;
  }

  rv = GetFlatTextLengthInRange(NodePosition(mRootContent, 0),
                                NodePosition(tentativeCaretOffsets),
                                mRootContent,
                                &aEvent->mReply.mTentativeCaretOffset,
                                GetLineBreakType(aEvent));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!targetFrame->IsTextFrame()) {
    // There is no character at the point but there is tentative caret point.
    aEvent->mSucceeded = true;
    return NS_OK;
  }

  MOZ_ASSERT(
    aEvent->mReply.mTentativeCaretOffset != WidgetQueryContentEvent::NOT_FOUND,
    "The point is inside a character bounding box.  Why tentative caret point "
    "hasn't been found?");

  nsTextFrame* textframe = static_cast<nsTextFrame*>(targetFrame);
  nsIFrame::ContentOffsets contentOffsets =
    textframe->GetCharacterOffsetAtFramePoint(ptInTarget);
  NS_ENSURE_TRUE(contentOffsets.content, NS_ERROR_FAILURE);
  uint32_t offset;
  rv = GetFlatTextLengthInRange(NodePosition(mRootContent, 0),
                                NodePosition(contentOffsets),
                                mRootContent, &offset,
                                GetLineBreakType(aEvent));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  WidgetQueryContentEvent textRect(true, eQueryTextRect, aEvent->mWidget);
  WidgetQueryContentEvent::Options options(*aEvent);
  textRect.InitForQueryTextRect(offset, 1, options);
  rv = OnQueryTextRect(&textRect);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(textRect.mSucceeded, NS_ERROR_FAILURE);

  // currently, we don't need to get the actual text.
  aEvent->mReply.mOffset = offset;
  aEvent->mReply.mRect = textRect.mReply.mRect;
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
ContentEventHandler::OnQueryDOMWidgetHittest(WidgetQueryContentEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent must not be null");

  nsresult rv = InitBasic();
  if (NS_FAILED(rv)) {
    return rv;
  }

  aEvent->mSucceeded = false;
  aEvent->mReply.mWidgetIsHit = false;

  NS_ENSURE_TRUE(aEvent->mWidget, NS_ERROR_FAILURE);

  nsIPresShell* shell = mDocument->GetShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);
  nsIFrame* docFrame = shell->GetRootFrame();
  NS_ENSURE_TRUE(docFrame, NS_ERROR_FAILURE);

  LayoutDeviceIntPoint eventLoc =
    aEvent->mRefPoint + aEvent->mWidget->WidgetToScreenOffset();
  CSSIntRect docFrameRect = docFrame->GetScreenRect();
  CSSIntPoint eventLocCSS(
    docFrame->PresContext()->DevPixelsToIntCSSPixels(eventLoc.x) - docFrameRect.x,
    docFrame->PresContext()->DevPixelsToIntCSSPixels(eventLoc.y) - docFrameRect.y);

  Element* contentUnderMouse =
    mDocument->ElementFromPointHelper(eventLocCSS.x, eventLocCSS.y, false, false);
  if (contentUnderMouse) {
    nsIWidget* targetWidget = nullptr;
    nsIFrame* targetFrame = contentUnderMouse->GetPrimaryFrame();
    nsIObjectFrame* pluginFrame = do_QueryFrame(targetFrame);
    if (pluginFrame) {
      targetWidget = pluginFrame->GetWidget();
    } else if (targetFrame) {
      targetWidget = targetFrame->GetNearestWidget();
    }
    if (aEvent->mWidget == targetWidget) {
      aEvent->mReply.mWidgetIsHit = true;
    }
  }

  aEvent->mSucceeded = true;
  return NS_OK;
}

/* static */ nsresult
ContentEventHandler::GetFlatTextLengthInRange(
                       const NodePosition& aStartPosition,
                       const NodePosition& aEndPosition,
                       nsIContent* aRootContent,
                       uint32_t* aLength,
                       LineBreakType aLineBreakType,
                       bool aIsRemovingNode /* = false */)
{
  if (NS_WARN_IF(!aRootContent) || NS_WARN_IF(!aStartPosition.IsSet()) ||
      NS_WARN_IF(!aEndPosition.IsSet()) || NS_WARN_IF(!aLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aStartPosition == aEndPosition) {
    *aLength = 0;
    return NS_OK;
  }

  // Don't create nsContentIterator instance until it's really necessary since
  // destroying without initializing causes unexpected NS_ASSERTION() call.
  nsCOMPtr<nsIContentIterator> iter;

  // Working with ContentIterator, we may need to adjust the end position for
  // including it forcibly.
  NodePosition endPosition(aEndPosition);

  // This may be called for retrieving the text of removed nodes.  Even in this
  // case, the node thinks it's still in the tree because UnbindFromTree() will
  // be called after here.  However, the node was already removed from the
  // array of children of its parent.  So, be careful to handle this case.
  if (aIsRemovingNode) {
    DebugOnly<nsIContent*> parent = aStartPosition.Container()->GetParent();
    MOZ_ASSERT(parent && parent->ComputeIndexOf(aStartPosition.Container()) == -1,
      "At removing the node, the node shouldn't be in the array of children "
      "of its parent");
    MOZ_ASSERT(aStartPosition.Container() == endPosition.Container(),
      "At removing the node, start and end node should be same");
    MOZ_ASSERT(aStartPosition.Offset() == 0,
      "When the node is being removed, the start offset should be 0");
    MOZ_ASSERT(static_cast<uint32_t>(endPosition.Offset()) ==
                 endPosition.Container()->GetChildCount(),
      "When the node is being removed, the end offset should be child count");
    iter = NS_NewPreContentIterator();
    nsresult rv = iter->Init(aStartPosition.Container());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    RawRange prevRawRange;
    nsresult rv =
      prevRawRange.SetStart(aStartPosition.AsRaw());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // When the end position is immediately after non-root element's open tag,
    // we need to include a line break caused by the open tag.
    if (endPosition.Container() != aRootContent &&
        endPosition.IsImmediatelyAfterOpenTag()) {
      if (endPosition.Container()->HasChildren()) {
        // When the end node has some children, move the end position to before
        // the open tag of its first child.
        nsINode* firstChild = endPosition.Container()->GetFirstChild();
        if (NS_WARN_IF(!firstChild)) {
          return NS_ERROR_FAILURE;
        }
        endPosition = NodePositionBefore(firstChild, 0);
      } else {
        // When the end node is empty, move the end position after the node.
        nsIContent* parentContent = endPosition.Container()->GetParent();
        if (NS_WARN_IF(!parentContent)) {
          return NS_ERROR_FAILURE;
        }
        int32_t indexInParent = parentContent->ComputeIndexOf(endPosition.Container());
        if (NS_WARN_IF(indexInParent < 0)) {
          return NS_ERROR_FAILURE;
        }
        endPosition = NodePositionBefore(parentContent, indexInParent + 1);
      }
    }

    if (endPosition.IsSetAndValid()) {
      // Offset is within node's length; set end of range to that offset
      rv = prevRawRange.SetEnd(endPosition.AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      iter = NS_NewPreContentIterator();
      rv = iter->Init(prevRawRange.Start().AsRaw(), prevRawRange.End().AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (endPosition.Container() != aRootContent) {
      // Offset is past node's length; set end of range to end of node
      rv = prevRawRange.SetEndAfter(endPosition.Container());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      iter = NS_NewPreContentIterator();
      rv = iter->Init(prevRawRange.Start().AsRaw(), prevRawRange.End().AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // Offset is past the root node; set end of range to end of root node
      iter = NS_NewPreContentIterator();
      rv = iter->Init(aRootContent);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  *aLength = 0;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    if (!node->IsContent()) {
      continue;
    }
    nsIContent* content = node->AsContent();

    if (node->IsText()) {
      // Note: our range always starts from offset 0
      if (node == endPosition.Container()) {
        // NOTE: We should have an offset here, as endPosition.Container() is a
        // nsINode::eTEXT, which always has an offset.
        *aLength += GetTextLength(content, aLineBreakType,
                                  endPosition.Offset());
      } else {
        *aLength += GetTextLength(content, aLineBreakType);
      }
    } else if (ShouldBreakLineBefore(content, aRootContent)) {
      // If the start position is start of this node but doesn't include the
      // open tag, don't append the line break length.
      if (node == aStartPosition.Container() && !aStartPosition.IsBeforeOpenTag()) {
        continue;
      }
      // If the end position is before the open tag, don't append the line
      // break length.
      if (node == endPosition.Container() && endPosition.IsBeforeOpenTag()) {
        continue;
      }
      *aLength += GetBRLength(aLineBreakType);
    }
  }
  return NS_OK;
}

nsresult
ContentEventHandler::GetStartOffset(const RawRange& aRawRange,
                                    uint32_t* aOffset,
                                    LineBreakType aLineBreakType)
{
  // To match the "no skip start" hack in nsContentIterator::Init, when range
  // offset is 0 and the range node is not a container, we have to assume the
  // range _includes_ the node, which means the start offset should _not_
  // include the node.
  //
  // For example, for this content: <br>abc, and range (<br>, 0)-("abc", 1), the
  // range includes the linebreak from <br>, so the start offset should _not_
  // include <br>, and the start offset should be 0.
  //
  // However, for this content: <p/>abc, and range (<p>, 0)-("abc", 1), the
  // range does _not_ include the linebreak from <p> because <p> is a container,
  // so the start offset _should_ include <p>, and the start offset should be 1.

  nsINode* startNode = aRawRange.GetStartContainer();
  bool startIsContainer = true;
  if (startNode->IsHTMLElement()) {
    nsAtom* name = startNode->NodeInfo()->NameAtom();
    startIsContainer =
      nsHTMLElement::IsContainer(nsHTMLTags::AtomTagToId(name));
  }
  const NodePosition& startPos =
    startIsContainer ?
      NodePosition(startNode, aRawRange.StartOffset()) :
      NodePositionBefore(startNode, aRawRange.StartOffset());
  return GetFlatTextLengthInRange(
           NodePosition(mRootContent, 0), startPos,
           mRootContent, aOffset, aLineBreakType);
}

nsresult
ContentEventHandler::AdjustCollapsedRangeMaybeIntoTextNode(RawRange& aRawRange)
{
  MOZ_ASSERT(aRawRange.Collapsed());

  if (!aRawRange.Collapsed()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsINode> container = aRawRange.GetStartContainer();
  int32_t offsetInParentNode = aRawRange.StartOffset();
  if (NS_WARN_IF(!container) || NS_WARN_IF(offsetInParentNode < 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  // If the node is text node, we don't need to modify aRawRange.
  if (container->IsText()) {
    return NS_OK;
  }

  // If the container is not a text node but it has a text node at the offset,
  // we should adjust the range into the text node.
  // NOTE: This is emulating similar situation of EditorBase.
  nsINode* childNode = nullptr;
  int32_t offsetInChildNode = -1;
  if (!offsetInParentNode && container->HasChildren()) {
    // If the range is the start of the container, adjusted the range to the
    // start of the first child.
    childNode = container->GetFirstChild();
    offsetInChildNode = 0;
  } else if (static_cast<uint32_t>(offsetInParentNode) <
               container->GetChildCount()) {
    // If the range is next to a child node, adjust the range to the end of
    // the previous child.
    childNode = container->GetChildAt_Deprecated(offsetInParentNode - 1);
    offsetInChildNode = childNode->Length();
  }

  // But if the found node isn't a text node, we cannot modify the range.
  if (!childNode || !childNode->IsText() ||
      NS_WARN_IF(offsetInChildNode < 0)) {
    return NS_OK;
  }

  nsresult rv =
    aRawRange.CollapseTo(RawRangeBoundary(childNode, offsetInChildNode));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
ContentEventHandler::ConvertToRootRelativeOffset(nsIFrame* aFrame,
                                                 nsRect& aRect)
{
  NS_ASSERTION(aFrame, "aFrame must not be null");

  nsPresContext* thisPC = aFrame->PresContext();
  nsPresContext* rootPC = thisPC->GetRootPresContext();
  if (NS_WARN_IF(!rootPC)) {
    return NS_ERROR_FAILURE;
  }
  nsIFrame* rootFrame = rootPC->PresShell()->GetRootFrame();
  if (NS_WARN_IF(!rootFrame)) {
    return NS_ERROR_FAILURE;
  }

  aRect = nsLayoutUtils::TransformFrameRectToAncestor(aFrame, aRect, rootFrame);

  // TransformFrameRectToAncestor returned the rect in the ancestor's appUnits,
  // but we want it in aFrame's units (in case of different full-zoom factors),
  // so convert back.
  aRect = aRect.ScaleToOtherAppUnitsRoundOut(rootPC->AppUnitsPerDevPixel(),
                                             thisPC->AppUnitsPerDevPixel());

  return NS_OK;
}

static void AdjustRangeForSelection(nsIContent* aRoot,
                                    nsINode** aNode,
                                    int32_t* aNodeOffset)
{
  nsINode* node = *aNode;
  int32_t nodeOffset = *aNodeOffset;
  if (aRoot == node || NS_WARN_IF(!node->GetParent()) ||
      !node->IsText()) {
    return;
  }

  // When the offset is at the end of the text node, set it to after the
  // text node, to make sure the caret is drawn on a new line when the last
  // character of the text node is '\n' in <textarea>.
  int32_t textLength =
    static_cast<int32_t>(node->AsContent()->TextLength());
  MOZ_ASSERT(nodeOffset <= textLength, "Offset is past length of text node");
  if (nodeOffset != textLength) {
    return;
  }

  nsIContent* aRootParent = aRoot->GetParent();
  if (NS_WARN_IF(!aRootParent)) {
    return;
  }
  // If the root node is not an anonymous div of <textarea>, we don't need to
  // do this hack.  If you did this, ContentEventHandler couldn't distinguish
  // if the range includes open tag of the next node in some cases, e.g.,
  // textNode]<p></p> vs. textNode<p>]</p>
  if (!aRootParent->IsHTMLElement(nsGkAtoms::textarea)) {
    return;
  }

  *aNode = node->GetParent();
  MOZ_ASSERT((*aNode)->ComputeIndexOf(node) != -1);
  *aNodeOffset = (*aNode)->ComputeIndexOf(node) + 1;
}

nsresult
ContentEventHandler::OnSelectionEvent(WidgetSelectionEvent* aEvent)
{
  aEvent->mSucceeded = false;

  // Get selection to manipulate
  // XXX why do we need to get them from ISM? This method should work fine
  //     without ISM.
  RefPtr<Selection> sel;
  nsresult rv =
    IMEStateManager::GetFocusSelectionAndRoot(getter_AddRefs(sel),
                                              getter_AddRefs(mRootContent));
  mSelection = sel;
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = Init(aEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get range from offset and length
  RawRange rawRange;
  rv = SetRawRangeFromFlatTextOffset(&rawRange,
                                     aEvent->mOffset, aEvent->mLength,
                                     GetLineBreakType(aEvent),
                                     aEvent->mExpandToClusterBoundary);
  NS_ENSURE_SUCCESS(rv, rv);

  nsINode* startNode = rawRange.GetStartContainer();
  nsINode* endNode = rawRange.GetEndContainer();
  int32_t startNodeOffset = rawRange.StartOffset();
  int32_t endNodeOffset = rawRange.EndOffset();
  AdjustRangeForSelection(mRootContent, &startNode, &startNodeOffset);
  AdjustRangeForSelection(mRootContent, &endNode, &endNodeOffset);
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode) ||
      NS_WARN_IF(startNodeOffset < 0) || NS_WARN_IF(endNodeOffset < 0)) {
    return NS_ERROR_UNEXPECTED;
  }

  mSelection->StartBatchChanges();

  // Clear selection first before setting
  rv = mSelection->RemoveAllRangesTemporarily();
  // Need to call EndBatchChanges at the end even if call failed
  if (NS_SUCCEEDED(rv)) {
    if (aEvent->mReversed) {
      rv = mSelection->Collapse(endNode, endNodeOffset);
    } else {
      rv = mSelection->Collapse(startNode, startNodeOffset);
    }
    if (NS_SUCCEEDED(rv) &&
        (startNode != endNode || startNodeOffset != endNodeOffset)) {
      if (aEvent->mReversed) {
        rv = mSelection->Extend(startNode, startNodeOffset);
      } else {
        rv = mSelection->Extend(endNode, endNodeOffset);
      }
    }
  }

  // Pass the eSetSelection events reason along with the BatchChange-end
  // selection change notifications.
  mSelection->EndBatchChanges(aEvent->mReason);
  NS_ENSURE_SUCCESS(rv, rv);

  mSelection->ScrollIntoView(
    nsISelectionController::SELECTION_FOCUS_REGION,
    nsIPresShell::ScrollAxis(), nsIPresShell::ScrollAxis(), 0);
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsRect
ContentEventHandler::FrameRelativeRect::RectRelativeTo(
                                          nsIFrame* aDestFrame) const
{
  if (!mBaseFrame || NS_WARN_IF(!aDestFrame)) {
    return nsRect();
  }

  if (NS_WARN_IF(aDestFrame->PresContext() != mBaseFrame->PresContext())) {
    return nsRect();
  }

  if (aDestFrame == mBaseFrame) {
    return mRect;
  }

  nsIFrame* rootFrame = mBaseFrame->PresShell()->GetRootFrame();
  nsRect baseFrameRectInRootFrame =
    nsLayoutUtils::TransformFrameRectToAncestor(mBaseFrame, nsRect(),
                                                rootFrame);
  nsRect destFrameRectInRootFrame =
    nsLayoutUtils::TransformFrameRectToAncestor(aDestFrame, nsRect(),
                                                rootFrame);
  nsPoint difference =
    destFrameRectInRootFrame.TopLeft() - baseFrameRectInRootFrame.TopLeft();
  return mRect - difference;
}

} // namespace mozilla

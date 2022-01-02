/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentEventHandler.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsCaret.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCopySupport.h"
#include "nsElementTable.h"
#include "nsFocusManager.h"
#include "nsFontMetrics.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsQueryObject.h"
#include "nsRange.h"
#include "nsTextFragment.h"
#include "nsTextFrame.h"
#include "nsView.h"
#include "mozilla/ViewportUtils.h"

#include <algorithm>

namespace mozilla {

using namespace dom;
using namespace widget;

/******************************************************************/
/* ContentEventHandler::RawRange                                  */
/******************************************************************/

void ContentEventHandler::RawRange::AssertStartIsBeforeOrEqualToEnd() {
  MOZ_ASSERT(*nsContentUtils::ComparePoints(
                 mStart.Container(),
                 static_cast<int32_t>(*mStart.Offset(
                     NodePosition::OffsetFilter::kValidOrInvalidOffsets)),
                 mEnd.Container(),
                 static_cast<int32_t>(*mEnd.Offset(
                     NodePosition::OffsetFilter::kValidOrInvalidOffsets))) <=
             0);
}

nsresult ContentEventHandler::RawRange::SetStart(
    const RawRangeBoundary& aStart) {
  nsINode* newRoot = RangeUtils::ComputeRootNode(aStart.Container());
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

nsresult ContentEventHandler::RawRange::SetEnd(const RawRangeBoundary& aEnd) {
  nsINode* newRoot = RangeUtils::ComputeRootNode(aEnd.Container());
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

nsresult ContentEventHandler::RawRange::SetEndAfter(nsINode* aEndContainer) {
  return SetEnd(RangeUtils::GetRawRangeBoundaryAfter(aEndContainer));
}

void ContentEventHandler::RawRange::SetStartAndEnd(const nsRange* aRange) {
  DebugOnly<nsresult> rv =
      SetStartAndEnd(aRange->StartRef().AsRaw(), aRange->EndRef().AsRaw());
  MOZ_ASSERT(!aRange->IsPositioned() || NS_SUCCEEDED(rv));
}

nsresult ContentEventHandler::RawRange::SetStartAndEnd(
    const RawRangeBoundary& aStart, const RawRangeBoundary& aEnd) {
  nsINode* newStartRoot = RangeUtils::ComputeRootNode(aStart.Container());
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
    MOZ_ASSERT(*aStart.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets) <=
               *aEnd.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets));
    mRoot = newStartRoot;
    mStart = aStart;
    mEnd = aEnd;
    return NS_OK;
  }

  nsINode* newEndRoot = RangeUtils::ComputeRootNode(aEnd.Container());
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

nsresult ContentEventHandler::RawRange::SelectNodeContents(
    nsINode* aNodeToSelectContents) {
  nsINode* newRoot = RangeUtils::ComputeRootNode(aNodeToSelectContents);
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
//        2.3 are handled correctly, the loop with ContentIterator shouldn't
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
    : mDocument(aPresContext->Document()) {}

nsresult ContentEventHandler::InitBasic(bool aRequireFlush) {
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  if (aRequireFlush) {
    // If text frame which has overflowing selection underline is dirty,
    // we need to flush the pending reflow here.
    mDocument->FlushPendingNotifications(FlushType::Layout);
  }
  return NS_OK;
}

nsresult ContentEventHandler::InitRootContent(
    const Selection& aNormalSelection) {
  // Root content should be computed with normal selection because normal
  // selection is typically has at least one range but the other selections
  // not so.  If there is a range, computing its root is easy, but if
  // there are no ranges, we need to use ancestor limit instead.
  MOZ_ASSERT(aNormalSelection.Type() == SelectionType::eNormal);

  if (!aNormalSelection.RangeCount()) {
    // If there is no selection range, we should compute the selection root
    // from ancestor limiter or root content of the document.
    mRootContent = aNormalSelection.GetAncestorLimiter();
    if (!mRootContent) {
      mRootContent = mDocument->GetRootElement();
      if (NS_WARN_IF(!mRootContent)) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    return NS_OK;
  }

  RefPtr<const nsRange> range(aNormalSelection.GetRangeAt(0));
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_UNEXPECTED;
  }

  // If there is a selection, we should retrieve the selection root from
  // the range since when the window is inactivated, the ancestor limiter
  // of selection was cleared by blur event handler of EditorBase but the
  // selection range still keeps storing the nodes.  If the active element of
  // the deactive window is <input> or <textarea>, we can compute the
  // selection root from them.
  nsCOMPtr<nsINode> startNode = range->GetStartContainer();
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

  RefPtr<PresShell> presShell = mDocument->GetPresShell();
  mRootContent = startNode->GetSelectionRootContent(presShell);
  if (NS_WARN_IF(!mRootContent)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult ContentEventHandler::InitCommon(SelectionType aSelectionType,
                                         bool aRequireFlush) {
  if (mSelection && mSelection->Type() == aSelectionType) {
    return NS_OK;
  }

  mSelection = nullptr;
  mRootContent = nullptr;
  mFirstSelectedRawRange.Clear();

  nsresult rv = InitBasic(aRequireFlush);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsFrameSelection> frameSel;
  if (PresShell* presShell = mDocument->GetPresShell()) {
    frameSel = presShell->GetLastFocusedFrameSelection();
  }
  if (NS_WARN_IF(!frameSel)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mSelection = frameSel->GetSelection(aSelectionType);
  if (NS_WARN_IF(!mSelection)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<Selection> normalSelection;
  if (mSelection->Type() == SelectionType::eNormal) {
    normalSelection = mSelection;
  } else {
    normalSelection = frameSel->GetSelection(SelectionType::eNormal);
    if (NS_WARN_IF(!normalSelection)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  rv = InitRootContent(*normalSelection);
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
  rv = mFirstSelectedRawRange.CollapseTo(RawRangeBoundary(mRootContent, 0u));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult ContentEventHandler::Init(WidgetQueryContentEvent* aEvent) {
  NS_ASSERTION(aEvent, "aEvent must not be null");
  MOZ_ASSERT(aEvent->mMessage == eQuerySelectedText ||
             aEvent->mInput.mSelectionType == SelectionType::eNormal);

  if (NS_WARN_IF(!aEvent->mInput.IsValidOffset()) ||
      NS_WARN_IF(!aEvent->mInput.IsValidEventMessage(aEvent->mMessage))) {
    return NS_ERROR_FAILURE;
  }

  // Note that we should ignore WidgetQueryContentEvent::Input::mSelectionType
  // if the event isn't eQuerySelectedText.
  SelectionType selectionType = aEvent->mMessage == eQuerySelectedText
                                    ? aEvent->mInput.mSelectionType
                                    : SelectionType::eNormal;
  if (NS_WARN_IF(selectionType == SelectionType::eNone)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = InitCommon(selectionType, aEvent->NeedsToFlushLayout());
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

  // Ideally, we should emplace only when we return succeeded event.
  // However, we need to emplace here since it's hard to store the various
  // result.  Intead, `HandleQueryContentEvent()` will reset `mReply` if
  // corresponding handler returns error.
  aEvent->EmplaceReply();

  aEvent->mReply->mContentsRoot = mRootContent.get();

  aEvent->mReply->mHasSelection = !mSelection->IsCollapsed();

  nsRect r;
  nsIFrame* frame = nsCaret::GetGeometry(mSelection, &r);
  if (!frame) {
    frame = mRootContent->GetPrimaryFrame();
    if (NS_WARN_IF(!frame)) {
      return NS_ERROR_FAILURE;
    }
  }
  aEvent->mReply->mFocusedWidget = frame->GetNearestWidget();

  return NS_OK;
}

nsresult ContentEventHandler::Init(WidgetSelectionEvent* aEvent) {
  NS_ASSERTION(aEvent, "aEvent must not be null");

  nsresult rv = InitCommon();
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mSucceeded = false;

  return NS_OK;
}

nsIContent* ContentEventHandler::GetFocusedContent() {
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  return nsFocusManager::GetFocusedDescendant(
      window, nsFocusManager::eIncludeAllDescendants,
      getter_AddRefs(focusedWindow));
}

nsresult ContentEventHandler::QueryContentRect(
    nsIContent* aContent, WidgetQueryContentEvent* aEvent) {
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

  aEvent->mReply->mRect = LayoutDeviceIntRect::FromAppUnitsToOutside(
      resultRect, presContext->AppUnitsPerDevPixel());
  // Returning empty rect may cause native IME confused, let's make sure to
  // return non-empty rect.
  EnsureNonEmptyRect(aEvent->mReply->mRect);

  return NS_OK;
}

// Editor places a padding <br> element under its root content if the editor
// doesn't have any text. This happens even for single line editors.
// When we get text content and when we change the selection,
// we don't want to include the padding <br> elements at the end.
static bool IsContentBR(const nsIContent& aContent) {
  const HTMLBRElement* brElement = HTMLBRElement::FromNode(aContent);
  return brElement && !brElement->IsPaddingForEmptyLastLine() &&
         !brElement->IsPaddingForEmptyEditor();
}

static bool IsPaddingBR(const nsIContent& aContent) {
  return aContent.IsHTMLElement(nsGkAtoms::br) && !IsContentBR(aContent);
}

static void ConvertToNativeNewlines(nsString& aString) {
#if defined(XP_WIN)
  aString.ReplaceSubstring(u"\n"_ns, u"\r\n"_ns);
#endif
}

static void AppendString(nsString& aString, const Text& aTextNode) {
  const uint32_t oldXPLength = aString.Length();
  aTextNode.TextFragment().AppendTo(aString);
  if (aTextNode.HasFlag(NS_MAYBE_MASKED)) {
    EditorUtils::MaskString(aString, aTextNode, oldXPLength, 0);
  }
}

static void AppendSubString(nsString& aString, const Text& aTextNode,
                            uint32_t aXPOffset, uint32_t aXPLength) {
  const uint32_t oldXPLength = aString.Length();
  aTextNode.TextFragment().AppendTo(aString, aXPOffset, aXPLength);
  if (aTextNode.HasFlag(NS_MAYBE_MASKED)) {
    EditorUtils::MaskString(aString, aTextNode, oldXPLength, aXPOffset);
  }
}

#if defined(XP_WIN)
static uint32_t CountNewlinesInXPLength(const Text& aTextNode,
                                        uint32_t aXPLength) {
  const nsTextFragment& textFragment = aTextNode.TextFragment();
  // For automated tests, we should abort on debug build.
  MOZ_ASSERT(aXPLength == UINT32_MAX || aXPLength <= textFragment.GetLength(),
             "aXPLength is out-of-bounds");
  const uint32_t length = std::min(aXPLength, textFragment.GetLength());
  uint32_t newlines = 0;
  for (uint32_t i = 0; i < length; ++i) {
    if (textFragment.CharAt(i) == '\n') {
      ++newlines;
    }
  }
  return newlines;
}

static uint32_t CountNewlinesInNativeLength(const Text& aTextNode,
                                            uint32_t aNativeLength) {
  const nsTextFragment& textFragment = aTextNode.TextFragment();
  // For automated tests, we should abort on debug build.
  MOZ_ASSERT((aNativeLength == UINT32_MAX ||
              aNativeLength <= textFragment.GetLength() * 2),
             "aNativeLength is unexpected value");
  const uint32_t xpLength = textFragment.GetLength();
  uint32_t newlines = 0;
  for (uint32_t i = 0, nativeOffset = 0;
       i < xpLength && nativeOffset < aNativeLength; ++i, ++nativeOffset) {
    // For automated tests, we should abort on debug build.
    MOZ_ASSERT(i < xpLength, "i is out-of-bounds");
    if (textFragment.CharAt(i) == '\n') {
      ++newlines;
      ++nativeOffset;
    }
  }
  return newlines;
}
#endif

/* static */
uint32_t ContentEventHandler::GetNativeTextLength(const Text& aTextNode,
                                                  uint32_t aStartOffset,
                                                  uint32_t aEndOffset) {
  MOZ_ASSERT(aEndOffset >= aStartOffset,
             "aEndOffset must be equals or larger than aStartOffset");
  if (aStartOffset == aEndOffset) {
    return 0;
  }
  return GetTextLength(aTextNode, LINE_BREAK_TYPE_NATIVE, aEndOffset) -
         GetTextLength(aTextNode, LINE_BREAK_TYPE_NATIVE, aStartOffset);
}

/* static */
uint32_t ContentEventHandler::GetNativeTextLength(const Text& aTextNode,
                                                  uint32_t aMaxLength) {
  return GetTextLength(aTextNode, LINE_BREAK_TYPE_NATIVE, aMaxLength);
}

/* static inline */
uint32_t ContentEventHandler::GetBRLength(LineBreakType aLineBreakType) {
#if defined(XP_WIN)
  // Length of \r\n
  return (aLineBreakType == LINE_BREAK_TYPE_NATIVE) ? 2 : 1;
#else
  return 1;
#endif
}

/* static */
uint32_t ContentEventHandler::GetTextLength(const Text& aTextNode,
                                            LineBreakType aLineBreakType,
                                            uint32_t aMaxLength) {
  const uint32_t textLengthDifference =
#if defined(XP_WIN)
      // On Windows, the length of a native newline ("\r\n") is twice the length
      // of the XP newline ("\n"), so XP length is equal to the length of the
      // native offset plus the number of newlines encountered in the string.
      (aLineBreakType == LINE_BREAK_TYPE_NATIVE)
          ? CountNewlinesInXPLength(aTextNode, aMaxLength)
          : 0;
#else
      // On other platforms, the native and XP newlines are the same.
      0;
#endif

  const uint32_t length =
      std::min(aTextNode.TextFragment().GetLength(), aMaxLength);
  return length + textLengthDifference;
}

static uint32_t ConvertToXPOffset(const Text& aTextNode,
                                  uint32_t aNativeOffset) {
#if defined(XP_WIN)
  // On Windows, the length of a native newline ("\r\n") is twice the length of
  // the XP newline ("\n"), so XP offset is equal to the length of the native
  // offset minus the number of newlines encountered in the string.
  return aNativeOffset - CountNewlinesInNativeLength(aTextNode, aNativeOffset);
#else
  // On other platforms, the native and XP newlines are the same.
  return aNativeOffset;
#endif
}

/* static */
bool ContentEventHandler::ShouldBreakLineBefore(
    const nsIContent& aContent, const nsINode* aRootNode /* = nullptr */) {
  // We don't need to append linebreak at the start of the root element.
  if (&aContent == aRootNode) {
    return false;
  }

  // If it's not an HTML element (including other markup language's elements),
  // we shouldn't insert like break before that for now.  Becoming this is a
  // problem must be edge case.  E.g., when ContentEventHandler is used with
  // MathML or SVG elements.
  if (!aContent.IsHTMLElement()) {
    return false;
  }

  // If the element is <br>, we need to check if the <br> is caused by web
  // content.  Otherwise, i.e., it's caused by internal reason of Gecko,
  // it shouldn't be exposed as a line break to flatten text.
  if (aContent.IsHTMLElement(nsGkAtoms::br)) {
    return IsContentBR(aContent);
  }

  // Note that ideally, we should refer the style of the primary frame of
  // aContent for deciding if it's an inline.  However, it's difficult
  // IMEContentObserver to notify IME of text change caused by style change.
  // Therefore, currently, we should check only from the tag for now.
  if (aContent.IsAnyOfHTMLElements(
          nsGkAtoms::a, nsGkAtoms::abbr, nsGkAtoms::acronym, nsGkAtoms::b,
          nsGkAtoms::bdi, nsGkAtoms::bdo, nsGkAtoms::big, nsGkAtoms::cite,
          nsGkAtoms::code, nsGkAtoms::data, nsGkAtoms::del, nsGkAtoms::dfn,
          nsGkAtoms::em, nsGkAtoms::font, nsGkAtoms::i, nsGkAtoms::ins,
          nsGkAtoms::kbd, nsGkAtoms::mark, nsGkAtoms::s, nsGkAtoms::samp,
          nsGkAtoms::small, nsGkAtoms::span, nsGkAtoms::strike,
          nsGkAtoms::strong, nsGkAtoms::sub, nsGkAtoms::sup, nsGkAtoms::time,
          nsGkAtoms::tt, nsGkAtoms::u, nsGkAtoms::var)) {
    return false;
  }

  // If the element is unknown element, we shouldn't insert line breaks before
  // it since unknown elements should be ignored.
  RefPtr<HTMLUnknownElement> unknownHTMLElement =
      do_QueryObject(const_cast<nsIContent*>(&aContent));
  return !unknownHTMLElement;
}

nsresult ContentEventHandler::GenerateFlatTextContent(
    nsIContent* aContent, nsString& aString, LineBreakType aLineBreakType) {
  MOZ_ASSERT(aString.IsEmpty());

  RawRange rawRange;
  nsresult rv = rawRange.SelectNodeContents(aContent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return GenerateFlatTextContent(rawRange, aString, aLineBreakType);
}

nsresult ContentEventHandler::GenerateFlatTextContent(
    const RawRange& aRawRange, nsString& aString,
    LineBreakType aLineBreakType) {
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
    AppendSubString(aString, *startNode->AsText(), aRawRange.StartOffset(),
                    aRawRange.EndOffset() - aRawRange.StartOffset());
    ConvertToNativeNewlines(aString);
    return NS_OK;
  }

  PreContentIterator preOrderIter;
  nsresult rv =
      preOrderIter.Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  for (; !preOrderIter.IsDone(); preOrderIter.Next()) {
    nsINode* node = preOrderIter.GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    if (!node->IsContent()) {
      continue;
    }

    if (const Text* textNode = Text::FromNode(node)) {
      if (textNode == startNode) {
        AppendSubString(aString, *textNode, aRawRange.StartOffset(),
                        textNode->TextLength() - aRawRange.StartOffset());
      } else if (textNode == endNode) {
        AppendSubString(aString, *textNode, 0, aRawRange.EndOffset());
      } else {
        AppendString(aString, *textNode);
      }
    } else if (ShouldBreakLineBefore(*node->AsContent(), mRootContent)) {
      aString.Append(char16_t('\n'));
    }
  }
  if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
    ConvertToNativeNewlines(aString);
  }
  return NS_OK;
}

static FontRange* AppendFontRange(nsTArray<FontRange>& aFontRanges,
                                  uint32_t aBaseOffset) {
  FontRange* fontRange = aFontRanges.AppendElement();
  fontRange->mStartOffset = aBaseOffset;
  return fontRange;
}

/* static */
uint32_t ContentEventHandler::GetTextLengthInRange(
    const Text& aTextNode, uint32_t aXPStartOffset, uint32_t aXPEndOffset,
    LineBreakType aLineBreakType) {
  return aLineBreakType == LINE_BREAK_TYPE_NATIVE
             ? GetNativeTextLength(aTextNode, aXPStartOffset, aXPEndOffset)
             : aXPEndOffset - aXPStartOffset;
}

/* static */
void ContentEventHandler::AppendFontRanges(FontRangeArray& aFontRanges,
                                           const Text& aTextNode,
                                           uint32_t aBaseOffset,
                                           uint32_t aXPStartOffset,
                                           uint32_t aXPEndOffset,
                                           LineBreakType aLineBreakType) {
  nsIFrame* frame = aTextNode.GetPrimaryFrame();
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
    uint32_t frameXPStart = std::max(
        static_cast<uint32_t>(curr->GetContentOffset()), aXPStartOffset);
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
        frameXPEnd = std::min(static_cast<uint32_t>(next->GetContentEnd()),
                              aXPEndOffset);
        next =
            frameXPEnd < aXPEndOffset ? next->GetNextContinuation() : nullptr;
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
        baseOffset += GetTextLengthInRange(aTextNode, lastXPEndOffset,
                                           startXPOffset, aLineBreakType);
      }

      FontRange* fontRange = AppendFontRange(aFontRanges, baseOffset);
      fontRange->mFontName.Append(NS_ConvertUTF8toUTF16(font->GetName()));

      ParentLayerToScreenScale2D cumulativeResolution =
          ParentLayerToParentLayerScale(
              frame->PresShell()->GetCumulativeResolution()) *
          nsLayoutUtils::GetTransformToAncestorScaleCrossProcessForFrameMetrics(
              frame);
      float scale =
          std::max(cumulativeResolution.xScale, cumulativeResolution.yScale);

      fontRange->mFontSize = font->GetAdjustedSize() * scale;

      // The converted original offset may exceed the range,
      // hence we need to clamp it.
      uint32_t endXPOffset =
          iter.ConvertSkippedToOriginal(runIter.GetStringEnd());
      endXPOffset = std::min(frameXPEnd, endXPOffset);
      baseOffset += GetTextLengthInRange(aTextNode, startXPOffset, endXPOffset,
                                         aLineBreakType);
      lastXPEndOffset = endXPOffset;
    }
    if (lastXPEndOffset < frameXPEnd) {
      // Create range for skipped trailing chars. It also handles case
      // that the whole frame contains only skipped chars.
      AppendFontRange(aFontRanges, baseOffset);
      baseOffset += GetTextLengthInRange(aTextNode, lastXPEndOffset, frameXPEnd,
                                         aLineBreakType);
    }

    curr = next;
  }
}

nsresult ContentEventHandler::GenerateFlatFontRanges(
    const RawRange& aRawRange, FontRangeArray& aFontRanges, uint32_t& aLength,
    LineBreakType aLineBreakType) {
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
  uint32_t baseOffset = 0;
  PreContentIterator preOrderIter;
  nsresult rv =
      preOrderIter.Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  for (; !preOrderIter.IsDone(); preOrderIter.Next()) {
    nsINode* node = preOrderIter.GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    if (!node->IsContent()) {
      continue;
    }
    nsIContent* content = node->AsContent();

    if (const Text* textNode = Text::FromNode(content)) {
      const uint32_t startOffset =
          textNode != startNode ? 0 : aRawRange.StartOffset();
      const uint32_t endOffset =
          textNode != endNode ? textNode->TextLength() : aRawRange.EndOffset();
      AppendFontRanges(aFontRanges, *textNode, baseOffset, startOffset,
                       endOffset, aLineBreakType);
      baseOffset += GetTextLengthInRange(*textNode, startOffset, endOffset,
                                         aLineBreakType);
    } else if (ShouldBreakLineBefore(*content, mRootContent)) {
      if (aFontRanges.IsEmpty()) {
        MOZ_ASSERT(baseOffset == 0);
        FontRange* fontRange = AppendFontRange(aFontRanges, baseOffset);
        if (nsIFrame* frame = content->GetPrimaryFrame()) {
          const nsFont& font = frame->GetParent()->StyleFont()->mFont;
          const StyleFontFamilyList& fontList = font.family.families;
          MOZ_ASSERT(!fontList.list.IsEmpty(), "Empty font family?");
          const StyleSingleFontFamily* fontName =
              fontList.list.IsEmpty() ? nullptr : &fontList.list.AsSpan()[0];
          nsAutoCString name;
          if (fontName) {
            fontName->AppendToString(name, false);
          }
          AppendUTF8toUTF16(name, fontRange->mFontName);

          ParentLayerToScreenScale2D cumulativeResolution =
              ParentLayerToParentLayerScale(
                  frame->PresShell()->GetCumulativeResolution()) *
              nsLayoutUtils::
                  GetTransformToAncestorScaleCrossProcessForFrameMetrics(frame);

          float scale = std::max(cumulativeResolution.xScale,
                                 cumulativeResolution.yScale);

          fontRange->mFontSize = frame->PresContext()->CSSPixelsToDevPixels(
              font.size.ToCSSPixels() * scale);
        }
      }
      baseOffset += GetBRLength(aLineBreakType);
    }
  }

  aLength = baseOffset;
  return NS_OK;
}

nsresult ContentEventHandler::ExpandToClusterBoundary(
    Text& aTextNode, bool aForward, uint32_t* aXPOffset) const {
  // XXX This method assumes that the frame boundaries must be cluster
  // boundaries. It's false, but no problem now, maybe.
  if (*aXPOffset == 0 || *aXPOffset == aTextNode.TextLength()) {
    return NS_OK;
  }

  NS_ASSERTION(*aXPOffset <= aTextNode.TextLength(), "offset is out of range.");

  MOZ_DIAGNOSTIC_ASSERT(mDocument->GetPresShell());
  int32_t offsetInFrame;
  CaretAssociationHint hint =
      aForward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* frame = nsFrameSelection::GetFrameForNodeOffset(
      &aTextNode, int32_t(*aXPOffset), hint, &offsetInFrame);
  if (frame) {
    auto [startOffset, endOffset] = frame->GetOffsets();
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
    if (textFrame->PeekOffsetCharacter(aForward, &newOffsetInFrame, options) ==
        nsIFrame::FOUND) {
      *aXPOffset = startOffset + newOffsetInFrame;
      return NS_OK;
    }
  }

  // If the frame isn't available, we only can check surrogate pair...
  if (aTextNode.TextFragment().IsLowSurrogateFollowingHighSurrogateAt(
          *aXPOffset)) {
    *aXPOffset += aForward ? 1 : -1;
  }
  return NS_OK;
}

nsresult ContentEventHandler::SetRawRangeFromFlatTextOffset(
    RawRange* aRawRange, uint32_t aOffset, uint32_t aLength,
    LineBreakType aLineBreakType, bool aExpandToClusterBoundaries,
    uint32_t* aNewOffset, Text** aLastTextNode) {
  if (aNewOffset) {
    *aNewOffset = aOffset;
  }
  if (aLastTextNode) {
    *aLastTextNode = nullptr;
  }

  // Special case like <br contenteditable>
  if (!mRootContent->HasChildren()) {
    nsresult rv = aRawRange->CollapseTo(RawRangeBoundary(mRootContent, 0u));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  PreContentIterator preOrderIter;
  nsresult rv = preOrderIter.Init(mRootContent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t offset = 0;
  uint32_t endOffset = aOffset + aLength;
  bool startSet = false;
  for (; !preOrderIter.IsDone(); preOrderIter.Next()) {
    nsINode* node = preOrderIter.GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    // FYI: mRootContent shouldn't cause any text. So, we can skip it simply.
    if (node == mRootContent || !node->IsContent()) {
      continue;
    }
    nsIContent* const content = node->AsContent();
    Text* const contentAsText = Text::FromNode(content);

    if (aLastTextNode && contentAsText) {
      NS_IF_RELEASE(*aLastTextNode);
      NS_ADDREF(*aLastTextNode = contentAsText);
    }

    uint32_t textLength = contentAsText
                              ? GetTextLength(*contentAsText, aLineBreakType)
                              : (ShouldBreakLineBefore(*content, mRootContent)
                                     ? GetBRLength(aLineBreakType)
                                     : 0);
    if (!textLength) {
      continue;
    }

    // When the start offset is in between accumulated offset and the last
    // offset of the node, the node is the start node of the range.
    if (!startSet && aOffset <= offset + textLength) {
      nsINode* startNode = nullptr;
      int32_t startNodeOffset = -1;
      if (contentAsText) {
        // Rule #1.1: [textNode or text[Node or textNode[
        uint32_t xpOffset = aOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          xpOffset = ConvertToXPOffset(*contentAsText, xpOffset);
        }

        if (aExpandToClusterBoundaries) {
          const uint32_t oldXPOffset = xpOffset;
          nsresult rv =
              ExpandToClusterBoundary(*contentAsText, false, &xpOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          if (aNewOffset) {
            // This is correct since a cluster shouldn't include line break.
            *aNewOffset -= (oldXPOffset - xpOffset);
          }
        }
        startNode = contentAsText;
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
      MOZ_ASSERT(startSet, "The start of the range should've been set already");
      if (contentAsText) {
        // Rule #2.1: ]textNode or text]Node or textNode]
        uint32_t xpOffset = endOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          const uint32_t xpOffsetCurrent =
              ConvertToXPOffset(*contentAsText, xpOffset);
          if (xpOffset && GetBRLength(aLineBreakType) > 1) {
            MOZ_ASSERT(GetBRLength(aLineBreakType) == 2);
            const uint32_t xpOffsetPre =
                ConvertToXPOffset(*contentAsText, xpOffset - 1);
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
          nsresult rv =
              ExpandToClusterBoundary(*contentAsText, true, &xpOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        NS_ASSERTION(xpOffset <= INT32_MAX, "The end node offset is too large");
        nsresult rv = aRawRange->SetEnd(contentAsText, xpOffset);
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
        MOZ_ASSERT(false,
                   "This case should've already been handled at "
                   "the last node which caused some text");
        return NS_ERROR_FAILURE;
      }

      if (content->HasChildren() &&
          ShouldBreakLineBefore(*content, mRootContent)) {
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

/* static */
LineBreakType ContentEventHandler::GetLineBreakType(
    WidgetQueryContentEvent* aEvent) {
  return GetLineBreakType(aEvent->mUseNativeLineBreak);
}

/* static */
LineBreakType ContentEventHandler::GetLineBreakType(
    WidgetSelectionEvent* aEvent) {
  return GetLineBreakType(aEvent->mUseNativeLineBreak);
}

/* static */
LineBreakType ContentEventHandler::GetLineBreakType(bool aUseNativeLineBreak) {
  return aUseNativeLineBreak ? LINE_BREAK_TYPE_NATIVE : LINE_BREAK_TYPE_XP;
}

nsresult ContentEventHandler::HandleQueryContentEvent(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
  switch (aEvent->mMessage) {
    case eQuerySelectedText:
      rv = OnQuerySelectedText(aEvent);
      break;
    case eQueryTextContent:
      rv = OnQueryTextContent(aEvent);
      break;
    case eQueryCaretRect:
      rv = OnQueryCaretRect(aEvent);
      break;
    case eQueryTextRect:
      rv = OnQueryTextRect(aEvent);
      break;
    case eQueryTextRectArray:
      rv = OnQueryTextRectArray(aEvent);
      break;
    case eQueryEditorRect:
      rv = OnQueryEditorRect(aEvent);
      break;
    case eQueryContentState:
      rv = OnQueryContentState(aEvent);
      break;
    case eQuerySelectionAsTransferable:
      rv = OnQuerySelectionAsTransferable(aEvent);
      break;
    case eQueryCharacterAtPoint:
      rv = OnQueryCharacterAtPoint(aEvent);
      break;
    case eQueryDOMWidgetHittest:
      rv = OnQueryDOMWidgetHittest(aEvent);
      break;
    default:
      break;
  }
  if (NS_FAILED(rv)) {
    aEvent->mReply.reset();  // Mark the query failed.
    return rv;
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

// Similar to nsFrameSelection::GetFrameForNodeOffset,
// but this is more flexible for OnQueryTextRect to use
static nsresult GetFrameForTextRect(const nsINode* aNode, int32_t aNodeOffset,
                                    bool aHint, nsIFrame** aReturnFrame) {
  NS_ENSURE_TRUE(aNode && aNode->IsContent(), NS_ERROR_UNEXPECTED);
  nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  int32_t childNodeOffset = 0;
  return frame->GetChildFrameContainingOffset(aNodeOffset, aHint,
                                              &childNodeOffset, aReturnFrame);
}

nsresult ContentEventHandler::OnQuerySelectedText(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(aEvent->mReply->mOffsetAndData.isNothing());

  if (!mFirstSelectedRawRange.IsPositioned()) {
    MOZ_ASSERT(aEvent->mInput.mSelectionType != SelectionType::eNormal);
    MOZ_ASSERT(aEvent->mReply->mOffsetAndData.isNothing());
    MOZ_ASSERT(!aEvent->mReply->mHasSelection);
    // This is special case that `mReply` is emplaced, but mOffsetAndData is
    // not emplaced but treated as succeeded because of no selection ranges
    // is a usual case.
    return NS_OK;
  }

  nsINode* const startNode = mFirstSelectedRawRange.GetStartContainer();
  nsINode* const endNode = mFirstSelectedRawRange.GetEndContainer();

  // Make sure the selection is within the root content range.
  if (!startNode->IsInclusiveDescendantOf(mRootContent) ||
      !endNode->IsInclusiveDescendantOf(mRootContent)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  uint32_t startOffset = 0;
  if (NS_WARN_IF(NS_FAILED(GetStartOffset(mFirstSelectedRawRange, &startOffset,
                                          lineBreakType)))) {
    return NS_ERROR_FAILURE;
  }

  const RangeBoundary& anchorRef = mSelection->RangeCount() > 0
                                       ? mSelection->AnchorRef()
                                       : mFirstSelectedRawRange.Start();
  const RangeBoundary& focusRef = mSelection->RangeCount() > 0
                                      ? mSelection->FocusRef()
                                      : mFirstSelectedRawRange.End();
  if (NS_WARN_IF(!anchorRef.IsSet()) || NS_WARN_IF(!focusRef.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  if (mSelection->RangeCount()) {
    // If there is only one selection range, the anchor/focus node and offset
    // are the information of the range.  Therefore, we have the direction
    // information.
    if (mSelection->RangeCount() == 1) {
      // The selection's points should always be comparable, independent of the
      // selection (see nsISelectionController.idl).
      Maybe<int32_t> compare =
          nsContentUtils::ComparePoints(anchorRef, focusRef);
      if (compare.isNothing()) {
        return NS_ERROR_FAILURE;
      }

      aEvent->mReply->mReversed = compare.value() > 0;
    }
    // However, if there are 2 or more selection ranges, we have no information
    // of that.
    else {
      aEvent->mReply->mReversed = false;
    }

    nsString selectedString;
    if (!mFirstSelectedRawRange.Collapsed() &&
        NS_WARN_IF(NS_FAILED(GenerateFlatTextContent(
            mFirstSelectedRawRange, selectedString, lineBreakType)))) {
      return NS_ERROR_FAILURE;
    }
    aEvent->mReply->mOffsetAndData.emplace(startOffset, selectedString,
                                           OffsetAndDataFor::SelectedString);
  } else {
    NS_ASSERTION(anchorRef == focusRef,
                 "When mSelection doesn't have selection, "
                 "mFirstSelectedRawRange must be collapsed");

    aEvent->mReply->mReversed = false;
    aEvent->mReply->mOffsetAndData.emplace(startOffset, EmptyString(),
                                           OffsetAndDataFor::SelectedString);
  }

  nsIFrame* frame = nullptr;
  rv = GetFrameForTextRect(
      focusRef.Container(),
      focusRef.Offset(RangeBoundary::OffsetFilter::kValidOffsets).valueOr(0),
      true, &frame);
  if (NS_SUCCEEDED(rv) && frame) {
    aEvent->mReply->mWritingMode = frame->GetWritingMode();
  } else {
    aEvent->mReply->mWritingMode = WritingMode();
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryTextContent(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(aEvent->mReply->mOffsetAndData.isNothing());

  LineBreakType lineBreakType = GetLineBreakType(aEvent);

  RawRange rawRange;
  uint32_t startOffset = 0;
  if (NS_WARN_IF(NS_FAILED(SetRawRangeFromFlatTextOffset(
          &rawRange, aEvent->mInput.mOffset, aEvent->mInput.mLength,
          lineBreakType, false, &startOffset)))) {
    return NS_ERROR_FAILURE;
  }

  nsString textInRange;
  if (NS_WARN_IF(NS_FAILED(
          GenerateFlatTextContent(rawRange, textInRange, lineBreakType)))) {
    return NS_ERROR_FAILURE;
  }

  aEvent->mReply->mOffsetAndData.emplace(startOffset, textInRange,
                                         OffsetAndDataFor::EditorString);

  if (aEvent->mWithFontRanges) {
    uint32_t fontRangeLength;
    if (NS_WARN_IF(NS_FAILED(
            GenerateFlatFontRanges(rawRange, aEvent->mReply->mFontRanges,
                                   fontRangeLength, lineBreakType)))) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(fontRangeLength == aEvent->mReply->DataLength(),
               "Font ranges doesn't match the string");
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

void ContentEventHandler::EnsureNonEmptyRect(nsRect& aRect) const {
  // See the comment in ContentEventHandler.h why this doesn't set them to
  // one device pixel.
  aRect.height = std::max(1, aRect.height);
  aRect.width = std::max(1, aRect.width);
}

void ContentEventHandler::EnsureNonEmptyRect(LayoutDeviceIntRect& aRect) const {
  aRect.height = std::max(1, aRect.height);
  aRect.width = std::max(1, aRect.width);
}

ContentEventHandler::FrameAndNodeOffset
ContentEventHandler::GetFirstFrameInRangeForTextRect(
    const RawRange& aRawRange) {
  NodePosition nodePosition;
  PreContentIterator preOrderIter;
  nsresult rv =
      preOrderIter.Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FrameAndNodeOffset();
  }
  for (; !preOrderIter.IsDone(); preOrderIter.Next()) {
    nsINode* node = preOrderIter.GetCurrentNode();
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
        nodePosition = {node, offsetInNode};
        break;
      }
      continue;
    }

    // If the element node causes a line break before it, it's the first
    // node causing text.
    if (ShouldBreakLineBefore(*node->AsContent(), mRootContent) ||
        IsPaddingBR(*node->AsContent())) {
      nodePosition = {node, 0};
    }
  }

  if (!nodePosition.IsSetAndValid()) {
    return FrameAndNodeOffset();
  }

  nsIFrame* firstFrame = nullptr;
  GetFrameForTextRect(
      nodePosition.Container(),
      *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets), true,
      &firstFrame);
  return FrameAndNodeOffset(
      firstFrame,
      *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets));
}

ContentEventHandler::FrameAndNodeOffset
ContentEventHandler::GetLastFrameInRangeForTextRect(const RawRange& aRawRange) {
  NodePosition nodePosition;
  PreContentIterator preOrderIter;
  nsresult rv =
      preOrderIter.Init(aRawRange.Start().AsRaw(), aRawRange.End().AsRaw());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FrameAndNodeOffset();
  }

  const RangeBoundary& endPoint = aRawRange.End();
  MOZ_ASSERT(endPoint.IsSetAndValid());
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
  if (endPoint.Container()->IsText()) {
    // Don't set nextNodeOfRangeEnd to the start node of aRawRange because if
    // the container of the end is same as start node of the range, the text
    // node shouldn't be next of range end even if the offset is 0.  This
    // could occur with empty text node.
    if (endPoint.IsStartOfContainer() &&
        aRawRange.GetStartContainer() != endPoint.Container()) {
      nextNodeOfRangeEnd = endPoint.Container();
    }
  } else if (endPoint.IsSetAndValid()) {
    nextNodeOfRangeEnd = endPoint.GetChildAtOffset();
  }

  for (preOrderIter.Last(); !preOrderIter.IsDone(); preOrderIter.Prev()) {
    nsINode* node = preOrderIter.GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }

    if (!node->IsContent() || node == nextNodeOfRangeEnd) {
      continue;
    }

    if (node->IsText()) {
      CheckedInt<int32_t> offset;
      if (node == aRawRange.GetEndContainer()) {
        offset = aRawRange.EndOffset();
      } else {
        offset = node->Length();
      }

      nodePosition = {node, offset.value()};

      // If the text node is empty or the last node of the range but the index
      // is 0, we should store current position but continue looking for
      // previous node (If there are no nodes before it, we should use current
      // node position for returning its frame).
      if (*nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets) ==
          0) {
        continue;
      }
      break;
    }

    if (ShouldBreakLineBefore(*node->AsContent(), mRootContent) ||
        IsPaddingBR(*node->AsContent())) {
      nodePosition = {node, 0};
      break;
    }
  }

  if (!nodePosition.IsSet()) {
    return FrameAndNodeOffset();
  }

  nsIFrame* lastFrame = nullptr;
  GetFrameForTextRect(
      nodePosition.Container(),
      *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets), true,
      &lastFrame);
  if (!lastFrame) {
    return FrameAndNodeOffset();
  }

  // If the last frame is a text frame, we need to check if the range actually
  // includes at least one character in the range.  Therefore, if it's not a
  // text frame, we need to do nothing anymore.
  if (!lastFrame->IsTextFrame()) {
    return FrameAndNodeOffset(
        lastFrame,
        *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets));
  }

  int32_t start = lastFrame->GetOffsets().first;

  // If the start offset in the node is same as the computed offset in the
  // node and it's not 0, the frame shouldn't be added to the text rect.  So,
  // this should return previous text frame and its last offset if there is
  // at least one text frame.
  if (*nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets) &&
      *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets) ==
          static_cast<uint32_t>(start)) {
    const CheckedInt<int32_t> newNodePositionOffset{
        *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets) - 1};

    nodePosition = {nodePosition.Container(), newNodePositionOffset.value()};
    GetFrameForTextRect(
        nodePosition.Container(),
        *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets), true,
        &lastFrame);
    if (NS_WARN_IF(!lastFrame)) {
      return FrameAndNodeOffset();
    }
  }

  return FrameAndNodeOffset(
      lastFrame,
      *nodePosition.Offset(NodePosition::OffsetFilter::kValidOffsets));
}

ContentEventHandler::FrameRelativeRect
ContentEventHandler::GetLineBreakerRectBefore(nsIFrame* aFrame) {
  // Note that this method should be called only with an element's frame whose
  // open tag causes a line break or moz-<br> for computing empty last line's
  // rect.
  MOZ_ASSERT(aFrame->GetContent());
  MOZ_ASSERT(ShouldBreakLineBefore(*aFrame->GetContent(), mRootContent) ||
             IsPaddingBR(*aFrame->GetContent()));

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
ContentEventHandler::GuessLineBreakerRectAfter(const Text& aTextNode) {
  FrameRelativeRect result;
  const int32_t length = static_cast<int32_t>(aTextNode.TextLength());
  if (NS_WARN_IF(length < 0)) {
    return result;
  }
  // Get the last nsTextFrame which is caused by aTextNode.  Note that
  // a text node can cause multiple text frames, e.g., the text is too long
  // and wrapped by its parent block or the text has line breakers and its
  // white-space property respects the line breakers (e.g., |pre|).
  nsIFrame* lastTextFrame = nullptr;
  nsresult rv = GetFrameForTextRect(&aTextNode, length, true, &lastTextFrame);
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!lastTextFrame)) {
    return result;
  }
  const nsRect kLastTextFrameRect = lastTextFrame->GetRect();
  if (lastTextFrame->GetWritingMode().IsVertical()) {
    // Below of the last text frame.
    result.mRect.SetRect(0, kLastTextFrameRect.height, kLastTextFrameRect.width,
                         0);
  } else {
    // Right of the last text frame (not bidi-aware).
    result.mRect.SetRect(kLastTextFrameRect.width, 0, 0,
                         kLastTextFrameRect.height);
  }
  result.mBaseFrame = lastTextFrame;
  return result;
}

ContentEventHandler::FrameRelativeRect
ContentEventHandler::GuessFirstCaretRectIn(nsIFrame* aFrame) {
  const WritingMode kWritingMode = aFrame->GetWritingMode();
  nsPresContext* presContext = aFrame->PresContext();

  // Computes the font height, but if it's not available, we should use
  // default font size of Firefox.  The default font size in default settings
  // is 16px.
  RefPtr<nsFontMetrics> fontMetrics =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);
  const nscoord kMaxHeight = fontMetrics
                                 ? fontMetrics->MaxHeight()
                                 : 16 * presContext->AppUnitsPerDevPixel();

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

nsresult ContentEventHandler::OnQueryTextRectArray(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = Init(aEvent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(aEvent->mReply->mOffsetAndData.isNothing());

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
    RefPtr<Text> lastTextNode;
    RawRange rawRange;
    nsresult rv =
        SetRawRangeFromFlatTextOffset(&rawRange, offset, 1, lineBreakType, true,
                                      nullptr, getter_AddRefs(lastTextNode));
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
      AppendSubString(chars, *firstContent->AsText(), firstFrame.mOffsetInNode,
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
        nsresult rv = SetRawRangeFromFlatTextOffset(&rawRangeToPrevOffset,
                                                    aEvent->mInput.mOffset - 1,
                                                    1, lineBreakType, true);
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
    else if (ShouldBreakLineBefore(*firstContent, mRootContent) ||
             IsPaddingBR(*firstContent)) {
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
      else if (!firstFrame->IsBrFrame() && lastTextNode) {
        FrameRelativeRect brRectRelativeToLastTextFrame =
            GuessLineBreakerRectAfter(*lastTextNode);
        if (NS_WARN_IF(!brRectRelativeToLastTextFrame.IsValid())) {
          return NS_ERROR_FAILURE;
        }
        // Look for the last text frame for lastTextNode.
        nsIFrame* primaryFrame = lastTextNode->GetPrimaryFrame();
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
        nsresult rv = SetRawRangeFromFlatTextOffset(
            &rawRange, aEvent->mInput.mOffset - 1, 1, lineBreakType, true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NS_ERROR_UNEXPECTED;
        }
        FrameAndNodeOffset frameForPrevious =
            GetFirstFrameInRangeForTextRect(rawRange);
        startsBetweenLineBreaker = frameForPrevious.mFrame == firstFrame.mFrame;
      }
    } else {
      NS_WARNING(
          "The frame is neither a text frame nor a frame whose content "
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

      nsPresContext* presContext = baseFrame->PresContext();
      rect = LayoutDeviceIntRect::FromAppUnitsToOutside(
          charRect, presContext->AppUnitsPerDevPixel());
      if (nsPresContext* rootContext =
              presContext->GetInProcessRootContentDocumentPresContext()) {
        rect = RoundedOut(ViewportUtils::DocumentRelativeLayoutToVisual(
            rect, rootContext->PresShell()));
      }
      // Returning empty rect may cause native IME confused, let's make sure to
      // return non-empty rect.
      EnsureNonEmptyRect(rect);

      aEvent->mReply->mRectArray.AppendElement(rect);
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
      aEvent->mReply->mRectArray.AppendElement(rect);
      offset++;
    }
  }

  // If the query range is longer than actual content length, we should append
  // caret rect at the end of the content as the last character rect because
  // native IME may want to query character rect at the end of contents for
  // deciding the position of a popup window (e.g., suggest window for next
  // word).  Note that when this method hasn't appended character rects, it
  // means that the offset is too large or the query range is collapsed.
  if (offset < kEndOffset || aEvent->mReply->mRectArray.IsEmpty()) {
    // If we've already retrieved some character rects before current offset,
    // we can guess the last rect from the last character's rect unless it's a
    // line breaker.  (If it's a line breaker, the caret rect is in next line.)
    if (!aEvent->mReply->mRectArray.IsEmpty() && !wasLineBreaker) {
      rect = aEvent->mReply->mRectArray.LastElement();
      if (isVertical) {
        rect.y = rect.YMost() + 1;
        rect.height = 1;
        MOZ_ASSERT(rect.width);
      } else {
        rect.x = rect.XMost() + 1;
        rect.width = 1;
        MOZ_ASSERT(rect.height);
      }
      aEvent->mReply->mRectArray.AppendElement(rect);
    } else {
      // Note that don't use eQueryCaretRect here because if caret is at the
      // end of the content, it returns actual caret rect instead of computing
      // the rect itself.  It means that the result depends on caret position.
      // So, we shouldn't use it for consistency result in automated tests.
      WidgetQueryContentEvent queryTextRectEvent(eQueryTextRect, *aEvent);
      WidgetQueryContentEvent::Options options(*aEvent);
      queryTextRectEvent.InitForQueryTextRect(offset, 1, options);
      if (NS_WARN_IF(NS_FAILED(OnQueryTextRect(&queryTextRectEvent))) ||
          NS_WARN_IF(queryTextRectEvent.Failed())) {
        return NS_ERROR_FAILURE;
      }
      if (queryTextRectEvent.mReply->mWritingMode.IsVertical()) {
        queryTextRectEvent.mReply->mRect.height = 1;
      } else {
        queryTextRectEvent.mReply->mRect.width = 1;
      }
      aEvent->mReply->mRectArray.AppendElement(
          queryTextRectEvent.mReply->mRect);
    }
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryTextRect(WidgetQueryContentEvent* aEvent) {
  // If mLength is 0 (this may be caused by bug of native IME), we should
  // redirect this event to OnQueryCaretRect().
  if (!aEvent->mInput.mLength) {
    return OnQueryCaretRect(aEvent);
  }

  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(aEvent->mReply->mOffsetAndData.isNothing());

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  RawRange rawRange;
  RefPtr<Text> lastTextNode;
  uint32_t startOffset = 0;
  if (NS_WARN_IF(NS_FAILED(SetRawRangeFromFlatTextOffset(
          &rawRange, aEvent->mInput.mOffset, aEvent->mInput.mLength,
          lineBreakType, true, &startOffset, getter_AddRefs(lastTextNode))))) {
    return NS_ERROR_FAILURE;
  }
  nsString string;
  if (NS_WARN_IF(NS_FAILED(
          GenerateFlatTextContent(rawRange, string, lineBreakType)))) {
    return NS_ERROR_FAILURE;
  }
  aEvent->mReply->mOffsetAndData.emplace(startOffset, string,
                                         OffsetAndDataFor::EditorString);

  // used to iterate over all contents and their frames
  PostContentIterator postOrderIter;
  rv = postOrderIter.Init(rawRange.Start().AsRaw(), rawRange.End().AsRaw());
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
        const Text* textNode = Text::FromNode(lastFrame->GetContent());
        MOZ_ASSERT(textNode);
        if (textNode) {
          relativeRect = GuessLineBreakerRectAfter(*textNode);
        }
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
      aEvent->mReply->mWritingMode = lastFrame->GetWritingMode();
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
      aEvent->mReply->mWritingMode = rootContentFrame->GetWritingMode();
    }
    aEvent->mReply->mRect = LayoutDeviceIntRect::FromAppUnitsToOutside(
        rect, presContext->AppUnitsPerDevPixel());
    if (nsPresContext* rootContext =
            presContext->GetInProcessRootContentDocumentPresContext()) {
      aEvent->mReply->mRect =
          RoundedOut(ViewportUtils::DocumentRelativeLayoutToVisual(
              aEvent->mReply->mRect, rootContext->PresShell()));
    }
    EnsureNonEmptyRect(aEvent->mReply->mRect);

    MOZ_ASSERT(aEvent->Succeeded());
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
  else if (!firstFrame->IsBrFrame() && lastTextNode) {
    FrameRelativeRect brRectAfterLastChar =
        GuessLineBreakerRectAfter(*lastTextNode);
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
        postOrderIter.Next();
        nsINode* node = postOrderIter.GetCurrentNode();
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
      } while (!frame && !postOrderIter.IsDone());
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

  nsPresContext* presContext = lastFrame->PresContext();
  aEvent->mReply->mRect = LayoutDeviceIntRect::FromAppUnitsToOutside(
      rect, presContext->AppUnitsPerDevPixel());
  if (nsPresContext* rootContext =
          presContext->GetInProcessRootContentDocumentPresContext()) {
    aEvent->mReply->mRect =
        RoundedOut(ViewportUtils::DocumentRelativeLayoutToVisual(
            aEvent->mReply->mRect, rootContext->PresShell()));
  }
  // Returning empty rect may cause native IME confused, let's make sure to
  // return non-empty rect.
  EnsureNonEmptyRect(aEvent->mReply->mRect);
  aEvent->mReply->mWritingMode = lastFrame->GetWritingMode();

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryEditorRect(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_WARN_IF(NS_FAILED(QueryContentRect(mRootContent, aEvent)))) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryCaretRect(
    WidgetQueryContentEvent* aEvent) {
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
      rv = GetStartOffset(mFirstSelectedRawRange, &offset,
                          GetLineBreakType(aEvent));
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == aEvent->mInput.mOffset) {
        rv = ConvertToRootRelativeOffset(caretFrame, caretRect);
        NS_ENSURE_SUCCESS(rv, rv);
        nsPresContext* presContext = caretFrame->PresContext();
        aEvent->mReply->mRect = LayoutDeviceIntRect::FromAppUnitsToOutside(
            caretRect, presContext->AppUnitsPerDevPixel());
        if (nsPresContext* rootContext =
                presContext->GetInProcessRootContentDocumentPresContext()) {
          aEvent->mReply->mRect =
              RoundedOut(ViewportUtils::DocumentRelativeLayoutToVisual(
                  aEvent->mReply->mRect, rootContext->PresShell()));
        }
        // Returning empty rect may cause native IME confused, let's make sure
        // to return non-empty rect.
        EnsureNonEmptyRect(aEvent->mReply->mRect);
        aEvent->mReply->mWritingMode = caretFrame->GetWritingMode();
        aEvent->mReply->mOffsetAndData.emplace(
            aEvent->mInput.mOffset, EmptyString(),
            OffsetAndDataFor::SelectedString);

        MOZ_ASSERT(aEvent->Succeeded());
        return NS_OK;
      }
    }
  }

  // Otherwise, we should guess the caret rect from the character's rect.
  WidgetQueryContentEvent queryTextRectEvent(eQueryTextRect, *aEvent);
  WidgetQueryContentEvent::Options options(*aEvent);
  queryTextRectEvent.InitForQueryTextRect(aEvent->mInput.mOffset, 1, options);
  if (NS_WARN_IF(NS_FAILED(OnQueryTextRect(&queryTextRectEvent))) ||
      NS_WARN_IF(queryTextRectEvent.Failed())) {
    return NS_ERROR_FAILURE;
  }
  queryTextRectEvent.mReply->TruncateData();
  aEvent->mReply->mOffsetAndData =
      std::move(queryTextRectEvent.mReply->mOffsetAndData);
  aEvent->mReply->mRect = std::move(queryTextRectEvent.mReply->mRect);
  aEvent->mReply->mWritingMode =
      std::move(queryTextRectEvent.mReply->mWritingMode);
  if (aEvent->mReply->WritingModeRef().IsVertical()) {
    aEvent->mReply->mRect.height = 1;
  } else {
    aEvent->mReply->mRect.width = 1;
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryContentState(
    WidgetQueryContentEvent* aEvent) {
  if (NS_FAILED(Init(aEvent))) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(aEvent->mReply.isSome());
  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQuerySelectionAsTransferable(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(aEvent->mReply.isSome());

  if (!aEvent->mReply->mHasSelection) {
    MOZ_ASSERT(!aEvent->mReply->mTransferable);
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(nsCopySupport::GetTransferableForSelection(
          mSelection, mDocument,
          getter_AddRefs(aEvent->mReply->mTransferable))))) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryCharacterAtPoint(
    WidgetQueryContentEvent* aEvent) {
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(aEvent->mReply->mOffsetAndData.isNothing());
  MOZ_ASSERT(aEvent->mReply->mTentativeCaretOffset.isNothing());

  PresShell* presShell = mDocument->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  nsIFrame* rootFrame = presShell->GetRootFrame();
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

  WidgetQueryContentEvent queryCharAtPointOnRootWidgetEvent(
      true, eQueryCharacterAtPoint, rootWidget);
  queryCharAtPointOnRootWidgetEvent.mUseNativeLineBreak =
      aEvent->mUseNativeLineBreak;
  queryCharAtPointOnRootWidgetEvent.mRefPoint = aEvent->mRefPoint;
  if (rootWidget != aEvent->mWidget) {
    queryCharAtPointOnRootWidgetEvent.mRefPoint +=
        aEvent->mWidget->WidgetToScreenOffset() -
        rootWidget->WidgetToScreenOffset();
  }
  nsPoint ptInRoot = nsLayoutUtils::GetEventCoordinatesRelativeTo(
      &queryCharAtPointOnRootWidgetEvent, RelativeTo{rootFrame});

  nsIFrame* targetFrame =
      nsLayoutUtils::GetFrameForPoint(RelativeTo{rootFrame}, ptInRoot);
  if (!targetFrame || !targetFrame->GetContent() ||
      !targetFrame->GetContent()->IsInclusiveDescendantOf(mRootContent)) {
    // There is no character at the point.
    MOZ_ASSERT(aEvent->Succeeded());
    return NS_OK;
  }
  nsPoint ptInTarget = ptInRoot + rootFrame->GetOffsetToCrossDoc(targetFrame);
  int32_t rootAPD = rootFrame->PresContext()->AppUnitsPerDevPixel();
  int32_t targetAPD = targetFrame->PresContext()->AppUnitsPerDevPixel();
  ptInTarget = ptInTarget.ScaleToOtherAppUnits(rootAPD, targetAPD);

  nsIFrame::ContentOffsets tentativeCaretOffsets =
      targetFrame->GetContentOffsetsFromPoint(ptInTarget);
  if (!tentativeCaretOffsets.content ||
      !tentativeCaretOffsets.content->IsInclusiveDescendantOf(mRootContent)) {
    // There is no character nor tentative caret point at the point.
    MOZ_ASSERT(aEvent->Succeeded());
    return NS_OK;
  }

  uint32_t tentativeCaretOffset = 0;
  if (NS_WARN_IF(NS_FAILED(GetFlatTextLengthInRange(
          NodePosition(mRootContent, 0), NodePosition(tentativeCaretOffsets),
          mRootContent, &tentativeCaretOffset, GetLineBreakType(aEvent))))) {
    return NS_ERROR_FAILURE;
  }

  aEvent->mReply->mTentativeCaretOffset.emplace(tentativeCaretOffset);
  if (!targetFrame->IsTextFrame()) {
    // There is no character at the point but there is tentative caret point.
    MOZ_ASSERT(aEvent->Succeeded());
    return NS_OK;
  }

  nsTextFrame* textframe = static_cast<nsTextFrame*>(targetFrame);
  nsIFrame::ContentOffsets contentOffsets =
      textframe->GetCharacterOffsetAtFramePoint(ptInTarget);
  NS_ENSURE_TRUE(contentOffsets.content, NS_ERROR_FAILURE);
  uint32_t offset = 0;
  if (NS_WARN_IF(NS_FAILED(GetFlatTextLengthInRange(
          NodePosition(mRootContent, 0), NodePosition(contentOffsets),
          mRootContent, &offset, GetLineBreakType(aEvent))))) {
    return NS_ERROR_FAILURE;
  }

  WidgetQueryContentEvent queryTextRectEvent(true, eQueryTextRect,
                                             aEvent->mWidget);
  WidgetQueryContentEvent::Options options(*aEvent);
  queryTextRectEvent.InitForQueryTextRect(offset, 1, options);
  if (NS_WARN_IF(NS_FAILED(OnQueryTextRect(&queryTextRectEvent))) ||
      NS_WARN_IF(queryTextRectEvent.Failed())) {
    return NS_ERROR_FAILURE;
  }

  aEvent->mReply->mOffsetAndData =
      std::move(queryTextRectEvent.mReply->mOffsetAndData);
  aEvent->mReply->mRect = queryTextRectEvent.mReply->mRect;

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

nsresult ContentEventHandler::OnQueryDOMWidgetHittest(
    WidgetQueryContentEvent* aEvent) {
  NS_ASSERTION(aEvent, "aEvent must not be null");

  nsresult rv = InitBasic();
  if (NS_FAILED(rv)) {
    return rv;
  }

  aEvent->mReply->mWidgetIsHit = false;

  NS_ENSURE_TRUE(aEvent->mWidget, NS_ERROR_FAILURE);

  PresShell* presShell = mDocument->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  nsIFrame* docFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(docFrame, NS_ERROR_FAILURE);

  LayoutDeviceIntPoint eventLoc =
      aEvent->mRefPoint + aEvent->mWidget->WidgetToScreenOffset();
  CSSIntRect docFrameRect = docFrame->GetScreenRect();
  CSSIntPoint eventLocCSS(
      docFrame->PresContext()->DevPixelsToIntCSSPixels(eventLoc.x) -
          docFrameRect.x,
      docFrame->PresContext()->DevPixelsToIntCSSPixels(eventLoc.y) -
          docFrameRect.y);

  if (Element* contentUnderMouse = mDocument->ElementFromPointHelper(
          eventLocCSS.x, eventLocCSS.y, false, false, ViewportType::Visual)) {
    if (nsIFrame* targetFrame = contentUnderMouse->GetPrimaryFrame()) {
      if (aEvent->mWidget == targetFrame->GetNearestWidget()) {
        aEvent->mReply->mWidgetIsHit = true;
      }
    }
  }

  MOZ_ASSERT(aEvent->Succeeded());
  return NS_OK;
}

/* static */
nsresult ContentEventHandler::GetFlatTextLengthInRange(
    const NodePosition& aStartPosition, const NodePosition& aEndPosition,
    nsIContent* aRootContent, uint32_t* aLength, LineBreakType aLineBreakType,
    bool aIsRemovingNode /* = false */) {
  if (NS_WARN_IF(!aRootContent) || NS_WARN_IF(!aStartPosition.IsSet()) ||
      NS_WARN_IF(!aEndPosition.IsSet()) || NS_WARN_IF(!aLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aStartPosition == aEndPosition) {
    *aLength = 0;
    return NS_OK;
  }

  PreContentIterator preOrderIter;

  // Working with ContentIterator, we may need to adjust the end position for
  // including it forcibly.
  NodePosition endPosition(aEndPosition);

  // This may be called for retrieving the text of removed nodes.  Even in this
  // case, the node thinks it's still in the tree because UnbindFromTree() will
  // be called after here.  However, the node was already removed from the
  // array of children of its parent.  So, be careful to handle this case.
  if (aIsRemovingNode) {
    DebugOnly<nsIContent*> parent = aStartPosition.Container()->GetParent();
    MOZ_ASSERT(
        parent && parent->ComputeIndexOf(aStartPosition.Container()) == -1,
        "At removing the node, the node shouldn't be in the array of children "
        "of its parent");
    MOZ_ASSERT(aStartPosition.Container() == endPosition.Container(),
               "At removing the node, start and end node should be same");
    MOZ_ASSERT(*aStartPosition.Offset(
                   NodePosition::OffsetFilter::kValidOrInvalidOffsets) == 0,
               "When the node is being removed, the start offset should be 0");
    MOZ_ASSERT(
        static_cast<uint32_t>(*endPosition.Offset(
            NodePosition::OffsetFilter::kValidOrInvalidOffsets)) ==
            endPosition.Container()->GetChildCount(),
        "When the node is being removed, the end offset should be child count");
    nsresult rv = preOrderIter.Init(aStartPosition.Container());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    RawRange prevRawRange;
    nsresult rv = prevRawRange.SetStart(aStartPosition.AsRaw());
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
        int32_t indexInParent =
            parentContent->ComputeIndexOf(endPosition.Container());
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
      rv = preOrderIter.Init(prevRawRange.Start().AsRaw(),
                             prevRawRange.End().AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (endPosition.Container() != aRootContent) {
      // Offset is past node's length; set end of range to end of node
      rv = prevRawRange.SetEndAfter(endPosition.Container());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = preOrderIter.Init(prevRawRange.Start().AsRaw(),
                             prevRawRange.End().AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // Offset is past the root node; set end of range to end of root node
      rv = preOrderIter.Init(aRootContent);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  *aLength = 0;
  for (; !preOrderIter.IsDone(); preOrderIter.Next()) {
    nsINode* node = preOrderIter.GetCurrentNode();
    if (NS_WARN_IF(!node)) {
      break;
    }
    if (!node->IsContent()) {
      continue;
    }
    nsIContent* content = node->AsContent();

    if (const Text* textNode = Text::FromNode(content)) {
      // Note: our range always starts from offset 0
      if (node == endPosition.Container()) {
        // NOTE: We should have an offset here, as endPosition.Container() is a
        // nsINode::eTEXT, which always has an offset.
        *aLength += GetTextLength(
            *textNode, aLineBreakType,
            *endPosition.Offset(
                NodePosition::OffsetFilter::kValidOrInvalidOffsets));
      } else {
        *aLength += GetTextLength(*textNode, aLineBreakType);
      }
    } else if (ShouldBreakLineBefore(*content, aRootContent)) {
      // If the start position is start of this node but doesn't include the
      // open tag, don't append the line break length.
      if (node == aStartPosition.Container() &&
          !aStartPosition.IsBeforeOpenTag()) {
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

nsresult ContentEventHandler::GetStartOffset(const RawRange& aRawRange,
                                             uint32_t* aOffset,
                                             LineBreakType aLineBreakType) {
  // To match the "no skip start" hack in ContentIterator::Init, when range
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
      startIsContainer ? NodePosition(startNode, aRawRange.StartOffset())
                       : NodePositionBefore(startNode, aRawRange.StartOffset());
  return GetFlatTextLengthInRange(NodePosition(mRootContent, 0), startPos,
                                  mRootContent, aOffset, aLineBreakType);
}

nsresult ContentEventHandler::AdjustCollapsedRangeMaybeIntoTextNode(
    RawRange& aRawRange) {
  MOZ_ASSERT(aRawRange.Collapsed());

  if (!aRawRange.Collapsed()) {
    return NS_ERROR_INVALID_ARG;
  }

  const RangeBoundary& startPoint = aRawRange.Start();
  if (NS_WARN_IF(!startPoint.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // If the node does not have children like a text node, we don't need to
  // modify aRawRange.
  if (!startPoint.Container()->HasChildren()) {
    return NS_OK;
  }

  // If the container is not a text node but it has a text node at the offset,
  // we should adjust the range into the text node.
  // NOTE: This is emulating similar situation of EditorBase.
  if (startPoint.IsStartOfContainer()) {
    // If the range is the start of the container, adjusted the range to the
    // start of the first child.
    if (!startPoint.Container()->GetFirstChild()->IsText()) {
      return NS_OK;
    }
    nsresult rv = aRawRange.CollapseTo(
        RawRangeBoundary(startPoint.Container()->GetFirstChild(), 0u));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!startPoint.IsSetAndValid()) {
    return NS_OK;
  }

  // If start of the range is next to a child node, adjust the range to the
  // end of the previous child (i.e., startPoint.Ref()).
  if (!startPoint.Ref()->IsText()) {
    return NS_OK;
  }
  nsresult rv = aRawRange.CollapseTo(
      RawRangeBoundary(startPoint.Ref(), startPoint.Ref()->Length()));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult ContentEventHandler::ConvertToRootRelativeOffset(nsIFrame* aFrame,
                                                          nsRect& aRect) {
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

static void AdjustRangeForSelection(nsIContent* aRoot, nsINode** aNode,
                                    int32_t* aNodeOffset) {
  nsINode* node = *aNode;
  int32_t nodeOffset = *aNodeOffset;
  if (aRoot == node || NS_WARN_IF(!node->GetParent()) || !node->IsText()) {
    return;
  }

  // When the offset is at the end of the text node, set it to after the
  // text node, to make sure the caret is drawn on a new line when the last
  // character of the text node is '\n' in <textarea>.
  int32_t textLength = static_cast<int32_t>(node->AsContent()->TextLength());
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

nsresult ContentEventHandler::OnSelectionEvent(WidgetSelectionEvent* aEvent) {
  aEvent->mSucceeded = false;

  // Get selection to manipulate
  // XXX why do we need to get them from ISM? This method should work fine
  //     without ISM.
  RefPtr<Selection> sel;
  nsresult rv = IMEStateManager::GetFocusSelectionAndRoot(
      getter_AddRefs(sel), getter_AddRefs(mRootContent));
  mSelection = sel;
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = Init(aEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get range from offset and length
  RawRange rawRange;
  rv = SetRawRangeFromFlatTextOffset(&rawRange, aEvent->mOffset,
                                     aEvent->mLength, GetLineBreakType(aEvent),
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

  if (aEvent->mReversed) {
    nsCOMPtr<nsINode> startNodeStrong(startNode);
    nsCOMPtr<nsINode> endNodeStrong(endNode);
    ErrorResult error;
    MOZ_KnownLive(mSelection)
        ->SetBaseAndExtentInLimiter(*endNodeStrong, endNodeOffset,
                                    *startNodeStrong, startNodeOffset, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  } else {
    nsCOMPtr<nsINode> startNodeStrong(startNode);
    nsCOMPtr<nsINode> endNodeStrong(endNode);
    ErrorResult error;
    MOZ_KnownLive(mSelection)
        ->SetBaseAndExtentInLimiter(*startNodeStrong, startNodeOffset,
                                    *endNodeStrong, endNodeOffset, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  // `ContentEventHandler` is a `MOZ_STACK_CLASS`, so `mSelection` is known to
  // be alive.
  MOZ_KnownLive(mSelection)
      ->ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                       ScrollAxis(), ScrollAxis(), 0);
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsRect ContentEventHandler::FrameRelativeRect::RectRelativeTo(
    nsIFrame* aDestFrame) const {
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
  nsRect baseFrameRectInRootFrame = nsLayoutUtils::TransformFrameRectToAncestor(
      mBaseFrame, nsRect(), rootFrame);
  nsRect destFrameRectInRootFrame = nsLayoutUtils::TransformFrameRectToAncestor(
      aDestFrame, nsRect(), rootFrame);
  nsPoint difference =
      destFrameRectInRootFrame.TopLeft() - baseFrameRectInRootFrame.TopLeft();
  return mRect - difference;
}

}  // namespace mozilla

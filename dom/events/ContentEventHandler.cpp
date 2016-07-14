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
#include "nsFocusManager.h"
#include "nsFontMetrics.h"
#include "nsFrameSelection.h"
#include "nsIContentIterator.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
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
  : mPresContext(aPresContext)
  , mPresShell(aPresContext->GetPresShell())
  , mSelection(nullptr)
  , mFirstSelectedRange(nullptr)
  , mRootContent(nullptr)
{
}

nsresult
ContentEventHandler::InitBasic()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_AVAILABLE);

  // If text frame which has overflowing selection underline is dirty,
  // we need to flush the pending reflow here.
  mPresShell->FlushPendingNotifications(Flush_Layout);

  // Flushing notifications can cause mPresShell to be destroyed (bug 577963).
  NS_ENSURE_TRUE(!mPresShell->IsDestroying(), NS_ERROR_FAILURE);

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
    nsresult rv =
      aNormalSelection->GetAncestorLimiter(getter_AddRefs(mRootContent));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_FAILURE;
    }
    if (!mRootContent) {
      mRootContent = mPresShell->GetDocument()->GetRootElement();
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
  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }

  // See bug 537041 comment 5, the range could have removed node.
  if (NS_WARN_IF(startNode->GetUncomposedDoc() != mPresShell->GetDocument())) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(startNode->GetUncomposedDoc() == endNode->GetUncomposedDoc(),
               "firstNormalSelectionRange crosses the document boundary");

  mRootContent = startNode->GetSelectionRootContent(mPresShell);
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
  mFirstSelectedRange = nullptr;
  mRootContent = nullptr;

  nsresult rv = InitBasic();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionController> selectionController =
    mPresShell->GetSelectionControllerForFocusedContent();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsCOMPtr<nsISelection> selection;
  rv = selectionController->GetSelection(ToRawSelectionType(aSelectionType),
                                         getter_AddRefs(selection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_UNEXPECTED;
  }

  mSelection = static_cast<Selection*>(selection.get());
  if (NS_WARN_IF(!mSelection)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<Selection> normalSelection;
  if (mSelection->Type() == SelectionType::eNormal) {
    normalSelection = mSelection;
  } else {
    nsCOMPtr<nsISelection> domSelection;
    nsresult rv =
      selectionController->GetSelection(
                             nsISelectionController::SELECTION_NORMAL,
                             getter_AddRefs(domSelection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_UNEXPECTED;
    }
    if (NS_WARN_IF(!domSelection)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    normalSelection = domSelection->AsSelection();
    if (NS_WARN_IF(!normalSelection)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  rv = InitRootContent(normalSelection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mSelection->RangeCount()) {
    mFirstSelectedRange = mSelection->GetRangeAt(0);
    if (NS_WARN_IF(!mFirstSelectedRange)) {
      return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
  }

  // Even if there are no selection ranges, it's usual case if aSelectionType
  // is a special selection.
  if (aSelectionType != SelectionType::eNormal) {
    MOZ_ASSERT(!mFirstSelectedRange);
    return NS_OK;
  }

  // But otherwise, we need to assume that there is a selection range at the
  // beginning of the root content if aSelectionType is eNormal.
  rv = nsRange::CreateRange(mRootContent, 0, mRootContent, 0,
                            getter_AddRefs(mFirstSelectedRange));
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!mFirstSelectedRange)) {
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
      rv = GetFlatTextLengthBefore(mFirstSelectedRange,
                                   &selectionStart, lineBreakType);
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
  nsIDocument* doc = mPresShell->GetDocument();
  if (!doc) {
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowOuter> window = doc->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  return nsFocusManager::GetFocusedDescendant(window, true,
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
  NS_PRECONDITION(aContent, "aContent must not be null");

  nsIFrame* frame = aContent->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // get rect for first frame
  nsRect resultRect(nsPoint(0, 0), frame->GetRect().Size());
  nsresult rv = ConvertToRootRelativeOffset(frame, resultRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // account for any additional frames
  while ((frame = frame->GetNextContinuation()) != nullptr) {
    nsRect frameRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    resultRect.UnionRect(resultRect, frameRect);
  }

  aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
      resultRect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel()));
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
#if defined(XP_WIN)
  aString.ReplaceSubstring(NS_LITERAL_STRING("\n"), NS_LITERAL_STRING("\r\n"));
#endif
}

static void AppendString(nsAString& aString, nsIContent* aContent)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
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
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
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
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
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
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
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
  if (NS_WARN_IF(!aContent->IsNodeOfType(nsINode::eTEXT))) {
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
  if (NS_WARN_IF(!aContent->IsNodeOfType(nsINode::eTEXT))) {
    return 0;
  }
  return GetTextLength(aContent, LINE_BREAK_TYPE_NATIVE, aMaxLength);
}

/* static */ uint32_t
ContentEventHandler::GetNativeTextLengthBefore(nsIContent* aContent,
                                               nsINode* aRootNode)
{
  if (NS_WARN_IF(aContent->IsNodeOfType(nsINode::eTEXT))) {
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
  MOZ_ASSERT(aContent->IsNodeOfType(nsINode::eTEXT));

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
ContentEventHandler::GenerateFlatTextContent(nsRange* aRange,
                                             nsAFlatString& aString,
                                             LineBreakType aLineBreakType)
{
  NS_ASSERTION(aString.IsEmpty(), "aString must be empty string");

  if (aRange->Collapsed()) {
    return NS_OK;
  }

  nsINode* startNode = aRange->GetStartParent();
  nsINode* endNode = aRange->GetEndParent();
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }

  if (startNode == endNode && startNode->IsNodeOfType(nsINode::eTEXT)) {
    nsIContent* content = startNode->AsContent();
    AppendSubString(aString, content, aRange->StartOffset(),
                    aRange->EndOffset() - aRange->StartOffset());
    ConvertToNativeNewlines(aString);
    return NS_OK;
  }

  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv = iter->Init(aRange);
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

    if (content->IsNodeOfType(nsINode::eTEXT)) {
      if (content == startNode) {
        AppendSubString(aString, content, aRange->StartOffset(),
                        content->TextLength() - aRange->StartOffset());
      } else if (content == endNode) {
        AppendSubString(aString, content, 0, aRange->EndOffset());
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
  MOZ_ASSERT(aContent->IsNodeOfType(nsINode::eTEXT));

  return aLineBreakType == LINE_BREAK_TYPE_NATIVE ?
    GetNativeTextLength(aContent, aXPStartOffset, aXPEndOffset) :
    aXPEndOffset - aXPStartOffset;
}

/* static */ void
ContentEventHandler::AppendFontRanges(FontRangeArray& aFontRanges,
                                      nsIContent* aContent,
                                      int32_t aBaseOffset,
                                      int32_t aXPStartOffset,
                                      int32_t aXPEndOffset,
                                      LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aContent->IsNodeOfType(nsINode::eTEXT));

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    // It is a non-rendered content, create an empty range for it.
    AppendFontRange(aFontRanges, aBaseOffset);
    return;
  }

  int32_t baseOffset = aBaseOffset;
  nsTextFrame* curr = do_QueryFrame(frame);
  MOZ_ASSERT(curr, "Not a text frame");
  while (curr) {
    int32_t frameXPStart = std::max(curr->GetContentOffset(), aXPStartOffset);
    int32_t frameXPEnd = std::min(curr->GetContentEnd(), aXPEndOffset);
    if (frameXPStart >= frameXPEnd) {
      curr = static_cast<nsTextFrame*>(curr->GetNextContinuation());
      continue;
    }

    gfxSkipCharsIterator iter = curr->EnsureTextRun(nsTextFrame::eInflated);
    gfxTextRun* textRun = curr->GetTextRun(nsTextFrame::eInflated);

    nsTextFrame* next = nullptr;
    if (frameXPEnd < aXPEndOffset) {
      next = static_cast<nsTextFrame*>(curr->GetNextContinuation());
      while (next && next->GetTextRun(nsTextFrame::eInflated) == textRun) {
        frameXPEnd = std::min(next->GetContentEnd(), aXPEndOffset);
        next = frameXPEnd < aXPEndOffset ?
          static_cast<nsTextFrame*>(next->GetNextContinuation()) : nullptr;
      }
    }

    gfxTextRun::Range skipRange(iter.ConvertOriginalToSkipped(frameXPStart),
                                iter.ConvertOriginalToSkipped(frameXPEnd));
    gfxTextRun::GlyphRunIterator runIter(textRun, skipRange);
    int32_t lastXPEndOffset = frameXPStart;
    while (runIter.NextRun()) {
      gfxFont* font = runIter.GetGlyphRun()->mFont.get();
      int32_t startXPOffset =
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
      int32_t endXPOffset =
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
ContentEventHandler::GenerateFlatFontRanges(nsRange* aRange,
                                            FontRangeArray& aFontRanges,
                                            uint32_t& aLength,
                                            LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aFontRanges.IsEmpty(), "aRanges must be empty array");

  if (aRange->Collapsed()) {
    return NS_OK;
  }

  nsINode* startNode = aRange->GetStartParent();
  nsINode* endNode = aRange->GetEndParent();
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }

  // baseOffset is the flattened offset of each content node.
  int32_t baseOffset = 0;
  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv = iter->Init(aRange);
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

    if (content->IsNodeOfType(nsINode::eTEXT)) {
      int32_t startOffset = content != startNode ? 0 : aRange->StartOffset();
      int32_t endOffset = content != endNode ?
        content->TextLength() : aRange->EndOffset();
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
            fontList.GetFontlist()[0];
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
  if (!aContent->IsNodeOfType(nsINode::eTEXT) ||
      *aXPOffset == 0 || *aXPOffset == aContent->TextLength()) {
    return NS_OK;
  }

  NS_ASSERTION(*aXPOffset <= aContent->TextLength(),
               "offset is out of range.");

  RefPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
  int32_t offsetInFrame;
  CaretAssociationHint hint =
    aForward ? CARET_ASSOCIATE_BEFORE : CARET_ASSOCIATE_AFTER;
  nsIFrame* frame = fs->GetFrameForNodeOffset(aContent, int32_t(*aXPOffset),
                                              hint, &offsetInFrame);
  if (!frame) {
    // This content doesn't have any frames, we only can check surrogate pair...
    const nsTextFragment* text = aContent->GetText();
    NS_ENSURE_TRUE(text, NS_ERROR_FAILURE);
    if (NS_IS_LOW_SURROGATE(text->CharAt(*aXPOffset)) &&
        NS_IS_HIGH_SURROGATE(text->CharAt(*aXPOffset - 1))) {
      *aXPOffset += aForward ? 1 : -1;
    }
    return NS_OK;
  }
  int32_t startOffset, endOffset;
  nsresult rv = frame->GetOffsets(startOffset, endOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aXPOffset == static_cast<uint32_t>(startOffset) ||
      *aXPOffset == static_cast<uint32_t>(endOffset)) {
    return NS_OK;
  }
  if (frame->GetType() != nsGkAtoms::textFrame) {
    return NS_ERROR_FAILURE;
  }
  nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
  int32_t newOffsetInFrame = *aXPOffset - startOffset;
  newOffsetInFrame += aForward ? -1 : 1;
  textFrame->PeekOffsetCharacter(aForward, &newOffsetInFrame);
  *aXPOffset = startOffset + newOffsetInFrame;
  return NS_OK;
}

nsresult
ContentEventHandler::SetRangeFromFlatTextOffset(nsRange* aRange,
                                                uint32_t aOffset,
                                                uint32_t aLength,
                                                LineBreakType aLineBreakType,
                                                bool aExpandToClusterBoundaries,
                                                uint32_t* aNewOffset)
{
  if (aNewOffset) {
    *aNewOffset = aOffset;
  }

  // Special case like <br contenteditable>
  if (!mRootContent->HasChildren()) {
    nsresult rv = aRange->SetStart(mRootContent, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = aRange->SetEnd(mRootContent, 0);
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

    uint32_t textLength =
      content->IsNodeOfType(nsINode::eTEXT) ?
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
      if (content->IsNodeOfType(nsINode::eTEXT)) {
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
        startNodeOffset = startNode->IndexOf(content);
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
        startNodeOffset = startNode->IndexOf(content) + 1;
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
      rv = aRange->SetStart(startNode, startNodeOffset);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      startSet = true;

      if (!aLength) {
        rv = aRange->SetEnd(startNode, startNodeOffset);
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
      if (content->IsNodeOfType(nsINode::eTEXT)) {
        // Rule #2.1: ]textNode or text]Node or textNode]
        uint32_t xpOffset = endOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          xpOffset = ConvertToXPOffset(content, xpOffset);
        }
        if (aExpandToClusterBoundaries) {
          rv = ExpandToClusterBoundary(content, true, &xpOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        NS_ASSERTION(xpOffset <= INT32_MAX,
          "The end node offset is too large");
        rv = aRange->SetEnd(content, static_cast<int32_t>(xpOffset));
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
        rv = aRange->SetEnd(content, 0);
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
      int32_t indexInParent = endNode->IndexOf(content);
      if (NS_WARN_IF(indexInParent == -1)) {
        // The content is being removed from the parent!
        return NS_ERROR_FAILURE;
      }
      rv = aRange->SetEnd(endNode, indexInParent + 1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    offset += textLength;
  }

  if (!startSet) {
    MOZ_ASSERT(!mRootContent->IsNodeOfType(nsINode::eTEXT));
    if (!offset) {
      // Rule #1.5: <root>[</root>
      // When there are no nodes causing text, the start of the DOM range
      // should be start of the root node since clicking on such editor (e.g.,
      // <div contenteditable><span></span></div>) sets caret to the start of
      // the editor (i.e., before <span> in the example).
      rv = aRange->SetStart(mRootContent, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (!aLength) {
        rv = aRange->SetEnd(mRootContent, 0);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
    } else {
      // Rule #1.5: [</root>
      rv = aRange->SetStart(mRootContent,
                     static_cast<int32_t>(mRootContent->GetChildCount()));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    if (aNewOffset) {
      *aNewOffset = offset;
    }
  }
  // Rule #2.5: ]</root>
  rv = aRange->SetEnd(mRootContent,
                      static_cast<int32_t>(mRootContent->GetChildCount()));
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
  NS_ENSURE_TRUE(aNode && aNode->IsNodeOfType(nsINode::eCONTENT),
                 NS_ERROR_UNEXPECTED);
  nsIContent* content = static_cast<nsIContent*>(aNode);
  nsIFrame* frame = content->GetPrimaryFrame();
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

  if (!mFirstSelectedRange) {
    MOZ_ASSERT(aEvent->mInput.mSelectionType != SelectionType::eNormal);
    MOZ_ASSERT(aEvent->mReply.mOffset == WidgetQueryContentEvent::NOT_FOUND);
    MOZ_ASSERT(aEvent->mReply.mString.IsEmpty());
    MOZ_ASSERT(!aEvent->mReply.mHasSelection);
    aEvent->mSucceeded = true;
    return NS_OK;
  }

  nsINode* const startNode = mFirstSelectedRange->GetStartParent();
  nsINode* const endNode = mFirstSelectedRange->GetEndParent();

  // Make sure the selection is within the root content range.
  if (!nsContentUtils::ContentIsDescendantOf(startNode, mRootContent) ||
      !nsContentUtils::ContentIsDescendantOf(endNode, mRootContent)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(aEvent->mReply.mString.IsEmpty(),
               "The reply string must be empty");

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  rv = GetFlatTextLengthBefore(mFirstSelectedRange,
                               &aEvent->mReply.mOffset, lineBreakType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> anchorNode, focusNode;
  int32_t anchorOffset, focusOffset;
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

    if (!mFirstSelectedRange->Collapsed()) {
      rv = GenerateFlatTextContent(mFirstSelectedRange, aEvent->mReply.mString,
                                   lineBreakType);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      aEvent->mReply.mString.Truncate();
    }
  } else {
    NS_ASSERTION(mFirstSelectedRange->Collapsed(),
      "When mSelection doesn't have selection, mFirstSelectedRange must be "
      "collapsed");
    anchorNode = focusNode = mFirstSelectedRange->GetStartParent();
    if (NS_WARN_IF(!anchorNode)) {
      return NS_ERROR_FAILURE;
    }
    anchorOffset = focusOffset =
      static_cast<int32_t>(mFirstSelectedRange->StartOffset());
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

  RefPtr<nsRange> range = new nsRange(mRootContent);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset,
                                  aEvent->mInput.mLength, lineBreakType, false,
                                  &aEvent->mReply.mOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GenerateFlatTextContent(range, aEvent->mReply.mString, lineBreakType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aEvent->mWithFontRanges) {
    uint32_t fontRangeLength;
    rv = GenerateFlatFontRanges(range, aEvent->mReply.mFontRanges,
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

// Adjust to use a child node if possible
// to make the returned rect more accurate
static nsINode* AdjustTextRectNode(nsINode* aNode,
                                   int32_t& aNodeOffset)
{
  int32_t childCount = int32_t(aNode->GetChildCount());
  nsINode* node = aNode;
  if (childCount) {
    if (aNodeOffset < childCount) {
      node = aNode->GetChildAt(aNodeOffset);
      aNodeOffset = 0;
    } else if (aNodeOffset == childCount) {
      node = aNode->GetChildAt(childCount - 1);
      aNodeOffset = node->IsNodeOfType(nsINode::eTEXT) ?
        static_cast<int32_t>(static_cast<nsIContent*>(node)->TextLength()) : 1;
    }
  }
  return node;
}

static
nsIFrame*
GetFirstFrameInRange(nsRange* aRange, int32_t& aNodeOffset)
{
  // used to iterate over all contents and their frames
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  iter->Init(aRange);

  // get the starting frame
  aNodeOffset = aRange->StartOffset();
  nsINode* node = iter->GetCurrentNode();
  if (!node) {
    node = AdjustTextRectNode(aRange->GetStartParent(), aNodeOffset);
  }
  nsIFrame* firstFrame = nullptr;
  GetFrameForTextRect(node, aNodeOffset, true, &firstFrame);
  return firstFrame;
}

nsresult
ContentEventHandler::OnQueryTextRectArray(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  RefPtr<nsRange> range = new nsRange(mRootContent);

  LayoutDeviceIntRect rect;
  uint32_t offset = aEvent->mInput.mOffset;
  const uint32_t kEndOffset = offset + aEvent->mInput.mLength;
  while (offset < kEndOffset) {
    rv = SetRangeFromFlatTextOffset(range, offset, 1, lineBreakType, true,
                                    nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // get the starting frame
    int32_t nodeOffset = -1;
    nsIFrame* firstFrame = GetFirstFrameInRange(range, nodeOffset);
    if (NS_WARN_IF(!firstFrame)) {
      return NS_ERROR_FAILURE;
    }

    // get the starting frame rect
    nsRect frameRect(nsPoint(0, 0), firstFrame->GetRect().Size());
    rv = ConvertToRootRelativeOffset(firstFrame, frameRect);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    AutoTArray<nsRect, 16> charRects;
    rv = firstFrame->GetCharacterRectsInRange(nodeOffset, kEndOffset - offset,
                                              charRects);
    if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(charRects.IsEmpty())) {
      return rv;
    }

    for (size_t i = 0; i < charRects.Length(); i++) {
      nsRect charRect = charRects[i];
      charRect.x += frameRect.x;
      charRect.y += frameRect.y;

      rect = LayoutDeviceIntRect::FromUnknownRect(
               charRect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel()));

      // Ensure at least 1px width and height for avoiding empty rect.
      rect.height = std::max(1, rect.height);
      rect.width = std::max(1, rect.width);

      aEvent->mReply.mRectArray.AppendElement(rect);
      offset++;
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

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  RefPtr<nsRange> range = new nsRange(mRootContent);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset,
                                  aEvent->mInput.mLength, lineBreakType, true,
                                  &aEvent->mReply.mOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GenerateFlatTextContent(range, aEvent->mReply.mString, lineBreakType);
  NS_ENSURE_SUCCESS(rv, rv);

  // used to iterate over all contents and their frames
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  iter->Init(range);

  // get the starting frame
  int32_t nodeOffset = range->StartOffset();
  nsINode* node = iter->GetCurrentNode();
  if (!node) {
    node = AdjustTextRectNode(range->GetStartParent(), nodeOffset);
  }
  nsIFrame* firstFrame = nullptr;
  rv = GetFrameForTextRect(node, nodeOffset, true, &firstFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the starting frame rect
  nsRect rect(nsPoint(0, 0), firstFrame->GetRect().Size());
  rv = ConvertToRootRelativeOffset(firstFrame, rect);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect frameRect = rect;
  nsPoint ptOffset;
  firstFrame->GetPointFromOffset(nodeOffset, &ptOffset);
  // minus 1 to avoid creating an empty rect
  if (firstFrame->GetWritingMode().IsVertical()) {
    rect.y += ptOffset.y - 1;
    rect.height -= ptOffset.y - 1;
  } else {
    rect.x += ptOffset.x - 1;
    rect.width -= ptOffset.x - 1;
  }

  // get the ending frame
  nodeOffset = range->EndOffset();
  node = AdjustTextRectNode(range->GetEndParent(), nodeOffset);
  nsIFrame* lastFrame = nullptr;
  rv = GetFrameForTextRect(node, nodeOffset, range->Collapsed(), &lastFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  // iterate over all covered frames
  for (nsIFrame* frame = firstFrame; frame != lastFrame;) {
    frame = frame->GetNextContinuation();
    if (!frame) {
      do {
        iter->Next();
        node = iter->GetCurrentNode();
        if (!node) {
          break;
        }
        if (!node->IsNodeOfType(nsINode::eCONTENT)) {
          continue;
        }
        frame = static_cast<nsIContent*>(node)->GetPrimaryFrame();
      } while (!frame && !iter->IsDone());
      if (!frame) {
        // this can happen when the end offset of the range is 0.
        frame = lastFrame;
      }
    }
    frameRect.SetRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    if (frame != lastFrame) {
      // not last frame, so just add rect to previous result
      rect.UnionRect(rect, frameRect);
    }
  }

  // get the ending frame rect
  lastFrame->GetPointFromOffset(nodeOffset, &ptOffset);
  // minus 1 to avoid creating an empty rect
  if (lastFrame->GetWritingMode().IsVertical()) {
    frameRect.height -= lastFrame->GetRect().height - ptOffset.y - 1;
  } else {
    frameRect.width -= lastFrame->GetRect().width - ptOffset.x - 1;
  }

  if (firstFrame == lastFrame) {
    rect.IntersectRect(rect, frameRect);
  } else {
    rect.UnionRect(rect, frameRect);
  }
  aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
      rect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel()));
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

  LineBreakType lineBreakType = GetLineBreakType(aEvent);

  nsRect caretRect;

  // When the selection is collapsed and the queried offset is current caret
  // position, we should return the "real" caret rect.
  if (mSelection->IsCollapsed()) {
    nsIFrame* caretFrame = nsCaret::GetGeometry(mSelection, &caretRect);
    if (caretFrame) {
      uint32_t offset;
      rv = GetFlatTextLengthBefore(mFirstSelectedRange,
                                   &offset, lineBreakType);
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == aEvent->mInput.mOffset) {
        rv = ConvertToRootRelativeOffset(caretFrame, caretRect);
        NS_ENSURE_SUCCESS(rv, rv);
        nscoord appUnitsPerDevPixel =
          caretFrame->PresContext()->AppUnitsPerDevPixel();
        aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
          caretRect.ToOutsidePixels(appUnitsPerDevPixel));
        aEvent->mReply.mWritingMode = caretFrame->GetWritingMode();
        aEvent->mReply.mOffset = aEvent->mInput.mOffset;
        aEvent->mSucceeded = true;
        return NS_OK;
      }
    }
  }

  // Otherwise, we should set the guessed caret rect.
  RefPtr<nsRange> range = new nsRange(mRootContent);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mInput.mOffset, 0,
                                  lineBreakType, true,
                                  &aEvent->mReply.mOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustCollapsedRangeMaybeIntoTextNode(range);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t xpOffsetInFrame;
  nsIFrame* frame;
  rv = GetStartFrameAndOffset(range, frame, xpOffsetInFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPoint posInFrame;
  rv = frame->GetPointFromOffset(range->StartOffset(), &posInFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mReply.mWritingMode = frame->GetWritingMode();
  bool isVertical = aEvent->mReply.mWritingMode.IsVertical();

  nsRect rect;
  rect.x = posInFrame.x;
  rect.y = posInFrame.y;

  RefPtr<nsFontMetrics> fontMetrics =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(frame);
  if (isVertical) {
    rect.width = fontMetrics->MaxHeight();
    rect.height = caretRect.height;
  } else {
    rect.width = caretRect.width;
    rect.height = fontMetrics->MaxHeight();
  }

  rv = ConvertToRootRelativeOffset(frame, rect);
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mReply.mRect = LayoutDeviceIntRect::FromUnknownRect(
      rect.ToOutsidePixels(mPresContext->AppUnitsPerDevPixel()));
  // If the caret rect is empty, let's make it non-empty rect.
  if (!aEvent->mReply.mRect.width) {
    aEvent->mReply.mRect.width = 1;
  }
  if (!aEvent->mReply.mRect.height) {
    aEvent->mReply.mRect.height = 1;
  }
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

  nsCOMPtr<nsIDocument> doc = mPresShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  rv = nsCopySupport::GetTransferableForSelection(
         mSelection, doc, getter_AddRefs(aEvent->mReply.mTransferable));
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

  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, NS_ERROR_FAILURE);
  nsIWidget* rootWidget = rootFrame->GetNearestWidget();
  NS_ENSURE_TRUE(rootWidget, NS_ERROR_FAILURE);

  // The root frame's widget might be different, e.g., the event was fired on
  // a popup but the rootFrame is the document root.
  if (rootWidget != aEvent->mWidget) {
    NS_PRECONDITION(aEvent->mWidget, "The event must have the widget");
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

  if (targetFrame->GetType() != nsGkAtoms::textFrame) {
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

  nsIDocument* doc = mPresShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  nsIFrame* docFrame = mPresShell->GetRootFrame();
  NS_ENSURE_TRUE(docFrame, NS_ERROR_FAILURE);

  LayoutDeviceIntPoint eventLoc =
    aEvent->mRefPoint + aEvent->mWidget->WidgetToScreenOffset();
  nsIntRect docFrameRect = docFrame->GetScreenRect(); // Returns CSS pixels
  CSSIntPoint eventLocCSS(
    mPresContext->DevPixelsToIntCSSPixels(eventLoc.x) - docFrameRect.x,
    mPresContext->DevPixelsToIntCSSPixels(eventLoc.y) - docFrameRect.y);

  Element* contentUnderMouse =
    doc->ElementFromPointHelper(eventLocCSS.x, eventLocCSS.y, false, false);
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
  if (NS_WARN_IF(!aRootContent) || NS_WARN_IF(!aStartPosition.IsValid()) ||
      NS_WARN_IF(!aEndPosition.IsValid()) || NS_WARN_IF(!aLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aStartPosition == aEndPosition) {
    *aLength = 0;
    return NS_OK;
  }

  // Don't create nsContentIterator instance until it's really necessary since
  // destroying without initializing causes unexpected NS_ASSERTION() call.
  nsCOMPtr<nsIContentIterator> iter;

  // This may be called for retrieving the text of removed nodes.  Even in this
  // case, the node thinks it's still in the tree because UnbindFromTree() will
  // be called after here.  However, the node was already removed from the
  // array of children of its parent.  So, be careful to handle this case.
  if (aIsRemovingNode) {
    DebugOnly<nsIContent*> parent = aStartPosition.mNode->GetParent();
    MOZ_ASSERT(parent && parent->IndexOf(aStartPosition.mNode) == -1,
      "At removing the node, the node shouldn't be in the array of children "
      "of its parent");
    MOZ_ASSERT(aStartPosition.mNode == aEndPosition.mNode,
      "At removing the node, start and end node should be same");
    MOZ_ASSERT(aStartPosition.mOffset == 0,
      "When the node is being removed, the start offset should be 0");
    MOZ_ASSERT(static_cast<uint32_t>(aEndPosition.mOffset) ==
                 aEndPosition.mNode->GetChildCount(),
      "When the node is being removed, the end offset should be child count");
    iter = NS_NewPreContentIterator();
    nsresult rv = iter->Init(aStartPosition.mNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    RefPtr<nsRange> prev = new nsRange(aRootContent);
    nsresult rv = aStartPosition.SetToRangeStart(prev);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // When the end position is immediately after non-root element's open tag,
    // we need to include a line break caused by the open tag.
    NodePosition endPosition;
    if (aEndPosition.mNode != aRootContent &&
        aEndPosition.IsImmediatelyAfterOpenTag()) {
      if (aEndPosition.mNode->HasChildren()) {
        // When the end node has some children, move the end position to the
        // start of its first child.
        nsINode* firstChild = aEndPosition.mNode->GetFirstChild();
        if (NS_WARN_IF(!firstChild)) {
          return NS_ERROR_FAILURE;
        }
        endPosition = NodePosition(firstChild, 0);
      } else {
        // When the end node is empty, move the end position after the node.
        nsIContent* parentContent = aEndPosition.mNode->GetParent();
        if (NS_WARN_IF(!parentContent)) {
          return NS_ERROR_FAILURE;
        }
        int32_t indexInParent = parentContent->IndexOf(aEndPosition.mNode);
        if (NS_WARN_IF(indexInParent < 0)) {
          return NS_ERROR_FAILURE;
        }
        endPosition = NodePosition(parentContent, indexInParent + 1);
      }
    } else {
      endPosition = aEndPosition;
    }

    if (endPosition.OffsetIsValid()) {
      // Offset is within node's length; set end of range to that offset
      rv = endPosition.SetToRangeEnd(prev);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      iter = NS_NewPreContentIterator();
      rv = iter->Init(prev);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (endPosition.mNode != aRootContent) {
      // Offset is past node's length; set end of range to end of node
      rv = endPosition.SetToRangeEndAfter(prev);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      iter = NS_NewPreContentIterator();
      rv = iter->Init(prev);
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

    if (node->IsNodeOfType(nsINode::eTEXT)) {
      // Note: our range always starts from offset 0
      if (node == aEndPosition.mNode) {
        *aLength += GetTextLength(content, aLineBreakType,
                                  aEndPosition.mOffset);
      } else {
        *aLength += GetTextLength(content, aLineBreakType);
      }
    } else if (ShouldBreakLineBefore(content, aRootContent)) {
      // If the start position is start of this node but doesn't include the
      // open tag, don't append the line break length.
      if (node == aStartPosition.mNode && !aStartPosition.IsBeforeOpenTag()) {
        continue;
      }
      *aLength += GetBRLength(aLineBreakType);
    }
  }
  return NS_OK;
}

nsresult
ContentEventHandler::GetFlatTextLengthBefore(nsRange* aRange,
                                             uint32_t* aOffset,
                                             LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aRange);
  return GetFlatTextLengthInRange(
           NodePosition(mRootContent, 0),
           NodePositionBefore(aRange->GetStartParent(), aRange->StartOffset()),
           mRootContent, aOffset, aLineBreakType);
}

nsresult
ContentEventHandler::AdjustCollapsedRangeMaybeIntoTextNode(nsRange* aRange)
{
  MOZ_ASSERT(aRange);
  MOZ_ASSERT(aRange->Collapsed());

  if (!aRange || !aRange->Collapsed()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsINode> parentNode = aRange->GetStartParent();
  int32_t offsetInParentNode = aRange->StartOffset();
  if (NS_WARN_IF(!parentNode) || NS_WARN_IF(offsetInParentNode < 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  // If the node is text node, we don't need to modify aRange.
  if (parentNode->IsNodeOfType(nsINode::eTEXT)) {
    return NS_OK;
  }

  // If the parent is not a text node but it has a text node at the offset,
  // we should adjust the range into the text node.
  // NOTE: This is emulating similar situation of EditorBase.
  nsINode* childNode = nullptr;
  int32_t offsetInChildNode = -1;
  if (!offsetInParentNode && parentNode->HasChildren()) {
    // If the range is the start of the parent, adjusted the range to the
    // start of the first child.
    childNode = parentNode->GetFirstChild();
    offsetInChildNode = 0;
  } else if (static_cast<uint32_t>(offsetInParentNode) <
               parentNode->GetChildCount()) {
    // If the range is next to a child node, adjust the range to the end of
    // the previous child.
    childNode = parentNode->GetChildAt(offsetInParentNode - 1);
    offsetInChildNode = childNode->Length();
  }

  // But if the found node isn't a text node, we cannot modify the range.
  if (!childNode || !childNode->IsNodeOfType(nsINode::eTEXT) ||
      NS_WARN_IF(offsetInChildNode < 0)) {
    return NS_OK;
  }

  nsresult rv = aRange->Set(childNode, offsetInChildNode,
                            childNode, offsetInChildNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
ContentEventHandler::GetStartFrameAndOffset(const nsRange* aRange,
                                            nsIFrame*& aFrame,
                                            int32_t& aOffsetInFrame)
{
  MOZ_ASSERT(aRange);

  aFrame = nullptr;
  aOffsetInFrame = -1;

  nsINode* node = aRange->GetStartParent();
  if (NS_WARN_IF(!node) ||
      NS_WARN_IF(!node->IsNodeOfType(nsINode::eCONTENT))) {
    return NS_ERROR_FAILURE;
  }
  nsIContent* content = static_cast<nsIContent*>(node);
  RefPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
  aFrame = fs->GetFrameForNodeOffset(content, aRange->StartOffset(),
                                     fs->GetHint(), &aOffsetInFrame);
  if (NS_WARN_IF(!aFrame)) {
    return NS_ERROR_FAILURE;
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
      !node->IsNodeOfType(nsINode::eTEXT)) {
    return;
  }

  // When the offset is at the end of the text node, set it to after the
  // text node, to make sure the caret is drawn on a new line when the last
  // character of the text node is '\n' in <textarea>.
  int32_t textLength =
    static_cast<int32_t>(static_cast<nsIContent*>(node)->TextLength());
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
  MOZ_ASSERT((*aNode)->IndexOf(node) != -1);
  *aNodeOffset = (*aNode)->IndexOf(node) + 1;
}

nsresult
ContentEventHandler::OnSelectionEvent(WidgetSelectionEvent* aEvent)
{
  aEvent->mSucceeded = false;

  // Get selection to manipulate
  // XXX why do we need to get them from ISM? This method should work fine
  //     without ISM.
  nsCOMPtr<nsISelection> sel;
  nsresult rv =
    IMEStateManager::GetFocusSelectionAndRoot(getter_AddRefs(sel),
                                              getter_AddRefs(mRootContent));
  mSelection = sel ? sel->AsSelection() : nullptr;
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = Init(aEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get range from offset and length
  RefPtr<nsRange> range = new nsRange(mRootContent);
  rv = SetRangeFromFlatTextOffset(range, aEvent->mOffset, aEvent->mLength,
                                  GetLineBreakType(aEvent),
                                  aEvent->mExpandToClusterBoundary);
  NS_ENSURE_SUCCESS(rv, rv);

  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
  int32_t startNodeOffset = range->StartOffset();
  int32_t endNodeOffset = range->EndOffset();
  AdjustRangeForSelection(mRootContent, &startNode, &startNodeOffset);
  AdjustRangeForSelection(mRootContent, &endNode, &endNodeOffset);
  if (NS_WARN_IF(!startNode) || NS_WARN_IF(!endNode) ||
      NS_WARN_IF(startNodeOffset < 0) || NS_WARN_IF(endNodeOffset < 0)) {
    return NS_ERROR_UNEXPECTED;
  }

  mSelection->StartBatchChanges();

  // Clear selection first before setting
  rv = mSelection->RemoveAllRanges();
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
  mSelection->EndBatchChangesInternal(aEvent->mReason);
  NS_ENSURE_SUCCESS(rv, rv);

  mSelection->ScrollIntoViewInternal(
    nsISelectionController::SELECTION_FOCUS_REGION,
    false, nsIPresShell::ScrollAxis(), nsIPresShell::ScrollAxis());
  aEvent->mSucceeded = true;
  return NS_OK;
}

} // namespace mozilla

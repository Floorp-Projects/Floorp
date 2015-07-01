/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentEventHandler.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
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
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMRange.h"
#include "nsIFrame.h"
#include "nsIObjectFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
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
ContentEventHandler::InitCommon()
{
  if (mSelection) {
    return NS_OK;
  }

  nsresult rv = InitBasic();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCopySupport::GetSelectionForCopy(mPresShell->GetDocument(),
                                     getter_AddRefs(mSelection));

  nsCOMPtr<nsIDOMRange> firstRange;
  rv = mSelection->GetRangeAt(0, getter_AddRefs(firstRange));
  // This shell doesn't support selection.
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
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
ContentEventHandler::Init(WidgetQueryContentEvent* aEvent)
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
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(doc->GetWindow());
  nsCOMPtr<nsPIDOMWindow> focusedWindow;
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
  nsresult rv = ConvertToRootViewRelativeOffset(frame, resultRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // account for any additional frames
  while ((frame = frame->GetNextContinuation()) != nullptr) {
    nsRect frameRect(nsPoint(0, 0), frame->GetRect().Size());
    rv = ConvertToRootViewRelativeOffset(frame, frameRect);
    NS_ENSURE_SUCCESS(rv, rv);
    resultRect.UnionRect(resultRect, frameRect);
  }

  aEvent->mReply.mRect = LayoutDevicePixel::FromUntyped(
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
#if defined(XP_MACOSX)
  // XXX Mac OS X doesn't use "\r".
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
  return GetTextLength(aContent, LINE_BREAK_TYPE_NATIVE, aMaxLength);
}

static inline uint32_t
GetBRLength(LineBreakType aLineBreakType)
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
  if (aContent->IsNodeOfType(nsINode::eTEXT)) {
    uint32_t textLengthDifference =
#if defined(XP_MACOSX)
      // On Mac, the length of a native newline ("\r") is equal to the length of
      // the XP newline ("\n"), so the native length is the same as the XP
      // length.
      0;
#elif defined(XP_WIN)
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
  } else if (IsContentBR(aContent)) {
    return GetBRLength(aLineBreakType);
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
                                        nsAFlatString& aString,
                                        LineBreakType aLineBreakType)
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

  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node) {
      break;
    }
    if (!node->IsNodeOfType(nsINode::eCONTENT)) {
      continue;
    }
    nsIContent* content = static_cast<nsIContent*>(node);

    if (content->IsNodeOfType(nsINode::eTEXT)) {
      if (content == startNode) {
        AppendSubString(aString, content, aRange->StartOffset(),
                        content->TextLength() - aRange->StartOffset());
      } else if (content == endNode) {
        AppendSubString(aString, content, 0, aRange->EndOffset());
      } else {
        AppendString(aString, content);
      }
    } else if (IsContentBR(content)) {
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

    uint32_t skipStart = iter.ConvertOriginalToSkipped(frameXPStart);
    uint32_t skipEnd = iter.ConvertOriginalToSkipped(frameXPEnd);
    gfxTextRun::GlyphRunIterator runIter(
      textRun, skipStart, skipEnd - skipStart);
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

/* static */ nsresult
ContentEventHandler::GenerateFlatFontRanges(nsRange* aRange,
                                            FontRangeArray& aFontRanges,
                                            uint32_t& aLength,
                                            LineBreakType aLineBreakType)
{
  MOZ_ASSERT(aFontRanges.IsEmpty(), "aRanges must be empty array");

  nsINode* startNode = aRange->GetStartParent();
  nsINode* endNode = aRange->GetEndParent();
  if (NS_WARN_IF(!startNode || !endNode)) {
    return NS_ERROR_FAILURE;
  }

  // baseOffset is the flattened offset of each content node.
  int32_t baseOffset = 0;
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  for (iter->Init(aRange); !iter->IsDone(); iter->Next()) {
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
    } else if (IsContentBR(content)) {
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

  nsRefPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
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

  nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
  nsresult rv = iter->Init(mRootContent);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t offset = 0;
  uint32_t endOffset = aOffset + aLength;
  bool startSet = false;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node) {
      break;
    }
    if (!node->IsNodeOfType(nsINode::eCONTENT)) {
      continue;
    }
    nsIContent* content = static_cast<nsIContent*>(node);

    uint32_t textLength = GetTextLength(content, aLineBreakType);
    if (!textLength) {
      continue;
    }

    if (offset <= aOffset && aOffset < offset + textLength) {
      uint32_t xpOffset;
      if (!content->IsNodeOfType(nsINode::eTEXT)) {
        xpOffset = 0;
      } else {
        xpOffset = aOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          xpOffset = ConvertToXPOffset(content, xpOffset);
        }
      }

      if (aExpandToClusterBoundaries) {
        uint32_t oldXPOffset = xpOffset;
        rv = ExpandToClusterBoundary(content, false, &xpOffset);
        NS_ENSURE_SUCCESS(rv, rv);
        if (aNewOffset) {
          // This is correct since a cluster shouldn't include line break.
          *aNewOffset -= (oldXPOffset - xpOffset);
        }
      }

      rv = aRange->SetStart(content, int32_t(xpOffset));
      NS_ENSURE_SUCCESS(rv, rv);
      startSet = true;
      if (aLength == 0) {
        // Ensure that the end offset and the start offset are same.
        rv = aRange->SetEnd(content, int32_t(xpOffset));
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    if (endOffset <= offset + textLength) {
      nsINode* endNode = content;
      uint32_t xpOffset;
      if (content->IsNodeOfType(nsINode::eTEXT)) {
        xpOffset = endOffset - offset;
        if (aLineBreakType == LINE_BREAK_TYPE_NATIVE) {
          xpOffset = ConvertToXPOffset(content, xpOffset);
        }
        if (aExpandToClusterBoundaries) {
          rv = ExpandToClusterBoundary(content, true, &xpOffset);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else {
        // Use first position of next node, because the end node is ignored
        // by ContentIterator when the offset is zero.
        xpOffset = 0;
        iter->Next();
        if (iter->IsDone()) {
          break;
        }
        endNode = iter->GetCurrentNode();
      }

      rv = aRange->SetEnd(endNode, int32_t(xpOffset));
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    offset += textLength;
  }

  if (offset < aOffset) {
    return NS_ERROR_FAILURE;
  }

  if (!startSet) {
    MOZ_ASSERT(!mRootContent->IsNodeOfType(nsINode::eTEXT));
    rv = aRange->SetStart(mRootContent, int32_t(mRootContent->GetChildCount()));
    NS_ENSURE_SUCCESS(rv, rv);
    if (aNewOffset) {
      *aNewOffset = offset;
    }
  }
  rv = aRange->SetEnd(mRootContent, int32_t(mRootContent->GetChildCount()));
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsIDOMRange::SetEnd failed");
  return rv;
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

  NS_ASSERTION(aEvent->mReply.mString.IsEmpty(),
               "The reply string must be empty");

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  rv = GetFlatTextOffsetOfRange(mRootContent, mFirstSelectedRange,
                                &aEvent->mReply.mOffset, lineBreakType);
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
    rv = GenerateFlatTextContent(mFirstSelectedRange, aEvent->mReply.mString,
                                 lineBreakType);
    NS_ENSURE_SUCCESS(rv, rv);
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

  nsRefPtr<nsRange> range = new nsRange(mRootContent);
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

nsresult
ContentEventHandler::OnQueryTextRect(WidgetQueryContentEvent* aEvent)
{
  nsresult rv = Init(aEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  LineBreakType lineBreakType = GetLineBreakType(aEvent);
  nsRefPtr<nsRange> range = new nsRange(mRootContent);
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
  rv = ConvertToRootViewRelativeOffset(firstFrame, rect);
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
    rv = ConvertToRootViewRelativeOffset(frame, frameRect);
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
  aEvent->mReply.mRect = LayoutDevicePixel::FromUntyped(
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

  // When the selection is collapsed and the queried offset is current caret
  // position, we should return the "real" caret rect.
  bool selectionIsCollapsed;
  rv = mSelection->GetIsCollapsed(&selectionIsCollapsed);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect caretRect;

  if (selectionIsCollapsed) {
    nsIFrame* caretFrame = nsCaret::GetGeometry(mSelection, &caretRect);
    if (caretFrame) {
      uint32_t offset;
      rv = GetFlatTextOffsetOfRange(mRootContent, mFirstSelectedRange, &offset,
                                    lineBreakType);
      NS_ENSURE_SUCCESS(rv, rv);
      if (offset == aEvent->mInput.mOffset) {
        rv = ConvertToRootViewRelativeOffset(caretFrame, caretRect);
        NS_ENSURE_SUCCESS(rv, rv);
        nscoord appUnitsPerDevPixel =
          caretFrame->PresContext()->AppUnitsPerDevPixel();
        aEvent->mReply.mRect = LayoutDevicePixel::FromUntyped(
          caretRect.ToOutsidePixels(appUnitsPerDevPixel));
        aEvent->mReply.mWritingMode = caretFrame->GetWritingMode();
        aEvent->mReply.mOffset = aEvent->mInput.mOffset;
        aEvent->mSucceeded = true;
        return NS_OK;
      }
    }
  }

  // Otherwise, we should set the guessed caret rect.
  nsRefPtr<nsRange> range = new nsRange(mRootContent);
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

  nscoord fontHeight = 0;
  float inflation = nsLayoutUtils::FontSizeInflationFor(frame);
  nsRefPtr<nsFontMetrics> fontMetrics;
  rv = nsLayoutUtils::GetFontMetricsForFrame(frame, getter_AddRefs(fontMetrics),
                                             inflation);
  if (NS_WARN_IF(!fontMetrics)) {
    // If we cannot get font height, use frame size instead.
    fontHeight = isVertical ? frame->GetSize().width : frame->GetSize().height;
  } else {
    fontHeight = fontMetrics->MaxAscent() + fontMetrics->MaxDescent();
  }
  if (isVertical) {
    rect.width = fontHeight;
    rect.height = caretRect.height;
  } else {
    rect.width = caretRect.width;
    rect.height = fontHeight;
  }

  rv = ConvertToRootViewRelativeOffset(frame, rect);
  NS_ENSURE_SUCCESS(rv, rv);

  aEvent->mReply.mRect = LayoutDevicePixel::FromUntyped(
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
  if (rootWidget != aEvent->widget) {
    NS_PRECONDITION(aEvent->widget, "The event must have the widget");
    nsView* view = nsView::GetViewFor(aEvent->widget);
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
    rootFrame = view->GetFrame();
    NS_ENSURE_TRUE(rootFrame, NS_ERROR_FAILURE);
    rootWidget = rootFrame->GetNearestWidget();
    NS_ENSURE_TRUE(rootWidget, NS_ERROR_FAILURE);
  }

  WidgetQueryContentEvent eventOnRoot(true, NS_QUERY_CHARACTER_AT_POINT,
                                      rootWidget);
  eventOnRoot.mUseNativeLineBreak = aEvent->mUseNativeLineBreak;
  eventOnRoot.refPoint = aEvent->refPoint;
  if (rootWidget != aEvent->widget) {
    eventOnRoot.refPoint += aEvent->widget->WidgetToScreenOffset() -
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

  rv = GetFlatTextOffsetOfRange(mRootContent, tentativeCaretOffsets.content,
                                tentativeCaretOffsets.offset,
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
  rv = GetFlatTextOffsetOfRange(mRootContent, contentOffsets.content,
                                contentOffsets.offset, &offset,
                                GetLineBreakType(aEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  WidgetQueryContentEvent textRect(true, NS_QUERY_TEXT_RECT, aEvent->widget);
  textRect.InitForQueryTextRect(offset, 1, aEvent->mUseNativeLineBreak);
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

  NS_ENSURE_TRUE(aEvent->widget, NS_ERROR_FAILURE);

  nsIDocument* doc = mPresShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  nsIFrame* docFrame = mPresShell->GetRootFrame();
  NS_ENSURE_TRUE(docFrame, NS_ERROR_FAILURE);

  LayoutDeviceIntPoint eventLoc = aEvent->refPoint + aEvent->widget->WidgetToScreenOffset();
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
    if (aEvent->widget == targetWidget) {
      aEvent->mReply.mWidgetIsHit = true;
    }
  }

  aEvent->mSucceeded = true;
  return NS_OK;
}

/* static */ nsresult
ContentEventHandler::GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                              nsINode* aNode,
                                              int32_t aNodeOffset,
                                              uint32_t* aOffset,
                                              LineBreakType aLineBreakType)
{
  NS_ENSURE_STATE(aRootContent);
  NS_ASSERTION(aOffset, "param is invalid");

  nsRefPtr<nsRange> prev = new nsRange(aRootContent);
  nsCOMPtr<nsIDOMNode> rootDOMNode(do_QueryInterface(aRootContent));
  prev->SetStart(rootDOMNode, 0);

  nsCOMPtr<nsIDOMNode> startDOMNode(do_QueryInterface(aNode));
  NS_ASSERTION(startDOMNode, "startNode doesn't have nsIDOMNode");

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();

  if (aNode->Length() >= static_cast<uint32_t>(aNodeOffset)) {
    // Offset is within node's length; set end of range to that offset
    prev->SetEnd(startDOMNode, aNodeOffset);
    iter->Init(prev);
  } else if (aNode != static_cast<nsINode*>(aRootContent)) {
    // Offset is past node's length; set end of range to end of node
    prev->SetEndAfter(startDOMNode);
    iter->Init(prev);
  } else {
    // Offset is past the root node; set end of range to end of root node
    iter->Init(aRootContent);
  }

  nsCOMPtr<nsINode> startNode = do_QueryInterface(startDOMNode);
  nsINode* endNode = aNode;

  *aOffset = 0;
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    if (!node) {
      break;
    }
    if (!node->IsNodeOfType(nsINode::eCONTENT)) {
      continue;
    }
    nsIContent* content = static_cast<nsIContent*>(node);

    if (node->IsNodeOfType(nsINode::eTEXT)) {
      // Note: our range always starts from offset 0
      if (node == endNode) {
        *aOffset += GetTextLength(content, aLineBreakType, aNodeOffset);
      } else {
        *aOffset += GetTextLength(content, aLineBreakType);
      }
    } else if (IsContentBR(content)) {
#if defined(XP_WIN)
      // On Windows, the length of the newline is 2.
      *aOffset += (aLineBreakType == LINE_BREAK_TYPE_NATIVE) ? 2 : 1;
#else
      // On other platforms, the length of the newline is 1.
      *aOffset += 1;
#endif
    }
  }
  return NS_OK;
}

/* static */ nsresult
ContentEventHandler::GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                              nsRange* aRange,
                                              uint32_t* aOffset,
                                              LineBreakType aLineBreakType)
{
  nsINode* startNode = aRange->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  int32_t startOffset = aRange->StartOffset();
  return GetFlatTextOffsetOfRange(aRootContent, startNode, startOffset,
                                  aOffset, aLineBreakType);
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
  // NOTE: This is emulating similar situation of nsEditor.
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
  nsRefPtr<nsFrameSelection> fs = mPresShell->FrameSelection();
  aFrame = fs->GetFrameForNodeOffset(content, aRange->StartOffset(),
                                     fs->GetHint(), &aOffsetInFrame);
  if (NS_WARN_IF(!aFrame)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
ContentEventHandler::ConvertToRootViewRelativeOffset(nsIFrame* aFrame,
                                                     nsRect& aRect)
{
  NS_ASSERTION(aFrame, "aFrame must not be null");

  nsView* view = nullptr;
  nsPoint posInView;
  aFrame->GetOffsetFromView(posInView, &view);
  if (!view) {
    return NS_ERROR_FAILURE;
  }
  aRect += posInView + view->GetOffsetTo(nullptr);
  return NS_OK;
}

static void AdjustRangeForSelection(nsIContent* aRoot,
                                    nsINode** aNode,
                                    int32_t* aNodeOffset)
{
  nsINode* node = *aNode;
  int32_t nodeOffset = *aNodeOffset;
  if (aRoot != node && node->GetParent()) {
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      // When the offset is at the end of the text node, set it to after the
      // text node, to make sure the caret is drawn on a new line when the last
      // character of the text node is '\n'
      int32_t nodeLength =
        static_cast<int32_t>(static_cast<nsIContent*>(node)->TextLength());
      MOZ_ASSERT(nodeOffset <= nodeLength, "Offset is past length of text node");
      if (nodeOffset == nodeLength) {
        node = node->GetParent();
        nodeOffset = node->IndexOf(*aNode) + 1;
      }
    } else {
      node = node->GetParent();
      nodeOffset = node->IndexOf(*aNode) + (nodeOffset ? 1 : 0);
    }
  }

  nsIContent* brContent = node->GetChildAt(nodeOffset - 1);
  while (brContent && brContent->IsHTMLElement()) {
    if (!brContent->IsHTMLElement(nsGkAtoms::br) || IsContentBR(brContent)) {
      break;
    }
    brContent = node->GetChildAt(--nodeOffset - 1);
  }
  *aNode = node;
  *aNodeOffset = std::max(nodeOffset, 0);
}

nsresult
ContentEventHandler::OnSelectionEvent(WidgetSelectionEvent* aEvent)
{
  aEvent->mSucceeded = false;

  // Get selection to manipulate
  // XXX why do we need to get them from ISM? This method should work fine
  //     without ISM.
  nsresult rv =
    IMEStateManager::GetFocusSelectionAndRoot(getter_AddRefs(mSelection),
                                              getter_AddRefs(mRootContent));
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = Init(aEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get range from offset and length
  nsRefPtr<nsRange> range = new nsRange(mRootContent);
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
      rv = mSelection->Collapse(endDomNode, endNodeOffset);
    } else {
      rv = mSelection->Collapse(startDomNode, startNodeOffset);
    }
    if (NS_SUCCEEDED(rv) &&
        (startDomNode != endDomNode || startNodeOffset != endNodeOffset)) {
      if (aEvent->mReversed) {
        rv = mSelection->Extend(startDomNode, startNodeOffset);
      } else {
        rv = mSelection->Extend(endDomNode, endNodeOffset);
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

} // namespace mozilla

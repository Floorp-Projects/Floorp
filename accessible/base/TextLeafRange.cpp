/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextLeafRange.h"

#include "HyperTextAccessible-inl.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/CacheConstants.h"
#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/LocalAccessible.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/Casting.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/Document.h"
#include "mozilla/intl/Segmenter.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsBlockFrame.h"
#include "nsContentUtils.h"
#include "nsFrameSelection.h"
#include "nsIAccessiblePivot.h"
#include "nsILineIterator.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsStyleStructInlines.h"
#include "nsTArray.h"
#include "nsTextFrame.h"
#include "nsUnicodeProperties.h"
#include "Pivot.h"
#include "TextAttrs.h"

using mozilla::intl::WordBreaker;

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
  nsTextFrame* frame = do_QueryFrame(aAcc->GetFrame());
  if (!frame) {
    MOZ_ASSERT(!aAcc->HasOwnContent(),
               "No text frame because this is a XUL label[value] text leaf.");
    return static_cast<int32_t>(aRenderedOffset);
  }

  if (frame->StyleText()->WhiteSpaceIsSignificant() &&
      frame->StyleText()->NewlineIsSignificant(frame)) {
    // Spaces and new lines aren't altered, so the content and rendered offsets
    // are the same. This happens in pre-formatted text and text fields.
    return static_cast<int32_t>(aRenderedOffset);
  }

  nsIFrame::RenderedText text =
      frame->GetRenderedText(aRenderedOffset, aRenderedOffset + 1,
                             nsIFrame::TextOffsetType::OffsetsInRenderedText,
                             nsIFrame::TrailingWhitespace::DontTrim);
  return text.mOffsetWithinNodeText;
}

static uint32_t ContentToRenderedOffset(LocalAccessible* aAcc,
                                        int32_t aContentOffset) {
  nsTextFrame* frame = do_QueryFrame(aAcc->GetFrame());
  if (!frame) {
    MOZ_ASSERT(!aAcc->HasOwnContent(),
               "No text frame because this is a XUL label[value] text leaf.");
    return aContentOffset;
  }

  if (frame->StyleText()->WhiteSpaceIsSignificant() &&
      frame->StyleText()->NewlineIsSignificant(frame)) {
    // Spaces and new lines aren't altered, so the content and rendered offsets
    // are the same. This happens in pre-formatted text and text fields.
    return aContentOffset;
  }

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

static HyperTextAccessible* HyperTextFor(LocalAccessible* aAcc) {
  for (LocalAccessible* acc = aAcc; acc; acc = acc->LocalParent()) {
    if (HyperTextAccessible* ht = acc->AsHyperText()) {
      return ht;
    }
  }
  return nullptr;
}

static Accessible* NextLeaf(Accessible* aOrigin) {
  MOZ_ASSERT(aOrigin);
  Accessible* doc = nsAccUtils::DocumentFor(aOrigin);
  Pivot pivot(doc);
  auto rule = LeafRule();
  return pivot.Next(aOrigin, rule);
}

static Accessible* PrevLeaf(Accessible* aOrigin) {
  MOZ_ASSERT(aOrigin);
  Accessible* doc = nsAccUtils::DocumentFor(aOrigin);
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
  auto [thisBlock, thisLineFrame] = thisFrame->GetContainingBlockForLine(
      /* aLockScroll */ false);
  if (!thisBlock) {
    // We couldn't get the containing block for this frame. In that case, we
    // play it safe and assume this is the beginning of a new line.
    return true;
  }
  nsIFrame::GetLastLeaf(&prevFrame);
  // The previous leaf might cross lines. We want to compare against the last
  // line.
  prevFrame = prevFrame->LastContinuation();
  auto [prevBlock, prevLineFrame] = prevFrame->GetContainingBlockForLine(
      /* aLockScroll */ false);
  if (thisBlock != prevBlock) {
    // If the blocks are different, that means there's nothing before us on the
    // same line, so we're at the start.
    return true;
  }
  if (nsBlockFrame* block = do_QueryFrame(thisBlock)) {
    // If we have a block frame, it's faster for us to use
    // BlockInFlowLineIterator because it uses the line cursor.
    bool found = false;
    block->SetupLineCursorForQuery();
    nsBlockInFlowLineIterator prevIt(block, prevLineFrame, &found);
    if (!found) {
      // Error; play it safe.
      return true;
    }
    found = false;
    nsBlockInFlowLineIterator thisIt(block, thisLineFrame, &found);
    // if the lines are different, that means there's nothing before us on the
    // same line, so we're at the start.
    return !found || prevIt.GetLine() != thisIt.GetLine();
  }
  AutoAssertNoDomMutations guard;
  nsILineIterator* it = prevBlock->GetLineIterator();
  MOZ_ASSERT(it, "GetLineIterator impl in line-container blocks is infallible");
  int32_t prevLineNum = it->FindLineContaining(prevLineFrame);
  if (prevLineNum < 0) {
    // Error; play it safe.
    return true;
  }
  int32_t thisLineNum = it->FindLineContaining(thisLineFrame, prevLineNum);
  // if the blocks and line numbers are different, that means there's nothing
  // before us on the same line, so we're at the start.
  return thisLineNum != prevLineNum;
}

/**
 * There are many kinds of word break, but we only need to treat punctuation and
 * space specially.
 */
enum WordBreakClass { eWbcSpace = 0, eWbcPunct, eWbcOther };

static WordBreakClass GetWordBreakClass(char16_t aChar) {
  // Based on IsSelectionInlineWhitespace and IsSelectionNewline in
  // layout/generic/nsTextFrame.cpp.
  const char16_t kCharNbsp = 0xA0;
  switch (aChar) {
    case ' ':
    case kCharNbsp:
    case '\t':
    case '\f':
    case '\n':
    case '\r':
      return eWbcSpace;
    default:
      break;
  }
  // Based on ClusterIterator::IsPunctuation in
  // layout/generic/nsTextFrame.cpp.
  uint8_t cat = unicode::GetGeneralCategory(aChar);
  switch (cat) {
    case HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION: /* Pc */
      if (aChar == '_' &&
          !StaticPrefs::layout_word_select_stop_at_underscore()) {
        return eWbcOther;
      }
      [[fallthrough]];
    case HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION:    /* Pd */
    case HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION:   /* Pe */
    case HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION:   /* Pf */
    case HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION: /* Pi */
    case HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION:   /* Po */
    case HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION:    /* Ps */
    case HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL:     /* Sc */
    case HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL:         /* Sm */
    case HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL:        /* So */
      return eWbcPunct;
    default:
      break;
  }
  return eWbcOther;
}

/**
 * Words can cross Accessibles. To work out whether we're at the start of a
 * word, we might have to check the previous leaf. This class handles querying
 * the previous WordBreakClass, crossing Accessibles if necessary.
 */
class PrevWordBreakClassWalker {
 public:
  PrevWordBreakClassWalker(Accessible* aAcc, const nsAString& aText,
                           int32_t aOffset)
      : mAcc(aAcc), mText(aText), mOffset(aOffset) {
    mClass = GetWordBreakClass(mText.CharAt(mOffset));
  }

  WordBreakClass CurClass() { return mClass; }

  Maybe<WordBreakClass> PrevClass() {
    for (;;) {
      if (!PrevChar()) {
        return Nothing();
      }
      WordBreakClass curClass = GetWordBreakClass(mText.CharAt(mOffset));
      if (curClass != mClass) {
        mClass = curClass;
        return Some(curClass);
      }
    }
    MOZ_ASSERT_UNREACHABLE();
    return Nothing();
  }

  bool IsStartOfGroup() {
    if (!PrevChar()) {
      // There are no characters before us.
      return true;
    }
    WordBreakClass curClass = GetWordBreakClass(mText.CharAt(mOffset));
    // We wanted to peek at the previous character, not really move to it.
    ++mOffset;
    return curClass != mClass;
  }

 private:
  bool PrevChar() {
    if (mOffset > 0) {
      --mOffset;
      return true;
    }
    if (!mAcc) {
      // PrevChar was called already and failed.
      return false;
    }
    mAcc = PrevLeaf(mAcc);
    if (!mAcc) {
      return false;
    }
    mText.Truncate();
    mAcc->AppendTextTo(mText);
    mOffset = static_cast<int32_t>(mText.Length()) - 1;
    return true;
  }

  Accessible* mAcc;
  nsAutoString mText;
  int32_t mOffset;
  WordBreakClass mClass;
};

/**
 * WordBreaker breaks at all space, punctuation, etc. We want to emulate
 * layout, so that's not what we want. This function determines whether this
 * is acceptable as the start of a word for our purposes.
 */
static bool IsAcceptableWordStart(Accessible* aAcc, const nsAutoString& aText,
                                  int32_t aOffset) {
  PrevWordBreakClassWalker walker(aAcc, aText, aOffset);
  if (!walker.IsStartOfGroup()) {
    // If we're not at the start of a WordBreaker group, this can't be the
    // start of a word.
    return false;
  }
  WordBreakClass curClass = walker.CurClass();
  if (curClass == eWbcSpace) {
    // Space isn't the start of a word.
    return false;
  }
  Maybe<WordBreakClass> prevClass = walker.PrevClass();
  if (curClass == eWbcPunct && (!prevClass || prevClass.value() != eWbcSpace)) {
    // Punctuation isn't the start of a word (unless it is after space).
    return false;
  }
  if (!prevClass || prevClass.value() != eWbcPunct) {
    // If there's nothing before this or the group before this isn't
    // punctuation, this is the start of a word.
    return true;
  }
  // At this point, we know the group before this is punctuation.
  if (!StaticPrefs::layout_word_select_stop_at_punctuation()) {
    // When layout.word_select.stop_at_punctuation is false (defaults to true),
    // if there is punctuation before this, this is not the start of a word.
    return false;
  }
  Maybe<WordBreakClass> prevPrevClass = walker.PrevClass();
  if (!prevPrevClass || prevPrevClass.value() == eWbcSpace) {
    // If there is punctuation before this and space (or nothing) before the
    // punctuation, this is not the start of a word.
    return false;
  }
  return true;
}

class BlockRule : public PivotRule {
 public:
  virtual uint16_t Match(Accessible* aAcc) override {
    if (RefPtr<nsAtom>(aAcc->DisplayStyle()) == nsGkAtoms::block ||
        aAcc->IsHTMLListItem() ||
        // XXX Bullets are inline-block, but the old local implementation treats
        // them as block because IsBlockFrame() returns true. Semantically,
        // they shouldn't be treated as blocks, so this should be removed once
        // we only have a single implementation to deal with.
        (aAcc->IsText() && aAcc->Role() == roles::LISTITEM_MARKER)) {
      return nsIAccessibleTraversalRule::FILTER_MATCH;
    }
    return nsIAccessibleTraversalRule::FILTER_IGNORE;
  }
};

/**
 * Find spelling error DOM ranges overlapping the requested LocalAccessible and
 * offsets. This includes ranges that begin or end outside of the given
 * LocalAccessible. Note that the offset arguments are rendered offsets, but
 * because the returned ranges are DOM ranges, those offsets are content
 * offsets. See the documentation for dom::Selection::GetRangesForIntervalArray
 * for information about the aAllowAdjacent argument.
 */
static nsTArray<nsRange*> FindDOMSpellingErrors(LocalAccessible* aAcc,
                                                int32_t aRenderedStart,
                                                int32_t aRenderedEnd,
                                                bool aAllowAdjacent = false) {
  if (!aAcc->IsTextLeaf() || !aAcc->HasOwnContent()) {
    return {};
  }
  nsIFrame* frame = aAcc->GetFrame();
  RefPtr<nsFrameSelection> frameSel =
      frame ? frame->GetFrameSelection() : nullptr;
  dom::Selection* domSel =
      frameSel ? frameSel->GetSelection(SelectionType::eSpellCheck) : nullptr;
  if (!domSel) {
    return {};
  }
  nsINode* node = aAcc->GetNode();
  uint32_t contentStart = RenderedToContentOffset(aAcc, aRenderedStart);
  uint32_t contentEnd =
      aRenderedEnd == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT
          ? dom::CharacterData::FromNode(node)->TextLength()
          : RenderedToContentOffset(aAcc, aRenderedEnd);
  nsTArray<nsRange*> domRanges;
  domSel->GetRangesForIntervalArray(node, contentStart, node, contentEnd,
                                    aAllowAdjacent, &domRanges);
  return domRanges;
}

/*** TextLeafPoint ***/

TextLeafPoint::TextLeafPoint(Accessible* aAcc, int32_t aOffset) {
  if (aOffset != nsIAccessibleText::TEXT_OFFSET_CARET && aAcc->HasChildren()) {
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
  return mAcc->IsBefore(aPoint.mAcc);
}

bool TextLeafPoint::operator<=(const TextLeafPoint& aPoint) const {
  return *this == aPoint || *this < aPoint;
}

bool TextLeafPoint::IsEmptyLastLine() const {
  if (mAcc->IsHTMLBr() && mOffset == 1) {
    return true;
  }
  if (!mAcc->IsTextLeaf()) {
    return false;
  }
  if (mOffset < static_cast<int32_t>(nsAccUtils::TextLength(mAcc))) {
    return false;
  }
  nsAutoString text;
  mAcc->AppendTextTo(text, mOffset - 1, 1);
  return text.CharAt(0) == '\n';
}

char16_t TextLeafPoint::GetChar() const {
  nsAutoString text;
  mAcc->AppendTextTo(text, mOffset, 1);
  return text.CharAt(0);
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
    // This can happen if this is an empty element with display: contents. In
    // that case, this Accessible contains no lines.
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
    // This can happen if this is an empty element with display: contents. In
    // that case, this Accessible contains no lines.
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

TextLeafPoint TextLeafPoint::FindLineStartSameRemoteAcc(
    nsDirection aDirection, bool aIncludeOrigin) const {
  RemoteAccessible* acc = mAcc->AsRemote();
  MOZ_ASSERT(acc);
  auto lines = acc->GetCachedTextLines();
  if (!lines) {
    return TextLeafPoint();
  }
  size_t index;
  // If BinarySearch returns true, mOffset is in the array and index points at
  // it. If BinarySearch returns false, mOffset is not in the array and index
  // points at the next line start after mOffset.
  if (BinarySearch(*lines, 0, lines->Length(), mOffset, &index)) {
    if (aIncludeOrigin) {
      return *this;
    }
    if (aDirection == eDirNext) {
      // We don't want to include the origin. Get the next line start.
      ++index;
    }
  }
  MOZ_ASSERT(index <= lines->Length());
  if ((aDirection == eDirNext && index == lines->Length()) ||
      (aDirection == eDirPrevious && index == 0)) {
    return TextLeafPoint();
  }
  // index points at the line start after mOffset.
  if (aDirection == eDirPrevious) {
    --index;
  }
  return TextLeafPoint(mAcc, lines->ElementAt(index));
}

TextLeafPoint TextLeafPoint::FindLineStartSameAcc(nsDirection aDirection,
                                                  bool aIncludeOrigin) const {
  if (mAcc->IsLocal()) {
    return aDirection == eDirNext
               ? FindNextLineStartSameLocalAcc(aIncludeOrigin)
               : FindPrevLineStartSameLocalAcc(aIncludeOrigin);
  }
  return FindLineStartSameRemoteAcc(aDirection, aIncludeOrigin);
}

TextLeafPoint TextLeafPoint::FindPrevWordStartSameAcc(
    bool aIncludeOrigin) const {
  if (mOffset == 0 && !aIncludeOrigin) {
    // We can't go back any further and the caller doesn't want the origin
    // included, so there's nothing more to do.
    return TextLeafPoint();
  }
  nsAutoString text;
  mAcc->AppendTextTo(text);
  TextLeafPoint lineStart = *this;
  if (!aIncludeOrigin || (lineStart.mOffset == 1 && text.Length() == 1 &&
                          text.CharAt(0) == '\n')) {
    // We're not interested in a line that starts here, either because
    // aIncludeOrigin is false or because we're at the end of a line break
    // node.
    --lineStart.mOffset;
  }
  // A word never starts with a line feed character. If there are multiple
  // consecutive line feed characters and we're after the first of them, the
  // previous line start will be a line feed character. Skip this and any prior
  // consecutive line feed first.
  for (; lineStart.mOffset >= 0 && text.CharAt(lineStart.mOffset) == '\n';
       --lineStart.mOffset) {
  }
  if (lineStart.mOffset < 0) {
    // There's no line start for our purposes.
    lineStart = TextLeafPoint();
  } else {
    lineStart =
        lineStart.FindLineStartSameAcc(eDirPrevious, /* aIncludeOrigin */ true);
  }
  // Keep walking backward until we find an acceptable word start.
  intl::WordRange word;
  if (mOffset == 0) {
    word.mBegin = 0;
  } else if (mOffset == static_cast<int32_t>(text.Length())) {
    word = WordBreaker::FindWord(text.get(), text.Length(), mOffset - 1);
  } else {
    word = WordBreaker::FindWord(text.get(), text.Length(), mOffset);
  }
  for (;; word = WordBreaker::FindWord(text.get(), text.Length(),
                                       word.mBegin - 1)) {
    if (!aIncludeOrigin && static_cast<int32_t>(word.mBegin) == mOffset) {
      // A word possibly starts at the origin, but the caller doesn't want this
      // included.
      MOZ_ASSERT(word.mBegin != 0);
      continue;
    }
    if (lineStart && static_cast<int32_t>(word.mBegin) < lineStart.mOffset) {
      // A line start always starts a new word.
      return lineStart;
    }
    if (IsAcceptableWordStart(mAcc, text, static_cast<int32_t>(word.mBegin))) {
      break;
    }
    if (word.mBegin == 0) {
      // We can't go back any further.
      if (lineStart) {
        // A line start always starts a new word.
        return lineStart;
      }
      return TextLeafPoint();
    }
  }
  return TextLeafPoint(mAcc, static_cast<int32_t>(word.mBegin));
}

TextLeafPoint TextLeafPoint::FindNextWordStartSameAcc(
    bool aIncludeOrigin) const {
  nsAutoString text;
  mAcc->AppendTextTo(text);
  int32_t wordStart = mOffset;
  if (aIncludeOrigin) {
    if (wordStart == 0) {
      if (IsAcceptableWordStart(mAcc, text, 0)) {
        return *this;
      }
    } else {
      // The origin might start a word, so search from just before it.
      --wordStart;
    }
  }
  TextLeafPoint lineStart = FindLineStartSameAcc(eDirNext, aIncludeOrigin);
  if (lineStart) {
    // A word never starts with a line feed character. If there are multiple
    // consecutive line feed characters, lineStart will point at the second of
    // them. Skip this and any subsequent consecutive line feed.
    for (; lineStart.mOffset < static_cast<int32_t>(text.Length()) &&
           text.CharAt(lineStart.mOffset) == '\n';
         ++lineStart.mOffset) {
    }
    if (lineStart.mOffset == static_cast<int32_t>(text.Length())) {
      // There's no line start for our purposes.
      lineStart = TextLeafPoint();
    }
  }
  // Keep walking forward until we find an acceptable word start.
  intl::WordBreakIteratorUtf16 wordBreakIter(text);
  Maybe<uint32_t> nextBreak = wordBreakIter.Seek(wordStart);
  for (;;) {
    if (!nextBreak || *nextBreak == text.Length()) {
      if (lineStart) {
        // A line start always starts a new word.
        return lineStart;
      }
      return TextLeafPoint();
    }
    wordStart = AssertedCast<int32_t>(*nextBreak);
    if (lineStart && wordStart > lineStart.mOffset) {
      // A line start always starts a new word.
      return lineStart;
    }
    if (IsAcceptableWordStart(mAcc, text, wordStart)) {
      break;
    }
    nextBreak = wordBreakIter.Next();
  }
  return TextLeafPoint(mAcc, wordStart);
}

bool TextLeafPoint::IsCaretAtEndOfLine() const {
  MOZ_ASSERT(IsCaret());
  if (LocalAccessible* acc = mAcc->AsLocal()) {
    HyperTextAccessible* ht = HyperTextFor(acc);
    if (!ht) {
      return false;
    }
    // Use HyperTextAccessible::IsCaretAtEndOfLine. Eventually, we'll want to
    // move that code into TextLeafPoint, but existing code depends on it living
    // in HyperTextAccessible (including caret events).
    return ht->IsCaretAtEndOfLine();
  }
  return mAcc->AsRemote()->Document()->IsCaretAtEndOfLine();
}

TextLeafPoint TextLeafPoint::ActualizeCaret(bool aAdjustAtEndOfLine) const {
  MOZ_ASSERT(IsCaret());
  HyperTextAccessibleBase* ht;
  int32_t htOffset;
  if (LocalAccessible* acc = mAcc->AsLocal()) {
    // Use HyperTextAccessible::CaretOffset. Eventually, we'll want to move
    // that code into TextLeafPoint, but existing code depends on it living in
    // HyperTextAccessible (including caret events).
    ht = HyperTextFor(acc);
    if (!ht) {
      return TextLeafPoint();
    }
    htOffset = ht->CaretOffset();
    if (htOffset == -1) {
      return TextLeafPoint();
    }
  } else {
    // Ideally, we'd cache the caret as a leaf, but our events are based on
    // HyperText for now.
    std::tie(ht, htOffset) = mAcc->AsRemote()->Document()->GetCaret();
    if (!ht) {
      return TextLeafPoint();
    }
  }
  if (aAdjustAtEndOfLine && htOffset > 0 && IsCaretAtEndOfLine()) {
    // It is the same character offset when the caret is visually at the very
    // end of a line or the start of a new line (soft line break). Getting text
    // at the line should provide the line with the visual caret. Otherwise,
    // screen readers will announce the wrong line as the user presses up or
    // down arrow and land at the end of a line.
    --htOffset;
  }
  return ht->ToTextLeafPoint(htOffset);
}

TextLeafPoint TextLeafPoint::FindBoundary(AccessibleTextBoundary aBoundaryType,
                                          nsDirection aDirection,
                                          bool aIncludeOrigin) const {
  if (IsCaret()) {
    if (aBoundaryType == nsIAccessibleText::BOUNDARY_CHAR) {
      if (IsCaretAtEndOfLine()) {
        // The caret is at the end of the line. Return no character.
        return ActualizeCaret(/* aAdjustAtEndOfLine */ false);
      }
    }
    return ActualizeCaret().FindBoundary(aBoundaryType, aDirection,
                                         aIncludeOrigin);
  }
  if (aBoundaryType == nsIAccessibleText::BOUNDARY_LINE_END) {
    return FindLineEnd(aDirection, aIncludeOrigin);
  }
  if (aBoundaryType == nsIAccessibleText::BOUNDARY_WORD_END) {
    return FindWordEnd(aDirection, aIncludeOrigin);
  }
  if ((aBoundaryType == nsIAccessibleText::BOUNDARY_LINE_START ||
       aBoundaryType == nsIAccessibleText::BOUNDARY_PARAGRAPH) &&
      aIncludeOrigin && aDirection == eDirPrevious && IsEmptyLastLine()) {
    // If we're at an empty line at the end of an Accessible,  we don't want to
    // walk into the previous line. For example, this can happen if the caret
    // is positioned on an empty line at the end of a textarea.
    return *this;
  }
  if (aBoundaryType == nsIAccessibleText::BOUNDARY_CHAR && aIncludeOrigin) {
    return *this;
  }
  TextLeafPoint searchFrom = *this;
  bool includeOrigin = aIncludeOrigin;
  for (;;) {
    TextLeafPoint boundary;
    // Search for the boundary within the current Accessible.
    switch (aBoundaryType) {
      case nsIAccessibleText::BOUNDARY_CHAR:
        if (aDirection == eDirPrevious && searchFrom.mOffset > 0) {
          boundary.mAcc = searchFrom.mAcc;
          boundary.mOffset = searchFrom.mOffset - 1;
        } else if (aDirection == eDirNext) {
          if (includeOrigin) {
            // We've moved to the next leaf. That means we've set the offset
            // to 0, so we're already at the next character.
            boundary = searchFrom;
          } else if (searchFrom.mOffset + 1 <
                     static_cast<int32_t>(
                         nsAccUtils::TextLength(searchFrom.mAcc))) {
            boundary.mAcc = searchFrom.mAcc;
            boundary.mOffset = searchFrom.mOffset + 1;
          }
        }
        break;
      case nsIAccessibleText::BOUNDARY_WORD_START:
        if (aDirection == eDirPrevious) {
          boundary = searchFrom.FindPrevWordStartSameAcc(includeOrigin);
        } else {
          boundary = searchFrom.FindNextWordStartSameAcc(includeOrigin);
        }
        break;
      case nsIAccessibleText::BOUNDARY_LINE_START:
        boundary = searchFrom.FindLineStartSameAcc(aDirection, includeOrigin);
        break;
      case nsIAccessibleText::BOUNDARY_PARAGRAPH:
        boundary = searchFrom.FindParagraphSameAcc(aDirection, includeOrigin);
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
      return TextLeafPoint(
          searchFrom.mAcc,
          aDirection == eDirPrevious
              ? 0
              : static_cast<int32_t>(nsAccUtils::TextLength(searchFrom.mAcc)));
    }
    searchFrom.mAcc = acc;
    // When searching backward, search from the end of the text in the
    // Accessible. When searching forward, search from the start of the text.
    searchFrom.mOffset = aDirection == eDirPrevious
                             ? static_cast<int32_t>(nsAccUtils::TextLength(acc))
                             : 0;
    // The start/end of the Accessible might be a boundary. If so, we must stop
    // on it.
    includeOrigin = true;
  }
  MOZ_ASSERT_UNREACHABLE();
  return TextLeafPoint();
}

TextLeafPoint TextLeafPoint::FindLineEnd(nsDirection aDirection,
                                         bool aIncludeOrigin) const {
  if (aDirection == eDirPrevious && IsEmptyLastLine()) {
    // If we're at an empty line at the end of an Accessible,  we don't want to
    // walk into the previous line. For example, this can happen if the caret
    // is positioned on an empty line at the end of a textarea.
    // Because we want the line end, we must walk back to the line feed
    // character.
    return FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
  }
  if (aIncludeOrigin && IsLineFeedChar()) {
    return *this;
  }
  if (aDirection == eDirPrevious && !aIncludeOrigin) {
    // If there is a line feed immediately before us, return that.
    TextLeafPoint prevChar =
        FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
    if (prevChar.IsLineFeedChar()) {
      return prevChar;
    }
  }
  TextLeafPoint searchFrom = *this;
  if (aDirection == eDirNext && (IsLineFeedChar() || IsEmptyLastLine())) {
    // If we search for the next line start from a line feed, we'll get the
    // character immediately following the line feed. We actually want the
    // next line start after that. Skip the line feed.
    searchFrom = FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirNext);
  }
  TextLeafPoint lineStart = searchFrom.FindBoundary(
      nsIAccessibleText::BOUNDARY_LINE_START, aDirection, aIncludeOrigin);
  // If there is a line feed before this line start (at the end of the previous
  // line), we must return that.
  TextLeafPoint prevChar = lineStart.FindBoundary(
      nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious, false);
  if (prevChar && prevChar.IsLineFeedChar()) {
    return prevChar;
  }
  return lineStart;
}

bool TextLeafPoint::IsSpace() const {
  return GetWordBreakClass(GetChar()) == eWbcSpace;
}

TextLeafPoint TextLeafPoint::FindWordEnd(nsDirection aDirection,
                                         bool aIncludeOrigin) const {
  char16_t origChar = GetChar();
  const bool origIsSpace = GetWordBreakClass(origChar) == eWbcSpace;
  bool prevIsSpace = false;
  if (aDirection == eDirPrevious || (aIncludeOrigin && origIsSpace) ||
      !origChar) {
    TextLeafPoint prev =
        FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious, false);
    if (aDirection == eDirPrevious && prev == *this) {
      return *this;  // Can't go any further.
    }
    prevIsSpace = prev.IsSpace();
    if (aIncludeOrigin && origIsSpace && !prevIsSpace) {
      // The origin is space, but the previous character is not. This means
      // we're at the end of a word.
      return *this;
    }
  }
  TextLeafPoint boundary = *this;
  if (aDirection == eDirPrevious && !prevIsSpace) {
    // If there isn't space immediately before us, first find the start of the
    // previous word.
    boundary = FindBoundary(nsIAccessibleText::BOUNDARY_WORD_START,
                            eDirPrevious, aIncludeOrigin);
  } else if (aDirection == eDirNext &&
             (origIsSpace || (!origChar && prevIsSpace))) {
    // We're within the space at the end of the word. Skip over the space. We
    // can do that by searching for the next word start.
    boundary =
        FindBoundary(nsIAccessibleText::BOUNDARY_WORD_START, eDirNext, false);
    if (boundary.IsSpace()) {
      // The next word starts with a space. This can happen if there is a space
      // after or at the start of a block element.
      return boundary;
    }
  }
  if (aDirection == eDirNext) {
    boundary = boundary.FindBoundary(nsIAccessibleText::BOUNDARY_WORD_START,
                                     eDirNext, aIncludeOrigin);
  }
  // At this point, boundary is either the start of a word or at a space. A
  // word ends at the beginning of consecutive space. Therefore, skip back to
  // the start of any space before us.
  TextLeafPoint prev = boundary;
  for (;;) {
    prev = prev.FindBoundary(nsIAccessibleText::BOUNDARY_CHAR, eDirPrevious);
    if (prev == boundary) {
      break;  // Can't go any further.
    }
    if (!prev.IsSpace()) {
      break;
    }
    boundary = prev;
  }
  return boundary;
}

TextLeafPoint TextLeafPoint::FindParagraphSameAcc(nsDirection aDirection,
                                                  bool aIncludeOrigin) const {
  if (mAcc->IsTextLeaf() &&
      // We don't want to copy strings unnecessarily. See below for the context
      // of these individual conditions.
      ((aIncludeOrigin && mOffset > 0) || aDirection == eDirNext ||
       mOffset >= 2)) {
    // If there is a line feed, a new paragraph begins after it.
    nsAutoString text;
    mAcc->AppendTextTo(text);
    if (aIncludeOrigin && mOffset > 0 && text.CharAt(mOffset - 1) == '\n') {
      return TextLeafPoint(mAcc, mOffset);
    }
    int32_t lfOffset = -1;
    if (aDirection == eDirNext) {
      lfOffset = text.FindChar('\n', mOffset);
    } else if (mOffset >= 2) {
      // A line feed at mOffset - 1 means the origin begins a new paragraph,
      // but we already handled aIncludeOrigin above. Therefore, we search from
      // mOffset - 2.
      lfOffset = text.RFindChar('\n', mOffset - 2);
    }
    if (lfOffset != -1 && lfOffset + 1 < static_cast<int32_t>(text.Length())) {
      return TextLeafPoint(mAcc, lfOffset + 1);
    }
  }

  // Check whether this Accessible begins a paragraph.
  if ((!aIncludeOrigin && mOffset == 0) ||
      (aDirection == eDirNext && mOffset > 0)) {
    // The caller isn't interested in whether this Accessible begins a
    // paragraph.
    return TextLeafPoint();
  }
  Accessible* prevLeaf = PrevLeaf(mAcc);
  BlockRule blockRule;
  Pivot pivot(nsAccUtils::DocumentFor(mAcc));
  Accessible* prevBlock = pivot.Prev(mAcc, blockRule);
  // Check if we're the first leaf after a block element.
  if (prevBlock) {
    if (
        // If there's no previous leaf, we must be the first leaf after the
        // block.
        !prevLeaf ||
        // A block can be a leaf; e.g. an empty div or paragraph.
        prevBlock == prevLeaf) {
      return TextLeafPoint(mAcc, 0);
    }
    if (prevBlock->IsAncestorOf(mAcc)) {
      // We're inside the block.
      if (!prevBlock->IsAncestorOf(prevLeaf)) {
        // The previous leaf isn't inside the block. That means we're the first
        // leaf in the block.
        return TextLeafPoint(mAcc, 0);
      }
    } else {
      // We aren't inside the block, so the block ends before us.
      if (prevBlock->IsAncestorOf(prevLeaf)) {
        // The previous leaf is inside the block. That means we're the first
        // leaf after the block. This case is necessary because a block causes a
        // paragraph break both before and after it.
        return TextLeafPoint(mAcc, 0);
      }
    }
  }
  if (!prevLeaf || prevLeaf->IsHTMLBr()) {
    // We're the first leaf after a line break or the start of the document.
    return TextLeafPoint(mAcc, 0);
  }
  if (prevLeaf->IsTextLeaf()) {
    // There's a text leaf before us. Check if it ends with a line feed.
    nsAutoString text;
    prevLeaf->AppendTextTo(text, nsAccUtils::TextLength(prevLeaf) - 1, 1);
    if (text.CharAt(0) == '\n') {
      return TextLeafPoint(mAcc, 0);
    }
  }
  return TextLeafPoint();
}

bool TextLeafPoint::IsInSpellingError() const {
  if (LocalAccessible* acc = mAcc->AsLocal()) {
    auto domRanges = FindDOMSpellingErrors(acc, mOffset, mOffset + 1);
    // If there is a spelling error overlapping this character, we're in a
    // spelling error.
    return !domRanges.IsEmpty();
  }

  RemoteAccessible* acc = mAcc->AsRemote();
  MOZ_ASSERT(acc);
  if (!acc->mCachedFields) {
    return false;
  }
  auto spellingErrors =
      acc->mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::spelling);
  if (!spellingErrors) {
    return false;
  }
  size_t index;
  const bool foundOrigin = BinarySearch(
      *spellingErrors, 0, spellingErrors->Length(), mOffset, &index);
  // In spellingErrors, even indices are start offsets, odd indices are end
  // offsets.
  const bool foundStart = index % 2 == 0;
  if (foundOrigin) {
    // mOffset is a spelling error boundary. If it's a start offset, we're in a
    // spelling error.
    return foundStart;
  }
  // index points at the next spelling error boundary after mOffset.
  if (index == 0) {
    return false;  // No spelling errors before mOffset.
  }
  if (foundStart) {
    // We're not in a spelling error because it starts after mOffset.
    return false;
  }
  // A spelling error ends after mOffset.
  return true;
}

TextLeafPoint TextLeafPoint::FindSpellingErrorSameAcc(
    nsDirection aDirection, bool aIncludeOrigin) const {
  if (!aIncludeOrigin && mOffset == 0 && aDirection == eDirPrevious) {
    return TextLeafPoint();
  }
  if (LocalAccessible* acc = mAcc->AsLocal()) {
    // We want to find both start and end points, so we pass true for
    // aAllowAdjacent.
    auto domRanges =
        aDirection == eDirNext
            ? FindDOMSpellingErrors(acc, mOffset,
                                    nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT,
                                    /* aAllowAdjacent */ true)
            : FindDOMSpellingErrors(acc, 0, mOffset,
                                    /* aAllowAdjacent */ true);
    nsINode* node = acc->GetNode();
    if (aDirection == eDirNext) {
      for (nsRange* domRange : domRanges) {
        if (domRange->GetStartContainer() == node) {
          int32_t matchOffset = static_cast<int32_t>(ContentToRenderedOffset(
              acc, static_cast<int32_t>(domRange->StartOffset())));
          if ((aIncludeOrigin && matchOffset == mOffset) ||
              matchOffset > mOffset) {
            return TextLeafPoint(mAcc, matchOffset);
          }
        }
        if (domRange->GetEndContainer() == node) {
          int32_t matchOffset = static_cast<int32_t>(ContentToRenderedOffset(
              acc, static_cast<int32_t>(domRange->EndOffset())));
          if ((aIncludeOrigin && matchOffset == mOffset) ||
              matchOffset > mOffset) {
            return TextLeafPoint(mAcc, matchOffset);
          }
        }
      }
    } else {
      for (nsRange* domRange : Reversed(domRanges)) {
        if (domRange->GetEndContainer() == node) {
          int32_t matchOffset = static_cast<int32_t>(ContentToRenderedOffset(
              acc, static_cast<int32_t>(domRange->EndOffset())));
          if ((aIncludeOrigin && matchOffset == mOffset) ||
              matchOffset < mOffset) {
            return TextLeafPoint(mAcc, matchOffset);
          }
        }
        if (domRange->GetStartContainer() == node) {
          int32_t matchOffset = static_cast<int32_t>(ContentToRenderedOffset(
              acc, static_cast<int32_t>(domRange->StartOffset())));
          if ((aIncludeOrigin && matchOffset == mOffset) ||
              matchOffset < mOffset) {
            return TextLeafPoint(mAcc, matchOffset);
          }
        }
      }
    }
    return TextLeafPoint();
  }

  RemoteAccessible* acc = mAcc->AsRemote();
  MOZ_ASSERT(acc);
  if (!acc->mCachedFields) {
    return TextLeafPoint();
  }
  auto spellingErrors =
      acc->mCachedFields->GetAttribute<nsTArray<int32_t>>(nsGkAtoms::spelling);
  if (!spellingErrors) {
    return TextLeafPoint();
  }
  size_t index;
  if (BinarySearch(*spellingErrors, 0, spellingErrors->Length(), mOffset,
                   &index)) {
    // mOffset is in spellingErrors.
    if (aIncludeOrigin) {
      return *this;
    }
    if (aDirection == eDirNext) {
      // We don't want the origin, so move to the next spelling error boundary
      // after mOffset.
      ++index;
    }
  }
  // index points at the next spelling error boundary after mOffset.
  if (aDirection == eDirNext) {
    if (spellingErrors->Length() == index) {
      return TextLeafPoint();  // No spelling error boundary after us.
    }
    return TextLeafPoint(mAcc, (*spellingErrors)[index]);
  }
  if (index == 0) {
    return TextLeafPoint();  // No spelling error boundary before us.
  }
  // Decrement index so it points at a spelling error boundary before mOffset.
  --index;
  if ((*spellingErrors)[index] == -1) {
    MOZ_ASSERT(index == 0);
    // A spelling error starts before mAcc.
    return TextLeafPoint();
  }
  return TextLeafPoint(mAcc, (*spellingErrors)[index]);
}

/* static */
nsTArray<int32_t> TextLeafPoint::GetSpellingErrorOffsets(
    LocalAccessible* aAcc) {
  nsINode* node = aAcc->GetNode();
  auto domRanges = FindDOMSpellingErrors(
      aAcc, 0, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT);
  // Our offsets array will contain two offsets for each range: one for the
  // start, one for the end. That is, the array is of the form:
  // [r1start, r1end, r2start, r2end, ...]
  nsTArray<int32_t> offsets(domRanges.Length() * 2);
  for (nsRange* domRange : domRanges) {
    if (domRange->GetStartContainer() == node) {
      offsets.AppendElement(static_cast<int32_t>(ContentToRenderedOffset(
          aAcc, static_cast<int32_t>(domRange->StartOffset()))));
    } else {
      // This range overlaps aAcc, but starts before it.
      // This can only happen for the first range.
      MOZ_ASSERT(domRange == *domRanges.begin() && offsets.IsEmpty());
      // Using -1 here means this won't be treated as the start of a spelling
      // error range, while still indicating that we're within a spelling error.
      offsets.AppendElement(-1);
    }
    if (domRange->GetEndContainer() == node) {
      offsets.AppendElement(static_cast<int32_t>(ContentToRenderedOffset(
          aAcc, static_cast<int32_t>(domRange->EndOffset()))));
    } else {
      // This range overlaps aAcc, but ends after it.
      // This can only happen for the last range.
      MOZ_ASSERT(domRange == *domRanges.rbegin());
      // We don't append -1 here because this would just make things harder for
      // a binary search.
    }
  }
  return offsets;
}

/* static */
void TextLeafPoint::UpdateCachedSpellingError(dom::Document* aDocument,
                                              const nsRange& aRange) {
  DocAccessible* docAcc = GetExistingDocAccessible(aDocument);
  if (!docAcc) {
    return;
  }
  LocalAccessible* startAcc = docAcc->GetAccessible(aRange.GetStartContainer());
  LocalAccessible* endAcc = docAcc->GetAccessible(aRange.GetEndContainer());
  if (!startAcc || !endAcc) {
    return;
  }
  for (Accessible* acc = startAcc; acc; acc = NextLeaf(acc)) {
    if (acc->IsTextLeaf()) {
      docAcc->QueueCacheUpdate(acc->AsLocal(), CacheDomain::Spelling);
    }
    if (acc == endAcc) {
      // Subtle: We check this here rather than in the loop condition because
      // we want to include endAcc but stop once we reach it. Putting it in the
      // loop condition would mean we stop at endAcc, but we would also exclude
      // it; i.e. we wouldn't push the cache for it.
      break;
    }
  }
}

already_AddRefed<AccAttributes> TextLeafPoint::GetTextAttributesLocalAcc(
    bool aIncludeDefaults) const {
  LocalAccessible* acc = mAcc->AsLocal();
  MOZ_ASSERT(acc);
  MOZ_ASSERT(acc->IsText());
  // TextAttrsMgr wants a HyperTextAccessible.
  LocalAccessible* parent = acc->LocalParent();
  HyperTextAccessible* hyperAcc = parent->AsHyperText();
  MOZ_ASSERT(hyperAcc);
  RefPtr<AccAttributes> attributes = new AccAttributes();
  if (hyperAcc) {
    TextAttrsMgr mgr(hyperAcc, aIncludeDefaults, acc,
                     acc ? acc->IndexInParent() : -1);
    mgr.GetAttributes(attributes, nullptr, nullptr);
  }
  return attributes.forget();
}

already_AddRefed<AccAttributes> TextLeafPoint::GetTextAttributes(
    bool aIncludeDefaults) const {
  if (!mAcc->IsText()) {
    return nullptr;
  }
  RefPtr<AccAttributes> attrs;
  if (mAcc->IsLocal()) {
    attrs = GetTextAttributesLocalAcc(aIncludeDefaults);
  } else {
    attrs = new AccAttributes();
    if (aIncludeDefaults) {
      Accessible* parent = mAcc->Parent();
      if (parent && parent->IsRemote() && parent->IsHyperText()) {
        if (auto defAttrs = parent->AsRemote()->GetCachedTextAttributes()) {
          defAttrs->CopyTo(attrs);
        }
      }
    }
    if (auto thisAttrs = mAcc->AsRemote()->GetCachedTextAttributes()) {
      thisAttrs->CopyTo(attrs);
    }
  }
  if (IsInSpellingError()) {
    attrs->SetAttribute(nsGkAtoms::invalid, nsGkAtoms::spelling);
  }
  return attrs.forget();
}

TextLeafPoint TextLeafPoint::FindTextAttrsStart(nsDirection aDirection,
                                                bool aIncludeOrigin) const {
  if (IsCaret()) {
    return ActualizeCaret().FindTextAttrsStart(aDirection, aIncludeOrigin);
  }
  const bool isRemote = mAcc->IsRemote();
  RefPtr<const AccAttributes> lastAttrs =
      isRemote ? mAcc->AsRemote()->GetCachedTextAttributes()
               : GetTextAttributesLocalAcc();
  if (aIncludeOrigin && aDirection == eDirNext && mOffset == 0) {
    // Even when searching forward, the only way to know whether the origin is
    // the start of a text attrs run is to compare with the previous sibling.
    // Anything other than text breaks an attrs run.
    TextLeafPoint point;
    point.mAcc = mAcc->PrevSibling();
    if (!point.mAcc || !point.mAcc->IsText()) {
      return *this;
    }
    // For RemoteAccessible, we can get attributes from the cache without any
    // calculation or copying.
    RefPtr<const AccAttributes> attrs =
        isRemote ? point.mAcc->AsRemote()->GetCachedTextAttributes()
                 : point.GetTextAttributesLocalAcc();
    if (attrs && lastAttrs && !attrs->Equal(lastAttrs)) {
      return *this;
    }
  }
  TextLeafPoint lastPoint = *this;
  for (;;) {
    if (TextLeafPoint spelling = lastPoint.FindSpellingErrorSameAcc(
            aDirection, aIncludeOrigin && lastPoint.mAcc == mAcc)) {
      // A spelling error starts or ends somewhere in the Accessible we're
      // considering. This causes an attribute change, so return that point.
      return spelling;
    }
    TextLeafPoint point;
    point.mAcc = aDirection == eDirNext ? lastPoint.mAcc->NextSibling()
                                        : lastPoint.mAcc->PrevSibling();
    if (!point.mAcc || !point.mAcc->IsText()) {
      break;
    }
    RefPtr<const AccAttributes> attrs =
        isRemote ? point.mAcc->AsRemote()->GetCachedTextAttributes()
                 : point.GetTextAttributesLocalAcc();
    if (attrs && lastAttrs && !attrs->Equal(lastAttrs)) {
      // The attributes change here. If we're moving forward, we want to
      // return this point. If we're moving backward, we've now moved before
      // the start of the attrs run containing the origin, so return that start
      // point; i.e. the start of the last Accessible we hit.
      if (aDirection == eDirPrevious) {
        point = lastPoint;
        point.mOffset = 0;
      }
      if (!aIncludeOrigin && point == *this) {
        MOZ_ASSERT(aDirection == eDirPrevious);
        // The origin is the start of an attrs run, but the caller doesn't want
        // the origin included.
        continue;
      }
      return point;
    }
    lastPoint = point;
    if (aDirection == eDirPrevious) {
      // On the next iteration, we want to search for spelling errors from the
      // end of this Accessible.
      lastPoint.mOffset =
          static_cast<int32_t>(nsAccUtils::TextLength(point.mAcc));
    }
    lastAttrs = attrs;
  }
  // We couldn't move any further. Use the start/end.
  return TextLeafPoint(
      lastPoint.mAcc,
      aDirection == eDirPrevious
          ? 0
          : static_cast<int32_t>(nsAccUtils::TextLength(lastPoint.mAcc)));
}

LayoutDeviceIntRect TextLeafPoint::CharBounds() {
  if (mAcc && !mAcc->IsText()) {
    // If we're dealing with an empty container, return the
    // accessible's non-text bounds.
    return mAcc->Bounds();
  }

  if (!mAcc || !mAcc->IsRemote() || !mAcc->AsRemote() ||
      !mAcc->AsRemote()->mCachedFields) {
    return LayoutDeviceIntRect();
  }

  RemoteAccessible* acc = mAcc->AsRemote();
  if (Maybe<nsTArray<nsRect>> charBounds = acc->GetCachedCharData()) {
    if (mOffset < static_cast<int32_t>(charBounds->Length())) {
      return acc->BoundsWithOffset(Some(charBounds->ElementAt(mOffset)));
    }
    // It is valid for a client to call this with an offset 1 after the last
    // character because of the insertion point at the end of text boxes.
    MOZ_ASSERT(mOffset == static_cast<int32_t>(charBounds->Length()));
  }

  return LayoutDeviceIntRect();
}

bool TextLeafPoint::ContainsPoint(int32_t aX, int32_t aY) {
  if (mAcc && !mAcc->IsText()) {
    // If we're dealing with an empty embedded object, use the
    // accessible's non-text bounds.
    return mAcc->Bounds().Contains(aX, aY);
  }

  return CharBounds().Contains(aX, aY);
}

}  // namespace mozilla::a11y

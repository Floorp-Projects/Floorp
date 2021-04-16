/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozInlineSpellWordUtil.h"

#include <algorithm>
#include <utility>

#include "mozilla/BinarySearch.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/Logging.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Element.h"

#include "nsDebug.h"
#include "nsAtom.h"
#include "nsComponentManagerUtils.h"
#include "nsUnicodeProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsRange.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"

using namespace mozilla;

static LazyLogModule sInlineSpellWordUtilLog{"InlineSpellWordUtil"};

// IsIgnorableCharacter
//
//    These characters are ones that we should ignore in input.

inline bool IsIgnorableCharacter(char ch) {
  return (ch == static_cast<char>(0xAD));  // SOFT HYPHEN
}

inline bool IsIgnorableCharacter(char16_t ch) {
  return (ch == 0xAD ||   // SOFT HYPHEN
          ch == 0x1806);  // MONGOLIAN TODO SOFT HYPHEN
}

// IsConditionalPunctuation
//
//    Some characters (like apostrophes) require characters on each side to be
//    part of a word, and are otherwise punctuation.

inline bool IsConditionalPunctuation(char ch) {
  return (ch == '\'' ||                    // RIGHT SINGLE QUOTATION MARK
          ch == static_cast<char>(0xB7));  // MIDDLE DOT
}

inline bool IsConditionalPunctuation(char16_t ch) {
  return (ch == '\'' || ch == 0x2019 ||  // RIGHT SINGLE QUOTATION MARK
          ch == 0x00B7);                 // MIDDLE DOT
}

static bool IsAmbiguousDOMWordSeprator(char16_t ch) {
  // This class may be CHAR_CLASS_SEPARATOR, but it depends on context.
  return (ch == '@' || ch == ':' || ch == '.' || ch == '/' || ch == '-' ||
          IsConditionalPunctuation(ch));
}

static bool IsAmbiguousDOMWordSeprator(char ch) {
  // This class may be CHAR_CLASS_SEPARATOR, but it depends on context.
  return IsAmbiguousDOMWordSeprator(static_cast<char16_t>(ch));
}

// IsDOMWordSeparator
//
//    Determines if the given character should be considered as a DOM Word
//    separator. Basically, this is whitespace, although it could also have
//    certain punctuation that we know ALWAYS breaks words. This is important.
//    For example, we can't have any punctuation that could appear in a URL
//    or email address in this, because those need to always fit into a single
//    DOM word.

static bool IsDOMWordSeparator(char ch) {
  // simple spaces or no-break space
  return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
          ch == static_cast<char>(0xA0));
}

static bool IsDOMWordSeparator(char16_t ch) {
  // simple spaces
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') return true;

  // complex spaces - check only if char isn't ASCII (uncommon)
  if (ch >= 0xA0 && (ch == 0x00A0 ||  // NO-BREAK SPACE
                     ch == 0x2002 ||  // EN SPACE
                     ch == 0x2003 ||  // EM SPACE
                     ch == 0x2009 ||  // THIN SPACE
                     ch == 0x3000))   // IDEOGRAPHIC SPACE
    return true;

  // otherwise not a space
  return false;
}

// static
Maybe<mozInlineSpellWordUtil> mozInlineSpellWordUtil::Create(
    const TextEditor& aTextEditor) {
  dom::Document* document = aTextEditor.GetDocument();
  if (NS_WARN_IF(!document)) {
    return Nothing();
  }

  const bool isContentEditableOrDesignMode = !!aTextEditor.AsHTMLEditor();

  // Find the root node for the editor. For contenteditable the mRootNode could
  // change to shadow root if the begin and end are inside the shadowDOM.
  nsINode* rootNode = aTextEditor.GetRoot();
  if (NS_WARN_IF(!rootNode)) {
    return Nothing();
  }

  mozInlineSpellWordUtil util{*document, isContentEditableOrDesignMode,
                              *rootNode};
  return Some(std::move(util));
}

static inline bool IsSpellCheckingTextNode(nsINode* aNode) {
  nsIContent* parent = aNode->GetParent();
  if (parent &&
      parent->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::style))
    return false;
  return aNode->IsText();
}

typedef void (*OnLeaveNodeFunPtr)(nsINode* aNode, void* aClosure);

// Find the next node in the DOM tree in preorder.
// Calls OnLeaveNodeFunPtr when the traversal leaves a node, which is
// why we can't just use GetNextNode here, sadly.
static nsINode* FindNextNode(nsINode* aNode, const nsINode* aRoot,
                             OnLeaveNodeFunPtr aOnLeaveNode, void* aClosure) {
  MOZ_ASSERT(aNode, "Null starting node?");

  nsINode* next = aNode->GetFirstChild();
  if (next) return next;

  // Don't look at siblings or otherwise outside of aRoot
  if (aNode == aRoot) return nullptr;

  next = aNode->GetNextSibling();
  if (next) return next;

  // Go up
  for (;;) {
    if (aOnLeaveNode) {
      aOnLeaveNode(aNode, aClosure);
    }

    next = aNode->GetParent();
    if (next == aRoot || !next) return nullptr;
    aNode = next;

    next = aNode->GetNextSibling();
    if (next) return next;
  }
}

// aNode is not a text node. Find the first text node starting at aNode/aOffset
// in a preorder DOM traversal.
static nsINode* FindNextTextNode(nsINode* aNode, int32_t aOffset,
                                 const nsINode* aRoot) {
  MOZ_ASSERT(aNode, "Null starting node?");
  NS_ASSERTION(!IsSpellCheckingTextNode(aNode),
               "FindNextTextNode should start with a non-text node");

  nsINode* checkNode;
  // Need to start at the aOffset'th child
  nsIContent* child = aNode->GetChildAt_Deprecated(aOffset);

  if (child) {
    checkNode = child;
  } else {
    // aOffset was beyond the end of the child list.
    // goto next node after the last descendant of aNode in
    // a preorder DOM traversal.
    checkNode = aNode->GetNextNonChildNode(aRoot);
  }

  while (checkNode && !IsSpellCheckingTextNode(checkNode)) {
    checkNode = checkNode->GetNextNode(aRoot);
  }
  return checkNode;
}

// mozInlineSpellWordUtil::SetPositionAndEnd
//
//    We have two ranges "hard" and "soft". The hard boundary is simply
//    the scope of the root node. The soft boundary is that which is set
//    by the caller of this class by calling this function. If this function is
//    not called, the soft boundary is the same as the hard boundary.
//
//    When we reach the soft boundary (mSoftEnd), we keep
//    going until we reach the end of a word. This allows the caller to set the
//    end of the range to anything, and we will always check whole multiples of
//    words. When we reach the hard boundary we stop no matter what.
//
//    There is no beginning soft boundary. This is because we only go to the
//    previous node once, when finding the previous word boundary in
//    SetPosition(). You might think of the soft boundary as being this initial
//    position.

nsresult mozInlineSpellWordUtil::SetPositionAndEnd(nsINode* aPositionNode,
                                                   int32_t aPositionOffset,
                                                   nsINode* aEndNode,
                                                   int32_t aEndOffset) {
  MOZ_LOG(sInlineSpellWordUtilLog, LogLevel::Debug,
          ("%s: pos=(%p, %i), end=(%p, %i)", __FUNCTION__, aPositionNode,
           aPositionOffset, aEndNode, aEndOffset));

  MOZ_ASSERT(aPositionNode, "Null begin node?");
  MOZ_ASSERT(aEndNode, "Null end node?");

  NS_ASSERTION(mRootNode, "Not initialized");

  // Find a appropriate root if we are dealing with contenteditable nodes which
  // are in the shadow DOM.
  if (mIsContentEditableOrDesignMode) {
    nsINode* rootNode = aPositionNode->SubtreeRoot();
    if (rootNode != aEndNode->SubtreeRoot()) {
      return NS_ERROR_FAILURE;
    }

    if (mozilla::dom::ShadowRoot::FromNode(rootNode)) {
      mRootNode = rootNode;
    }
  }

  InvalidateWords();

  if (!IsSpellCheckingTextNode(aPositionNode)) {
    // Start at the start of the first text node after aNode/aOffset.
    aPositionNode = FindNextTextNode(aPositionNode, aPositionOffset, mRootNode);
    aPositionOffset = 0;
  }
  mSoftBegin = NodeOffset(aPositionNode, aPositionOffset);

  if (!IsSpellCheckingTextNode(aEndNode)) {
    // End at the start of the first text node after aEndNode/aEndOffset.
    aEndNode = FindNextTextNode(aEndNode, aEndOffset, mRootNode);
    aEndOffset = 0;
  }
  mSoftEnd = NodeOffset(aEndNode, aEndOffset);

  nsresult rv = EnsureWords();
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t textOffset = MapDOMPositionToSoftTextOffset(mSoftBegin);
  if (textOffset < 0) {
    return NS_OK;
  }

  mNextWordIndex = FindRealWordContaining(textOffset, HINT_END, true);
  return NS_OK;
}

nsresult mozInlineSpellWordUtil::EnsureWords() {
  if (mSoftTextValid) return NS_OK;
  AdjustSoftBeginAndBuildSoftText();

  mRealWords.Clear();
  Result<RealWords, nsresult> realWords = BuildRealWords();
  if (realWords.isErr()) {
    return realWords.unwrapErr();
  }

  mRealWords = realWords.unwrap();
  mSoftTextValid = true;
  return NS_OK;
}

nsresult mozInlineSpellWordUtil::MakeRangeForWord(const RealWord& aWord,
                                                  nsRange** aRange) const {
  NodeOffset begin =
      MapSoftTextOffsetToDOMPosition(aWord.mSoftTextOffset, HINT_BEGIN);
  NodeOffset end = MapSoftTextOffsetToDOMPosition(aWord.EndOffset(), HINT_END);
  return MakeRange(begin, end, aRange);
}
void mozInlineSpellWordUtil::MakeNodeOffsetRangeForWord(
    const RealWord& aWord, NodeOffsetRange* aNodeOffsetRange) {
  NodeOffset begin =
      MapSoftTextOffsetToDOMPosition(aWord.mSoftTextOffset, HINT_BEGIN);
  NodeOffset end = MapSoftTextOffsetToDOMPosition(aWord.EndOffset(), HINT_END);
  *aNodeOffsetRange = NodeOffsetRange(begin, end);
}

// mozInlineSpellWordUtil::GetRangeForWord

nsresult mozInlineSpellWordUtil::GetRangeForWord(nsINode* aWordNode,
                                                 int32_t aWordOffset,
                                                 nsRange** aRange) {
  // Set our soft end and start
  NodeOffset pt(aWordNode, aWordOffset);

  if (!mSoftTextValid || pt != mSoftBegin || pt != mSoftEnd) {
    InvalidateWords();
    mSoftBegin = mSoftEnd = pt;
    nsresult rv = EnsureWords();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  int32_t offset = MapDOMPositionToSoftTextOffset(pt);
  if (offset < 0) return MakeRange(pt, pt, aRange);
  int32_t wordIndex = FindRealWordContaining(offset, HINT_BEGIN, false);
  if (wordIndex < 0) return MakeRange(pt, pt, aRange);
  return MakeRangeForWord(mRealWords[wordIndex], aRange);
}

// This is to fix characters that the spellchecker may not like
static void NormalizeWord(const nsAString& aInput, int32_t aPos, int32_t aLen,
                          nsAString& aOutput) {
  aOutput.Truncate();
  for (int32_t i = 0; i < aLen; i++) {
    char16_t ch = aInput.CharAt(i + aPos);

    // remove ignorable characters from the word
    if (IsIgnorableCharacter(ch)) continue;

    // the spellchecker doesn't handle curly apostrophes in all languages
    if (ch == 0x2019) {  // RIGHT SINGLE QUOTATION MARK
      ch = '\'';
    }

    aOutput.Append(ch);
  }
}

// mozInlineSpellWordUtil::GetNextWord
//
//    FIXME-optimization: we shouldn't have to generate a range every single
//    time. It would be better if the inline spellchecker didn't require a
//    range unless the word was misspelled. This may or may not be possible.

bool mozInlineSpellWordUtil::GetNextWord(nsAString& aText,
                                         NodeOffsetRange* aNodeOffsetRange,
                                         bool* aSkipChecking) {
  MOZ_LOG(sInlineSpellWordUtilLog, LogLevel::Debug,
          ("%s: mNextWordIndex=%d", __FUNCTION__, mNextWordIndex));

  if (mNextWordIndex < 0 || mNextWordIndex >= int32_t(mRealWords.Length())) {
    mNextWordIndex = -1;
    *aSkipChecking = true;
    return false;
  }

  const RealWord& word = mRealWords[mNextWordIndex];
  MakeNodeOffsetRangeForWord(word, aNodeOffsetRange);
  ++mNextWordIndex;
  *aSkipChecking = !word.mCheckableWord;
  ::NormalizeWord(mSoftText, word.mSoftTextOffset, word.mLength, aText);

  MOZ_LOG(sInlineSpellWordUtilLog, LogLevel::Debug,
          ("%s: returning: %s (skip=%d)", __FUNCTION__,
           NS_ConvertUTF16toUTF8(aText).get(), *aSkipChecking));

  return true;
}

// mozInlineSpellWordUtil::MakeRange
//
//    Convenience function for creating a range over the current document.

nsresult mozInlineSpellWordUtil::MakeRange(NodeOffset aBegin, NodeOffset aEnd,
                                           nsRange** aRange) const {
  NS_ENSURE_ARG_POINTER(aBegin.mNode);
  if (!mDocument) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  RefPtr<nsRange> range = nsRange::Create(aBegin.mNode, aBegin.mOffset,
                                          aEnd.mNode, aEnd.mOffset, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  MOZ_ASSERT(range);
  range.forget(aRange);
  return NS_OK;
}

// static
already_AddRefed<nsRange> mozInlineSpellWordUtil::MakeRange(
    const NodeOffsetRange& aRange) {
  IgnoredErrorResult ignoredError;
  RefPtr<nsRange> range =
      nsRange::Create(aRange.Begin().Node(), aRange.Begin().Offset(),
                      aRange.End().Node(), aRange.End().Offset(), ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(), "Creating a range failed");
  return range.forget();
}

/*********** Word Splitting ************/

// classifies a given character in the DOM word
enum CharClass {
  CHAR_CLASS_WORD,
  CHAR_CLASS_SEPARATOR,
  CHAR_CLASS_END_OF_INPUT
};

// Encapsulates DOM-word to real-word splitting
template <class T>
struct MOZ_STACK_CLASS WordSplitState {
  const T& mDOMWordText;
  int32_t mDOMWordOffset;
  CharClass mCurCharClass;

  explicit WordSplitState(const T& aString)
      : mDOMWordText(aString),
        mDOMWordOffset(0),
        mCurCharClass(CHAR_CLASS_END_OF_INPUT) {}

  CharClass ClassifyCharacter(int32_t aIndex, bool aRecurse) const;
  void Advance();
  void AdvanceThroughSeparators();
  void AdvanceThroughWord();

  // Finds special words like email addresses and URLs that may start at the
  // current position, and returns their length, or 0 if not found. This allows
  // arbitrary word breaking rules to be used for these special entities, as
  // long as they can not contain whitespace.
  bool IsSpecialWord() const;

  // Similar to IsSpecialWord except that this takes a split word as
  // input. This checks for things that do not require special word-breaking
  // rules.
  bool ShouldSkipWord(int32_t aStart, int32_t aLength) const;

  // Finds the last sequence of DOM word separators before aBeforeOffset and
  // returns the offset to its first element.
  Maybe<int32_t> FindOffsetOfLastDOMWordSeparatorSequence(
      int32_t aBeforeOffset) const;

  char16_t GetUnicharAt(int32_t aIndex) const;
};

// WordSplitState::ClassifyCharacter
template <class T>
CharClass WordSplitState<T>::ClassifyCharacter(int32_t aIndex,
                                               bool aRecurse) const {
  NS_ASSERTION(aIndex >= 0 && aIndex <= int32_t(mDOMWordText.Length()),
               "Index out of range");
  if (aIndex == int32_t(mDOMWordText.Length())) return CHAR_CLASS_SEPARATOR;

  // this will classify the character, we want to treat "ignorable" characters
  // such as soft hyphens, and also ZWJ and ZWNJ as word characters.
  nsUGenCategory charCategory =
      mozilla::unicode::GetGenCategory(GetUnicharAt(aIndex));
  if (charCategory == nsUGenCategory::kLetter ||
      IsIgnorableCharacter(mDOMWordText[aIndex]) ||
      mDOMWordText[aIndex] == 0x200C /* ZWNJ */ ||
      mDOMWordText[aIndex] == 0x200D /* ZWJ */)
    return CHAR_CLASS_WORD;

  // If conditional punctuation is surrounded immediately on both sides by word
  // characters it also counts as a word character.
  if (IsConditionalPunctuation(mDOMWordText[aIndex])) {
    if (!aRecurse) {
      // not allowed to look around, this punctuation counts like a separator
      return CHAR_CLASS_SEPARATOR;
    }

    // check the left-hand character
    if (aIndex == 0) return CHAR_CLASS_SEPARATOR;
    if (ClassifyCharacter(aIndex - 1, false) != CHAR_CLASS_WORD)
      return CHAR_CLASS_SEPARATOR;
    // If the previous charatcer is a word-char, make sure that it's not a
    // special dot character.
    if (mDOMWordText[aIndex - 1] == '.') return CHAR_CLASS_SEPARATOR;

    // now we know left char is a word-char, check the right-hand character
    if (aIndex == int32_t(mDOMWordText.Length() - 1)) {
      return CHAR_CLASS_SEPARATOR;
    }

    if (ClassifyCharacter(aIndex + 1, false) != CHAR_CLASS_WORD)
      return CHAR_CLASS_SEPARATOR;
    // If the next charatcer is a word-char, make sure that it's not a
    // special dot character.
    if (mDOMWordText[aIndex + 1] == '.') return CHAR_CLASS_SEPARATOR;

    // char on either side is a word, this counts as a word
    return CHAR_CLASS_WORD;
  }

  // The dot character, if appearing at the end of a word, should
  // be considered part of that word.  Example: "etc.", or
  // abbreviations
  if (aIndex > 0 && mDOMWordText[aIndex] == '.' &&
      mDOMWordText[aIndex - 1] != '.' &&
      ClassifyCharacter(aIndex - 1, false) != CHAR_CLASS_WORD) {
    return CHAR_CLASS_WORD;
  }

  // all other punctuation
  if (charCategory == nsUGenCategory::kSeparator ||
      charCategory == nsUGenCategory::kOther ||
      charCategory == nsUGenCategory::kPunctuation ||
      charCategory == nsUGenCategory::kSymbol) {
    // Don't break on hyphens, as hunspell handles them on its own.
    if (aIndex > 0 && mDOMWordText[aIndex] == '-' &&
        mDOMWordText[aIndex - 1] != '-' &&
        ClassifyCharacter(aIndex - 1, false) == CHAR_CLASS_WORD) {
      // A hyphen is only meaningful as a separator inside a word
      // if the previous and next characters are a word character.
      if (aIndex == int32_t(mDOMWordText.Length()) - 1)
        return CHAR_CLASS_SEPARATOR;
      if (mDOMWordText[aIndex + 1] != '.' &&
          ClassifyCharacter(aIndex + 1, false) == CHAR_CLASS_WORD)
        return CHAR_CLASS_WORD;
    }
    return CHAR_CLASS_SEPARATOR;
  }

  // any other character counts as a word
  return CHAR_CLASS_WORD;
}

// WordSplitState::Advance
template <class T>
void WordSplitState<T>::Advance() {
  NS_ASSERTION(mDOMWordOffset >= 0, "Negative word index");
  NS_ASSERTION(mDOMWordOffset < (int32_t)mDOMWordText.Length(),
               "Length beyond end");

  mDOMWordOffset++;
  if (mDOMWordOffset >= (int32_t)mDOMWordText.Length())
    mCurCharClass = CHAR_CLASS_END_OF_INPUT;
  else
    mCurCharClass = ClassifyCharacter(mDOMWordOffset, true);
}

// WordSplitState::AdvanceThroughSeparators
template <class T>
void WordSplitState<T>::AdvanceThroughSeparators() {
  while (mCurCharClass == CHAR_CLASS_SEPARATOR) Advance();
}

// WordSplitState::AdvanceThroughWord
template <class T>
void WordSplitState<T>::AdvanceThroughWord() {
  while (mCurCharClass == CHAR_CLASS_WORD) Advance();
}

// WordSplitState::IsSpecialWord
template <class T>
bool WordSplitState<T>::IsSpecialWord() const {
  // Search for email addresses. We simply define these as any sequence of
  // characters with an '@' character in the middle. The DOM word is already
  // split on whitepace, so we know that everything to the end is the address
  int32_t firstColon = -1;
  for (int32_t i = mDOMWordOffset; i < int32_t(mDOMWordText.Length()); i++) {
    if (mDOMWordText[i] == '@') {
      // only accept this if there are unambiguous word characters (don't bother
      // recursing to disambiguate apostrophes) on each side. This prevents
      // classifying, e.g. "@home" as an email address

      // Use this condition to only accept words with '@' in the middle of
      // them. It works, but the inlinespellcker doesn't like this. The problem
      // is that you type "fhsgfh@" that's a misspelled word followed by a
      // symbol, but when you type another letter "fhsgfh@g" that first word
      // need to be unmarked misspelled. It doesn't do this. it only checks the
      // current position for potentially removing a spelling range.
      if (i > 0 && ClassifyCharacter(i - 1, false) == CHAR_CLASS_WORD &&
          i < (int32_t)mDOMWordText.Length() - 1 &&
          ClassifyCharacter(i + 1, false) == CHAR_CLASS_WORD) {
        return true;
      }
    } else if (mDOMWordText[i] == ':' && firstColon < 0) {
      firstColon = i;

      // If the first colon is followed by a slash, consider it a URL
      // This will catch things like asdf://foo.com
      if (firstColon < (int32_t)mDOMWordText.Length() - 1 &&
          mDOMWordText[firstColon + 1] == '/') {
        return true;
      }
    }
  }

  // Check the text before the first colon against some known protocols. It
  // is impossible to check against all protocols, especially since you can
  // plug in new protocols. We also don't want to waste time here checking
  // against a lot of obscure protocols.
  if (firstColon > mDOMWordOffset) {
    nsString protocol(
        Substring(mDOMWordText, mDOMWordOffset, firstColon - mDOMWordOffset));
    if (protocol.EqualsIgnoreCase("http") ||
        protocol.EqualsIgnoreCase("https") ||
        protocol.EqualsIgnoreCase("news") ||
        protocol.EqualsIgnoreCase("file") ||
        protocol.EqualsIgnoreCase("javascript") ||
        protocol.EqualsIgnoreCase("data") || protocol.EqualsIgnoreCase("ftp")) {
      return true;
    }
  }

  // not anything special
  return false;
}

// WordSplitState::ShouldSkipWord
template <class T>
bool WordSplitState<T>::ShouldSkipWord(int32_t aStart, int32_t aLength) const {
  int32_t last = aStart + aLength;

  // check to see if the word contains a digit
  for (int32_t i = aStart; i < last; i++) {
    if (mozilla::unicode::GetGenCategory(GetUnicharAt(i)) ==
        nsUGenCategory::kNumber) {
      return true;
    }
  }

  // not special
  return false;
}

template <class T>
Maybe<int32_t> WordSplitState<T>::FindOffsetOfLastDOMWordSeparatorSequence(
    const int32_t aBeforeOffset) const {
  for (int32_t i = aBeforeOffset - 1; i >= 0; --i) {
    if (IsDOMWordSeparator(mDOMWordText[i]) ||
        (!IsAmbiguousDOMWordSeprator(mDOMWordText[i]) &&
         ClassifyCharacter(i, true) == CHAR_CLASS_SEPARATOR)) {
      // Be greedy, find as many separators as we can
      for (int32_t j = i - 1; j >= 0; --j) {
        if (IsDOMWordSeparator(mDOMWordText[j]) ||
            (!IsAmbiguousDOMWordSeprator(mDOMWordText[j]) &&
             ClassifyCharacter(j, true) == CHAR_CLASS_SEPARATOR)) {
          i = j;
        } else {
          break;
        }
      }
      return Some(i);
    }
  }
  return Nothing();
}

template <>
char16_t WordSplitState<nsDependentSubstring>::GetUnicharAt(
    int32_t aIndex) const {
  return mDOMWordText[aIndex];
}

template <>
char16_t WordSplitState<nsDependentCSubstring>::GetUnicharAt(
    int32_t aIndex) const {
  return static_cast<char16_t>(static_cast<uint8_t>(mDOMWordText[aIndex]));
}

static inline bool IsBRElement(nsINode* aNode) {
  return aNode->IsHTMLElement(nsGkAtoms::br);
}

/**
 * Given a TextNode, finds the last sequence of DOM word separators before
 * aBeforeOffset and returns the offset to its first element.
 *
 * @param aContent the TextNode to check.
 * @param aBeforeOffset the offset in the TextNode before which we will search
 *        for the DOM separator. You can pass INT32_MAX to search the entire
 *        length of the string.
 */
static Maybe<int32_t> FindOffsetOfLastDOMWordSeparatorSequence(
    nsIContent* aContent, int32_t aBeforeOffset) {
  const nsTextFragment* textFragment = aContent->GetText();
  NS_ASSERTION(textFragment, "Where is our text?");
  int32_t end = std::min(aBeforeOffset, int32_t(textFragment->GetLength()));

  if (textFragment->Is2b()) {
    nsDependentSubstring targetText(textFragment->Get2b(), end);
    WordSplitState<nsDependentSubstring> state(targetText);
    return state.FindOffsetOfLastDOMWordSeparatorSequence(end);
  }

  nsDependentCSubstring targetText(textFragment->Get1b(), end);
  WordSplitState<nsDependentCSubstring> state(targetText);
  return state.FindOffsetOfLastDOMWordSeparatorSequence(end);
}

/**
 * Check if there's a DOM word separator before aBeforeOffset in this node.
 * Always returns true if it's a BR element.
 * aSeparatorOffset is set to the index of the first character in the last
 * separator if any is found (0 for BR elements).
 *
 * This function does not modify aSeparatorOffset when it returns false.
 */
static bool ContainsDOMWordSeparator(nsINode* aNode, int32_t aBeforeOffset,
                                     int32_t* aSeparatorOffset) {
  if (IsBRElement(aNode)) {
    *aSeparatorOffset = 0;
    return true;
  }

  if (!IsSpellCheckingTextNode(aNode)) return false;

  const Maybe<int32_t> separatorOffset =
      FindOffsetOfLastDOMWordSeparatorSequence(aNode->AsContent(),
                                               aBeforeOffset);
  if (separatorOffset) {
    *aSeparatorOffset = *separatorOffset;
    return true;
  }

  return false;
}

static bool IsBreakElement(nsINode* aNode) {
  if (!aNode->IsElement()) {
    return false;
  }

  dom::Element* element = aNode->AsElement();
  if (element->IsHTMLElement(nsGkAtoms::br)) {
    return true;
  }

  // If we don't have a frame, we don't consider ourselves a break
  // element.  In particular, words can span us.
  nsIFrame* frame = element->GetPrimaryFrame();
  if (!frame) {
    return false;
  }

  auto* disp = frame->StyleDisplay();
  // Anything that's not an inline element is a break element.
  // XXXbz should replaced inlines be break elements, though?
  // Also should inline-block and such be break elements?
  //
  // FIXME(emilio): We should teach the spell checker to deal with generated
  // content (it doesn't at all), then remove the IsListItem() check, as there
  // could be no marker, etc...
  return !disp->IsInlineFlow() || disp->IsListItem();
}

struct CheckLeavingBreakElementClosure {
  bool mLeftBreakElement;
};

static void CheckLeavingBreakElement(nsINode* aNode, void* aClosure) {
  CheckLeavingBreakElementClosure* cl =
      static_cast<CheckLeavingBreakElementClosure*>(aClosure);
  if (!cl->mLeftBreakElement && IsBreakElement(aNode)) {
    cl->mLeftBreakElement = true;
  }
}

void mozInlineSpellWordUtil::NormalizeWord(nsAString& aWord) {
  nsAutoString result;
  ::NormalizeWord(aWord, 0, aWord.Length(), result);
  aWord = result;
}

void mozInlineSpellWordUtil::AdjustSoftBeginAndBuildSoftText() {
  MOZ_LOG(sInlineSpellWordUtilLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // First we have to work backwards from mSoftBegin to find a text node
  // containing a DOM word separator, a non-inline-element
  // boundary, or the hard start node. That's where we'll start building the
  // soft string from.
  nsINode* node = mSoftBegin.mNode;
  int32_t firstOffsetInNode = 0;
  int32_t checkBeforeOffset = mSoftBegin.mOffset;
  while (node) {
    if (ContainsDOMWordSeparator(node, checkBeforeOffset, &firstOffsetInNode)) {
      if (node == mSoftBegin.mNode) {
        // If we find a word separator on the first node, look at the preceding
        // word on the text node as well.
        int32_t newOffset = 0;
        if (firstOffsetInNode > 0) {
          // Try to find the previous word boundary in the current node. If
          // we can't find one, start checking previous sibling nodes (if any
          // adjacent ones exist) to see if we can find any text nodes with
          // DOM word separators. We bail out as soon as we see a node that is
          // not a text node, or we run out of previous sibling nodes. In the
          // event that we simply cannot find any preceding word separator, the
          // offset is set to 0, and the soft text beginning node is set to the
          // "most previous" text node before the original starting node, or
          // kept at the original starting node if no previous text nodes exist.
          if (!ContainsDOMWordSeparator(node, firstOffsetInNode - 1,
                                        &newOffset)) {
            nsIContent* prevNode = node->GetPreviousSibling();
            while (prevNode && IsSpellCheckingTextNode(prevNode)) {
              mSoftBegin.mNode = prevNode;
              const Maybe<int32_t> separatorOffset =
                  FindOffsetOfLastDOMWordSeparatorSequence(prevNode, INT32_MAX);
              if (separatorOffset) {
                newOffset = *separatorOffset;
                break;
              }
              prevNode = prevNode->GetPreviousSibling();
            }
          }
        }
        firstOffsetInNode = newOffset;
        mSoftBegin.mOffset = newOffset;
      }
      break;
    }
    checkBeforeOffset = INT32_MAX;
    if (IsBreakElement(node)) {
      // Since GetPreviousContent follows tree *preorder*, we're about to
      // traverse up out of 'node'. Since node induces breaks (e.g., it's a
      // block), don't bother trying to look outside it, just stop now.
      break;
    }
    // GetPreviousContent below expects mRootNode to be an ancestor of node.
    if (!node->IsInclusiveDescendantOf(mRootNode)) {
      break;
    }
    node = node->GetPreviousContent(mRootNode);
  }

  // Now build up the string moving forward through the DOM until we reach
  // the soft end and *then* see a DOM word separator, a non-inline-element
  // boundary, or the hard end node.
  mSoftText.Truncate();
  mSoftTextDOMMapping.Clear();
  bool seenSoftEnd = false;
  // Leave this outside the loop so large heap string allocations can be reused
  // across iterations
  while (node) {
    if (node == mSoftEnd.mNode) {
      seenSoftEnd = true;
    }

    bool exit = false;
    if (IsSpellCheckingTextNode(node)) {
      nsIContent* content = static_cast<nsIContent*>(node);
      NS_ASSERTION(content, "Where is our content?");
      const nsTextFragment* textFragment = content->GetText();
      NS_ASSERTION(textFragment, "Where is our text?");
      int32_t lastOffsetInNode = textFragment->GetLength();

      if (seenSoftEnd) {
        // check whether we can stop after this
        for (int32_t i = node == mSoftEnd.mNode ? mSoftEnd.mOffset : 0;
             i < int32_t(textFragment->GetLength()); ++i) {
          if (IsDOMWordSeparator(textFragment->CharAt(i))) {
            exit = true;
            // stop at the first separator after the soft end point
            lastOffsetInNode = i;
            break;
          }
        }
      }

      if (firstOffsetInNode < lastOffsetInNode) {
        int32_t len = lastOffsetInNode - firstOffsetInNode;
        mSoftTextDOMMapping.AppendElement(DOMTextMapping(
            NodeOffset(node, firstOffsetInNode), mSoftText.Length(), len));

        bool ok = textFragment->AppendTo(mSoftText, firstOffsetInNode, len,
                                         mozilla::fallible);
        if (!ok) {
          // probably out of memory, remove from mSoftTextDOMMapping
          mSoftTextDOMMapping.RemoveLastElement();
          exit = true;
        }
      }

      firstOffsetInNode = 0;
    }

    if (exit) break;

    CheckLeavingBreakElementClosure closure = {false};
    node = FindNextNode(node, mRootNode, CheckLeavingBreakElement, &closure);
    if (closure.mLeftBreakElement || (node && IsBreakElement(node))) {
      // We left, or are entering, a break element (e.g., block). Maybe we can
      // stop now.
      if (seenSoftEnd) break;
      // Record the break
      mSoftText.Append(' ');
    }
  }

  MOZ_LOG(sInlineSpellWordUtilLog, LogLevel::Debug,
          ("%s: got DOM string: %s", __FUNCTION__,
           NS_ConvertUTF16toUTF8(mSoftText).get()));
}

auto mozInlineSpellWordUtil::BuildRealWords() const
    -> Result<RealWords, nsresult> {
  // This is pretty simple. We just have to walk mSoftText, tokenizing it
  // into "real words".
  // We do an outer traversal of words delimited by IsDOMWordSeparator, calling
  // SplitDOMWordAndAppendTo on each of those DOM words
  int32_t wordStart = -1;
  RealWords realWords;
  for (int32_t i = 0; i < int32_t(mSoftText.Length()); ++i) {
    if (IsDOMWordSeparator(mSoftText.CharAt(i))) {
      if (wordStart >= 0) {
        nsresult rv = SplitDOMWordAndAppendTo(wordStart, i, realWords);
        if (NS_FAILED(rv)) {
          return Err(rv);
        }
        wordStart = -1;
      }
    } else {
      if (wordStart < 0) {
        wordStart = i;
      }
    }
  }
  if (wordStart >= 0) {
    nsresult rv =
        SplitDOMWordAndAppendTo(wordStart, mSoftText.Length(), realWords);
    if (NS_FAILED(rv)) {
      return Err(rv);
    }
  }

  return realWords;
}

/*********** DOM/realwords<->mSoftText mapping functions ************/

int32_t mozInlineSpellWordUtil::MapDOMPositionToSoftTextOffset(
    NodeOffset aNodeOffset) const {
  if (!mSoftTextValid) {
    NS_ERROR("Soft text must be valid if we're to map into it");
    return -1;
  }

  for (int32_t i = 0; i < int32_t(mSoftTextDOMMapping.Length()); ++i) {
    const DOMTextMapping& map = mSoftTextDOMMapping[i];
    if (map.mNodeOffset.mNode == aNodeOffset.mNode) {
      // Allow offsets at either end of the string, in particular, allow the
      // offset that's at the end of the contributed string
      int32_t offsetInContributedString =
          aNodeOffset.mOffset - map.mNodeOffset.mOffset;
      if (offsetInContributedString >= 0 &&
          offsetInContributedString <= map.mLength)
        return map.mSoftTextOffset + offsetInContributedString;
      return -1;
    }
  }
  return -1;
}

namespace {

template <class T>
class FirstLargerOffset {
  int32_t mSoftTextOffset;

 public:
  explicit FirstLargerOffset(int32_t aSoftTextOffset)
      : mSoftTextOffset(aSoftTextOffset) {}
  int operator()(const T& t) const {
    // We want the first larger offset, so never return 0 (which would
    // short-circuit evaluation before finding the last such offset).
    return mSoftTextOffset < t.mSoftTextOffset ? -1 : 1;
  }
};

template <class T>
bool FindLastNongreaterOffset(const nsTArray<T>& aContainer,
                              int32_t aSoftTextOffset, size_t* aIndex) {
  if (aContainer.Length() == 0) {
    return false;
  }

  BinarySearchIf(aContainer, 0, aContainer.Length(),
                 FirstLargerOffset<T>(aSoftTextOffset), aIndex);
  if (*aIndex > 0) {
    // There was at least one mapping with offset <= aSoftTextOffset. Step back
    // to find the last element with |mSoftTextOffset <= aSoftTextOffset|.
    *aIndex -= 1;
  } else {
    // Every mapping had offset greater than aSoftTextOffset.
    MOZ_ASSERT(aContainer[*aIndex].mSoftTextOffset > aSoftTextOffset);
  }
  return true;
}

}  // namespace

NodeOffset mozInlineSpellWordUtil::MapSoftTextOffsetToDOMPosition(
    int32_t aSoftTextOffset, DOMMapHint aHint) const {
  NS_ASSERTION(mSoftTextValid,
               "Soft text must be valid if we're to map out of it");
  if (!mSoftTextValid) return NodeOffset(nullptr, -1);

  // Find the last mapping, if any, such that mSoftTextOffset <= aSoftTextOffset
  size_t index;
  bool found =
      FindLastNongreaterOffset(mSoftTextDOMMapping, aSoftTextOffset, &index);
  if (!found) {
    return NodeOffset(nullptr, -1);
  }

  // 'index' is now the last mapping, if any, such that
  // mSoftTextOffset <= aSoftTextOffset.
  // If we're doing HINT_END, then we may want to return the end of the
  // the previous mapping instead of the start of this mapping
  if (aHint == HINT_END && index > 0) {
    const DOMTextMapping& map = mSoftTextDOMMapping[index - 1];
    if (map.mSoftTextOffset + map.mLength == aSoftTextOffset)
      return NodeOffset(map.mNodeOffset.mNode,
                        map.mNodeOffset.mOffset + map.mLength);
  }

  // We allow ourselves to return the end of this mapping even if we're
  // doing HINT_START. This will only happen if there is no mapping which this
  // point is the start of. I'm not 100% sure this is OK...
  const DOMTextMapping& map = mSoftTextDOMMapping[index];
  int32_t offset = aSoftTextOffset - map.mSoftTextOffset;
  if (offset >= 0 && offset <= map.mLength)
    return NodeOffset(map.mNodeOffset.mNode, map.mNodeOffset.mOffset + offset);

  return NodeOffset(nullptr, -1);
}

// static
void mozInlineSpellWordUtil::ToString(const DOMMapHint aHint,
                                      nsACString& aResult) {
  switch (aHint) {
    case HINT_BEGIN:
      aResult.AssignLiteral("begin");
      break;
    case HINT_END:
      aResult.AssignLiteral("end");
      break;
  }
}

int32_t mozInlineSpellWordUtil::FindRealWordContaining(
    int32_t aSoftTextOffset, DOMMapHint aHint, bool aSearchForward) const {
  if (MOZ_LOG_TEST(sInlineSpellWordUtilLog, LogLevel::Debug)) {
    nsAutoCString hint;
    mozInlineSpellWordUtil::ToString(aHint, hint);

    MOZ_LOG(
        sInlineSpellWordUtilLog, LogLevel::Debug,
        ("%s: offset=%i, hint=%s, searchForward=%i.", __FUNCTION__,
         aSoftTextOffset, hint.get(), static_cast<int32_t>(aSearchForward)));
  }

  NS_ASSERTION(mSoftTextValid,
               "Soft text must be valid if we're to map out of it");
  if (!mSoftTextValid) return -1;

  // Find the last word, if any, such that mSoftTextOffset <= aSoftTextOffset
  size_t index;
  bool found = FindLastNongreaterOffset(mRealWords, aSoftTextOffset, &index);
  if (!found) {
    return -1;
  }

  // 'index' is now the last word, if any, such that
  // mSoftTextOffset <= aSoftTextOffset.
  // If we're doing HINT_END, then we may want to return the end of the
  // the previous word instead of the start of this word
  if (aHint == HINT_END && index > 0) {
    const RealWord& word = mRealWords[index - 1];
    if (word.mSoftTextOffset + word.mLength == aSoftTextOffset)
      return index - 1;
  }

  // We allow ourselves to return the end of this word even if we're
  // doing HINT_START. This will only happen if there is no word which this
  // point is the start of. I'm not 100% sure this is OK...
  const RealWord& word = mRealWords[index];
  int32_t offset = aSoftTextOffset - word.mSoftTextOffset;
  if (offset >= 0 && offset <= static_cast<int32_t>(word.mLength)) return index;

  if (aSearchForward) {
    if (mRealWords[0].mSoftTextOffset > aSoftTextOffset) {
      // All words have mSoftTextOffset > aSoftTextOffset
      return 0;
    }
    // 'index' is the last word such that mSoftTextOffset <= aSoftTextOffset.
    // Word index+1, if it exists, will be the first with
    // mSoftTextOffset > aSoftTextOffset.
    if (index + 1 < mRealWords.Length()) return index + 1;
  }

  return -1;
}

// mozInlineSpellWordUtil::SplitDOMWordAndAppendTo

nsresult mozInlineSpellWordUtil::SplitDOMWordAndAppendTo(
    int32_t aStart, int32_t aEnd, nsTArray<RealWord>& aRealWords) const {
  nsDependentSubstring targetText(mSoftText, aStart, aEnd - aStart);
  WordSplitState<nsDependentSubstring> state(targetText);
  state.mCurCharClass = state.ClassifyCharacter(0, true);

  state.AdvanceThroughSeparators();
  if (state.mCurCharClass != CHAR_CLASS_END_OF_INPUT && state.IsSpecialWord()) {
    int32_t specialWordLength =
        state.mDOMWordText.Length() - state.mDOMWordOffset;
    if (!aRealWords.AppendElement(
            RealWord(aStart + state.mDOMWordOffset, specialWordLength, false),
            fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
  }

  while (state.mCurCharClass != CHAR_CLASS_END_OF_INPUT) {
    state.AdvanceThroughSeparators();
    if (state.mCurCharClass == CHAR_CLASS_END_OF_INPUT) break;

    // save the beginning of the word
    int32_t wordOffset = state.mDOMWordOffset;

    // find the end of the word
    state.AdvanceThroughWord();
    int32_t wordLen = state.mDOMWordOffset - wordOffset;
    if (!aRealWords.AppendElement(
            RealWord(aStart + wordOffset, wordLen,
                     !state.ShouldSkipWord(wordOffset, wordLen)),
            fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

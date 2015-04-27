/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozInlineSpellWordUtil.h"
#include "nsDebug.h"
#include "nsIAtom.h"
#include "nsComponentManagerUtils.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMElement.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"
#include "nsIDOMNode.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicodeProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "mozilla/dom/Element.h"
#include "nsRange.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include <algorithm>
#include "mozilla/BinarySearch.h"

using namespace mozilla;

// IsIgnorableCharacter
//
//    These characters are ones that we should ignore in input.

inline bool IsIgnorableCharacter(char16_t ch)
{
  return (ch == 0xAD ||   // SOFT HYPHEN
          ch == 0x1806);  // MONGOLIAN TODO SOFT HYPHEN
}

// IsConditionalPunctuation
//
//    Some characters (like apostrophes) require characters on each side to be
//    part of a word, and are otherwise punctuation.

inline bool IsConditionalPunctuation(char16_t ch)
{
  return (ch == '\'' ||
          ch == 0x2019 || // RIGHT SINGLE QUOTATION MARK
          ch == 0x00B7); // MIDDLE DOT
}

// mozInlineSpellWordUtil::Init

nsresult
mozInlineSpellWordUtil::Init(nsWeakPtr aWeakEditor)
{
  nsresult rv;

  // getting the editor can fail commonly because the editor was detached, so
  // don't assert
  nsCOMPtr<nsIEditor> editor = do_QueryReferent(aWeakEditor, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = editor->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(domDoc, NS_ERROR_NULL_POINTER);

  mDOMDocument = domDoc;
  mDocument = do_QueryInterface(domDoc);

  // Find the root node for the editor. For contenteditable we'll need something
  // cleverer here.
  nsCOMPtr<nsIDOMElement> rootElt;
  rv = editor->GetRootElement(getter_AddRefs(rootElt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINode> rootNode = do_QueryInterface(rootElt);
  mRootNode = rootNode;
  NS_ASSERTION(mRootNode, "GetRootElement returned null *and* claimed to suceed!");
  return NS_OK;
}

static inline bool
IsTextNode(nsINode* aNode)
{
  return aNode->IsNodeOfType(nsINode::eTEXT);
}

typedef void (* OnLeaveNodeFunPtr)(nsINode* aNode, void* aClosure);

// Find the next node in the DOM tree in preorder.
// Calls OnLeaveNodeFunPtr when the traversal leaves a node, which is
// why we can't just use GetNextNode here, sadly.
static nsINode*
FindNextNode(nsINode* aNode, nsINode* aRoot,
             OnLeaveNodeFunPtr aOnLeaveNode, void* aClosure)
{
  NS_PRECONDITION(aNode, "Null starting node?");

  nsINode* next = aNode->GetFirstChild();
  if (next)
    return next;
  
  // Don't look at siblings or otherwise outside of aRoot
  if (aNode == aRoot)
    return nullptr;

  next = aNode->GetNextSibling();
  if (next)
    return next;

  // Go up
  for (;;) {
    if (aOnLeaveNode) {
      aOnLeaveNode(aNode, aClosure);
    }
    
    next = aNode->GetParent();
    if (next == aRoot || ! next)
      return nullptr;
    aNode = next;
    
    next = aNode->GetNextSibling();
    if (next)
      return next;
  }
}

// aNode is not a text node. Find the first text node starting at aNode/aOffset
// in a preorder DOM traversal.
static nsINode*
FindNextTextNode(nsINode* aNode, int32_t aOffset, nsINode* aRoot)
{
  NS_PRECONDITION(aNode, "Null starting node?");
  NS_ASSERTION(!IsTextNode(aNode), "FindNextTextNode should start with a non-text node");

  nsINode* checkNode;
  // Need to start at the aOffset'th child
  nsIContent* child = aNode->GetChildAt(aOffset);

  if (child) {
    checkNode = child;
  } else {
    // aOffset was beyond the end of the child list. 
    // goto next node after the last descendant of aNode in
    // a preorder DOM traversal.
    checkNode = aNode->GetNextNonChildNode(aRoot);
  }
  
  while (checkNode && !IsTextNode(checkNode)) {
    checkNode = checkNode->GetNextNode(aRoot);
  }
  return checkNode;
}

// mozInlineSpellWordUtil::SetEnd
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

nsresult
mozInlineSpellWordUtil::SetEnd(nsINode* aEndNode, int32_t aEndOffset)
{
  NS_PRECONDITION(aEndNode, "Null end node?");

  NS_ASSERTION(mRootNode, "Not initialized");

  InvalidateWords();

  if (!IsTextNode(aEndNode)) {
    // End at the start of the first text node after aEndNode/aEndOffset.
    aEndNode = FindNextTextNode(aEndNode, aEndOffset, mRootNode);
    aEndOffset = 0;
  }
  mSoftEnd = NodeOffset(aEndNode, aEndOffset);
  return NS_OK;
}

nsresult
mozInlineSpellWordUtil::SetPosition(nsINode* aNode, int32_t aOffset)
{
  InvalidateWords();

  if (!IsTextNode(aNode)) {
    // Start at the start of the first text node after aNode/aOffset.
    aNode = FindNextTextNode(aNode, aOffset, mRootNode);
    aOffset = 0;
  }
  mSoftBegin = NodeOffset(aNode, aOffset);

  EnsureWords();
  
  int32_t textOffset = MapDOMPositionToSoftTextOffset(mSoftBegin);
  if (textOffset < 0)
    return NS_OK;
  mNextWordIndex = FindRealWordContaining(textOffset, HINT_END, true);
  return NS_OK;
}

void
mozInlineSpellWordUtil::EnsureWords()
{
  if (mSoftTextValid)
    return;
  BuildSoftText();
  BuildRealWords();
  mSoftTextValid = true;
}

nsresult
mozInlineSpellWordUtil::MakeRangeForWord(const RealWord& aWord, nsRange** aRange)
{
  NodeOffset begin = MapSoftTextOffsetToDOMPosition(aWord.mSoftTextOffset, HINT_BEGIN);
  NodeOffset end = MapSoftTextOffsetToDOMPosition(aWord.EndOffset(), HINT_END);
  return MakeRange(begin, end, aRange);
}

// mozInlineSpellWordUtil::GetRangeForWord

nsresult
mozInlineSpellWordUtil::GetRangeForWord(nsIDOMNode* aWordNode,
                                        int32_t aWordOffset,
                                        nsRange** aRange)
{
  // Set our soft end and start
  nsCOMPtr<nsINode> wordNode = do_QueryInterface(aWordNode);
  NodeOffset pt = NodeOffset(wordNode, aWordOffset);
  
  InvalidateWords();
  mSoftBegin = mSoftEnd = pt;
  EnsureWords();
  
  int32_t offset = MapDOMPositionToSoftTextOffset(pt);
  if (offset < 0)
    return MakeRange(pt, pt, aRange);
  int32_t wordIndex = FindRealWordContaining(offset, HINT_BEGIN, false);
  if (wordIndex < 0)
    return MakeRange(pt, pt, aRange);
  return MakeRangeForWord(mRealWords[wordIndex], aRange);
}

// This is to fix characters that the spellchecker may not like
static void
NormalizeWord(const nsSubstring& aInput, int32_t aPos, int32_t aLen, nsAString& aOutput)
{
  aOutput.Truncate();
  for (int32_t i = 0; i < aLen; i++) {
    char16_t ch = aInput.CharAt(i + aPos);

    // remove ignorable characters from the word
    if (IsIgnorableCharacter(ch))
      continue;

    // the spellchecker doesn't handle curly apostrophes in all languages
    if (ch == 0x2019) { // RIGHT SINGLE QUOTATION MARK
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

nsresult
mozInlineSpellWordUtil::GetNextWord(nsAString& aText, nsRange** aRange,
                                    bool* aSkipChecking)
{
#ifdef DEBUG_SPELLCHECK
  printf("GetNextWord called; mNextWordIndex=%d\n", mNextWordIndex);
#endif

  if (mNextWordIndex < 0 ||
      mNextWordIndex >= int32_t(mRealWords.Length())) {
    mNextWordIndex = -1;
    *aRange = nullptr;
    *aSkipChecking = true;
    return NS_OK;
  }
  
  const RealWord& word = mRealWords[mNextWordIndex];
  nsresult rv = MakeRangeForWord(word, aRange);
  NS_ENSURE_SUCCESS(rv, rv);
  ++mNextWordIndex;
  *aSkipChecking = !word.mCheckableWord;
  ::NormalizeWord(mSoftText, word.mSoftTextOffset, word.mLength, aText);

#ifdef DEBUG_SPELLCHECK
  printf("GetNextWord returning: %s (skip=%d)\n",
         NS_ConvertUTF16toUTF8(aText).get(), *aSkipChecking);
#endif
  
  return NS_OK;
}

// mozInlineSpellWordUtil::MakeRange
//
//    Convenience function for creating a range over the current document.

nsresult
mozInlineSpellWordUtil::MakeRange(NodeOffset aBegin, NodeOffset aEnd,
                                  nsRange** aRange)
{
  NS_ENSURE_ARG_POINTER(aBegin.mNode);
  if (!mDOMDocument)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<nsRange> range = new nsRange(aBegin.mNode);
  nsresult rv = range->Set(aBegin.mNode, aBegin.mOffset,
                           aEnd.mNode, aEnd.mOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  range.forget(aRange);

  return NS_OK;
}

/*********** DOM text extraction ************/

// IsDOMWordSeparator
//
//    Determines if the given character should be considered as a DOM Word
//    separator. Basically, this is whitespace, although it could also have
//    certain punctuation that we know ALWAYS breaks words. This is important.
//    For example, we can't have any punctuation that could appear in a URL
//    or email address in this, because those need to always fit into a single
//    DOM word.

static bool
IsDOMWordSeparator(char16_t ch)
{
  // simple spaces
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    return true;

  // complex spaces - check only if char isn't ASCII (uncommon)
  if (ch >= 0xA0 &&
      (ch == 0x00A0 ||  // NO-BREAK SPACE
       ch == 0x2002 ||  // EN SPACE
       ch == 0x2003 ||  // EM SPACE
       ch == 0x2009 ||  // THIN SPACE
       ch == 0x3000))   // IDEOGRAPHIC SPACE
    return true;

  // otherwise not a space
  return false;
}

static inline bool
IsBRElement(nsINode* aNode)
{
  return aNode->IsHTMLElement(nsGkAtoms::br);
}

/**
 * Given a TextNode, checks to see if there's a DOM word separator before
 * aBeforeOffset within it. This function does not modify aSeparatorOffset when
 * it returns false.
 *
 * @param aNode the TextNode to check.
 * @param aBeforeOffset the offset in the TextNode before which we will search
 *        for the DOM separator. You can pass INT32_MAX to search the entire
 *        length of the string.
 * @param aSeparatorOffset will be set to the offset of the first separator it
 *        encounters. Will not be written to if no separator is found.
 * @returns True if it found a separator.
 */
static bool
TextNodeContainsDOMWordSeparator(nsINode* aNode,
                                 int32_t aBeforeOffset,
                                 int32_t* aSeparatorOffset)
{
  // aNode is actually an nsIContent, since it's eTEXT
  nsIContent* content = static_cast<nsIContent*>(aNode);
  const nsTextFragment* textFragment = content->GetText();
  NS_ASSERTION(textFragment, "Where is our text?");
  for (int32_t i = std::min(aBeforeOffset, int32_t(textFragment->GetLength())) - 1; i >= 0; --i) {
    if (IsDOMWordSeparator(textFragment->CharAt(i))) {
      // Be greedy, find as many separators as we can
      for (int32_t j = i - 1; j >= 0; --j) {
        if (IsDOMWordSeparator(textFragment->CharAt(j))) {
          i = j;
        } else {
          break;
        }
      }
      *aSeparatorOffset = i;
      return true;
    }
  }
  return false;
}

/**
 * Check if there's a DOM word separator before aBeforeOffset in this node.
 * Always returns true if it's a BR element.
 * aSeparatorOffset is set to the index of the first character in the last
 * separator if any is found (0 for BR elements).
 *
 * This function does not modify aSeparatorOffset when it returns false.
 */
static bool
ContainsDOMWordSeparator(nsINode* aNode, int32_t aBeforeOffset,
                         int32_t* aSeparatorOffset)
{
  if (IsBRElement(aNode)) {
    *aSeparatorOffset = 0;
    return true;
  }

  if (!IsTextNode(aNode))
    return false;

  return TextNodeContainsDOMWordSeparator(aNode, aBeforeOffset,
                                          aSeparatorOffset);
}

static bool
IsBreakElement(nsINode* aNode)
{
  if (!aNode->IsElement()) {
    return false;
  }

  dom::Element *element = aNode->AsElement();
    
  if (element->IsHTMLElement(nsGkAtoms::br))
    return true;

  // If we don't have a frame, we don't consider ourselves a break
  // element.  In particular, words can span us.
  if (!element->GetPrimaryFrame())
    return false;

  // Anything that's not an inline element is a break element.
  // XXXbz should replaced inlines be break elements, though?
  return element->GetPrimaryFrame()->StyleDisplay()->mDisplay !=
    NS_STYLE_DISPLAY_INLINE;
}

struct CheckLeavingBreakElementClosure {
  bool          mLeftBreakElement;
};

static void
CheckLeavingBreakElement(nsINode* aNode, void* aClosure)
{
  CheckLeavingBreakElementClosure* cl =
    static_cast<CheckLeavingBreakElementClosure*>(aClosure);
  if (!cl->mLeftBreakElement && IsBreakElement(aNode)) {
    cl->mLeftBreakElement = true;
  }
}

void
mozInlineSpellWordUtil::NormalizeWord(nsSubstring& aWord)
{
  nsAutoString result;
  ::NormalizeWord(aWord, 0, aWord.Length(), result);
  aWord = result;
}

void
mozInlineSpellWordUtil::BuildSoftText()
{
  // First we have to work backwards from mSoftStart to find a text node
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
            nsINode* prevNode = node->GetPreviousSibling();
            while (prevNode && IsTextNode(prevNode)) {
              mSoftBegin.mNode = prevNode;
              if (TextNodeContainsDOMWordSeparator(prevNode, INT32_MAX,
                                                   &newOffset)) {
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
      // Since GetPreviousContent follows tree *preorder*, we're about to traverse
      // up out of 'node'. Since node induces breaks (e.g., it's a block),
      // don't bother trying to look outside it, just stop now.
      break;
    }
    // GetPreviousContent below expects mRootNode to be an ancestor of node.
    if (!nsContentUtils::ContentIsDescendantOf(node, mRootNode)) {
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
    if (IsTextNode(node)) {
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
        mSoftTextDOMMapping.AppendElement(
          DOMTextMapping(NodeOffset(node, firstOffsetInNode), mSoftText.Length(), len));

        bool ok = textFragment->AppendTo(mSoftText, firstOffsetInNode, len,
                                         mozilla::fallible);
        if (!ok) {
            // probably out of memory, remove from mSoftTextDOMMapping
            mSoftTextDOMMapping.RemoveElementAt(mSoftTextDOMMapping.Length() - 1);
            exit = true;
        }
      }

      firstOffsetInNode = 0;
    }

    if (exit)
      break;

    CheckLeavingBreakElementClosure closure = { false };
    node = FindNextNode(node, mRootNode, CheckLeavingBreakElement, &closure);
    if (closure.mLeftBreakElement || (node && IsBreakElement(node))) {
      // We left, or are entering, a break element (e.g., block). Maybe we can
      // stop now.
      if (seenSoftEnd)
        break;
      // Record the break
      mSoftText.Append(' ');
    }
  }
  
#ifdef DEBUG_SPELLCHECK
  printf("Got DOM string: %s\n", NS_ConvertUTF16toUTF8(mSoftText).get());
#endif
}

void
mozInlineSpellWordUtil::BuildRealWords()
{
  // This is pretty simple. We just have to walk mSoftText, tokenizing it
  // into "real words".
  // We do an outer traversal of words delimited by IsDOMWordSeparator, calling
  // SplitDOMWord on each of those DOM words
  int32_t wordStart = -1;
  mRealWords.Clear();
  for (int32_t i = 0; i < int32_t(mSoftText.Length()); ++i) {
    if (IsDOMWordSeparator(mSoftText.CharAt(i))) {
      if (wordStart >= 0) {
        SplitDOMWord(wordStart, i);
        wordStart = -1;
      }
    } else {
      if (wordStart < 0) {
        wordStart = i;
      }
    }
  }
  if (wordStart >= 0) {
    SplitDOMWord(wordStart, mSoftText.Length());
  }
}

/*********** DOM/realwords<->mSoftText mapping functions ************/

int32_t
mozInlineSpellWordUtil::MapDOMPositionToSoftTextOffset(NodeOffset aNodeOffset)
{
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

template<class T>
class FirstLargerOffset
{
  int32_t mSoftTextOffset;

public:
  explicit FirstLargerOffset(int32_t aSoftTextOffset) : mSoftTextOffset(aSoftTextOffset) {}
  int operator()(const T& t) const {
  // We want the first larger offset, so never return 0 (which would
  // short-circuit evaluation before finding the last such offset).
    return mSoftTextOffset < t.mSoftTextOffset ? -1 : 1;
  }
};

template<class T>
bool
FindLastNongreaterOffset(const nsTArray<T>& aContainer, int32_t aSoftTextOffset, size_t* aIndex)
{
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

} // namespace

mozInlineSpellWordUtil::NodeOffset
mozInlineSpellWordUtil::MapSoftTextOffsetToDOMPosition(int32_t aSoftTextOffset,
                                                       DOMMapHint aHint)
{
  NS_ASSERTION(mSoftTextValid, "Soft text must be valid if we're to map out of it");
  if (!mSoftTextValid)
    return NodeOffset(nullptr, -1);

  // Find the last mapping, if any, such that mSoftTextOffset <= aSoftTextOffset
  size_t index;
  bool found = FindLastNongreaterOffset(mSoftTextDOMMapping, aSoftTextOffset, &index);
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
      return NodeOffset(map.mNodeOffset.mNode, map.mNodeOffset.mOffset + map.mLength);
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

int32_t
mozInlineSpellWordUtil::FindRealWordContaining(int32_t aSoftTextOffset,
    DOMMapHint aHint, bool aSearchForward)
{
  NS_ASSERTION(mSoftTextValid, "Soft text must be valid if we're to map out of it");
  if (!mSoftTextValid)
    return -1;

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
  if (offset >= 0 && offset <= word.mLength)
    return index;

  if (aSearchForward) {
    if (mRealWords[0].mSoftTextOffset > aSoftTextOffset) {
      // All words have mSoftTextOffset > aSoftTextOffset
      return 0;
    }
    // 'index' is the last word such that mSoftTextOffset <= aSoftTextOffset.
    // Word index+1, if it exists, will be the first with
    // mSoftTextOffset > aSoftTextOffset.
    if (index + 1 < mRealWords.Length())
      return index + 1;
  }

  return -1;
}

/*********** Word Splitting ************/

// classifies a given character in the DOM word
enum CharClass {
  CHAR_CLASS_WORD,
  CHAR_CLASS_SEPARATOR,
  CHAR_CLASS_END_OF_INPUT };

// Encapsulates DOM-word to real-word splitting
struct MOZ_STACK_CLASS WordSplitState
{
  mozInlineSpellWordUtil*    mWordUtil;
  const nsDependentSubstring mDOMWordText;
  int32_t                    mDOMWordOffset;
  CharClass                  mCurCharClass;

  WordSplitState(mozInlineSpellWordUtil* aWordUtil,
                 const nsString& aString, int32_t aStart, int32_t aLen)
    : mWordUtil(aWordUtil), mDOMWordText(aString, aStart, aLen),
      mDOMWordOffset(0), mCurCharClass(CHAR_CLASS_END_OF_INPUT) {}

  CharClass ClassifyCharacter(int32_t aIndex, bool aRecurse) const;
  void Advance();
  void AdvanceThroughSeparators();
  void AdvanceThroughWord();

  // Finds special words like email addresses and URLs that may start at the
  // current position, and returns their length, or 0 if not found. This allows
  // arbitrary word breaking rules to be used for these special entities, as
  // long as they can not contain whitespace.
  bool IsSpecialWord();

  // Similar to IsSpecialWord except that this takes a split word as
  // input. This checks for things that do not require special word-breaking
  // rules.
  bool ShouldSkipWord(int32_t aStart, int32_t aLength);
};

// WordSplitState::ClassifyCharacter

CharClass
WordSplitState::ClassifyCharacter(int32_t aIndex, bool aRecurse) const
{
  NS_ASSERTION(aIndex >= 0 && aIndex <= int32_t(mDOMWordText.Length()),
               "Index out of range");
  if (aIndex == int32_t(mDOMWordText.Length()))
    return CHAR_CLASS_SEPARATOR;

  // this will classify the character, we want to treat "ignorable" characters
  // such as soft hyphens, and also ZWJ and ZWNJ as word characters.
  nsIUGenCategory::nsUGenCategory
    charCategory = mozilla::unicode::GetGenCategory(mDOMWordText[aIndex]);
  if (charCategory == nsIUGenCategory::kLetter ||
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
    if (aIndex == 0)
      return CHAR_CLASS_SEPARATOR;
    if (ClassifyCharacter(aIndex - 1, false) != CHAR_CLASS_WORD)
      return CHAR_CLASS_SEPARATOR;
    // If the previous charatcer is a word-char, make sure that it's not a
    // special dot character.
    if (mDOMWordText[aIndex - 1] == '.')
      return CHAR_CLASS_SEPARATOR;

    // now we know left char is a word-char, check the right-hand character
    if (aIndex == int32_t(mDOMWordText.Length()) - 1)
      return CHAR_CLASS_SEPARATOR;
    if (ClassifyCharacter(aIndex + 1, false) != CHAR_CLASS_WORD)
      return CHAR_CLASS_SEPARATOR;
    // If the next charatcer is a word-char, make sure that it's not a
    // special dot character.
    if (mDOMWordText[aIndex + 1] == '.')
      return CHAR_CLASS_SEPARATOR;

    // char on either side is a word, this counts as a word
    return CHAR_CLASS_WORD;
  }

  // The dot character, if appearing at the end of a word, should
  // be considered part of that word.  Example: "etc.", or
  // abbreviations
  if (aIndex > 0 &&
      mDOMWordText[aIndex] == '.' &&
      mDOMWordText[aIndex - 1] != '.' &&
      ClassifyCharacter(aIndex - 1, false) != CHAR_CLASS_WORD) {
    return CHAR_CLASS_WORD;
  }

  // all other punctuation
  if (charCategory == nsIUGenCategory::kSeparator ||
      charCategory == nsIUGenCategory::kOther ||
      charCategory == nsIUGenCategory::kPunctuation ||
      charCategory == nsIUGenCategory::kSymbol) {
    // Don't break on hyphens, as hunspell handles them on its own.
    if (aIndex > 0 &&
        mDOMWordText[aIndex] == '-' &&
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

void
WordSplitState::Advance()
{
  NS_ASSERTION(mDOMWordOffset >= 0, "Negative word index");
  NS_ASSERTION(mDOMWordOffset < (int32_t)mDOMWordText.Length(),
               "Length beyond end");

  mDOMWordOffset ++;
  if (mDOMWordOffset >= (int32_t)mDOMWordText.Length())
    mCurCharClass = CHAR_CLASS_END_OF_INPUT;
  else
    mCurCharClass = ClassifyCharacter(mDOMWordOffset, true);
}


// WordSplitState::AdvanceThroughSeparators

void
WordSplitState::AdvanceThroughSeparators()
{
  while (mCurCharClass == CHAR_CLASS_SEPARATOR)
    Advance();
}

// WordSplitState::AdvanceThroughWord

void
WordSplitState::AdvanceThroughWord()
{
  while (mCurCharClass == CHAR_CLASS_WORD)
    Advance();
}


// WordSplitState::IsSpecialWord

bool
WordSplitState::IsSpecialWord()
{
  // Search for email addresses. We simply define these as any sequence of
  // characters with an '@' character in the middle. The DOM word is already
  // split on whitepace, so we know that everything to the end is the address
  int32_t firstColon = -1;
  for (int32_t i = mDOMWordOffset;
       i < int32_t(mDOMWordText.Length()); i ++) {
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
    nsString protocol(Substring(mDOMWordText, mDOMWordOffset,
                      firstColon - mDOMWordOffset));
    if (protocol.EqualsIgnoreCase("http") ||
        protocol.EqualsIgnoreCase("https") ||
        protocol.EqualsIgnoreCase("news") ||
        protocol.EqualsIgnoreCase("file") ||
        protocol.EqualsIgnoreCase("javascript") ||
        protocol.EqualsIgnoreCase("data") ||
        protocol.EqualsIgnoreCase("ftp")) {
      return true;
    }
  }

  // not anything special
  return false;
}

// WordSplitState::ShouldSkipWord

bool
WordSplitState::ShouldSkipWord(int32_t aStart, int32_t aLength)
{
  int32_t last = aStart + aLength;

  // check to see if the word contains a digit
  for (int32_t i = aStart; i < last; i ++) {
    if (unicode::GetGenCategory(mDOMWordText[i]) == nsIUGenCategory::kNumber) {
      return true;
    }
  }

  // not special
  return false;
}

// mozInlineSpellWordUtil::SplitDOMWord

void
mozInlineSpellWordUtil::SplitDOMWord(int32_t aStart, int32_t aEnd)
{
  WordSplitState state(this, mSoftText, aStart, aEnd - aStart);
  state.mCurCharClass = state.ClassifyCharacter(0, true);

  state.AdvanceThroughSeparators();
  if (state.mCurCharClass != CHAR_CLASS_END_OF_INPUT &&
      state.IsSpecialWord()) {
    int32_t specialWordLength = state.mDOMWordText.Length() - state.mDOMWordOffset;
    mRealWords.AppendElement(
        RealWord(aStart + state.mDOMWordOffset, specialWordLength, false));

    return;
  }

  while (state.mCurCharClass != CHAR_CLASS_END_OF_INPUT) {
    state.AdvanceThroughSeparators();
    if (state.mCurCharClass == CHAR_CLASS_END_OF_INPUT)
      break;

    // save the beginning of the word
    int32_t wordOffset = state.mDOMWordOffset;

    // find the end of the word
    state.AdvanceThroughWord();
    int32_t wordLen = state.mDOMWordOffset - wordOffset;
    mRealWords.AppendElement(
      RealWord(aStart + wordOffset, wordLen,
               !state.ShouldSkipWord(wordOffset, wordLen)));
  }
}

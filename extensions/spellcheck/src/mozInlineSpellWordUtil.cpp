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
#include "nsIDOMHTMLBRElement.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicodeProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "nsRange.h"
#include "nsContentUtils.h"

using namespace mozilla;

// IsIgnorableCharacter
//
//    These characters are ones that we should ignore in input.

inline bool IsIgnorableCharacter(PRUnichar ch)
{
  return (ch == 0xAD ||   // SOFT HYPHEN
          ch == 0x1806);  // MONGOLIAN TODO SOFT HYPHEN
}

// IsConditionalPunctuation
//
//    Some characters (like apostrophes) require characters on each side to be
//    part of a word, and are otherwise punctuation.

inline bool IsConditionalPunctuation(PRUnichar ch)
{
  return (ch == '\'' ||
          ch == 0x2019); // RIGHT SINGLE QUOTATION MARK
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
    return nsnull;

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
      return nsnull;
    aNode = next;
    
    next = aNode->GetNextSibling();
    if (next)
      return next;
  }
}

// aNode is not a text node. Find the first text node starting at aNode/aOffset
// in a preorder DOM traversal.
static nsINode*
FindNextTextNode(nsINode* aNode, PRInt32 aOffset, nsINode* aRoot)
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
mozInlineSpellWordUtil::SetEnd(nsINode* aEndNode, PRInt32 aEndOffset)
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
mozInlineSpellWordUtil::SetPosition(nsINode* aNode, PRInt32 aOffset)
{
  InvalidateWords();

  if (!IsTextNode(aNode)) {
    // Start at the start of the first text node after aNode/aOffset.
    aNode = FindNextTextNode(aNode, aOffset, mRootNode);
    aOffset = 0;
  }
  mSoftBegin = NodeOffset(aNode, aOffset);

  EnsureWords();
  
  PRInt32 textOffset = MapDOMPositionToSoftTextOffset(mSoftBegin);
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
                                        PRInt32 aWordOffset,
                                        nsRange** aRange)
{
  // Set our soft end and start
  nsCOMPtr<nsINode> wordNode = do_QueryInterface(aWordNode);
  NodeOffset pt = NodeOffset(wordNode, aWordOffset);
  
  InvalidateWords();
  mSoftBegin = mSoftEnd = pt;
  EnsureWords();
  
  PRInt32 offset = MapDOMPositionToSoftTextOffset(pt);
  if (offset < 0)
    return MakeRange(pt, pt, aRange);
  PRInt32 wordIndex = FindRealWordContaining(offset, HINT_BEGIN, false);
  if (wordIndex < 0)
    return MakeRange(pt, pt, aRange);
  return MakeRangeForWord(mRealWords[wordIndex], aRange);
}

// This is to fix characters that the spellchecker may not like
static void
NormalizeWord(const nsSubstring& aInput, PRInt32 aPos, PRInt32 aLen, nsAString& aOutput)
{
  aOutput.Truncate();
  for (PRInt32 i = 0; i < aLen; i++) {
    PRUnichar ch = aInput.CharAt(i + aPos);

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
      mNextWordIndex >= PRInt32(mRealWords.Length())) {
    mNextWordIndex = -1;
    *aRange = nsnull;
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
  if (!mDOMDocument)
    return NS_ERROR_NOT_INITIALIZED;

  nsRefPtr<nsRange> range = new nsRange();
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
IsDOMWordSeparator(PRUnichar ch)
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
  return aNode->IsElement() &&
         aNode->AsElement()->IsHTML(nsGkAtoms::br);
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
ContainsDOMWordSeparator(nsINode* aNode, PRInt32 aBeforeOffset,
                         PRInt32* aSeparatorOffset)
{
  if (IsBRElement(aNode)) {
    *aSeparatorOffset = 0;
    return true;
  }
  
  if (!IsTextNode(aNode))
    return false;

  // aNode is actually an nsIContent, since it's eTEXT
  nsIContent* content = static_cast<nsIContent*>(aNode);
  const nsTextFragment* textFragment = content->GetText();
  NS_ASSERTION(textFragment, "Where is our text?");
  for (PRInt32 i = NS_MIN(aBeforeOffset, PRInt32(textFragment->GetLength())) - 1; i >= 0; --i) {
    if (IsDOMWordSeparator(textFragment->CharAt(i))) {
      // Be greedy, find as many separators as we can
      for (PRInt32 j = i - 1; j >= 0; --j) {
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

static bool
IsBreakElement(nsINode* aNode)
{
  if (!aNode->IsElement()) {
    return false;
  }

  dom::Element *element = aNode->AsElement();
    
  if (element->IsHTML(nsGkAtoms::br))
    return true;

  // If we don't have a frame, we don't consider ourselves a break
  // element.  In particular, words can span us.
  if (!element->GetPrimaryFrame())
    return false;

  // Anything that's not an inline element is a break element.
  // XXXbz should replaced inlines be break elements, though?
  return element->GetPrimaryFrame()->GetStyleDisplay()->mDisplay !=
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
  PRInt32 firstOffsetInNode = 0;
  PRInt32 checkBeforeOffset = mSoftBegin.mOffset;
  while (node) {
    if (ContainsDOMWordSeparator(node, checkBeforeOffset, &firstOffsetInNode)) {
      if (node == mSoftBegin.mNode) {
        // If we find a word separator on the first node, look at the preceding
        // word on the text node as well.
        PRInt32 newOffset = 0;
        if (firstOffsetInNode > 0) {
          // Try to find the previous word boundary.  We ignore the return value
          // of ContainsDOMWordSeparator here because there might be no preceding
          // word separator (such as when we're at the end of the first word in
          // the text node), in which case we just set the found offsets to 0.
          // Otherwise, ContainsDOMWordSeparator finds us the correct word
          // boundary so that we can avoid looking at too many words.
          ContainsDOMWordSeparator(node, firstOffsetInNode - 1, &newOffset);
        }
        firstOffsetInNode = newOffset;
        mSoftBegin.mOffset = newOffset;
      }
      break;
    }
    checkBeforeOffset = PR_INT32_MAX;
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
      PRInt32 lastOffsetInNode = textFragment->GetLength();

      if (seenSoftEnd) {
        // check whether we can stop after this
        for (PRInt32 i = node == mSoftEnd.mNode ? mSoftEnd.mOffset : 0;
             i < PRInt32(textFragment->GetLength()); ++i) {
          if (IsDOMWordSeparator(textFragment->CharAt(i))) {
            exit = true;
            // stop at the first separator after the soft end point
            lastOffsetInNode = i;
            break;
          }
        }
      }
      
      if (firstOffsetInNode < lastOffsetInNode) {
        PRInt32 len = lastOffsetInNode - firstOffsetInNode;
        mSoftTextDOMMapping.AppendElement(
          DOMTextMapping(NodeOffset(node, firstOffsetInNode), mSoftText.Length(), len));
        textFragment->AppendTo(mSoftText, firstOffsetInNode, len);
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
  PRInt32 wordStart = -1;
  mRealWords.Clear();
  for (PRInt32 i = 0; i < PRInt32(mSoftText.Length()); ++i) {
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

PRInt32
mozInlineSpellWordUtil::MapDOMPositionToSoftTextOffset(NodeOffset aNodeOffset)
{
  if (!mSoftTextValid) {
    NS_ERROR("Soft text must be valid if we're to map into it");
    return -1;
  }
  
  for (PRInt32 i = 0; i < PRInt32(mSoftTextDOMMapping.Length()); ++i) {
    const DOMTextMapping& map = mSoftTextDOMMapping[i];
    if (map.mNodeOffset.mNode == aNodeOffset.mNode) {
      // Allow offsets at either end of the string, in particular, allow the
      // offset that's at the end of the contributed string
      PRInt32 offsetInContributedString =
        aNodeOffset.mOffset - map.mNodeOffset.mOffset;
      if (offsetInContributedString >= 0 &&
          offsetInContributedString <= map.mLength)
        return map.mSoftTextOffset + offsetInContributedString;
      return -1;
    }
  }
  return -1;
}

mozInlineSpellWordUtil::NodeOffset
mozInlineSpellWordUtil::MapSoftTextOffsetToDOMPosition(PRInt32 aSoftTextOffset,
                                                       DOMMapHint aHint)
{
  NS_ASSERTION(mSoftTextValid, "Soft text must be valid if we're to map out of it");
  if (!mSoftTextValid)
    return NodeOffset(nsnull, -1);
  
  // The invariant is that the range start..end includes the last mapping,
  // if any, such that mSoftTextOffset <= aSoftTextOffset
  PRInt32 start = 0;
  PRInt32 end = mSoftTextDOMMapping.Length();
  while (end - start >= 2) {
    PRInt32 mid = (start + end)/2;
    const DOMTextMapping& map = mSoftTextDOMMapping[mid];
    if (map.mSoftTextOffset > aSoftTextOffset) {
      end = mid;
    } else {
      start = mid;
    }
  }
  
  if (start >= end)
    return NodeOffset(nsnull, -1);

  // 'start' is now the last mapping, if any, such that
  // mSoftTextOffset <= aSoftTextOffset.
  // If we're doing HINT_END, then we may want to return the end of the
  // the previous mapping instead of the start of this mapping
  if (aHint == HINT_END && start > 0) {
    const DOMTextMapping& map = mSoftTextDOMMapping[start - 1];
    if (map.mSoftTextOffset + map.mLength == aSoftTextOffset)
      return NodeOffset(map.mNodeOffset.mNode, map.mNodeOffset.mOffset + map.mLength);
  }
  
  // We allow ourselves to return the end of this mapping even if we're
  // doing HINT_START. This will only happen if there is no mapping which this
  // point is the start of. I'm not 100% sure this is OK...
  const DOMTextMapping& map = mSoftTextDOMMapping[start];
  PRInt32 offset = aSoftTextOffset - map.mSoftTextOffset;
  if (offset >= 0 && offset <= map.mLength)
    return NodeOffset(map.mNodeOffset.mNode, map.mNodeOffset.mOffset + offset);
    
  return NodeOffset(nsnull, -1);
}

PRInt32
mozInlineSpellWordUtil::FindRealWordContaining(PRInt32 aSoftTextOffset,
    DOMMapHint aHint, bool aSearchForward)
{
  NS_ASSERTION(mSoftTextValid, "Soft text must be valid if we're to map out of it");
  if (!mSoftTextValid)
    return -1;

  // The invariant is that the range start..end includes the last word,
  // if any, such that mSoftTextOffset <= aSoftTextOffset
  PRInt32 start = 0;
  PRInt32 end = mRealWords.Length();
  while (end - start >= 2) {
    PRInt32 mid = (start + end)/2;
    const RealWord& word = mRealWords[mid];
    if (word.mSoftTextOffset > aSoftTextOffset) {
      end = mid;
    } else {
      start = mid;
    }
  }
  
  if (start >= end)
    return -1;

  // 'start' is now the last word, if any, such that
  // mSoftTextOffset <= aSoftTextOffset.
  // If we're doing HINT_END, then we may want to return the end of the
  // the previous word instead of the start of this word
  if (aHint == HINT_END && start > 0) {
    const RealWord& word = mRealWords[start - 1];
    if (word.mSoftTextOffset + word.mLength == aSoftTextOffset)
      return start - 1;
  }
  
  // We allow ourselves to return the end of this word even if we're
  // doing HINT_START. This will only happen if there is no word which this
  // point is the start of. I'm not 100% sure this is OK...
  const RealWord& word = mRealWords[start];
  PRInt32 offset = aSoftTextOffset - word.mSoftTextOffset;
  if (offset >= 0 && offset <= word.mLength)
    return start;

  if (aSearchForward) {
    if (mRealWords[0].mSoftTextOffset > aSoftTextOffset) {
      // All words have mSoftTextOffset > aSoftTextOffset
      return 0;
    }
    // 'start' is the last word such that mSoftTextOffset <= aSoftTextOffset.
    // Word start+1, if it exists, will be the first with
    // mSoftTextOffset > aSoftTextOffset.
    if (start + 1 < PRInt32(mRealWords.Length()))
      return start + 1;
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
struct NS_STACK_CLASS WordSplitState
{
  mozInlineSpellWordUtil*    mWordUtil;
  const nsDependentSubstring mDOMWordText;
  PRInt32                    mDOMWordOffset;
  CharClass                  mCurCharClass;

  WordSplitState(mozInlineSpellWordUtil* aWordUtil,
                 const nsString& aString, PRInt32 aStart, PRInt32 aLen)
    : mWordUtil(aWordUtil), mDOMWordText(aString, aStart, aLen),
      mDOMWordOffset(0), mCurCharClass(CHAR_CLASS_END_OF_INPUT) {}

  CharClass ClassifyCharacter(PRInt32 aIndex, bool aRecurse) const;
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
  bool ShouldSkipWord(PRInt32 aStart, PRInt32 aLength);
};

// WordSplitState::ClassifyCharacter

CharClass
WordSplitState::ClassifyCharacter(PRInt32 aIndex, bool aRecurse) const
{
  NS_ASSERTION(aIndex >= 0 && aIndex <= PRInt32(mDOMWordText.Length()),
               "Index out of range");
  if (aIndex == PRInt32(mDOMWordText.Length()))
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
    if (aIndex == PRInt32(mDOMWordText.Length()) - 1)
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
      if (aIndex == PRInt32(mDOMWordText.Length()) - 1)
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
  NS_ASSERTION(mDOMWordOffset < (PRInt32)mDOMWordText.Length(),
               "Length beyond end");

  mDOMWordOffset ++;
  if (mDOMWordOffset >= (PRInt32)mDOMWordText.Length())
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
  PRInt32 firstColon = -1;
  for (PRInt32 i = mDOMWordOffset;
       i < PRInt32(mDOMWordText.Length()); i ++) {
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
          i < (PRInt32)mDOMWordText.Length() - 1 &&
          ClassifyCharacter(i + 1, false) == CHAR_CLASS_WORD) {
        return true;
      }
    } else if (mDOMWordText[i] == ':' && firstColon < 0) {
      firstColon = i;

      // If the first colon is followed by a slash, consider it a URL
      // This will catch things like asdf://foo.com
      if (firstColon < (PRInt32)mDOMWordText.Length() - 1 &&
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
WordSplitState::ShouldSkipWord(PRInt32 aStart, PRInt32 aLength)
{
  PRInt32 last = aStart + aLength;

  // check to see if the word contains a digit
  for (PRInt32 i = aStart; i < last; i ++) {
    PRUnichar ch = mDOMWordText[i];
    // XXX Shouldn't this be something a lot more complex, Unicode-based?
    if (ch >= '0' && ch <= '9')
      return true;
  }

  // not special
  return false;
}

// mozInlineSpellWordUtil::SplitDOMWord

void
mozInlineSpellWordUtil::SplitDOMWord(PRInt32 aStart, PRInt32 aEnd)
{
  WordSplitState state(this, mSoftText, aStart, aEnd - aStart);
  state.mCurCharClass = state.ClassifyCharacter(0, true);

  state.AdvanceThroughSeparators();
  if (state.mCurCharClass != CHAR_CLASS_END_OF_INPUT &&
      state.IsSpecialWord()) {
    PRInt32 specialWordLength = state.mDOMWordText.Length() - state.mDOMWordOffset;
    mRealWords.AppendElement(
        RealWord(aStart + state.mDOMWordOffset, specialWordLength, false));

    return;
  }

  while (state.mCurCharClass != CHAR_CLASS_END_OF_INPUT) {
    state.AdvanceThroughSeparators();
    if (state.mCurCharClass == CHAR_CLASS_END_OF_INPUT)
      break;

    // save the beginning of the word
    PRInt32 wordOffset = state.mDOMWordOffset;

    // find the end of the word
    state.AdvanceThroughWord();
    PRInt32 wordLen = state.mDOMWordOffset - wordOffset;
    mRealWords.AppendElement(
      RealWord(aStart + wordOffset, wordLen,
               !state.ShouldSkipWord(wordOffset, wordLen)));
  }
}

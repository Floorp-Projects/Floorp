/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozInlineSpellWordUtil_h
#define mozInlineSpellWordUtil_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Document.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

//#define DEBUG_SPELLCHECK

class nsRange;
class nsINode;

namespace mozilla {
class TextEditor;

namespace dom {
class Document;
}
}  // namespace mozilla

struct NodeOffset {
  nsCOMPtr<nsINode> mNode;
  int32_t mOffset;

  NodeOffset() : mOffset(0) {}
  NodeOffset(nsINode* aNode, int32_t aOffset)
      : mNode(aNode), mOffset(aOffset) {}

  bool operator==(const NodeOffset& aOther) const {
    return mNode == aOther.mNode && mOffset == aOther.mOffset;
  }

  bool operator!=(const NodeOffset& aOther) const { return !(*this == aOther); }

  nsINode* Node() const { return mNode.get(); }
  int32_t Offset() const { return mOffset; }
};

class NodeOffsetRange {
 private:
  NodeOffset mBegin;
  NodeOffset mEnd;

 public:
  NodeOffsetRange() {}
  NodeOffsetRange(NodeOffset b, NodeOffset e) : mBegin(b), mEnd(e) {}

  NodeOffset Begin() const { return mBegin; }

  NodeOffset End() const { return mEnd; }
};

/**
 *    This class extracts text from the DOM and builds it into a single string.
 *    The string includes whitespace breaks whereever non-inline elements begin
 *    and end. This string is broken into "real words", following somewhat
 *    complex rules; for example substrings that look like URLs or
 *    email addresses are treated as single words, but otherwise many kinds of
 *    punctuation are treated as word separators. GetNextWord provides a way
 *    to iterate over these "real words".
 *
 *    The basic operation is:
 *
 *    1. Call Init with the editor that you're using.
 *    2. Call SetPositionAndEnd to to initialize the current position inside the
 *       previously given range and set where you want to stop spellchecking.
 *       We'll stop at the word boundary after that. If SetEnd is not called,
 *       we'll stop at the end of the root element.
 *    3. Call GetNextWord over and over until it returns false.
 */

class MOZ_STACK_CLASS mozInlineSpellWordUtil {
 public:
  static mozilla::Maybe<mozInlineSpellWordUtil> Create(
      const mozilla::TextEditor& aTextEditor);

  // sets the current position, this should be inside the range. If we are in
  // the middle of a word, we'll move to its start.
  nsresult SetPositionAndEnd(nsINode* aPositionNode, int32_t aPositionOffset,
                             nsINode* aEndNode, int32_t aEndOffset);

  // Given a point inside or immediately following a word, this returns the
  // DOM range that exactly encloses that word's characters. The current
  // position will be at the end of the word. This will find the previous
  // word if the current position is space, so if you care that the point is
  // inside the word, you should check the range.
  //
  // THIS CHANGES THE CURRENT POSITION AND RANGE. It is designed to be called
  // before you actually generate the range you are interested in and iterate
  // the words in it.
  nsresult GetRangeForWord(nsINode* aWordNode, int32_t aWordOffset,
                           nsRange** aRange);

  // Convenience functions, object must be initialized
  nsresult MakeRange(NodeOffset aBegin, NodeOffset aEnd,
                     nsRange** aRange) const;
  static already_AddRefed<nsRange> MakeRange(const NodeOffsetRange& aRange);

  // Moves to the the next word in the range, and retrieves it's text and range.
  // false is returned when we are done checking.
  // aSkipChecking will be set if the word is "special" and shouldn't be
  // checked (e.g., an email address).
  bool GetNextWord(nsAString& aText, NodeOffsetRange* aNodeOffsetRange,
                   bool* aSkipChecking);

  // Call to normalize some punctuation. This function takes an autostring
  // so we can access characters directly.
  static void NormalizeWord(nsAString& aWord);

  mozilla::dom::Document* GetDocument() const { return mDocument; }
  const nsINode* GetRootNode() const { return mRootNode; }

 private:
  // A list of where we extracted text from, ordered by mSoftTextOffset. A given
  // DOM node appears at most once in this list.
  struct DOMTextMapping {
    NodeOffset mNodeOffset;
    int32_t mSoftTextOffset;
    int32_t mLength;

    DOMTextMapping(NodeOffset aNodeOffset, int32_t aSoftTextOffset,
                   int32_t aLength)
        : mNodeOffset(aNodeOffset),
          mSoftTextOffset(aSoftTextOffset),
          mLength(aLength) {}
  };

  struct SoftText {
    void AdjustBeginAndBuildText(NodeOffset aBegin, NodeOffset aEnd,
                                 const nsINode* aRootNode);

    void Invalidate() { mIsValid = false; }

    const NodeOffset& GetBegin() const { return mBegin; }
    const NodeOffset& GetEnd() const { return mEnd; }

    const nsTArray<DOMTextMapping>& GetDOMMapping() const {
      return mDOMMapping;
    }

    const nsString& GetValue() const { return mValue; }

    bool mIsValid = false;

   private:
    NodeOffset mBegin = NodeOffset(nullptr, 0);
    NodeOffset mEnd = NodeOffset(nullptr, 0);

    nsTArray<DOMTextMapping> mDOMMapping;

    // DOM text covering the soft range, with newlines added at block boundaries
    nsString mValue;
  };

  SoftText mSoftText;

  mozInlineSpellWordUtil(mozilla::dom::Document& aDocument,
                         bool aIsContentEditableOrDesignMode, nsINode& aRootNode

                         )
      : mDocument(&aDocument),
        mIsContentEditableOrDesignMode(aIsContentEditableOrDesignMode),
        mRootNode(&aRootNode),
        mNextWordIndex(-1) {}

  // cached stuff for the editor
  const RefPtr<mozilla::dom::Document> mDocument;
  const bool mIsContentEditableOrDesignMode;

  // range to check, see SetPosition and SetEnd
  const nsINode* mRootNode;

  // A list of the "real words" in mSoftText.mValue, ordered by mSoftTextOffset
  struct RealWord {
    int32_t mSoftTextOffset;
    uint32_t mLength : 31;
    uint32_t mCheckableWord : 1;

    RealWord(int32_t aOffset, uint32_t aLength, bool aCheckable)
        : mSoftTextOffset(aOffset),
          mLength(aLength),
          mCheckableWord(aCheckable) {
      static_assert(sizeof(RealWord) == 8,
                    "RealWord should be limited to 8 bytes");
      MOZ_ASSERT(aLength < INT32_MAX,
                 "Word length is too large to fit in the bitfield");
    }

    int32_t EndOffset() const { return mSoftTextOffset + mLength; }
  };
  using RealWords = nsTArray<RealWord>;
  RealWords mRealWords;
  int32_t mNextWordIndex;

  nsresult EnsureWords(NodeOffset aSoftBegin, NodeOffset aSoftEnd);

  int32_t MapDOMPositionToSoftTextOffset(NodeOffset aNodeOffset) const;
  // Map an offset into mSoftText.mValue to a DOM position. Note that two DOM
  // positions can map to the same mSoftText.mValue offset, e.g. given nodes
  // A=aaaa and B=bbbb forming aaaabbbb, (A,4) and (B,0) give the same string
  // offset. So, aHintBefore controls which position we return ... if aHint is
  // eEnd then the position indicates the END of a range so we return (A,4).
  // Otherwise the position indicates the START of a range so we return (B,0).
  enum DOMMapHint { HINT_BEGIN, HINT_END };
  NodeOffset MapSoftTextOffsetToDOMPosition(int32_t aSoftTextOffset,
                                            DOMMapHint aHint) const;

  static void ToString(DOMMapHint aHint, nsACString& aResult);

  // Finds the index of the real word containing aSoftTextOffset, or -1 if none
  // If it's exactly between two words, then if aHint is HINT_BEGIN, return the
  // later word (favouring the assumption that it's the BEGINning of a word),
  // otherwise return the earlier word (assuming it's the END of a word).
  // If aSearchForward is true, then if we don't find a word at the given
  // position, search forward until we do find a word and return that (if
  // found).
  int32_t FindRealWordContaining(int32_t aSoftTextOffset, DOMMapHint aHint,
                                 bool aSearchForward) const;

  mozilla::Result<RealWords, nsresult> BuildRealWords() const;

  nsresult SplitDOMWordAndAppendTo(int32_t aStart, int32_t aEnd,
                                   nsTArray<RealWord>& aRealWords) const;

  nsresult MakeRangeForWord(const RealWord& aWord, nsRange** aRange) const;
  void MakeNodeOffsetRangeForWord(const RealWord& aWord,
                                  NodeOffsetRange* aNodeOffsetRange);
};

#endif

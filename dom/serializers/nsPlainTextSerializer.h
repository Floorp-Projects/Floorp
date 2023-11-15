/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert a DOM into plaintext in a nice way
 * (eg for copy/paste as plaintext).
 */

#ifndef nsPlainTextSerializer_h__
#define nsPlainTextSerializer_h__

#include "mozilla/Maybe.h"
#include "nsAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentSerializer.h"
#include "nsIDocumentEncoder.h"
#include "nsString.h"
#include "nsTArray.h"

#include <stack>

class nsIContent;

namespace mozilla::dom {
class DocumentType;
class Element;
}  // namespace mozilla::dom

class nsPlainTextSerializer final : public nsIContentSerializer {
 public:
  nsPlainTextSerializer();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPlainTextSerializer)

  // nsIContentSerializer
  NS_IMETHOD Init(uint32_t flags, uint32_t aWrapColumn,
                  const mozilla::Encoding* aEncoding, bool aIsCopying,
                  bool aIsWholeDocument, bool* aNeedsPreformatScanning,
                  nsAString& aOutput) override;

  NS_IMETHOD AppendText(nsIContent* aText, int32_t aStartOffset,
                        int32_t aEndOffset) override;
  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection, int32_t aStartOffset,
                                int32_t aEndOffset) override;
  NS_IMETHOD AppendProcessingInstruction(
      mozilla::dom::ProcessingInstruction* aPI, int32_t aStartOffset,
      int32_t aEndOffset) override {
    return NS_OK;
  }
  NS_IMETHOD AppendComment(mozilla::dom::Comment* aComment,
                           int32_t aStartOffset, int32_t aEndOffset) override {
    return NS_OK;
  }
  NS_IMETHOD AppendDoctype(mozilla::dom::DocumentType* aDoctype) override {
    return NS_OK;
  }
  NS_IMETHOD AppendElementStart(
      mozilla::dom::Element* aElement,
      mozilla::dom::Element* aOriginalElement) override;
  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              mozilla::dom::Element* aOriginalElement) override;

  NS_IMETHOD FlushAndFinish() override;

  NS_IMETHOD Finish() override;

  NS_IMETHOD GetOutputLength(uint32_t& aLength) const override;

  NS_IMETHOD AppendDocumentStart(mozilla::dom::Document* aDocument) override;

  NS_IMETHOD ScanElementForPreformat(mozilla::dom::Element* aElement) override;
  NS_IMETHOD ForgetElementForPreformat(
      mozilla::dom::Element* aElement) override;

 private:
  ~nsPlainTextSerializer();

  nsresult GetAttributeValue(const nsAtom* aName, nsString& aValueRet) const;
  void AddToLine(const char16_t* aStringToAdd, int32_t aLength);

  void MaybeWrapAndOutputCompleteLines();

  // @param aSoftLineBreak A soft line break is a space followed by a linebreak
  // (cf. https://www.ietf.org/rfc/rfc3676.txt, section 4.2).
  void EndLine(bool aSoftLineBreak, bool aBreakBySpace = false);

  void EnsureVerticalSpace(int32_t noOfRows);

  void ConvertToLinesAndOutput(const nsAString& aString);

  void Write(const nsAString& aString);

  // @return true, iff the elements' whitespace and newline characters have to
  //         be preserved according to its style or because it's a `<pre>`
  //         element.
  bool IsElementPreformatted() const;
  bool IsInOL() const;
  bool IsInOlOrUl() const;
  bool IsCurrentNodeConverted() const;
  bool MustSuppressLeaf() const;

  /**
   * Returns the local name of the element as an atom if the element is an
   * HTML element and the atom is a static atom. Otherwise, nullptr is returned.
   */
  static nsAtom* GetIdForContent(nsIContent* aContent);
  nsresult DoOpenContainer(const nsAtom* aTag);
  void OpenContainerForOutputFormatted(const nsAtom* aTag);
  nsresult DoCloseContainer(const nsAtom* aTag);
  void CloseContainerForOutputFormatted(const nsAtom* aTag);
  nsresult DoAddLeaf(const nsAtom* aTag);

  void DoAddText();
  // @param aText Ignored if aIsLineBreak is true.
  void DoAddText(bool aIsLineBreak, const nsAString& aText);

  inline bool DoOutput() const { return mHeadLevel == 0; }

  static inline bool IsQuotedLine(const nsAString& aLine) {
    return !aLine.IsEmpty() && aLine.First() == char16_t('>');
  }

  // Stack handling functions
  bool GetLastBool(const nsTArray<bool>& aStack);
  void SetLastBool(nsTArray<bool>& aStack, bool aValue);
  void PushBool(nsTArray<bool>& aStack, bool aValue);
  bool PopBool(nsTArray<bool>& aStack);

  bool IsIgnorableRubyAnnotation(const nsAtom* aTag) const;

  // @return true, iff the elements' whitespace and newline characters have to
  //         be preserved according to its style or because it's a `<pre>`
  //         element.
  static bool IsElementPreformatted(mozilla::dom::Element* aElement);

  // https://drafts.csswg.org/css-display/#block-level
  static bool IsCssBlockLevelElement(mozilla::dom::Element* aElement);

 private:
  uint32_t mHeadLevel;

  class Settings {
   public:
    enum class HeaderStrategy {
      kNoIndentation,
      kIndentIncreasedWithHeaderLevel,
      kNumberHeadingsAndIndentSlightly
    };

    // May adapt the flags.
    //
    // @param aFlags As defined in nsIDocumentEncoder.idl.
    void Init(int32_t aFlags, uint32_t aWrapColumn);

    // Pref: converter.html2txt.structs.
    bool GetStructs() const { return mStructs; }

    // Pref: converter.html2txt.header_strategy.
    HeaderStrategy GetHeaderStrategy() const { return mHeaderStrategy; }

    // @return As defined in nsIDocumentEncoder.idl.
    int32_t GetFlags() const { return mFlags; }

    // @param aFlag As defined in nsIDocumentEncoder.idl. May consist of
    // multiple bitwise or'd flags.
    bool HasFlag(int32_t aFlag) const { return mFlags & aFlag; }

    // Whether the output should include ruby annotations.
    bool GetWithRubyAnnotation() const { return mWithRubyAnnotation; }

    uint32_t GetWrapColumn() const { return mWrapColumn; }

    bool MayWrap() const {
      return GetWrapColumn() && HasFlag(nsIDocumentEncoder::OutputFormatted |
                                        nsIDocumentEncoder::OutputWrap);
    }

    bool MayBreakLines() const {
      return !HasFlag(nsIDocumentEncoder::OutputDisallowLineBreaking);
    }

   private:
    // @param aPrefHeaderStrategy Pref: converter.html2txt.header_strategy.
    static HeaderStrategy Convert(int32_t aPrefHeaderStrategy);

    // Pref: converter.html2txt.structs.
    bool mStructs = true;

    // Pref: converter.html2txt.header_strategy.
    HeaderStrategy mHeaderStrategy =
        HeaderStrategy::kIndentIncreasedWithHeaderLevel;

    // Flags defined in nsIDocumentEncoder.idl.
    int32_t mFlags = 0;

    // Whether the output should include ruby annotations.
    bool mWithRubyAnnotation = false;

    // The wrap column is how many fixed-pitch narrow
    // (https://unicode.org/reports/tr11/) (e.g. Latin) characters
    // should be allowed on a line. There could be less chars if the chars
    // are wider than latin chars of more if the chars are more narrow.
    uint32_t mWrapColumn = 0;
  };

  Settings mSettings;

  struct Indentation {
    // The number of space characters to be inserted including the length of
    // mHeader.
    int32_t mLength = 0;

    // The header that has to be written in the indent.
    // That could be, for instance, the bullet in a bulleted list.
    nsString mHeader;
  };

  class CurrentLine {
   public:
    void ResetContentAndIndentationHeader();

    // @param aFlags As defined in nsIDocumentEncoder.idl.
    void MaybeReplaceNbspsInContent(int32_t aFlags);

    void CreateQuotesAndIndent(nsAString& aResult) const;

    bool HasContentOrIndentationHeader() const {
      return !mContent.IsEmpty() || !mIndentation.mHeader.IsEmpty();
    }

    // @param aLineBreaker May be nullptr.
    int32_t FindWrapIndexForContent(uint32_t aWrapColumn,
                                    bool aUseLineBreaker) const;

    // @return Combined width of cite quote level and indentation.
    uint32_t DeterminePrefixWidth() const {
      // XXX: Should calculate prefixwidth with GetUnicharStringWidth
      return (mCiteQuoteLevel > 0 ? mCiteQuoteLevel + 1 : 0) +
             mIndentation.mLength + uint32_t(mSpaceStuffed);
    }

    Indentation mIndentation;

    // The number of '>' characters.
    int32_t mCiteQuoteLevel = 0;

    // Whether this line is getting space-stuffed, see
    // https://datatracker.ietf.org/doc/html/rfc2646#section-4.4
    bool mSpaceStuffed = false;

    // Excludes indentation and quotes.
    nsString mContent;
  };

  CurrentLine mCurrentLine;

  class OutputManager {
   public:
    /**
     *  @param aFlags As defined in nsIDocumentEncoder.idl.
     *  @param aOutput An empty string.
     */
    OutputManager(int32_t aFlags, nsAString& aOutput);

    enum class StripTrailingWhitespaces { kMaybe, kNo };

    void Append(const CurrentLine& aCurrentLine,
                StripTrailingWhitespaces aStripTrailingWhitespaces);

    void AppendLineBreak();

    /**
     * This empties the current line cache without adding a NEWLINE.
     * Should not be used if line wrapping is of importance since
     * this function destroys the cache information.
     *
     * It will also write indentation and quotes if we believe us to be
     * at the start of the line.
     */
    void Flush(CurrentLine& aCurrentLine);

    bool IsAtFirstColumn() const { return mAtFirstColumn; }

    uint32_t GetOutputLength() const;

   private:
    /**
     * @param aString Last character is expected to not be a line break.
     */
    void Append(const nsAString& aString);

    // As defined in nsIDocumentEncoder.idl.
    const int32_t mFlags;

    nsAString& mOutput;

    bool mAtFirstColumn;

    nsString mLineBreak;
  };

  mozilla::Maybe<OutputManager> mOutputManager;

  // If we've just written out a cite blockquote, we need to remember it
  // so we don't duplicate spaces before a <pre wrap> (which mail uses to quote
  // old messages).
  bool mHasWrittenCiteBlockquote;

  int32_t mFloatingLines;  // To store the number of lazy line breaks

  // Treat quoted text as though it's preformatted -- don't wrap it.
  // Having it on a pref is a temporary measure, See bug 69638.
  int32_t mSpanLevel;

  int32_t mEmptyLines;  // Will be the number of empty lines before
                        // the current. 0 if we are starting a new
                        // line and -1 if we are in a line.

  bool mInWhitespace;
  bool mPreFormattedMail;  // we're dealing with special DOM
                           // used by Thunderbird code.

  // While handling a new tag, this variable should remind if any line break
  // is due because of a closing tag. Setting it to "TRUE" while closing the
  // tags. Hence opening tags are guaranteed to start with appropriate line
  // breaks.
  bool mLineBreakDue;

  bool mPreformattedBlockBoundary;

  int32_t mHeaderCounter[7]; /* For header-numbering:
                                Number of previous headers of
                                the same depth and in the same
                                section.
                                mHeaderCounter[1] for <h1> etc. */

  RefPtr<mozilla::dom::Element> mElement;

  // For handling table rows
  AutoTArray<bool, 8> mHasWrittenCellsForRow;

  // Values gotten in OpenContainer that is (also) needed in CloseContainer
  AutoTArray<bool, 8> mIsInCiteBlockquote;

  // The tag stack: the stack of tags we're operating on, so we can nest.
  // The stack only ever points to static atoms, so they don't need to be
  // refcounted.
  const nsAtom** mTagStack;
  uint32_t mTagStackIndex;

  // The stack indicating whether the elements we've been operating on are
  // CSS preformatted elements, so that we can tell if the text inside them
  // should be formatted.
  std::stack<bool> mPreformatStack;

  // Content in the stack above this index should be ignored:
  uint32_t mIgnoreAboveIndex;

  // The stack for ordered lists
  AutoTArray<int32_t, 100> mOLStack;

  uint32_t mULCount;

  bool mUseLineBreaker = false;

  // Conveniance constant. It would be nice to have it as a const static
  // variable, but that causes issues with OpenBSD and module unloading.
  const nsString kSpace;

  // mIgnoredChildNodeLevel is used to tell if current node is an ignorable
  // child node. The initial value of mIgnoredChildNodeLevel is 0. When
  // serializer enters those specific nodes, mIgnoredChildNodeLevel increases
  // and is greater than 0. Otherwise when serializer leaves those nodes,
  // mIgnoredChildNodeLevel decreases.
  uint32_t mIgnoredChildNodeLevel;
};

nsresult NS_NewPlainTextSerializer(nsIContentSerializer** aSerializer);

#endif

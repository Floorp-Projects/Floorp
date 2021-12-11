/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an XML DOM to an XML string that
 * could be parsed into more or less the original DOM.
 */

#ifndef nsXMLContentSerializer_h__
#define nsXMLContentSerializer_h__

#include "mozilla/Attributes.h"
#include "nsIContentSerializer.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"

#define kIndentStr u"  "_ns
#define kEndTag u"</"_ns

class nsAtom;
class nsINode;

namespace mozilla {
class Encoding;
}

class nsXMLContentSerializer : public nsIContentSerializer {
 public:
  nsXMLContentSerializer();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(uint32_t flags, uint32_t aWrapColumn,
                  const mozilla::Encoding* aEncoding, bool aIsCopying,
                  bool aRewriteEncodingDeclaration,
                  bool* aNeedsPreformatScanning, nsAString& aOutput) override;

  NS_IMETHOD AppendText(nsIContent* aText, int32_t aStartOffset,
                        int32_t aEndOffset) override;

  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection, int32_t aStartOffset,
                                int32_t aEndOffset) override;

  NS_IMETHOD AppendProcessingInstruction(
      mozilla::dom::ProcessingInstruction* aPI, int32_t aStartOffset,
      int32_t aEndOffset) override;

  NS_IMETHOD AppendComment(mozilla::dom::Comment* aComment,
                           int32_t aStartOffset, int32_t aEndOffset) override;

  NS_IMETHOD AppendDoctype(mozilla::dom::DocumentType* aDoctype) override;

  NS_IMETHOD AppendElementStart(
      mozilla::dom::Element* aElement,
      mozilla::dom::Element* aOriginalElement) override;

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              mozilla::dom::Element* aOriginalElement) override;

  NS_IMETHOD FlushAndFinish() override { return NS_OK; }

  NS_IMETHOD Finish() override;

  NS_IMETHOD GetOutputLength(uint32_t& aLength) const override;

  NS_IMETHOD AppendDocumentStart(mozilla::dom::Document* aDocument) override;

  NS_IMETHOD ScanElementForPreformat(mozilla::dom::Element* aElement) override {
    return NS_OK;
  }
  NS_IMETHOD ForgetElementForPreformat(
      mozilla::dom::Element* aElement) override {
    return NS_OK;
  }

 protected:
  virtual ~nsXMLContentSerializer();

  /**
   * Appends a char16_t character and increments the column position
   */
  [[nodiscard]] bool AppendToString(const char16_t aChar,
                                    nsAString& aOutputStr);

  /**
   * Appends a nsAString string and increments the column position
   */
  [[nodiscard]] bool AppendToString(const nsAString& aStr,
                                    nsAString& aOutputStr);

  /**
   * Appends a string by replacing all line-endings
   * by mLineBreak, except in the case of raw output.
   * It increments the column position.
   */
  [[nodiscard]] bool AppendToStringConvertLF(const nsAString& aStr,
                                             nsAString& aOutputStr);

  /**
   * Appends a string by wrapping it when necessary.
   * It updates the column position.
   */
  [[nodiscard]] bool AppendToStringWrapped(const nsAString& aStr,
                                           nsAString& aOutputStr);

  /**
   * Appends a string by formating and wrapping it when necessary
   * It updates the column position.
   */
  [[nodiscard]] bool AppendToStringFormatedWrapped(const nsAString& aStr,
                                                   nsAString& aOutputStr);

  // used by AppendToStringWrapped
  [[nodiscard]] bool AppendWrapped_WhitespaceSequence(
      nsAString::const_char_iterator& aPos,
      const nsAString::const_char_iterator aEnd,
      const nsAString::const_char_iterator aSequenceStart,
      nsAString& aOutputStr);

  // used by AppendToStringFormatedWrapped
  [[nodiscard]] bool AppendFormatedWrapped_WhitespaceSequence(
      nsAString::const_char_iterator& aPos,
      const nsAString::const_char_iterator aEnd,
      const nsAString::const_char_iterator aSequenceStart,
      bool& aMayIgnoreStartOfLineWhitespaceSequence, nsAString& aOutputStr);

  // used by AppendToStringWrapped and AppendToStringFormatedWrapped
  [[nodiscard]] bool AppendWrapped_NonWhitespaceSequence(
      nsAString::const_char_iterator& aPos,
      const nsAString::const_char_iterator aEnd,
      const nsAString::const_char_iterator aSequenceStart,
      bool& aMayIgnoreStartOfLineWhitespaceSequence,
      bool& aSequenceStartAfterAWhiteSpace, nsAString& aOutputStr);

  /**
   * add mLineBreak to the string
   * It updates the column position and other flags.
   */
  [[nodiscard]] bool AppendNewLineToString(nsAString& aOutputStr);

  /**
   * Appends a string by translating entities
   * It doesn't increment the column position
   */
  [[nodiscard]] virtual bool AppendAndTranslateEntities(const nsAString& aStr,
                                                        nsAString& aOutputStr);

  /**
   * Helper for virtual AppendAndTranslateEntities that does the actualy work.
   *
   * Do not call this directly.  Call it via the template helper below.
   */
 private:
  [[nodiscard]] static bool AppendAndTranslateEntities(
      const nsAString& aStr, nsAString& aOutputStr,
      const uint8_t aEntityTable[], uint16_t aMaxTableIndex,
      const char* const aStringTable[]);

 protected:
  /**
   * Helper for calling AppendAndTranslateEntities in a way that guarantees we
   * don't mess up our aEntityTable sizing.  This is a bit more complicated than
   * it could be, becaue sometimes we don't want to use all of aEntityTable, so
   * we have to allow passing the amount to use independently.  But we can
   * statically ensure it's not too big.
   *
   * The first integer template argument, which callers need to specify
   * explicitly, is the index of the last entry in aEntityTable that should be
   * considered for encoding as an entity reference.  The second integer
   * argument will be deduced from the actual table passed in.
   *
   * aEntityTable contains as values indices into aStringTable.  Those represent
   * the strings that should be used to replace the characters that are used to
   * index into aEntityTable.  aStringTable[0] should be nullptr, and characters
   * that do not need replacement should map to 0 in aEntityTable.
   */
  template <uint16_t LargestIndex, uint16_t TableLength>
  [[nodiscard]] bool AppendAndTranslateEntities(
      const nsAString& aStr, nsAString& aOutputStr,
      const uint8_t (&aEntityTable)[TableLength],
      const char* const aStringTable[]) {
    static_assert(LargestIndex < TableLength,
                  "Largest allowed index must be smaller than table length");
    return AppendAndTranslateEntities(aStr, aOutputStr, aEntityTable,
                                      LargestIndex, aStringTable);
  }

  /**
   * Max index that can be used with some of our entity tables.
   */
  static const uint16_t kGTVal = 62;

  /**
   * retrieve the text content of the node and append it to the given string
   * It doesn't increment the column position
   */
  nsresult AppendTextData(nsIContent* aNode, int32_t aStartOffset,
                          int32_t aEndOffset, nsAString& aStr,
                          bool aTranslateEntities);

  virtual nsresult PushNameSpaceDecl(const nsAString& aPrefix,
                                     const nsAString& aURI, nsIContent* aOwner);
  void PopNameSpaceDeclsFor(nsIContent* aOwner);

  /**
   * The problem that ConfirmPrefix fixes is that anyone can insert nodes
   * through the DOM that have a namespace URI and a random or empty or
   * previously existing prefix that's totally unrelated to the prefixes
   * declared at that point through xmlns attributes.  So what ConfirmPrefix
   * does is ensure that we can map aPrefix to the namespace URI aURI (for
   * example, that the prefix is not already mapped to some other namespace).
   * aPrefix will be adjusted, if necessary, so the value of the prefix
   * _after_ this call is what should be serialized.
   * @param aPrefix the prefix that may need adjusting
   * @param aURI the namespace URI we want aPrefix to point to
   * @param aElement the element we're working with (needed for proper default
   *                 namespace handling)
   * @param aIsAttribute true if we're confirming a prefix for an attribute.
   * @return true if we need to push the (prefix, uri) pair on the namespace
   *                 stack (note that this can happen even if the prefix is
   *                 empty).
   */
  bool ConfirmPrefix(nsAString& aPrefix, const nsAString& aURI,
                     nsIContent* aElement, bool aIsAttribute);
  /**
   * GenerateNewPrefix generates a new prefix and writes it to aPrefix
   */
  void GenerateNewPrefix(nsAString& aPrefix);

  uint32_t ScanNamespaceDeclarations(mozilla::dom::Element* aContent,
                                     mozilla::dom::Element* aOriginalElement,
                                     const nsAString& aTagNamespaceURI);

  [[nodiscard]] virtual bool SerializeAttributes(
      mozilla::dom::Element* aContent, mozilla::dom::Element* aOriginalElement,
      nsAString& aTagPrefix, const nsAString& aTagNamespaceURI,
      nsAtom* aTagName, nsAString& aStr, uint32_t aSkipAttr, bool aAddNSAttr);

  [[nodiscard]] bool SerializeAttr(const nsAString& aPrefix,
                                   const nsAString& aName,
                                   const nsAString& aValue, nsAString& aStr,
                                   bool aDoEscapeEntities);

  bool IsJavaScript(nsIContent* aContent, nsAtom* aAttrNameAtom,
                    int32_t aAttrNamespaceID, const nsAString& aValueString);

  /**
   * This method can be redefined to check if the element can be serialized.
   * It is called when the serialization of the start tag is asked
   * (AppendElementStart)
   * In this method you can also force the formating
   * by setting aForceFormat to true.
   * @return boolean  true if the element can be output
   */
  virtual bool CheckElementStart(mozilla::dom::Element* aElement,
                                 bool& aForceFormat, nsAString& aStr,
                                 nsresult& aResult);

  /**
   * This method is responsible for appending the '>' at the end of the start
   * tag, possibly preceded by '/' and maybe a ' ' before that too.
   *
   * aElement and aOriginalElement are the same as the corresponding arguments
   * to AppendElementStart.
   */
  [[nodiscard]] bool AppendEndOfElementStart(
      mozilla::dom::Element* aEleemnt, mozilla::dom::Element* aOriginalElement,
      nsAString& aStr);

  /**
   * This method can be redefine to serialize additional things just after
   * the serialization of the start tag.
   * (called at the end of AppendElementStart)
   */
  [[nodiscard]] virtual bool AfterElementStart(nsIContent* aContent,
                                               nsIContent* aOriginalElement,
                                               nsAString& aStr) {
    return true;
  };

  /**
   * This method can be redefined to check if the element can be serialized.
   * It is called when the serialization of the end tag is asked
   * (AppendElementEnd)
   * In this method you can also force the formating
   * by setting aForceFormat to true.
   * @return boolean  true if the element can be output
   */
  virtual bool CheckElementEnd(mozilla::dom::Element* aElement,
                               mozilla::dom::Element* aOriginalElement,
                               bool& aForceFormat, nsAString& aStr);

  /**
   * This method can be redefine to serialize additional things just after
   * the serialization of the end tag.
   * (called at the end of AppendElementStart)
   */
  virtual void AfterElementEnd(nsIContent* aContent, nsAString& aStr){};

  /**
   * Returns true if a line break should be inserted before an element open tag
   */
  virtual bool LineBreakBeforeOpen(int32_t aNamespaceID, nsAtom* aName);

  /**
   * Returns true if a line break should be inserted after an element open tag
   */
  virtual bool LineBreakAfterOpen(int32_t aNamespaceID, nsAtom* aName);

  /**
   * Returns true if a line break should be inserted after an element close tag
   */
  virtual bool LineBreakBeforeClose(int32_t aNamespaceID, nsAtom* aName);

  /**
   * Returns true if a line break should be inserted after an element close tag
   */
  virtual bool LineBreakAfterClose(int32_t aNamespaceID, nsAtom* aName);

  /**
   * add intendation. Call only in the case of formating and if the current
   * position is at 0. It updates the column position.
   */
  [[nodiscard]] bool AppendIndentation(nsAString& aStr);

  [[nodiscard]] bool IncrIndentation(nsAtom* aName);
  void DecrIndentation(nsAtom* aName);

  // Functions to check for newlines that needs to be added between nodes in
  // the root of a document. See mAddNewlineForRootNode
  [[nodiscard]] bool MaybeAddNewlineForRootNode(nsAString& aStr);
  void MaybeFlagNewlineForRootNode(nsINode* aNode);

  // Functions to check if we enter in or leave from a preformated content
  virtual void MaybeEnterInPreContent(nsIContent* aNode);
  virtual void MaybeLeaveFromPreContent(nsIContent* aNode);

  bool ShouldMaintainPreLevel() const;
  int32_t PreLevel() const {
    MOZ_ASSERT(ShouldMaintainPreLevel());
    return mPreLevel;
  }
  int32_t& PreLevel() {
    MOZ_ASSERT(ShouldMaintainPreLevel());
    return mPreLevel;
  }

  bool MaybeSerializeIsValue(mozilla::dom::Element* aElement, nsAString& aStr);

  int32_t mPrefixIndex;

  struct NameSpaceDecl {
    nsString mPrefix;
    nsString mURI;
    nsIContent* mOwner;
  };

  nsTArray<NameSpaceDecl> mNameSpaceStack;

  // nsIDocumentEncoder flags
  MOZ_INIT_OUTSIDE_CTOR uint32_t mFlags;

  // characters to use for line break
  nsString mLineBreak;

  // The charset that was passed to Init()
  nsCString mCharset;

  // current column position on the current line
  uint32_t mColPos;

  // true = pretty formating should be done (OutputFormated flag)
  MOZ_INIT_OUTSIDE_CTOR bool mDoFormat;

  // true = no formatting,(OutputRaw flag)
  // no newline convertion and no rewrap long lines even if OutputWrap is set.
  MOZ_INIT_OUTSIDE_CTOR bool mDoRaw;

  // true = wrapping should be done (OutputWrap flag)
  MOZ_INIT_OUTSIDE_CTOR bool mDoWrap;

  // true = we can break lines (OutputDisallowLineBreaking flag)
  MOZ_INIT_OUTSIDE_CTOR bool mAllowLineBreaking;

  // number of maximum column in a line, in the wrap mode
  MOZ_INIT_OUTSIDE_CTOR uint32_t mMaxColumn;

  // current indent value
  nsString mIndent;

  // this is the indentation level after the indentation reached
  // the maximum length of indentation
  int32_t mIndentOverflow;

  // says if the indentation has been already added on the current line
  bool mIsIndentationAddedOnCurrentLine;

  // the string which is currently added is in an attribute
  bool mInAttribute;

  // true = a newline character should be added. It's only
  // useful when serializing root nodes. see MaybeAddNewlineForRootNode and
  // MaybeFlagNewlineForRootNode
  bool mAddNewlineForRootNode;

  // Indicates that a space will be added if and only if content is
  // continued on the same line while serializing source.  Otherwise,
  // the newline character acts as the whitespace and no space is needed.
  // used when mDoFormat = true
  bool mAddSpace;

  // says that if the next string to add contains a newline character at the
  // begining, then this newline character should be ignored, because a
  // such character has already been added into the output string
  bool mMayIgnoreLineBreakSequence;

  bool mBodyOnly;
  int32_t mInBody;

  // Non-owning.
  nsAString* mOutput;

 private:
  // number of nested elements which have preformated content
  MOZ_INIT_OUTSIDE_CTOR int32_t mPreLevel;

  static const uint8_t kEntities[];
  static const uint8_t kAttrEntities[];
  static const char* const kEntityStrings[];
};

nsresult NS_NewXMLContentSerializer(nsIContentSerializer** aSerializer);

#endif

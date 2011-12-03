/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an XML DOM to an XML string that
 * could be parsed into more or less the original DOM.
 */

#ifndef nsXMLContentSerializer_h__
#define nsXMLContentSerializer_h__

#include "nsIContent.h"
#include "nsIContentSerializer.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIParser.h"

#define kIndentStr NS_LITERAL_STRING("  ")
#define kEndTag NS_LITERAL_STRING("</")

class nsIDOMNode;
class nsIAtom;

class nsXMLContentSerializer : public nsIContentSerializer {
 public:
  nsXMLContentSerializer();
  virtual ~nsXMLContentSerializer();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  const char* aCharSet, bool aIsCopying,
                  bool aRewriteEncodingDeclaration);

  NS_IMETHOD AppendText(nsIContent* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAString& aStr);

  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAString& aStr);

  NS_IMETHOD AppendProcessingInstruction(nsIContent* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAString& aStr);

  NS_IMETHOD AppendComment(nsIContent* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAString& aStr);
  
  NS_IMETHOD AppendDoctype(nsIContent *aDoctype,
                           nsAString& aStr);

  NS_IMETHOD AppendElementStart(mozilla::dom::Element* aElement,
                                mozilla::dom::Element* aOriginalElement,
                                nsAString& aStr);

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              nsAString& aStr);

  NS_IMETHOD Flush(nsAString& aStr) { return NS_OK; }

  NS_IMETHOD AppendDocumentStart(nsIDocument *aDocument,
                                 nsAString& aStr);

 protected:

  /**
   * Appends a PRUnichar character and increments the column position
   */
  void AppendToString(const PRUnichar aChar,
                      nsAString& aOutputStr);

  /**
   * Appends a nsAString string and increments the column position
   */
  void AppendToString(const nsAString& aStr,
                      nsAString& aOutputStr);

  /**
   * Appends a string by replacing all line-endings
   * by mLineBreak, except in the case of raw output.
   * It increments the column position.
   */
  void AppendToStringConvertLF(const nsAString& aStr,
                               nsAString& aOutputStr);

  /**
   * Appends a string by wrapping it when necessary.
   * It updates the column position.
   */
  void AppendToStringWrapped(const nsASingleFragmentString& aStr,
                             nsAString& aOutputStr);

  /**
   * Appends a string by formating and wrapping it when necessary
   * It updates the column position.
   */
  void AppendToStringFormatedWrapped(const nsASingleFragmentString& aStr,
                                     nsAString& aOutputStr);

  // used by AppendToStringWrapped
  void AppendWrapped_WhitespaceSequence(
          nsASingleFragmentString::const_char_iterator &aPos,
          const nsASingleFragmentString::const_char_iterator aEnd,
          const nsASingleFragmentString::const_char_iterator aSequenceStart,
          nsAString &aOutputStr);

  // used by AppendToStringFormatedWrapped
  void AppendFormatedWrapped_WhitespaceSequence(
          nsASingleFragmentString::const_char_iterator &aPos,
          const nsASingleFragmentString::const_char_iterator aEnd,
          const nsASingleFragmentString::const_char_iterator aSequenceStart,
          bool &aMayIgnoreStartOfLineWhitespaceSequence,
          nsAString &aOutputStr);

  // used by AppendToStringWrapped and AppendToStringFormatedWrapped
  void AppendWrapped_NonWhitespaceSequence(
          nsASingleFragmentString::const_char_iterator &aPos,
          const nsASingleFragmentString::const_char_iterator aEnd,
          const nsASingleFragmentString::const_char_iterator aSequenceStart,
          bool &aMayIgnoreStartOfLineWhitespaceSequence,
          bool &aSequenceStartAfterAWhiteSpace,
          nsAString &aOutputStr);

  /**
   * add mLineBreak to the string
   * It updates the column position and other flags.
   */
  void AppendNewLineToString(nsAString& aOutputStr);


  /**
   * Appends a string by translating entities
   * It doesn't increment the column position
   */
  virtual void AppendAndTranslateEntities(const nsAString& aStr,
                                          nsAString& aOutputStr);

  /**
   * retrieve the text content of the node and append it to the given string
   * It doesn't increment the column position
   */
  nsresult AppendTextData(nsIContent* aNode,
                          PRInt32 aStartOffset,
                          PRInt32 aEndOffset,
                          nsAString& aStr,
                          bool aTranslateEntities);

  virtual nsresult PushNameSpaceDecl(const nsAString& aPrefix,
                                     const nsAString& aURI,
                                     nsIContent* aOwner);
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
  bool ConfirmPrefix(nsAString& aPrefix,
                       const nsAString& aURI,
                       nsIContent* aElement,
                       bool aIsAttribute);
  /**
   * GenerateNewPrefix generates a new prefix and writes it to aPrefix
   */
  void GenerateNewPrefix(nsAString& aPrefix);

  PRUint32 ScanNamespaceDeclarations(nsIContent* aContent,
                                     nsIContent *aOriginalElement,
                                     const nsAString& aTagNamespaceURI);

  virtual void SerializeAttributes(nsIContent* aContent,
                                   nsIContent *aOriginalElement,
                                   nsAString& aTagPrefix,
                                   const nsAString& aTagNamespaceURI,
                                   nsIAtom* aTagName,
                                   nsAString& aStr,
                                   PRUint32 aSkipAttr,
                                   bool aAddNSAttr);

  void SerializeAttr(const nsAString& aPrefix,
                     const nsAString& aName,
                     const nsAString& aValue,
                     nsAString& aStr,
                     bool aDoEscapeEntities);

  bool IsJavaScript(nsIContent * aContent,
                      nsIAtom* aAttrNameAtom,
                      PRInt32 aAttrNamespaceID,
                      const nsAString& aValueString);

  /**
   * This method can be redefined to check if the element can be serialized.
   * It is called when the serialization of the start tag is asked 
   * (AppendElementStart)
   * In this method you can also force the formating
   * by setting aForceFormat to true.
   * @return boolean  true if the element can be output
   */
  virtual bool CheckElementStart(nsIContent * aContent,
                                   bool & aForceFormat,
                                   nsAString& aStr);

  /**
   * this method is responsible to finish the start tag,
   * in particulary to append the "greater than" sign
   */
  virtual void AppendEndOfElementStart(nsIContent *aOriginalElement,
                                       nsIAtom * aName,
                                       PRInt32 aNamespaceID,
                                       nsAString& aStr);

  /**
   * This method can be redefine to serialize additional things just after
   * after the serialization ot the start tag.
   * (called at the end of AppendElementStart)
   */
  virtual void AfterElementStart(nsIContent * aContent,
                                 nsIContent *aOriginalElement,
                                 nsAString& aStr) { };

  /**
   * This method can be redefined to check if the element can be serialized.
   * It is called when the serialization of the end tag is asked 
   * (AppendElementEnd)
   * In this method you can also force the formating
   * by setting aForceFormat to true.
   * @return boolean  true if the element can be output
   */
  virtual bool CheckElementEnd(nsIContent * aContent,
                                 bool & aForceFormat,
                                 nsAString& aStr);

  /**
   * This method can be redefine to serialize additional things just after
   * after the serialization ot the end tag.
   * (called at the end of AppendElementStart)
   */
  virtual void AfterElementEnd(nsIContent * aContent,
                               nsAString& aStr) { };

  /**
   * Returns true if a line break should be inserted before an element open tag
   */
  virtual bool LineBreakBeforeOpen(PRInt32 aNamespaceID, nsIAtom* aName);

  /**
   * Returns true if a line break should be inserted after an element open tag
   */
  virtual bool LineBreakAfterOpen(PRInt32 aNamespaceID, nsIAtom* aName);

  /**
   * Returns true if a line break should be inserted after an element close tag
   */
  virtual bool LineBreakBeforeClose(PRInt32 aNamespaceID, nsIAtom* aName);

  /**
   * Returns true if a line break should be inserted after an element close tag
   */
  virtual bool LineBreakAfterClose(PRInt32 aNamespaceID, nsIAtom* aName);

  /**
   * add intendation. Call only in the case of formating and if the current
   * position is at 0. It updates the column position.
   */
  void AppendIndentation(nsAString& aStr);
  void IncrIndentation(nsIAtom* aName);
  void DecrIndentation(nsIAtom* aName);

  // Functions to check for newlines that needs to be added between nodes in
  // the root of a document. See mAddNewlineForRootNode
  void MaybeAddNewlineForRootNode(nsAString& aStr);
  void MaybeFlagNewlineForRootNode(nsINode* aNode);

  // Functions to check if we enter in or leave from a preformated content
  virtual void MaybeEnterInPreContent(nsIContent* aNode);
  virtual void MaybeLeaveFromPreContent(nsIContent* aNode);

  PRInt32 mPrefixIndex;

  struct NameSpaceDecl {
    nsString mPrefix;
    nsString mURI;
    nsIContent* mOwner;
  };

  nsTArray<NameSpaceDecl> mNameSpaceStack;

  // nsIDocumentEncoder flags
  PRUint32  mFlags;

  // characters to use for line break
  nsString  mLineBreak;

  // The charset that was passed to Init()
  nsCString mCharset;
  
  // current column position on the current line
  PRUint32   mColPos;

  // true = pretty formating should be done (OutputFormated flag)
  bool mDoFormat;

  // true = no formatting,(OutputRaw flag)
  // no newline convertion and no rewrap long lines even if OutputWrap is set.
  bool mDoRaw;

  // true = wrapping should be done (OutputWrap flag)
  bool mDoWrap;

  // number of maximum column in a line, in the wrap mode
  PRUint32   mMaxColumn;

  // current indent value
  nsString   mIndent;

  // this is the indentation level after the indentation reached
  // the maximum length of indentation
  PRInt32    mIndentOverflow;

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
  bool          mAddSpace;

  // says that if the next string to add contains a newline character at the
  // begining, then this newline character should be ignored, because a
  // such character has already been added into the output string
  bool          mMayIgnoreLineBreakSequence;

  bool          mBodyOnly;
  PRInt32       mInBody;

  // number of nested elements which have preformated content
  PRInt32       mPreLevel;
};

nsresult
NS_NewXMLContentSerializer(nsIContentSerializer** aSerializer);

#endif 

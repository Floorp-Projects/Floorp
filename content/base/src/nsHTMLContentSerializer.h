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
 * nsIDocumentEncoder to convert an HTML (not XHTML!) DOM to an HTML
 * string that could be parsed into more or less the original DOM.
 */

#ifndef nsHTMLContentSerializer_h__
#define nsHTMLContentSerializer_h__

#include "nsXMLContentSerializer.h"
#include "nsIEntityConverter.h"
#include "nsString.h"

class nsIContent;
class nsIAtom;

class nsHTMLContentSerializer : public nsXMLContentSerializer {
 public:
  nsHTMLContentSerializer();
  virtual ~nsHTMLContentSerializer();

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  const char* aCharSet, PRBool aIsCopying,
                  PRBool aIsWholeDocument);

  NS_IMETHOD AppendText(nsIDOMText* aText, 
                        PRInt32 aStartOffset,
                        PRInt32 aEndOffset,
                        nsAString& aStr);
  NS_IMETHOD AppendElementStart(nsIDOMElement *aElement,
                                nsIDOMElement *aOriginalElement,
                                nsAString& aStr);
  
  NS_IMETHOD AppendElementEnd(nsIDOMElement *aElement,
                              nsAString& aStr);

  NS_IMETHOD AppendDocumentStart(nsIDOMDocument *aDocument,
                                 nsAString& aStr);
 protected:
  PRBool LineBreakBeforeOpen(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool LineBreakAfterOpen(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool LineBreakBeforeClose(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool LineBreakAfterClose(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool IsFirstChildOfOL(nsIDOMElement* aElement);
  void StartIndentation(nsIAtom* aName, 
                        PRBool aHasDirtyAttr,
                        nsAString& aStr);
  void EndIndentation(nsIAtom* aName, 
                      PRBool aHasDirtyAttr,
                      nsAString& aStr);
  nsresult GetEntityConverter(nsIEntityConverter** aConverter);
  void SerializeAttributes(nsIContent* aContent,
                           nsIAtom* aTagName,
                           nsAString& aStr);
  void SerializeLIValueAttribute(nsIDOMElement* aElement,
                                 nsAString& aStr);
  virtual void AppendToString(const PRUnichar* aStr,
                              PRInt32 aLength,
                              nsAString& aOutputStr);
  virtual void AppendToString(const PRUnichar aChar,
                              nsAString& aOutputStr);
  virtual void AppendToString(const nsAString& aStr,
                              nsAString& aOutputStr,
                              PRBool aTranslateEntities = PR_FALSE,
                              PRBool aIncrColumn = PR_TRUE);

  void AppendWrapped_WhitespaceSequence(
          nsASingleFragmentString::const_char_iterator &aPos,
          const nsASingleFragmentString::const_char_iterator aEnd,
          const nsASingleFragmentString::const_char_iterator aSequenceStart,
          PRBool &aMayIgnoreStartOfLineWhitespaceSequence,
          nsAString &aOutputStr);
  void AppendWrapped_NonWhitespaceSequence(
          nsASingleFragmentString::const_char_iterator &aPos,
          const nsASingleFragmentString::const_char_iterator aEnd,
          const nsASingleFragmentString::const_char_iterator aSequenceStart,
          PRBool &aMayIgnoreStartOfLineWhitespaceSequence,
          nsAString &aOutputStr);
  virtual void AppendToStringWrapped(const nsASingleFragmentString& aStr,
                                     nsAString& aOutputStr,
                                     PRBool aTranslateEntities);
  PRBool HasLongLines(const nsString& text, PRInt32& aLastNewlineOffset);
  nsresult EscapeURI(const nsAString& aURI, nsAString& aEscapedURI);
  PRBool IsJavaScript(nsIAtom* aAttrNameAtom, const nsAString& aAttrValueString);

  nsCOMPtr<nsIEntityConverter> mEntityConverter;

  PRInt32   mIndent;

  PRUint32  mInBody;

  PRPackedBool  mDoFormat;
  PRPackedBool  mDoHeader;
  PRPackedBool  mBodyOnly;
  PRPackedBool  mIsCopying; // Set to PR_TRUE only while copying

  // Indicates that a space will be added if and only if content is
  // continued on the same line while serializing source.  Otherwise,
  // the newline character acts as the whitespace and no space is needed.
  PRPackedBool  mAddSpace;
  PRPackedBool  mMayIgnoreLineBreakSequence;

  // This is to ensure that we only do meta tag fixups when dealing with
  // whole documents.
  PRPackedBool  mIsWholeDocument;

  // To keep track of First LI child of OL in selected range 
  PRPackedBool  mIsFirstChildOfOL;
  PRInt32       mPreLevel;

  /*
   * mInCDATA is set to PR_TRUE while the serializer is serializing
   * the content of a element whose content is considerd CDATA by the
   * serializer (such elements are 'script', 'style', 'noscript' and
   * possibly others) This doesn't have anything to do with if the
   * element is defined as CDATA in the DTD, it simply means we'll
   * output the content of the element without doing any entity encoding
   * what so ever.
   */
  PRPackedBool mInCDATA;

  PRInt32   mMaxColumn;

  // To keep track of startvalue of OL and first list item for nested lists
  struct olState {
    olState(PRInt32 aStart, PRBool aIsFirst):startVal(aStart),isFirstListItem(aIsFirst)
    {
    }
    PRInt32 startVal;
    PRBool isFirstListItem;
  };

  nsAutoVoidArray   mOLStateStack;// Stack to store one olState struct per <OL>.
};

nsresult
NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif

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
 * nsIDocumentEncoder to convert an XHTML (not HTML!) DOM to an XHTML
 * string that could be parsed into more or less the original DOM.
 */

#ifndef nsXHTMLContentSerializer_h__
#define nsXHTMLContentSerializer_h__

#include "nsXMLContentSerializer.h"
#include "nsIEntityConverter.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIContent;
class nsIAtom;

class nsXHTMLContentSerializer : public nsXMLContentSerializer {
 public:
  nsXHTMLContentSerializer();
  virtual ~nsXHTMLContentSerializer();

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  const char* aCharSet, bool aIsCopying,
                  bool aRewriteEncodingDeclaration);

  NS_IMETHOD AppendText(nsIContent* aText,
                        PRInt32 aStartOffset,
                        PRInt32 aEndOffset,
                        nsAString& aStr);

  NS_IMETHOD AppendDocumentStart(nsIDocument *aDocument,
                                 nsAString& aStr);

 protected:


  virtual bool CheckElementStart(nsIContent * aContent,
                          bool & aForceFormat,
                          nsAString& aStr);

  virtual void AppendEndOfElementStart(nsIContent *aOriginalElement,
                               nsIAtom * aName,
                               PRInt32 aNamespaceID,
                               nsAString& aStr);

  virtual void AfterElementStart(nsIContent * aContent,
                         nsIContent *aOriginalElement,
                         nsAString& aStr);

  virtual bool CheckElementEnd(nsIContent * aContent,
                          bool & aForceFormat,
                          nsAString& aStr);

  virtual void AfterElementEnd(nsIContent * aContent,
                               nsAString& aStr);

  virtual bool LineBreakBeforeOpen(PRInt32 aNamespaceID, nsIAtom* aName);
  virtual bool LineBreakAfterOpen(PRInt32 aNamespaceID, nsIAtom* aName);
  virtual bool LineBreakBeforeClose(PRInt32 aNamespaceID, nsIAtom* aName);
  virtual bool LineBreakAfterClose(PRInt32 aNamespaceID, nsIAtom* aName);

  bool HasLongLines(const nsString& text, PRInt32& aLastNewlineOffset);

  // functions to check if we enter in or leave from a preformated content
  virtual void MaybeEnterInPreContent(nsIContent* aNode);
  virtual void MaybeLeaveFromPreContent(nsIContent* aNode);

  virtual void SerializeAttributes(nsIContent* aContent,
                           nsIContent *aOriginalElement,
                           nsAString& aTagPrefix,
                           const nsAString& aTagNamespaceURI,
                           nsIAtom* aTagName,
                           nsAString& aStr,
                           PRUint32 aSkipAttr,
                           bool aAddNSAttr);

  bool IsFirstChildOfOL(nsIContent* aElement);

  void SerializeLIValueAttribute(nsIContent* aElement,
                                 nsAString& aStr);
  bool IsShorthandAttr(const nsIAtom* aAttrName,
                         const nsIAtom* aElementName);
  virtual void AppendAndTranslateEntities(const nsAString& aStr,
                                          nsAString& aOutputStr);
  nsresult EscapeURI(nsIContent* aContent,
                     const nsAString& aURI,
                     nsAString& aEscapedURI);

  nsCOMPtr<nsIEntityConverter> mEntityConverter;

  /*
   * isHTMLParser should be set to true by the HTML parser which inherits from
   * this class. It avoids to redefine methods just for few changes.
   */
  bool          mIsHTMLSerializer;

  bool          mDoHeader;
  bool          mIsCopying; // Set to true only while copying

  /*
   * mDisableEntityEncoding is higher than 0 while the serializer is serializing
   * the content of a element whose content is considerd CDATA by the
   * serializer (such elements are 'script', 'style', 'noscript' and
   * possibly others in XHTML) This doesn't have anything to do with if the
   * element is defined as CDATA in the DTD, it simply means we'll
   * output the content of the element without doing any entity encoding
   * what so ever.
   */
  PRInt32 mDisableEntityEncoding;

  // This is to ensure that we only do meta tag fixups when dealing with
  // whole documents.
  bool          mRewriteEncodingDeclaration;

  // To keep track of First LI child of OL in selected range 
  bool          mIsFirstChildOfOL;

  // To keep track of startvalue of OL and first list item for nested lists
  struct olState {
    olState(PRInt32 aStart, bool aIsFirst)
      : startVal(aStart),
        isFirstListItem(aIsFirst)
    {
    }

    olState(const olState & aOlState)
    {
      startVal = aOlState.startVal;
      isFirstListItem = aOlState.isFirstListItem;
    }

    // the value of the start attribute in the OL
    PRInt32 startVal;

    // is true only before the serialization of the first li of an ol
    // should be false for other li in the list
    bool isFirstListItem;
  };

  // Stack to store one olState struct per <OL>.
  nsAutoTArray<olState, 8> mOLStateStack;

  bool HasNoChildren(nsIContent* aContent);
};

nsresult
NS_NewXHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif

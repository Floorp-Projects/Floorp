/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHTMLContentSerializer_h__
#define nsHTMLContentSerializer_h__

#include "nsXMLContentSerializer.h"
#include "nsIParserService.h"
#include "nsIEntityConverter.h"

class nsIContent;
class nsIAtom;

class nsHTMLContentSerializer : public nsXMLContentSerializer {
 public:
  nsHTMLContentSerializer();
  virtual ~nsHTMLContentSerializer();

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  nsIAtom* aCharSet);

  NS_IMETHOD AppendText(nsIDOMText* aText, 
                        PRInt32 aStartOffset,
                        PRInt32 aEndOffset,
                        nsAWritableString& aStr);
  NS_IMETHOD AppendElementStart(nsIDOMElement *aElement,
                                nsAWritableString& aStr);
  
  NS_IMETHOD AppendElementEnd(nsIDOMElement *aElement,
                              nsAWritableString& aStr);
 protected:
  PRBool HasDirtyAttr(nsIContent* aContent);
  PRBool LineBreakBeforeOpen(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool LineBreakAfterOpen(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool LineBreakBeforeClose(nsIAtom* aName, PRBool aHasDirtyAttr);
  PRBool LineBreakAfterClose(nsIAtom* aName, PRBool aHasDirtyAttr);
  void StartIndentation(nsIAtom* aName, 
                        PRBool aHasDirtyAttr,
                        nsAWritableString& aStr);
  void EndIndentation(nsIAtom* aName, 
                      PRBool aHasDirtyAttr,
                      nsAWritableString& aStr);
  nsresult GetEntityConverter(nsIEntityConverter** aConverter);
  nsresult GetParserService(nsIParserService** aParserService);
  void SerializeAttributes(nsIContent* aContent,
                           nsIAtom* aTagName,
                           nsAWritableString& aStr);
  virtual void AppendToString(const PRUnichar* aStr,
                              PRInt32 aLength,
                              nsAWritableString& aOutputStr);
  virtual void AppendToString(const PRUnichar aChar,
                              nsAWritableString& aOutputStr);
  virtual void AppendToString(const nsAReadableString& aStr,
                              nsAWritableString& aOutputStr,
                              PRBool aTranslateEntities = PR_FALSE,
                              PRBool aIncrColumn = PR_TRUE);
  virtual void AppendToStringWrapped(const nsAReadableString& aStr,
                                     nsAWritableString& aOutputStr,
                                     PRBool aTranslateEntities);
  PRBool HasLongLines(const nsString& text, PRInt32& aLastNewlineOffset);
  nsresult EscapeURI(const nsAReadableString& aURI, nsAWritableString& aEscapedURI);
  PRBool IsJavaScript(nsIAtom* aAttrNameAtom, const nsAReadableString& aAttrValueString);

  nsCOMPtr<nsIParserService> mParserService;
  nsCOMPtr<nsIEntityConverter> mEntityConverter;

  PRInt32   mIndent;
  PRInt32   mColPos;
  PRBool    mInBody;
  PRUint32  mFlags;

  PRBool    mDoFormat;
  PRBool    mDoHeader;
  PRBool    mBodyOnly;
  PRInt32   mPreLevel;

  /*
   * mInCDATA is set to PR_TRUE while the serializer is serializing
   * the content of a element whose content is considerd CDATA by the
   * serializer (such elements are 'script', 'style', 'noscript' and
   * possibly others) This doesn't have anything to do with if the
   * element is defined as CDATA in the DTD, it simply means we'll
   * output the content of the element without doing any entity encoding
   * what so ever.
   */
  PRBool mInCDATA;

  PRInt32   mMaxColumn;

  nsString  mLineBreak;

  PRBool mIsLatin1;

  nsCOMPtr<nsIAtom> mCharSet;
};

extern nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn);

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
  
  PRInt32   mMaxColumn;
  
  nsString  mLineBreak;
};

extern nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif

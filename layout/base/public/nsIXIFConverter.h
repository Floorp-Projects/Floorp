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
 */

#ifndef nsIXIFConverter_h__
#define nsIXIFConverter_h__

#include "nsISupports.h"
#include "nsString.h"


class nsIDOMSelection;


#define NS_IXIFCONVERTER_IID  \
{/* {DEF52794-5F90-406f-B502-DE8050AC0938}*/ \
  0xdef52794, 0x5f90, 0x406f, \
   { 0xb5, 0x2, 0xde, 0x80, 0x50, 0xac, 0x9, 0x38 } }

/*
Will comment later. this will write data from content to a string.
*/

class nsIXIFConverter: public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXIFCONVERTER_IID; return iid; }

  NS_IMETHOD Init(nsAWritableString &aBuffer) = 0;

  NS_IMETHOD BeginStartTag(const nsAReadableString& aTag) = 0;
  NS_IMETHOD BeginStartTag(nsIAtom* aTag) = 0;
  NS_IMETHOD AddAttribute(const nsAReadableString& aName, const nsAReadableString& aValue) = 0;
  NS_IMETHOD AddAttribute(const nsAReadableString& aName, nsIAtom* aValue) = 0;
  NS_IMETHOD AddAttribute(const nsAReadableString& aName) = 0;
  NS_IMETHOD AddAttribute(nsIAtom* aName) = 0;

  //parameters normally: const nsAReadableString& aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE
  NS_IMETHOD FinishStartTag(const nsAReadableString& aTag, PRBool aIsEmpty , PRBool aAddReturn) = 0;  
  
  //parameters normally: nsIAtom* aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE
  NS_IMETHOD FinishStartTag(nsIAtom* aTag, PRBool aIsEmpty, PRBool aAddReturn) = 0;  

  // Short-cut for starting a new tag that has no attributes
  //default aAddReturn to true
  NS_IMETHOD AddStartTag(const nsAReadableString& aTag, PRBool aAddReturn) = 0;
  //default aAddReturn to true
  NS_IMETHOD AddStartTag(nsIAtom* aTag, PRBool aAddReturn) = 0;
  
  //parameter defaults: const nsAReadableString& aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE
  NS_IMETHOD AddEndTag(const nsAReadableString& aTag,PRBool aDoIndent, PRBool aDoReturn) = 0;
  //parameter defaults: nsIAtom* aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE
  NS_IMETHOD AddEndTag(nsIAtom* aTag,PRBool aDoIndent, PRBool aDoReturn) = 0;
  
  // High Level Methods

  NS_IMETHOD BeginContainer(nsIAtom* aTag) = 0;
  NS_IMETHOD EndContainer(nsIAtom* aTag) = 0;

  NS_IMETHOD BeginContainer(const nsAReadableString& aTag) = 0;
  NS_IMETHOD EndContainer(const nsAReadableString& aTag) = 0;

  NS_IMETHOD BeginLeaf(const nsAReadableString& aTag) = 0;
  NS_IMETHOD EndLeaf(const nsAReadableString& aTag) = 0;

  NS_IMETHOD AddContent(const nsAReadableString& aContent) = 0;
  NS_IMETHOD AddComment(const nsAReadableString& aComment) = 0;
  NS_IMETHOD AddContentComment(const nsAReadableString& aComment) = 0;
  
  NS_IMETHOD AddMarkupDeclaration(const nsAReadableString& aComment) = 0;

  NS_IMETHOD AddHTMLAttribute(const nsAReadableString& aName, const nsAReadableString& aValue) = 0;


  NS_IMETHOD BeginCSSStyleSheet() = 0;
  NS_IMETHOD EndCSSStyleSheet() = 0;

  NS_IMETHOD BeginCSSRule() = 0;
  NS_IMETHOD EndCSSRule() = 0;

  NS_IMETHOD BeginCSSSelectors() = 0;
  NS_IMETHOD AddCSSSelectors(const nsAReadableString& aSelectors) = 0;
  NS_IMETHOD EndCSSSelectors() = 0;

  NS_IMETHOD BeginCSSDeclarationList() = 0;
  NS_IMETHOD BeginCSSDeclaration() = 0;
  NS_IMETHOD AddCSSDeclaration(const nsAReadableString& aName, const nsAReadableString& aValue) = 0;
  NS_IMETHOD EndCSSDeclaration() = 0;
  NS_IMETHOD EndCSSDeclarationList() = 0;

  NS_IMETHOD IsMarkupEntity(const PRUnichar aChar, PRBool *aReturnVal) = 0;
  NS_IMETHOD AddMarkupEntity(const PRUnichar aChar, PRBool *aReturnVal) = 0;

  //NS_IMETHOD WriteDebugFile();        // saves to a temp file

  NS_IMETHOD SetSelection(nsIDOMSelection* aSelection) = 0;

  NS_IMETHOD GetSelection(nsIDOMSelection** aSelection) = 0;
};

#endif //nsIXIFConverter_h__

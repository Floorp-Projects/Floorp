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
#ifndef nsXIFConverter_h__
#define nsXIFConverter_h__

#include "nsString.h"
#include "nsWeakReference.h"
#include "nsIXIFConverter.h"

class nsIDOMSelection;

class nsXIFConverter :public nsIXIFConverter
{

private:
  PRInt32    mIndent;
  nsString  *mBuffer;
    
  nsString mAttr;
  nsString mName;
  nsString mValue;
  
  nsString mContent;
  nsString mComment;
  nsString mContainer;
  nsString mEntity;
  nsString mIsa;
  nsString mLeaf;

  nsString mSelector;
  nsString mRule;
  nsString mSheet;

  // We need to remember when we're inside a script,
  // and not encode entities in that case:
  PRBool mInScript;
   
  nsString mNULL;
  nsString mSpacing;
  nsString mSpace;
  nsString mLT; 
  nsString mGT;
  nsString mLF;
  nsString mSlash;
  nsString mBeginComment;
  nsString mEndComment;
  nsString mQuote;
  nsString mEqual;
  nsString mMarkupDeclarationOpen;
  nsWeakPtr mSelectionWeak;

public:
  NS_DECL_ISUPPORTS
  nsXIFConverter();
  virtual ~nsXIFConverter();

  NS_IMETHOD Init(nsString &aBuffer);
  NS_IMETHOD BeginStartTag(const nsString& aTag);
  NS_IMETHOD BeginStartTag(nsIAtom* aTag);
  NS_IMETHOD AddAttribute(const nsString& aName, const nsString& aValue);
  NS_IMETHOD AddAttribute(const nsString& aName, nsIAtom* aValue);
  NS_IMETHOD AddAttribute(const nsString& aName);
  NS_IMETHOD AddAttribute(nsIAtom* aName);

  //parameters normally: const nsString& aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE
  NS_IMETHOD FinishStartTag(const nsString& aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE);  
  
  //parameters normally: nsIAtom* aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE
  NS_IMETHOD FinishStartTag(nsIAtom* aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE);  

  // Short-cut for starting a new tag that has no attributes
  //default aAddReturn to true
  NS_IMETHOD AddStartTag(const nsString& aTag, PRBool aAddReturn = PR_TRUE);
  //default aAddReturn to true
  NS_IMETHOD AddStartTag(nsIAtom* aTag, PRBool aAddReturn = PR_TRUE);
  
  //parameter defaults: const nsString& aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE
  NS_IMETHOD AddEndTag(const nsString& aTag, PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE);
  //parameter defaults: nsIAtom* aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE
  NS_IMETHOD AddEndTag(nsIAtom* aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE);
  
  // High Level Methods

  NS_IMETHOD BeginContainer(nsIAtom* aTag);
  NS_IMETHOD EndContainer(nsIAtom* aTag);

  NS_IMETHOD BeginContainer(const nsString& aTag);
  NS_IMETHOD EndContainer(const nsString& aTag);

  NS_IMETHOD BeginLeaf(const nsString& aTag);
  NS_IMETHOD EndLeaf(const nsString& aTag);

  NS_IMETHOD AddContent(const nsString& aContent);
  NS_IMETHOD AddComment(const nsString& aComment);
  NS_IMETHOD AddContentComment(const nsString& aComment);
  
  NS_IMETHOD AddMarkupDeclaration(const nsString& aComment);

  NS_IMETHOD AddHTMLAttribute(const nsString& aName, const nsString& aValue);


  NS_IMETHOD BeginCSSStyleSheet();
  NS_IMETHOD EndCSSStyleSheet();

  NS_IMETHOD BeginCSSRule();
  NS_IMETHOD EndCSSRule();

  NS_IMETHOD BeginCSSSelectors();
  NS_IMETHOD AddCSSSelectors(const nsString& aSelectors);
  NS_IMETHOD EndCSSSelectors();

  NS_IMETHOD BeginCSSDeclarationList();
  NS_IMETHOD BeginCSSDeclaration();
  NS_IMETHOD AddCSSDeclaration(const nsString& aName, const nsString& aValue);
  NS_IMETHOD EndCSSDeclaration();
  NS_IMETHOD EndCSSDeclarationList();

  NS_IMETHOD IsMarkupEntity(const PRUnichar aChar, PRBool *aReturnVal);
  NS_IMETHOD AddMarkupEntity(const PRUnichar aChar, PRBool *aReturnVal);


  NS_IMETHOD SetSelection(nsIDOMSelection* aSelection);

  NS_IMETHOD GetSelection(nsIDOMSelection** aSelection);
//helper
  NS_IMETHOD WriteDebugFile();        // saves to a temp file
};

#endif

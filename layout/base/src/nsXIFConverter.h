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
  // XXX This is wrong. It violates XPCOM string ownership rules.
  // We're only getting away with this because instances of this
  // class are restricted to single function scope.
  nsAWritableString  *mBuffer;
    
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

  NS_IMETHOD Init(nsAWritableString& aBuffer);
  NS_IMETHOD BeginStartTag(const nsAReadableString& aTag);
  NS_IMETHOD BeginStartTag(nsIAtom* aTag);
  NS_IMETHOD AddAttribute(const nsAReadableString& aName, const nsAReadableString& aValue);
  NS_IMETHOD AddAttribute(const nsAReadableString& aName, nsIAtom* aValue);
  NS_IMETHOD AddAttribute(const nsAReadableString& aName);
  NS_IMETHOD AddAttribute(nsIAtom* aName);

  //parameters normally: const nsAReadableString& aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE
  NS_IMETHOD FinishStartTag(const nsAReadableString& aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE);  
  
  //parameters normally: nsIAtom* aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE
  NS_IMETHOD FinishStartTag(nsIAtom* aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE);  

  // Short-cut for starting a new tag that has no attributes
  //default aAddReturn to true
  NS_IMETHOD AddStartTag(const nsAReadableString& aTag, PRBool aAddReturn = PR_TRUE);
  //default aAddReturn to true
  NS_IMETHOD AddStartTag(nsIAtom* aTag, PRBool aAddReturn = PR_TRUE);
  
  //parameter defaults: const nsAReadableString& aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE
  NS_IMETHOD AddEndTag(const nsAReadableString& aTag, PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE);
  //parameter defaults: nsIAtom* aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE
  NS_IMETHOD AddEndTag(nsIAtom* aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE);
  
  // High Level Methods

  NS_IMETHOD BeginContainer(nsIAtom* aTag);
  NS_IMETHOD EndContainer(nsIAtom* aTag);

  NS_IMETHOD BeginContainer(const nsAReadableString& aTag);
  NS_IMETHOD EndContainer(const nsAReadableString& aTag);

  NS_IMETHOD BeginLeaf(const nsAReadableString& aTag);
  NS_IMETHOD EndLeaf(const nsAReadableString& aTag);

  NS_IMETHOD AddContent(const nsAReadableString& aContent);
  NS_IMETHOD AddComment(const nsAReadableString& aComment);
  NS_IMETHOD AddContentComment(const nsAReadableString& aComment);
  
  NS_IMETHOD AddMarkupDeclaration(const nsAReadableString& aComment);

  NS_IMETHOD AddHTMLAttribute(const nsAReadableString& aName, const nsAReadableString& aValue);


  NS_IMETHOD BeginCSSStyleSheet();
  NS_IMETHOD EndCSSStyleSheet();

  NS_IMETHOD BeginCSSRule();
  NS_IMETHOD EndCSSRule();

  NS_IMETHOD BeginCSSSelectors();
  NS_IMETHOD AddCSSSelectors(const nsAReadableString& aSelectors);
  NS_IMETHOD EndCSSSelectors();

  NS_IMETHOD BeginCSSDeclarationList();
  NS_IMETHOD BeginCSSDeclaration();
  NS_IMETHOD AddCSSDeclaration(const nsAReadableString& aName, const nsAReadableString& aValue);
  NS_IMETHOD EndCSSDeclaration();
  NS_IMETHOD EndCSSDeclarationList();

  NS_IMETHOD AppendEntity(const PRUnichar aChar, nsAWritableString* aStr,
                          nsAReadableString* aInsertIntoTag);
  NS_IMETHOD AppendWithEntityConversion(const nsAReadableString& aName,
                                        nsAWritableString& aOutStr);

  NS_IMETHOD SetSelection(nsIDOMSelection* aSelection);

  NS_IMETHOD GetSelection(nsIDOMSelection** aSelection);
//helper
  NS_IMETHOD WriteDebugFile();        // saves to a temp file
};

#endif

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

class nsIDOMSelection;

class nsXIFConverter
{

private:
  PRInt32    mIndent;
  nsString&  mBuffer;
    
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
  nsIDOMSelection*  mSelection;

public:

  nsXIFConverter(nsString& aBuffer);
  virtual ~nsXIFConverter();

  void BeginStartTag(const nsString& aTag);
  void BeginStartTag(nsIAtom* aTag);
  void AddAttribute(const nsString& aName, const nsString& aValue);
  void AddAttribute(const nsString& aName, nsIAtom* aValue);
  void AddAttribute(const nsString& aName);
  void AddAttribute(nsIAtom* aName);

  void FinishStartTag(const nsString& aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE);  
  void FinishStartTag(nsIAtom* aTag, PRBool aIsEmpty = PR_FALSE, PRBool aAddReturn = PR_TRUE);  

  // Short-cut for starting a new tag that has no attributes
  void AddStartTag(const nsString& aTag, PRBool aAddReturn = PR_TRUE);
  void AddStartTag(nsIAtom* aTag, PRBool aAddReturn = PR_TRUE);
  
  void AddEndTag(const nsString& aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE);
  void AddEndTag(nsIAtom* aTag,PRBool aDoIndent = PR_TRUE, PRBool aDoReturn = PR_TRUE);
  
  // High Level Methods

  void BeginContainer(nsIAtom* aTag);
  void EndContainer(nsIAtom* aTag);

  void BeginContainer(const nsString& aTag);
  void EndContainer(const nsString& aTag);

  void BeginLeaf(const nsString& aTag);
  void EndLeaf(const nsString& aTag);

  void AddContent(const nsString& aContent);
  void AddComment(const nsString& aComment);
  void AddContentComment(const nsString& aComment);
  
  void AddMarkupDeclaration(const nsString& aComment);

  void AddHTMLAttribute(const nsString& aName, const nsString& aValue);


  void BeginCSSStyleSheet();
  void EndCSSStyleSheet();

  void BeginCSSRule();
  void EndCSSRule();

  void BeginCSSSelectors();
  void AddCSSSelectors(const nsString& aSelectors);
  void EndCSSSelectors();

  void BeginCSSDeclarationList();
  void BeginCSSDeclaration();
  void AddCSSDeclaration(const nsString& aName, const nsString& aValue);
  void EndCSSDeclaration();
  void EndCSSDeclarationList();

  PRBool IsMarkupEntity(const PRUnichar aChar);
  PRBool AddMarkupEntity(const PRUnichar aChar);

#ifdef DEBUG_XIF
  void WriteDebugFile();        // saves to a temp file
#endif

  void SetSelection(nsIDOMSelection* aSelection);

  nsIDOMSelection*  GetSelection() {
    return mSelection;
  }
};

#endif

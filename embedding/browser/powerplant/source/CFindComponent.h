/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 * Conrad Carlen <conrad@ingress.com>
 * Based on nsFindComponent.cpp by Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

#ifndef __CFindComponent__
#define __CFindComponent__

#include "nsISupports.h"
#include "nsString.h"
#include "nsComPtr.h"
 
// Forward Declarations
class nsIDocShell;
class nsITextServicesDocument;
class nsIWordBreaker;

 /*
  * CFindComponent is a class which, given a docshell and search params,
  * does basic text searching. It will eventually be upgraded to a real
  * component - for now it's just a class.
 */
 
class CFindComponent
{
  public:
  
  enum CFHighlightStyle {
    eStdSelect
  };
                      CFindComponent();
  virtual             ~CFindComponent();
  
    // Must be called initially!
    // Call this whenever the docshell content changes
    // Passing NULL is OK - clears current state
  NS_IMETHOD          SetContext(nsIDocShell* aDocShell);
  
  
    // Initiates a find in the current context    
  NS_IMETHOD          Find(const nsString& searchStr,
                           PRBool caseSensitive,
                           PRBool searchBackward,
                           PRBool wrapSearch,
                           PRBool wholeWordOnly,
                           PRBool& didFind);
                           
    // Returns whether we can do FindNext
  NS_IMETHOD          CanFindNext(PRBool& canDo);

    // Returns the params to the last find
    // If there was none, searchStr is empty and others are default
  NS_IMETHOD          GetLastSearchString(nsString& searchString);
  NS_IMETHOD          GetLastCaseSensitive(PRBool& caseSensitive);
  NS_IMETHOD          GetLastSearchBackwards(PRBool& searchBackward);
  NS_IMETHOD          GetLastWrapSearch(PRBool& wrapSearch);
  NS_IMETHOD          GetLastEntireWord(PRBool& entireWord);

    // Finds the next using the params last given to Find  
  NS_IMETHOD          FindNext(PRBool& didFind);

    // Finds all occurrances from the top to bottom  
  NS_IMETHOD          FindAll(const nsString& searchStr,
                              PRBool caseSensitive,
                              PRInt32& numFound);


    // How we highlight found text
    // The default is to select it
  NS_IMETHOD          SetFindStyle(CFHighlightStyle style);
  
  
  protected:
  nsString            mLastSearchString;
  PRBool              mLastCaseSensitive, mLastSearchBackwards, mLastWrapSearch, mLastEntireWord;
  
  nsIDocShell         *mDocShell;
  nsCOMPtr<nsITextServicesDocument> mTextDoc;
  nsCOMPtr<nsIWordBreaker> mWordBreaker;  
  
  protected:
  NS_IMETHOD          CreateTSDocument(nsIDocShell* aWebShell, nsITextServicesDocument** aDoc);
  NS_IMETHOD          GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex);
  NS_IMETHOD          SetupDocForSearch(nsITextServicesDocument *aDoc, PRBool searchBackwards, PRInt32 *outBlockOffset);

  static PRInt32      FindInString(const nsString &searchStr, const nsString &patternStr,
						                       PRInt32 startOffset, PRBool searchBackwards,
						                       nsIWordBreaker* wordBreaker);
                                       
};
 
#endif // __CFindComponent__

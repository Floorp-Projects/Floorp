/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributors:
 *   
 *   
 */

#ifndef nsSpellCheckController_h__
#define nsSpellCheckController_h__

#include "nsITextServicesDocument.h"
#include "nsISpellCheckController.h"
#include "nsIWordBreakerFactory.h" // nsIWordBreaker
#include "nsIDOMNode.h"

#include "nsISpellChecker.h"
#include "nsSpellCheckUtils.h"     // CharBuffer

/** implementation of a text services object.
 *
 */
class nsSpellCheckController : public nsISpellCheckController
{
public:

  /** The default constructor.
   */
  nsSpellCheckController();

  /** The default destructor.
   */
  virtual ~nsSpellCheckController();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsISpellChecker method implementations. */
  NS_DECL_NSISPELLCHECKCONTROLLER

private:

  nsresult LoadTextBlockIntoBuffer();
  nsresult ReplaceAll(const nsString *aOldWord, const nsString *aNewWord);
  nsresult ReplaceAllOccurrences(const CharBuffer *aOldWord, const nsString *aNewWord);
  nsresult FindNextMisspelledWord(const PRUnichar* aText, 
                                  const PRUint32&  aTextLen,
                                  PRUint32&        aWLen,
                                  PRUint32&        aBeginWord,
                                  PRUint32&        aEndWord,
                                  PRUint32&        aOffset,
                                  PRUnichar*&      aWord,
                                  PRBool           aReturnWord,
                                  PRBool&          aIsMisspelled);
  nsresult SetDocument(nsITextServicesDocument *aDoc, nsIDOMRange* aInitialRange);
  nsresult FindBeginningOfWord(PRUint32& aPos);

  nsCOMPtr<nsITextServicesDocument> mDocument;
  CharBuffer      mBlockBuffer;
  CharBuffer      mWordBuffer;

  nsCOMPtr<nsIWordBreaker>      mWordBreaker;
  nsCOMPtr<nsISpellChecker>     mSpellChecker;
  nsString                      mText;
  PRUint32                      mOffset;   // starting offset to start spelling
  PRUint32                      mEndPoint; // end point of range to be spell checked

  nsCOMPtr<nsIDOMNode>          mStartNode;
  nsCOMPtr<nsIDOMNode>          mEndNode;
};

#define NS_SPELLCHECKCONTROLLER_CID                     \
{ /* 019718E4-CDB5-11d2-8D3C-000000000000 */    \
0x019718e4, 0xcdb5, 0x11d2,                     \
{ 0x8d, 0x3c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


#endif // nsSpellCheckController_h__

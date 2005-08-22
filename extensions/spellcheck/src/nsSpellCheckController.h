/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsSpellCheckController_h__
#define nsSpellCheckController_h__

#include "nsITextServicesDocument.h"
#include "nsISpellCheckController.h"
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

  nsCOMPtr<nsISpellChecker>     mSpellChecker;
  nsCOMPtr<nsIWordBreaker>      mWordBreaker;
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

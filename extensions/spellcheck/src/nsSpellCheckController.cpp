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

#include "nsSpellCheckController.h"
#include "nsIServiceManager.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"

#define NBSP_SPACE_CODE ((PRUnichar)32)

nsSpellCheckController::nsSpellCheckController() :
  mOffset(0),
  mEndPoint(0)
{
}

nsSpellCheckController::~nsSpellCheckController()
{
}

// #define DEBUG_SPELLCHECKGLUE_REFCNT 1

#ifdef DEBUG_SPELLCHECKGLUE_REFCNT

nsrefcnt nsSpellCheckController::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsSpellCheckController::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

NS_IMPL_QUERY_INTERFACE1(nsSpellCheckController, nsISpellChecker)
#else

NS_IMPL_ISUPPORTS1(nsSpellCheckController, nsISpellCheckController)

#endif


nsresult
NS_NewSpellCheckGlue(nsSpellCheckController** result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsSpellCheckController* spellCheckGlue = new nsSpellCheckController();
  NS_ENSURE_TRUE(spellCheckGlue, NS_ERROR_NULL_POINTER);

  *result = spellCheckGlue;
  NS_ADDREF(*result);
  return NS_OK;
}

//---------------------------------------------------------------------
// aInitialRange can be NULL
//
NS_IMETHODIMP
nsSpellCheckController::Init(nsITextServicesDocument* aDoc, nsIDOMRange* aInitialRange)
{
  NS_ENSURE_ARG_POINTER(aDoc);

  // Create INSO Spell Checker
  nsresult rv;
  mSpellChecker  = do_GetService(NS_SPELLCHECKER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create WordBreaker
  rv = nsSpellCheckUtils::GetWordBreaker(getter_AddRefs(mWordBreaker));
  NS_ENSURE_SUCCESS(rv, rv);

  // zero out offset before starting
  mOffset = 0;

  return SetDocument(aDoc, aInitialRange);
}

nsresult
nsSpellCheckController::SetDocument(nsITextServicesDocument *aDoc,nsIDOMRange* aInitialRange)
{
  NS_ENSURE_ARG_POINTER(aDoc);

  // XXX: Modify this method so that it can be called
  //      more than once with a different aDoc. This will
  //      allow us to reuse a spellchecker.

  mDocument = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  nsresult rv;
  if (!aInitialRange) {
    rv = mDocument->FirstBlock();

  } else  {
    rv = aInitialRange->GetStartContainer(getter_AddRefs(mStartNode));
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt32 startOffset;
    rv = aInitialRange->GetStartOffset(&startOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    mOffset = PRUint32(startOffset);

    rv = aInitialRange->GetEndContainer(getter_AddRefs(mEndNode));
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt32 endOffset;
    rv = aInitialRange->GetEndOffset(&endOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    mEndPoint = PRUint32(endOffset);
    
    nsITextServicesDocument::TSDBlockSelectionStatus selStatus;
    PRInt32 selOffset, selLength;

    rv = mDocument->FirstSelectedBlock(&selStatus, &selOffset, &selLength);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsSpellCheckUtils::LoadTextBlockIntoBuffer(mDocument, mSpellChecker, mBlockBuffer, mText, mOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aInitialRange) {
    mOffset = 0;
    mEndPoint = mText.Length() - 1;
  } else {
    rv = FindBeginningOfWord(mOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    if (mStartNode == mEndNode) {
      rv = FindBeginningOfWord(mEndPoint);
    }
  }

  return rv;
}

nsresult 
nsSpellCheckController::FindBeginningOfWord(PRUint32& aPos)
{
  const PRUnichar* text      = mText.get();
  PRUint32         textLen   = mText.Length();
  PRUint32         wlen      = 0;
  PRUint32         beginWord = 0;
  PRUint32         endWord   = 0;

  PRUint32 prvWordEnd;
  PRUint32 offset = 0;
  while (offset < textLen) {
    prvWordEnd = endWord;
    nsresult rv = mWordBreaker->FindWord(text, textLen, offset, &beginWord, &endWord);
    if (NS_FAILED(rv)) break;

    // The wordBreaker hands back the spaces inbetween the words 
    // so we need to skip any words that are all spaces
    const PRUnichar* start  = (text+offset);
    const PRUnichar* endPtr = (text+endWord);
    while (*start == NBSP_SPACE_CODE && start < endPtr) 
      start++;

    if (start == endPtr) {
      offset = endWord;
      continue;
    }

    offset = endWord+1;

    wlen = endWord - beginWord;
    if (endWord < aPos) {
      continue;
    }

    if (aPos >= beginWord) {
      aPos = beginWord;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSpellCheckController::GetSpellChecker(nsISpellChecker **aSpellChecker)
{
  NS_ENSURE_ARG_POINTER(aSpellChecker);
  *aSpellChecker = mSpellChecker;
  NS_IF_ADDREF(*aSpellChecker);
  return NS_OK;
}

NS_IMETHODIMP
nsSpellCheckController::NextMisspelledWord(PRUnichar **aWord)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_TRUE(mDocument && mSpellChecker && mWordBreaker, NS_ERROR_NULL_POINTER);

  // Init the return values.
  *aWord = nsnull;

  nsCOMPtr<nsIDOMNode> currentNode;
  mDocument->GetCurrentNode(getter_AddRefs(currentNode));

  // There might not have been any text in the
  // document. Just return NS_OK;
  if (mText.IsEmpty() || (mOffset >= mEndPoint && currentNode == mEndNode)) {
    return NS_OK;
  }

  nsresult result;

  // Now get the next misspelled word in the document.
  const PRUnichar* text         = mText.get();
  PRUint32         textLen      = mText.Length();
  PRUint32         wlen         = 0;
  PRUint32         beginWord    = 0;
  PRUint32         endWord      = 0;

  PRBool isMisspelled = PR_FALSE;
  while (!isMisspelled) {
    PRUnichar* word;
    result = FindNextMisspelledWord(text, textLen, mOffset, wlen, 
                                    beginWord, endWord, word, PR_TRUE, isMisspelled);
    if (NS_FAILED(result) || (beginWord > mEndPoint && currentNode == mEndNode)) {
      if (word) nsMemory::Free(word);
      return result;
    }

    if (isMisspelled) {
      *aWord = word;

    } else {
      // No more misspelled words in the current buffer.
      // Load another text block into the buffer, try again.

      result = mDocument->NextBlock();
      NS_ENSURE_SUCCESS(result, result);

      PRBool isDone;

      result = mDocument->IsDone(&isDone);
      NS_ENSURE_SUCCESS(result, result);

      if (isDone) {
        // No more blocks to process. We're done, so just
        // return OK for the result.
        return NS_OK;
      }

      result = nsSpellCheckUtils::LoadTextBlockIntoBuffer(mDocument, mSpellChecker, mBlockBuffer, mText, mOffset);
      NS_ENSURE_SUCCESS(result, result);

      mDocument->GetCurrentNode(getter_AddRefs(currentNode));
      if (currentNode == mEndNode) {
        FindBeginningOfWord(mEndPoint);
      }
      mOffset = 0;
      wlen = 0;
    }
  }

  // We have a misspelled word. Select it in the document,
  // and scroll the selection into view.

  // XXX: SetSelection() no longer scrolls the selection
  //      into view. Something changed underneath the hood
  //      in the presentation shell. Need to fix this!

  result = mDocument->SetSelection(beginWord, wlen);
  NS_ENSURE_SUCCESS(result, result);

  result = mDocument->ScrollSelectionIntoView();
  NS_ENSURE_SUCCESS(result, result);

  return result;

}

NS_IMETHODIMP
nsSpellCheckController::CheckWord(const PRUnichar *aWord, PRBool *aIsMisspelled)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aIsMisspelled);
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NULL_POINTER);

  // start by assuming everything is spelled correctly.
  *aIsMisspelled = PR_FALSE;

  return mSpellChecker->Check(aWord, aIsMisspelled);
}

NS_IMETHODIMP
nsSpellCheckController::Replace(const PRUnichar *aOldWord, const PRUnichar *aNewWord, PRBool aAllOccurrences)
{
  NS_ENSURE_ARG_POINTER(aOldWord);
  NS_ENSURE_ARG_POINTER(aNewWord);

  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  nsresult result;

  // XXX: To make this method work for a non-modal dialog, we will
  //      have to add code that compares the current selection with
  //      aOldWord.

  if (aAllOccurrences) {
    nsString oldWord(aOldWord);
    nsString newWord(aNewWord);
    result = ReplaceAll(&oldWord, &newWord);

  } else if (!*aNewWord) {
    result = mDocument->DeleteSelection();

  } else {
    nsString newWord(aNewWord);
    result = mDocument->InsertText(&newWord);
  }

  return result;
}

nsresult
nsSpellCheckController::ReplaceAll(const nsString *aOldWord, const nsString *aNewWord)
{
  NS_ENSURE_ARG_POINTER(aOldWord);
  NS_ENSURE_ARG_POINTER(aNewWord);
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NULL_POINTER);

  nsresult result;
  CharBuffer oldWord;

  nsCOMPtr<nsIUnicodeEncoder> unicodeEncoder;

  nsXPIDLString charSet;
  result = mSpellChecker->GetCharset(getter_Copies(charSet));
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;
    result = nsSpellCheckUtils::CreateUnicodeConverters(charSet, 
                                                        getter_AddRefs(unicodeEncoder), 
                                                        getter_AddRefs(unicodeDecoder));
    NS_ENSURE_TRUE(unicodeEncoder, NS_ERROR_NULL_POINTER);
  }

  result = nsSpellCheckUtils::ReadStringIntoBuffer(unicodeEncoder, aOldWord, &oldWord);
  NS_ENSURE_SUCCESS(result, result);

  // Skip the current occurrence of old word. It will be the last
  // occurrence we replace so that we can figure out where we were
  // before replace all started!

  result = ReplaceAllOccurrences(&oldWord, aNewWord);
  NS_ENSURE_SUCCESS(result, result);

  PRInt32 i, numBlocksBefore = 0;
  PRBool isDone = PR_FALSE;

  // Count the number of text blocks that came before the current
  // one, so that we know when to stop replacing after we hit the
  // end of the document and swing around to the beginning of the
  // document.

  while (!isDone) {
    ++numBlocksBefore;

    result = mDocument->PrevBlock();
    NS_ENSURE_SUCCESS(result, result);

    result = mDocument->IsDone(&isDone);
    NS_ENSURE_SUCCESS(result, result);
  }

  // Now reset the document so that we start replacing all occurrences
  // between the current block and the end of the document!

  result = mDocument->FirstBlock();
  NS_ENSURE_SUCCESS(result, result);

  i = numBlocksBefore;
  while (i-- > 0) {
    result = mDocument->NextBlock();
    NS_ENSURE_SUCCESS(result, result);
  }

  // Now replace all occurrences till we hit the end of the document!

  result = mDocument->IsDone(&isDone);
  NS_ENSURE_SUCCESS(result, result);

  while (!isDone) {
    result = nsSpellCheckUtils::LoadTextBlockIntoBuffer(mDocument, mSpellChecker, mBlockBuffer, mText, mOffset);
    NS_ENSURE_SUCCESS(result, result);

    result = ReplaceAllOccurrences(&oldWord, aNewWord);
    NS_ENSURE_SUCCESS(result, result);

    result = mDocument->NextBlock();
    NS_ENSURE_SUCCESS(result, result);

    result = mDocument->IsDone(&isDone);
    NS_ENSURE_SUCCESS(result, result);
  }

  // Now swing around to the beginning of the document, and
  // replace all occurrences till we hit the old current block!

  result = mDocument->FirstBlock();
  NS_ENSURE_SUCCESS(result, result);

  isDone = PR_FALSE;

  while (numBlocksBefore-- > 0 && !isDone) {
    result = nsSpellCheckUtils::LoadTextBlockIntoBuffer(mDocument, mSpellChecker, mBlockBuffer, mText, mOffset);
    NS_ENSURE_SUCCESS(result, result);

    result = ReplaceAllOccurrences(&oldWord, aNewWord);
    NS_ENSURE_SUCCESS(result, result);

    if (numBlocksBefore > 0) {
      result = mDocument->NextBlock();
      NS_ENSURE_SUCCESS(result, result);

      // Track isDone because I'm paranoid!

      result = mDocument->IsDone(&isDone);
      NS_ENSURE_SUCCESS(result, result);
    }
  }

  // Now reload the text into the buffer so that the
  // spelling checker is reset to look for the next
  // misspelled word.

  result = nsSpellCheckUtils::LoadTextBlockIntoBuffer(mDocument, mSpellChecker, mBlockBuffer, mText, mOffset);
  NS_ENSURE_SUCCESS(result, result);

  return NS_OK;
}

nsresult
nsSpellCheckController::ReplaceAllOccurrences(const CharBuffer *aOldWord, const nsString *aNewWord)
{
  NS_ENSURE_ARG_POINTER(aOldWord);
  NS_ENSURE_ARG_POINTER(aNewWord);
  NS_ENSURE_TRUE(mSpellChecker && mWordBreaker, NS_ERROR_NULL_POINTER);

  // Now get the next misspelled word in the document.
  const PRUnichar* text         = mText.get();
  PRUint32         textLen      = mText.Length();
  PRUint32         wlen         = 0;
  PRUint32         beginWord    = 0;
  PRUint32         endWord      = 0;
  PRUint32         offset       = 0;

  PRBool isMisspelled = PR_TRUE;
  while (isMisspelled) {
    PRUnichar* word;
    nsresult result  = FindNextMisspelledWord(text, textLen, offset, wlen, 
                                              beginWord, endWord, word, PR_FALSE, isMisspelled);
    NS_ENSURE_SUCCESS(result, result);

    if (isMisspelled) {

      mWordBuffer.AssureCapacity(wlen + 1);

      PRUint32 i;
      for (i = 0; i < wlen; i++)
        mWordBuffer.mData[i] = mBlockBuffer.mData[i + beginWord];

      mWordBuffer.mData[i]    = '\0';
      mWordBuffer.mDataLength = wlen;

      // XXX: Need to do a case insensitive comparison, and replace
      //      with a word that matches the misspelled word's caps.

      if (aOldWord->mDataLength == mWordBuffer.mDataLength &&
          !memcmp(aOldWord->mData, mWordBuffer.mData, aOldWord->mDataLength)) {
        // We found an occurrence of old word, so replace it!

        result = mDocument->SetSelection(beginWord, wlen);
        NS_ENSURE_SUCCESS(result, result);

        result = mDocument->ScrollSelectionIntoView();
        NS_ENSURE_SUCCESS(result, result);

        if (aNewWord->Length() > 0)
          result = mDocument->InsertText(aNewWord);
        else
          result = mDocument->DeleteSelection();

        NS_ENSURE_SUCCESS(result, result);
        offset = endWord;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsSpellCheckController::SpellCheckDOMRange(nsIDOMRange *aRangeToCheck, nsISelection *aSelectionOfWords)
{
  NS_ENSURE_ARG_POINTER(aRangeToCheck);
  NS_ENSURE_ARG_POINTER(aSelectionOfWords);

  NS_ENSURE_TRUE(mSpellChecker && mWordBreaker, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  mDocument->InitWithRange(aRangeToCheck);
  nsSpellCheckUtils::LoadTextBlockIntoBuffer(mDocument, mSpellChecker, mBlockBuffer, mText, mOffset);

  // You may want to clear this externally instead of here
  aSelectionOfWords->RemoveAllRanges();

  // Now get the next misspelled word in the document.
  const PRUnichar* text         = mText.get();
  PRUint32         textLen      = mText.Length();
  PRUint32         wlen         = 0;
  PRUint32         beginWord    = 0;
  PRUint32         endWord      = 0;

  PRBool isMisspelled = PR_TRUE;
  while (isMisspelled) {
    PRUnichar* word;
    nsresult result = FindNextMisspelledWord(text, textLen, mOffset, wlen, 
                                             beginWord, endWord, word, PR_FALSE, isMisspelled);
    NS_ENSURE_SUCCESS(result, result);

    if (isMisspelled) {// We found an occurrence of old word, so replace it!
      nsCOMPtr<nsIDOMRange> range;
      result = mDocument->GetDOMRangeFor(beginWord, wlen, getter_AddRefs(range));
      NS_ENSURE_SUCCESS(result, result);

      result = aSelectionOfWords->AddRange(range);
      NS_ENSURE_SUCCESS(result, result);
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------
// Helper Method
nsresult
nsSpellCheckController::FindNextMisspelledWord(const PRUnichar* aText, 
                                         const PRUint32&  aTextLen,
                                         PRUint32&        aOffset,
                                         PRUint32&        aWLen,
                                         PRUint32&        aBeginWord,
                                         PRUint32&        aEndWord,
                                         PRUnichar*&      aWord,
                                         PRBool           aReturnWord,
                                         PRBool&          aIsMisspelled)
{
  aWord = nsnull;
  aIsMisspelled = PR_FALSE;

#ifdef DEBUG_rods
  DUMPWORDS(mWordBreaker, aText, aTextLen);
#endif

  while (aOffset < aTextLen) {
    nsresult result = mWordBreaker->FindWord(aText, aTextLen, aOffset, &aBeginWord, &aEndWord);
    NS_ENSURE_SUCCESS(result, result);
    aWLen = aEndWord - aBeginWord;

    // The wordBreaker hands back the spaces inbetween the words 
    // so we need to skip any words that are all spaces
    const PRUnichar* start  = (aText+aOffset);
    const PRUnichar* endPtr = (aText+aEndWord);
    while (*start == NBSP_SPACE_CODE && start < endPtr) 
      start++;

    if (start == endPtr) {
      aOffset = aEndWord;
      continue;
    }

    PRUnichar* word = nsCRT::strndup(start, aWLen);
    aOffset = aEndWord;

    result = mSpellChecker->Check(word, &aIsMisspelled);
#if DEBUG_rods
    printf("Word [%s] MSP: %s\n", NS_LossyConvertUCS2toASCII(word).get(), aIsMisspelled ? "YES":"NO");
#endif
    if (NS_FAILED(result)) {
      nsMemory::Free(word);
      return result;
    }

    if (aIsMisspelled) {
      if (aReturnWord) {
        aWord = word;

      } else {
        nsMemory::Free(word);
      }
      return NS_OK;
    }
    nsMemory::Free(word);
  }

  return NS_OK;
}

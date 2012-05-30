/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozEnglishWordUtils.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicodeProperties.h"
#include "nsCRT.h"

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozEnglishWordUtils)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozEnglishWordUtils)

NS_INTERFACE_MAP_BEGIN(mozEnglishWordUtils)
  NS_INTERFACE_MAP_ENTRY(mozISpellI18NUtil)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozISpellI18NUtil)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozEnglishWordUtils)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_1(mozEnglishWordUtils,
                           mURLDetector)

mozEnglishWordUtils::mozEnglishWordUtils()
{
  mLanguage.AssignLiteral("en");

  nsresult rv;
  mURLDetector = do_CreateInstance(MOZ_TXTTOHTMLCONV_CONTRACTID, &rv);
}

mozEnglishWordUtils::~mozEnglishWordUtils()
{
}

/* attribute wstring language; */
NS_IMETHODIMP mozEnglishWordUtils::GetLanguage(PRUnichar * *aLanguage)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(aLanguage);

  *aLanguage = ToNewUnicode(mLanguage);
  if(!aLanguage) rv = NS_ERROR_OUT_OF_MEMORY;
  return rv;
 }

/* void GetRootForm (in wstring aWord, in PRUint32 type, [array, size_is (count)] out wstring words, out PRUint32 count); */
// return the possible root forms of aWord.
NS_IMETHODIMP mozEnglishWordUtils::GetRootForm(const PRUnichar *aWord, PRUint32 type, PRUnichar ***words, PRUint32 *count)
{
  nsAutoString word(aWord);
  PRUnichar **tmpPtr;
  PRInt32 length = word.Length();

  *count = 0;

  mozEnglishWordUtils::myspCapitalization ct = captype(word);
  switch (ct)
    {
    case HuhCap:
    case NoCap: 
      tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *));
      if (!tmpPtr)
        return NS_ERROR_OUT_OF_MEMORY;
      tmpPtr[0] = ToNewUnicode(word);
      if (!tmpPtr[0]) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(0, tmpPtr);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *words = tmpPtr;
      *count = 1;
      break;
    

    case AllCap:
      tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * 3);
      if (!tmpPtr)
        return NS_ERROR_OUT_OF_MEMORY;
      tmpPtr[0] = ToNewUnicode(word);
      if (!tmpPtr[0]) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(0, tmpPtr);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      ToLowerCase(tmpPtr[0], tmpPtr[0], length);

      tmpPtr[1] = ToNewUnicode(word);
      if (!tmpPtr[1]) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(1, tmpPtr);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      ToLowerCase(tmpPtr[1], tmpPtr[1], length);
      ToUpperCase(tmpPtr[1], tmpPtr[1], 1);

      tmpPtr[2] = ToNewUnicode(word);
      if (!tmpPtr[2]) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(2, tmpPtr);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      *words = tmpPtr;
      *count = 3;
      break;
 
    case InitCap:  
      tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * 2);
      if (!tmpPtr)
        return NS_ERROR_OUT_OF_MEMORY;

      tmpPtr[0] = ToNewUnicode(word);
      if (!tmpPtr[0]) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(0, tmpPtr);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      ToLowerCase(tmpPtr[0], tmpPtr[0], length);

      tmpPtr[1] = ToNewUnicode(word);
      if (!tmpPtr[1]) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(1, tmpPtr);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      *words = tmpPtr;
      *count = 2;
      break;
    default:
      return NS_ERROR_FAILURE; // should never get here;
    }
  return NS_OK;
}

// This needs vast improvement
bool mozEnglishWordUtils::ucIsAlpha(PRUnichar aChar)
{
  // XXX we have to fix callers to handle the full Unicode range
  return nsIUGenCategory::kLetter == mozilla::unicode::GetGenCategory(aChar);
}

/* void FindNextWord (in wstring word, in PRUint32 length, in PRUint32 offset, out PRUint32 begin, out PRUint32 end); */
NS_IMETHODIMP mozEnglishWordUtils::FindNextWord(const PRUnichar *word, PRUint32 length, PRUint32 offset, PRInt32 *begin, PRInt32 *end)
{
  const PRUnichar *p = word + offset;
  const PRUnichar *endbuf = word + length;
  const PRUnichar *startWord=p;
  if(p<endbuf){
    // XXX These loops should be modified to handle non-BMP characters.
    // if previous character is a word character, need to advance out of the word
    if (offset > 0 && ucIsAlpha(*(p-1))) {
      while (p < endbuf && ucIsAlpha(*p))
        p++;
    }
    while((p < endbuf) && (!ucIsAlpha(*p)))
      {
        p++;
      }
    startWord=p;
    while((p < endbuf) && ((ucIsAlpha(*p))||(*p=='\'')))
      { 
        p++;
      }
    
    // we could be trying to break down a url, we don't want to break a url into parts,
    // instead we want to find out if it really is a url and if so, skip it, advancing startWord 
    // to a point after the url.

    // before we spend more time looking to see if the word is a url, look for a url identifer
    // and make sure that identifer isn't the last character in the word fragment.
    if ( (*p == ':' || *p == '@' || *p == '.') &&  p < endbuf - 1) {

        // ok, we have a possible url...do more research to find out if we really have one
        // and determine the length of the url so we can skip over it.
       
        if (mURLDetector)
        {
          PRInt32 startPos = -1;
          PRInt32 endPos = -1;        

          mURLDetector->FindURLInPlaintext(startWord, endbuf - startWord, p - startWord, &startPos, &endPos);

          // ok, if we got a url, adjust the array bounds, skip the current url text and find the next word again
          if (startPos != -1 && endPos != -1) { 
            startWord = p + endPos + 1; // skip over the url
            p = startWord; // reset p

            // now recursively call FindNextWord to search for the next word now that we have skipped the url
            return FindNextWord(word, length, startWord - word, begin, end);
          }
        }
    }

    while((p > startWord)&&(*(p-1) == '\'')){  // trim trailing apostrophes
      p--;
    }
  }
  else{
    startWord = endbuf;
  }
  if(startWord == endbuf){
    *begin = -1;
    *end = -1;
  }
  else{
    *begin = startWord-word;
    *end = p-word;
  }
  return NS_OK;
}

mozEnglishWordUtils::myspCapitalization 
mozEnglishWordUtils::captype(const nsString &word)
{
  PRUnichar* lword=ToNewUnicode(word);  
  ToUpperCase(lword,lword,word.Length());
  if(word.Equals(lword)){
    nsMemory::Free(lword);
    return AllCap;
  }

  ToLowerCase(lword,lword,word.Length());
  if(word.Equals(lword)){
    nsMemory::Free(lword);
    return NoCap;
  }
  PRInt32 length=word.Length();
  if(Substring(word,1,length-1).Equals(lword+1)){
    nsMemory::Free(lword);
    return InitCap;
  }
  nsMemory::Free(lword);
  return HuhCap;
}

// Convert the list of words in iwords to the same capitalization aWord and 
// return them in owords.
NS_IMETHODIMP mozEnglishWordUtils::FromRootForm(const PRUnichar *aWord, const PRUnichar **iwords, PRUint32 icount, PRUnichar ***owords, PRUint32 *ocount)
{
  nsAutoString word(aWord);
  nsresult rv = NS_OK;

  PRInt32 length;
  PRUnichar **tmpPtr  = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *)*icount);
  if (!tmpPtr)
    return NS_ERROR_OUT_OF_MEMORY;

  mozEnglishWordUtils::myspCapitalization ct = captype(word);
  for(PRUint32 i = 0; i < icount; ++i) {
    length = NS_strlen(iwords[i]);
    tmpPtr[i] = (PRUnichar *) nsMemory::Alloc(sizeof(PRUnichar) * (length + 1));
    if (NS_UNLIKELY(!tmpPtr[i])) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, tmpPtr);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    memcpy(tmpPtr[i], iwords[i], (length + 1) * sizeof(PRUnichar));

    nsAutoString capTest(tmpPtr[i]);
    mozEnglishWordUtils::myspCapitalization newCt=captype(capTest);
    if(newCt == NoCap){
      switch(ct) 
        {
        case HuhCap:
        case NoCap:
          break;
        case AllCap:
          ToUpperCase(tmpPtr[i],tmpPtr[i],length);
          rv = NS_OK;
          break;
        case InitCap:  
          ToUpperCase(tmpPtr[i],tmpPtr[i],1);
          rv = NS_OK;
          break;
        default:
          rv = NS_ERROR_FAILURE; // should never get here;
          break;

        }
    }
  }
  if (NS_SUCCEEDED(rv)){
    *owords = tmpPtr;
    *ocount = icount;
  }
  return rv;
}


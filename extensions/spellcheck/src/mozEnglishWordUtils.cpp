/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein Deinst@world.std.com
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

#include "mozEnglishWordUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsUnicharUtilCIID.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "cattable.h"


static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);

NS_IMPL_ISUPPORTS1(mozEnglishWordUtils, mozISpellI18NUtil)

mozEnglishWordUtils::mozEnglishWordUtils()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mLanguage.Assign(NS_LITERAL_STRING("en"));
}

mozEnglishWordUtils::~mozEnglishWordUtils()
{
  /* destructor code */
}

/* attribute wstring language; */
NS_IMETHODIMP mozEnglishWordUtils::GetLanguage(PRUnichar * *aLanguage)
{
  nsresult res=NS_OK;
  NS_PRECONDITION(aLanguage != nsnull, "null ptr");
  if(!aLanguage){
    res = NS_ERROR_NULL_POINTER;
  }
  else{
    *aLanguage = ToNewUnicode(mLanguage);
    if(!aLanguage) res = NS_ERROR_OUT_OF_MEMORY;
  }
  return res;
}

/* attribute wstring charset; */
NS_IMETHODIMP mozEnglishWordUtils::GetCharset(PRUnichar * *aCharset)
{
  nsresult res=NS_OK;
  NS_PRECONDITION(aCharset != nsnull, "null ptr");
  if(!aCharset){
    res = NS_ERROR_NULL_POINTER;
  }
  else{
    *aCharset = ToNewUnicode(mCharset);
    if(!aCharset) res = NS_ERROR_OUT_OF_MEMORY;
  }
  return res;
}
NS_IMETHODIMP mozEnglishWordUtils::SetCharset(const PRUnichar * aCharset)
{
  nsresult res;

  mCharset = aCharset;

  nsCAutoString convCharset;
  convCharset.AssignWithConversion(mCharset); 
  
  nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return res;
  if(!ccm) return NS_ERROR_FAILURE;
  res=ccm->GetUnicodeEncoder(convCharset.get(),getter_AddRefs(mEncoder));
  if(mEncoder && NS_SUCCEEDED(res)){
    res=mEncoder->SetOutputErrorBehavior(mEncoder->kOnError_Signal,nsnull,'?');
  }
  if (NS_FAILED(res)) return res;
  res=ccm->GetUnicodeDecoder(convCharset.get(),getter_AddRefs(mDecoder));
  if (NS_FAILED(res)) return res;
  res = nsServiceManager::GetService(kUnicharUtilCID,NS_GET_IID(nsICaseConversion), getter_AddRefs(mCaseConv));
  return res;
}

/* void GetRootForm (in wstring aWord, in PRUint32 type, [array, size_is (count)] out string words, out PRUint32 count); */
// convert aWord to the spellcheck charset and return the possible root forms.
// If convertion errors occur, return an empty list.
NS_IMETHODIMP mozEnglishWordUtils::GetRootForm(const PRUnichar *aWord, PRUint32 type, char ***words, PRUint32 *count)
{
  nsAutoString word(aWord);
  nsresult res;
  char **tmpPtr;
  PRUnichar *tWord;
  PRInt32 inLength,outLength;
  *count = 0;

  mozEnglishWordUtils::myspCapitalization ct = captype(word);
  switch (ct)
    {
    case HuhCap:
    case NoCap: 
      tmpPtr = (char **)nsMemory::Alloc(sizeof(char *));
      inLength = word.Length();
      res = mEncoder->GetMaxLength(word.get(),inLength,&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[0] = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
      res = mEncoder->Convert(aWord,&inLength,tmpPtr[0],&outLength);
      tmpPtr[0][outLength]='\0';
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      *words = tmpPtr;
      *count = 1;
      break;
    

    case AllCap:
      tmpPtr = (char **)nsMemory::Alloc(sizeof(char *)*3);
      inLength = word.Length();
      res = mEncoder->GetMaxLength(word.get(),inLength,&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[0] = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
      res = mEncoder->Convert(aWord,&inLength,tmpPtr[0],&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[0][outLength]='\0';

      inLength = word.Length();
      tWord=ToNewUnicode(word);
      mCaseConv->ToLower(tWord,tWord,inLength);
      res = mEncoder->GetMaxLength(tWord,inLength,&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[1] = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
      res = mEncoder->Convert(tWord,&inLength,tmpPtr[1],&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr[1]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[1][outLength]='\0';
      nsMemory::Free(tWord);

      tWord=ToNewUnicode(word);
      mCaseConv->ToLower(tWord,tWord,inLength);
      mCaseConv->ToUpper(tWord,tWord,1);
      res = mEncoder->GetMaxLength(tWord,inLength,&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr[1]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[2] = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
      res = mEncoder->Convert(tWord,&inLength,tmpPtr[2],&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr[1]);
        nsMemory::Free(tmpPtr[2]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[2][outLength]='\0';
      nsMemory::Free(tWord);
      
      *words = tmpPtr;
      *count = 3;
      break;

    case InitCap:  
      tmpPtr = (char **)nsMemory::Alloc(sizeof(char *)*2);
      inLength = word.Length();
      res = mEncoder->GetMaxLength(word.get(),inLength,&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[0] = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
      res = mEncoder->Convert(aWord,&inLength,tmpPtr[0],&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[0][outLength]='\0';

      tWord=ToNewUnicode(word);
      inLength = word.Length();
      mCaseConv->ToLower(tWord,tWord,inLength);
      res = mEncoder->GetMaxLength(tWord,inLength,&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      tmpPtr[1] = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
      res = mEncoder->Convert(tWord,&inLength,tmpPtr[1],&outLength);
      if(NS_FAILED(res)|| res == NS_ERROR_UENC_NOMAPPING){
        nsMemory::Free(tmpPtr[1]);
        nsMemory::Free(tmpPtr[0]);
        nsMemory::Free(tmpPtr);
        *words=nsnull;
        break;
      }
      nsMemory::Free(tWord);
      tmpPtr[1][outLength]='\0';

      *words = tmpPtr;
      *count = 2;
      break;
    default:
      res=NS_ERROR_FAILURE; // should never get here;
      break;
    }
  return res;
}

// This needs vast improvement -- take the charset from he ISpell dictionary
static PRBool ucIsAlpha(PRUnichar c)
{
  if(5==GetCat(c)) return PR_TRUE;
  return PR_FALSE;
}

/* void FindNextWord (in wstring word, in PRUint32 length, in PRUint32 offset, out PRUint32 begin, out PRUint32 end); */
NS_IMETHODIMP mozEnglishWordUtils::FindNextWord(const PRUnichar *word, PRUint32 length, PRUint32 offset, PRInt32 *begin, PRInt32 *end)
{
  const PRUnichar *p = word + offset;
  const PRUnichar *endbuf = word + length;
  const PRUnichar *startWord=p;
  if(p<endbuf){
    while((p < endbuf) && (!ucIsAlpha(*p)))
      {
        p++;
      }
    startWord=p;
    while((p < endbuf) && ((ucIsAlpha(*p))||(*p=='\'')))
      { 
        p++;
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
  if(!mCaseConv) return HuhCap; //punt
  PRUnichar* lword=ToNewUnicode(word);  
  mCaseConv->ToUpper(lword,lword,word.Length());
  if(word.Equals(lword)){
    nsMemory::Free(lword);
    return AllCap;
  }

  mCaseConv->ToLower(lword,lword,word.Length());
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
// convert them to unicode then return them in owords.
NS_IMETHODIMP mozEnglishWordUtils::FromRootForm(const PRUnichar *aWord, const char **iwords, PRUint32 icount, PRUnichar ***owords, PRUint32 *ocount)
{
  nsAutoString word(aWord);
  nsresult res = NS_OK;

  PRInt32 inLength,outLength;
  PRUnichar **tmpPtr  = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *)*icount);
  if (!tmpPtr)
    return NS_ERROR_OUT_OF_MEMORY;

  mozEnglishWordUtils::myspCapitalization ct = captype(word);
  for(PRUint32 i=0;i<icount;i++){
    inLength = nsCRT::strlen(iwords[i]);
    res = mDecoder->GetMaxLength(iwords[i],inLength,&outLength);
    if(NS_FAILED(res))
      break;
    tmpPtr[i] = (PRUnichar *) nsMemory::Alloc(sizeof(PRUnichar *) * (outLength+1));
    res = mDecoder->Convert(iwords[i],&inLength,tmpPtr[i],&outLength);
    tmpPtr[i][outLength]=0;
    nsAutoString capTest(tmpPtr[i]);
    mozEnglishWordUtils::myspCapitalization newCt=captype(capTest);
    if(newCt == NoCap){
      switch(ct) 
        {
        case HuhCap:
        case NoCap:
          break;
        case AllCap:
          res=mCaseConv->ToUpper(tmpPtr[i],tmpPtr[i],outLength);
          break;
        case InitCap:  
          res=mCaseConv->ToUpper(tmpPtr[i],tmpPtr[i],1);
          break;
        default:
          res=NS_ERROR_FAILURE; // should never get here;
          break;

        }
    }
  }
  if(NS_SUCCEEDED(res)){
    *owords = tmpPtr;
    *ocount = icount;
  }
  return res;
}


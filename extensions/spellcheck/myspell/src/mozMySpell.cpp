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
 * Contributor(s): David Einstein <Deinst@world.std.com>
 *                 Kevin Hendricks <kevin.hendricks@sympatico.ca>
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
 *  This spellchecker is based on the MySpell spellchecker made for Open Office
 *  by Kevin Hendricks.  Although the algorithms and code, have changed 
 *  slightly, the architecture is still the same. The Mozilla implementation
 *  is designed to be compatible with the Open Office dictionaries.
 *  Please do not make changes to the affix or dictionary file formats 
 *  without attempting to coordinate with Kevin.  For more information 
 *  on the original MySpell see 
 *  http://whiteboard.openoffice.org/source/browse/whiteboard/lingucomponent/source/spellcheck/myspell/
 *
 *  A special thanks and credit goes to Geoff Kuenning
 * the creator of ispell.  MySpell's affix algorithms were
 * based on those of ispell which should be noted is
 * copyright Geoff Kuenning et.al. and now available
 * under a BSD style license. For more information on ispell
 * and affix compression in general, please see:
 * http://www.cs.ucla.edu/ficus-members/geoff/ispell.html
 * (the home page for ispell)
 *
 * ***** END LICENSE BLOCK ***** */

/* based on MySpell (c) 2001 by Kevin Hendicks  */

#include "mozMySpell.h"
#include "nsReadableUtils.h"
#include "nsIFile.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozISpellI18NManager.h"


NS_IMPL_ISUPPORTS1(mozMySpell, mozISpellCheckingEngine)
  
const PRInt32 kFirstDirSize=8;


mozMySpell::mozMySpell()
{
}

mozMySpell::~mozMySpell()
{
}

/* attribute wstring dictionary; */
NS_IMETHODIMP mozMySpell::GetDictionary(PRUnichar * *aDictionary)
{
  nsresult res=NS_OK;
  NS_PRECONDITION(aDictionary != nsnull, "null ptr");
  if(!aDictionary){
    res = NS_ERROR_NULL_POINTER;
  }
  else{
    *aDictionary = ToNewUnicode(mDictionary);
    if(!aDictionary) res = NS_ERROR_OUT_OF_MEMORY;
  }
  
  return res;
}

/* set the Dictionary.
 * This also Loads the dictionary and initializes the converter using the dictionaries converter
 */
NS_IMETHODIMP mozMySpell::SetDictionary(const PRUnichar * aDictionary)
{
  if(!aDictionary)
    return NS_ERROR_NULL_POINTER;

  nsresult res=NS_OK;
  
  if (!mDictionary.Equals(aDictionary)&&!(*aDictionary == 0)){
    mDictionary = aDictionary;
    res=mAMgr.Load(mDictionary);
    if(NS_FAILED(res)){
      NS_WARNING("Dictionary load failed");
      return res;
    }
    nsAutoString tryString;
    mAMgr.get_try_string(tryString);
    mSMgr.setup(tryString, 64, &mAMgr);
    nsString language;
    PRInt32 pos = mDictionary.FindChar('-');
    if(pos == -1){
      language.Assign(NS_LITERAL_STRING("en"));      
    }
    else{
      language = Substring(mDictionary,0,pos);
    }
    nsCOMPtr<mozISpellI18NManager> serv(do_GetService("@mozilla.org/spellchecker/i18nmanager;1", &res));
    if(serv && NS_SUCCEEDED(res)){
      res = serv->GetUtil(language.get(),getter_AddRefs(mConverter));
    }
  }
  return res;
}

/* readonly attribute wstring language; */
NS_IMETHODIMP mozMySpell::GetLanguage(PRUnichar * *aLanguage)
{
  nsresult res=NS_OK;
  NS_PRECONDITION(aLanguage != nsnull, "null ptr");
  if(!aLanguage){
    res = NS_ERROR_NULL_POINTER;
  }
  else{
    nsString language;
    PRInt32 pos = mDictionary.FindChar('-');
    if(pos == -1){
      language.Assign(NS_LITERAL_STRING("en"));      
    }
    else{
      language = Substring(mDictionary,0,pos);
    }
    *aLanguage = ToNewUnicode(language);
    if(!aLanguage) res = NS_ERROR_OUT_OF_MEMORY;
  }
  return res;
}

/* readonly attribute boolean providesPersonalDictionary; */
NS_IMETHODIMP mozMySpell::GetProvidesPersonalDictionary(PRBool *aProvidesPersonalDictionary)
{
  if(!aProvidesPersonalDictionary) return NS_ERROR_NULL_POINTER;
  *aProvidesPersonalDictionary=PR_FALSE;
  return NS_OK;
}

/* readonly attribute boolean providesWordUtils; */
NS_IMETHODIMP mozMySpell::GetProvidesWordUtils(PRBool *aProvidesWordUtils)
{
  if(!aProvidesWordUtils) return NS_ERROR_NULL_POINTER;
  *aProvidesWordUtils=PR_FALSE;
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP mozMySpell::GetName(PRUnichar * *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute wstring copyright; */
NS_IMETHODIMP mozMySpell::GetCopyright(PRUnichar * *aCopyright)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}




/* attribute mozIPersonalDictionary personalDictionary; */
NS_IMETHODIMP mozMySpell::GetPersonalDictionary(mozIPersonalDictionary * *aPersonalDictionary)
{
  return mAMgr.GetPersonalDictionary(aPersonalDictionary);
}
NS_IMETHODIMP mozMySpell::SetPersonalDictionary(mozIPersonalDictionary * aPersonalDictionary)
{
  return mAMgr.SetPersonalDictionary(aPersonalDictionary);
}

/* void GetDictionaryList ([array, size_is (count)] out wstring dictionaries, out PRUint32 count); */
NS_IMETHODIMP mozMySpell::GetDictionaryList(PRUnichar ***dictionaries, PRUint32 *count)
{
  nsresult res;
  if(!dictionaries || !count){
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr<nsISimpleEnumerator> dirEntries;
  PRBool hasMore = PR_FALSE;
  PRInt32  tempCount=0, i, arraySize = kFirstDirSize;;
  PRUnichar **newPtr, **tmpPtr;

  res=NS_OK;
  *dictionaries = 0;
  *count=0;

  res = NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(aFile));
  if (NS_FAILED(res)) return res;
  if(!aFile)return NS_ERROR_FAILURE;
  res = aFile->Append(NS_LITERAL_STRING("myspell"));
  if (NS_FAILED(res)) return res;
  res = aFile->GetDirectoryEntries(getter_AddRefs(dirEntries));
  if (NS_FAILED(res)) return  res;
  if(!dirEntries)return NS_ERROR_FAILURE;

  tmpPtr  = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *)*kFirstDirSize);
  if (!tmpPtr)
    return NS_ERROR_OUT_OF_MEMORY;


  while (NS_SUCCEEDED(dirEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> nextItem;
    nsCOMPtr<nsIFile> theFile;

    dirEntries->GetNext(getter_AddRefs(nextItem));
    theFile = do_QueryInterface(nextItem);
    
    if(theFile){
      nsString fileName;
      theFile->GetLeafName(fileName);
      PRInt32 dotLocation = fileName.FindChar('.');
      if((dotLocation != -1) && Substring(fileName,dotLocation,4).EqualsLiteral(".dic")){
        if(tempCount >= arraySize){
          arraySize = 2 * tempCount;
          newPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * arraySize);
          if (! newPtr){
            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(tempCount, tmpPtr);
            return NS_ERROR_OUT_OF_MEMORY;
          }
          for(i=0;i<tempCount;i++){
            newPtr[i] = tmpPtr[i];
          }
          nsMemory::Free(tmpPtr);
          tmpPtr=newPtr;
        }
        tmpPtr[tempCount++] = ToNewUnicode(Substring(fileName,0,dotLocation));
      }
    }
  }
  *dictionaries=tmpPtr;
  *count=tempCount;
  return res;
}

/* boolean Check (in wstring word); */
NS_IMETHODIMP mozMySpell::Check(const PRUnichar *aWord, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(mConverter);

  PRUnichar **tmpPtr;
  PRUint32 count,i;
  *aResult = PR_FALSE;

  nsresult rv = mConverter->GetRootForm(aWord, mozISpellI18NUtil::kCheck, &tmpPtr, &count);
  NS_ENSURE_SUCCESS(rv, rv);
  for(i=0 ; i<count ; i++){
    *aResult = mAMgr.check(nsDependentString(tmpPtr[i]));
    if (*aResult) break;
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, tmpPtr);
  return rv;
}

/* void Suggest (in wstring word, [array, size_is (count)] out wstring suggestions, out PRUint32 count); */
NS_IMETHODIMP mozMySpell::Suggest(const PRUnichar *aWord, PRUnichar ***aSuggestions, PRUint32 *aSuggestionCount)
{
  NS_ENSURE_ARG_POINTER(aSuggestions);
  NS_ENSURE_ARG_POINTER(aSuggestionCount);
  NS_ENSURE_ARG_POINTER(mConverter);

  *aSuggestions = 0;
  *aSuggestionCount=0;
  PRUnichar **tmpPtr;
  nsAutoString word(aWord);
  PRUnichar **slst = nsnull;
  PRUint32 count;
  PRUint32 ccount=0;

  nsresult rv = mConverter->GetRootForm(aWord, mozISpellI18NUtil::kSuggest, &tmpPtr, &count);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; (i < count) && NS_SUCCEEDED(rv) ; i++){
    rv = mSMgr.suggest(&slst, nsDependentString(tmpPtr[i]), &ccount); 
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, tmpPtr);
  if (ccount)
    rv=mConverter->FromRootForm(aWord, (const PRUnichar **)slst, ccount, aSuggestions, aSuggestionCount);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(ccount, slst);
  return rv;

}

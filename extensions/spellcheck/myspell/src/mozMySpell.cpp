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
 *                 Michiel van Leeuwen <mvl@exedo.nl>
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
#include "nsXPIDLString.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "mozISpellI18NManager.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include <stdlib.h>

NS_IMPL_ISUPPORTS3(mozMySpell,
                   mozISpellCheckingEngine,
                   nsIObserver,
                   nsISupportsWeakReference)

nsresult
mozMySpell::Init()
{
  if (!mDictionaries.Init())
    return NS_ERROR_OUT_OF_MEMORY;

  LoadDictionaryList();

  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  if (obs) {
    obs->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  return NS_OK;
}

mozMySpell::~mozMySpell()
{
  mPersonalDictionary = nsnull;
  delete mMySpell;
}

/* attribute wstring dictionary; */
NS_IMETHODIMP mozMySpell::GetDictionary(PRUnichar **aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  if (mDictionary.IsEmpty())
    return NS_ERROR_NOT_INITIALIZED;

  *aDictionary = ToNewUnicode(mDictionary);
  return *aDictionary ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* set the Dictionary.
 * This also Loads the dictionary and initializes the converter using the dictionaries converter
 */
NS_IMETHODIMP mozMySpell::SetDictionary(const PRUnichar *aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  if (mDictionary.Equals(aDictionary))
    return NS_OK;

  nsIFile* affFile = mDictionaries.GetWeak(aDictionary);
  if (!affFile)
    return NS_ERROR_FILE_NOT_FOUND;

  nsCAutoString dictFileName, affFileName;

  // XXX This isn't really good. nsIFile->NativePath isn't safe for all
  // character sets on Windows.
  // A better way would be to QI to nsILocalFile, and get a filehandle
  // from there. Only problem is that myspell wants a path

  nsresult rv = affFile->GetNativePath(affFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  dictFileName = affFileName;
  PRInt32 dotPos = dictFileName.RFindChar('.');
  if (dotPos == -1)
    return NS_ERROR_FAILURE;

  dictFileName.SetLength(dotPos);
  dictFileName.AppendLiteral(".aff");

  // SetDictionary can be called multiple times, so we might have a
  // valid mMySpell instance which needs cleaned up.
  delete mMySpell;

  mDictionary = aDictionary;

  mMySpell = new MySpell(affFileName.get(),
                         dictFileName.get());
  if (!mMySpell)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ccm->GetUnicodeDecoder(mMySpell->get_dic_encoding(),
                              getter_AddRefs(mDecoder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ccm->GetUnicodeEncoder(mMySpell->get_dic_encoding(),
                              getter_AddRefs(mEncoder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEncoder)
    mEncoder->SetOutputErrorBehavior(mEncoder->kOnError_Signal, nsnull, '?');

  PRInt32 pos = mDictionary.FindChar('-');
  if (pos == -1)
    pos = mDictionary.FindChar('_');

  if (pos == -1)
    mLanguage.Assign(mDictionary);
  else
    mLanguage = Substring(mDictionary, 0, pos);

  return NS_OK;
}

/* readonly attribute wstring language; */
NS_IMETHODIMP mozMySpell::GetLanguage(PRUnichar **aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  if (mDictionary.IsEmpty())
    return NS_ERROR_NOT_INITIALIZED;

  *aLanguage = ToNewUnicode(mLanguage);
  return *aLanguage ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute boolean providesPersonalDictionary; */
NS_IMETHODIMP mozMySpell::GetProvidesPersonalDictionary(PRBool *aProvidesPersonalDictionary)
{
  NS_ENSURE_ARG_POINTER(aProvidesPersonalDictionary);

  *aProvidesPersonalDictionary = PR_FALSE;
  return NS_OK;
}

/* readonly attribute boolean providesWordUtils; */
NS_IMETHODIMP mozMySpell::GetProvidesWordUtils(PRBool *aProvidesWordUtils)
{
  NS_ENSURE_ARG_POINTER(aProvidesWordUtils);

  *aProvidesWordUtils = PR_FALSE;
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
  *aPersonalDictionary = mPersonalDictionary;
  NS_IF_ADDREF(*aPersonalDictionary);
  return NS_OK;
}

NS_IMETHODIMP mozMySpell::SetPersonalDictionary(mozIPersonalDictionary * aPersonalDictionary)
{
  mPersonalDictionary = aPersonalDictionary;
  return NS_OK;
}

struct AppendNewStruct
{
  PRUnichar **dics;
  PRUint32 count;
  PRBool failed;
};

static PLDHashOperator
AppendNewString(const PRUnichar *aString, nsIFile* aFile, void* aClosure)
{
  AppendNewStruct *ans = (AppendNewStruct*) aClosure;
  ans->dics[ans->count] = NS_strdup(aString);
  if (!ans->dics[ans->count]) {
    ans->failed = PR_TRUE;
    return PL_DHASH_STOP;
  }

  ++ans->count;
  return PL_DHASH_NEXT;
}

/* void GetDictionaryList ([array, size_is (count)] out wstring dictionaries, out PRUint32 count); */
NS_IMETHODIMP mozMySpell::GetDictionaryList(PRUnichar ***aDictionaries,
                                            PRUint32 *aCount)
{
  if (!aDictionaries || !aCount)
    return NS_ERROR_NULL_POINTER;

  AppendNewStruct ans = {
    (PRUnichar**) NS_Alloc(sizeof(PRUnichar*) * mDictionaries.Count()),
    0,
    PR_FALSE
  };

  // This pointer is used during enumeration
  mDictionaries.EnumerateRead(AppendNewString, &ans);

  if (ans.failed) {
    while (ans.count) {
      --ans.count;
      NS_Free(ans.dics[ans.count]);
    }
    NS_Free(ans.dics);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aDictionaries = ans.dics;
  *aCount = ans.count;

  return NS_OK;
}

void
mozMySpell::LoadDictionaryList()
{
  mDictionaries.Clear();

  nsresult rv;

  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return;

  nsCOMPtr<nsIFile> dictDir;
  rv = dirSvc->Get(DICTIONARY_SEARCH_DIRECTORY,
                   NS_GET_IID(nsIFile), getter_AddRefs(dictDir));
  if (NS_FAILED(rv)) {
    // default to appdir/dictionaries
    rv = dirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                     NS_GET_IID(nsIFile), getter_AddRefs(dictDir));
    if (NS_FAILED(rv))
      return;

    dictDir->AppendNative(NS_LITERAL_CSTRING("dictionaries"));
  }

  LoadDictionariesFromDir(dictDir);

  nsCOMPtr<nsISimpleEnumerator> dictDirs;
  rv = dirSvc->Get(DICTIONARY_SEARCH_DIRECTORY_LIST,
                   NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(dictDirs));
  if (NS_FAILED(rv))
    return;

  PRBool hasMore;
  while (NS_SUCCEEDED(dictDirs->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    dictDirs->GetNext(getter_AddRefs(elem));

    dictDir = do_QueryInterface(elem);
    if (dictDir)
      LoadDictionariesFromDir(dictDir);
  }
}

void
mozMySpell::LoadDictionariesFromDir(nsIFile* aDir)
{
  nsresult rv;

  PRBool check = PR_FALSE;
  rv = aDir->Exists(&check);
  if (NS_FAILED(rv) || !check)
    return;

  rv = aDir->IsDirectory(&check);
  if (NS_FAILED(rv) || !check)
    return;

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(e));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIDirectoryEnumerator> files(do_QueryInterface(e));
  if (!files)
    return;

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(file))) && file) {
    nsAutoString leafName;
    file->GetLeafName(leafName);
    if (!StringEndsWith(leafName, NS_LITERAL_STRING(".dic")))
      continue;

    nsAutoString dict(leafName);
    dict.SetLength(dict.Length() - 4); // magic length of ".dic"

    // check for the presence of the .aff file
    leafName = dict;
    leafName.AppendLiteral(".aff");
    file->SetLeafName(leafName);
    rv = file->Exists(&check);
    if (NS_FAILED(rv) || !check)
      continue;

#ifdef DEBUG_bsmedberg
    printf("Adding dictionary: %s\n", NS_ConvertUTF16toUTF8(dict).get());
#endif

    mDictionaries.Put(dict.get(), file);
  }
}

nsresult mozMySpell::ConvertCharset(const PRUnichar* aStr, char ** aDst)
{
  NS_ENSURE_ARG_POINTER(aDst);
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_NULL_POINTER);

  PRInt32 outLength;
  PRInt32 inLength = nsCRT::strlen(aStr);
  nsresult rv = mEncoder->GetMaxLength(aStr, inLength, &outLength);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDst = (char *) nsMemory::Alloc(sizeof(char) * (outLength+1));
  NS_ENSURE_TRUE(*aDst, NS_ERROR_OUT_OF_MEMORY);

  rv = mEncoder->Convert(aStr, &inLength, *aDst, &outLength);
  if (NS_SUCCEEDED(rv))
    (*aDst)[outLength] = '\0'; 

  return rv;
}

/* boolean Check (in wstring word); */
NS_IMETHODIMP mozMySpell::Check(const PRUnichar *aWord, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mMySpell, NS_ERROR_FAILURE);

  nsXPIDLCString charsetWord;
  nsresult rv = ConvertCharset(aWord, getter_Copies(charsetWord));
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = mMySpell->spell(charsetWord);


  if (!*aResult && mPersonalDictionary) 
    rv = mPersonalDictionary->Check(aWord, mLanguage.get(), aResult);
  
  return rv;
}

/* void Suggest (in wstring word, [array, size_is (count)] out wstring suggestions, out PRUint32 count); */
NS_IMETHODIMP mozMySpell::Suggest(const PRUnichar *aWord, PRUnichar ***aSuggestions, PRUint32 *aSuggestionCount)
{
  NS_ENSURE_ARG_POINTER(aSuggestions);
  NS_ENSURE_ARG_POINTER(aSuggestionCount);
  NS_ENSURE_TRUE(mMySpell, NS_ERROR_FAILURE);

  nsresult rv;
  *aSuggestionCount = 0;
  
  nsXPIDLCString charsetWord;
  rv = ConvertCharset(aWord, getter_Copies(charsetWord));
  NS_ENSURE_SUCCESS(rv, rv);

  char ** wlst;
  *aSuggestionCount = mMySpell->suggest(&wlst, charsetWord);

  if (*aSuggestionCount) {    
    *aSuggestions  = (PRUnichar **)nsMemory::Alloc(*aSuggestionCount * sizeof(PRUnichar *));    
    if (*aSuggestions) {
      PRUint32 index = 0;
      for (index = 0; index < *aSuggestionCount && NS_SUCCEEDED(rv); ++index) {
        // Convert the suggestion to utf16     
        PRInt32 inLength = nsCRT::strlen(wlst[index]);
        PRInt32 outLength;
        rv = mDecoder->GetMaxLength(wlst[index], inLength, &outLength);
        if (NS_SUCCEEDED(rv))
        {
          (*aSuggestions)[index] = (PRUnichar *) nsMemory::Alloc(sizeof(PRUnichar) * (outLength+1));
          if ((*aSuggestions)[index])
          {
            rv = mDecoder->Convert(wlst[index], &inLength, (*aSuggestions)[index], &outLength);
            if (NS_SUCCEEDED(rv))
              (*aSuggestions)[index][outLength] = 0;
          } 
          else
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
      }

      if (NS_FAILED(rv))
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(index, *aSuggestions); // free the PRUnichar strings up to the point at which the error occurred
    }
    else // if (*aSuggestions)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(*aSuggestionCount, wlst);
  return rv;
}

NS_IMETHODIMP
mozMySpell::Observe(nsISupports* aSubj, const char *aTopic,
                    const PRUnichar *aData)
{
  NS_ASSERTION(!strcmp(aTopic, "profile-do-change"),
               "Unexpected observer topic");

  LoadDictionaryList();

  return NS_OK;
}

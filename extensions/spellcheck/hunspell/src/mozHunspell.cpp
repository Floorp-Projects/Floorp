/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 *
 * Contributor(s): Kevin Hendricks (kevin.hendricks@sympatico.ca)
 *                 David Einstein (deinst@world.std.com)
 *                 Michiel van Leeuwen (mvl@exedo.nl)
 *                 Caolan McNamara (cmc@openoffice.org)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Davide Prina
 *                 Giuseppe Modugno
 *                 Gianluca Turconi
 *                 Simon Brouwer
 *                 Noll Janos
 *                 Biro Arpad
 *                 Goldman Eleonora
 *                 Sarlos Tamas
 *                 Bencsath Boldizsar
 *                 Halacsy Peter
 *                 Dvornik Laszlo
 *                 Gefferth Andras
 *                 Nagy Viktor
 *                 Varga Daniel
 *                 Chris Halls
 *                 Rene Engelhard
 *                 Bram Moolenaar
 *                 Dafydd Jones
 *                 Harri Pitkanen
 *                 Andras Timar
 *                 Tor Lillqvist
 *                 Jesper Kristensen (mail@jesperkristensen.dk)
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
 ******* END LICENSE BLOCK *******/

#include "mozHunspell.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "mozISpellI18NManager.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "mozInlineSpellChecker.h"
#include "mozilla/Services.h"
#include <stdlib.h>
#include "nsIMemoryReporter.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozHunspell)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozHunspell)

NS_INTERFACE_MAP_BEGIN(mozHunspell)
  NS_INTERFACE_MAP_ENTRY(mozISpellCheckingEngine)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozISpellCheckingEngine)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozHunspell)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_3(mozHunspell,
                           mPersonalDictionary,
                           mEncoder,
                           mDecoder)

// Memory reporting stuff
static PRInt64 gHunspellAllocatedSize = 0;

void HunspellReportMemoryAllocation(void* ptr) {
  gHunspellAllocatedSize += moz_malloc_usable_size(ptr);
}
void HunspellReportMemoryDeallocation(void* ptr) {
  gHunspellAllocatedSize -= moz_malloc_usable_size(ptr);
}
static PRInt64 HunspellGetCurrentAllocatedSize() {
  return gHunspellAllocatedSize;
}

NS_MEMORY_REPORTER_IMPLEMENT(Hunspell,
    "explicit/spell-check",
    KIND_HEAP,
    UNITS_BYTES,
    HunspellGetCurrentAllocatedSize,
    "Memory used by the Hunspell spell checking engine.  This number accounts "
    "for the memory in use by Hunspell's internal data structures."
)

nsresult
mozHunspell::Init()
{
  if (!mDictionaries.Init())
    return NS_ERROR_OUT_OF_MEMORY;

  LoadDictionaryList();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  mHunspellReporter = new NS_MEMORY_REPORTER_NAME(Hunspell);
  NS_RegisterMemoryReporter(mHunspellReporter);

  return NS_OK;
}

mozHunspell::~mozHunspell()
{
  mPersonalDictionary = nsnull;
  delete mHunspell;

  NS_UnregisterMemoryReporter(mHunspellReporter);
}

/* attribute wstring dictionary; */
NS_IMETHODIMP mozHunspell::GetDictionary(PRUnichar **aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  *aDictionary = ToNewUnicode(mDictionary);
  return *aDictionary ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* set the Dictionary.
 * This also Loads the dictionary and initializes the converter using the dictionaries converter
 */
NS_IMETHODIMP mozHunspell::SetDictionary(const PRUnichar *aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  if (nsDependentString(aDictionary).IsEmpty()) {
    delete mHunspell;
    mHunspell = nsnull;
    mDictionary.AssignLiteral("");
    mAffixFileName.AssignLiteral("");
    mLanguage.AssignLiteral("");
    mDecoder = nsnull;
    mEncoder = nsnull;

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nsnull,
                           SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION,
                           nsnull);
    }
    return NS_OK;
  }

  nsIFile* affFile = mDictionaries.GetWeak(nsDependentString(aDictionary));
  if (!affFile)
    return NS_ERROR_FILE_NOT_FOUND;

  nsCAutoString dictFileName, affFileName;

  // XXX This isn't really good. nsIFile->NativePath isn't safe for all
  // character sets on Windows.
  // A better way would be to QI to nsILocalFile, and get a filehandle
  // from there. Only problem is that hunspell wants a path

  nsresult rv = affFile->GetNativePath(affFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mAffixFileName.Equals(affFileName.get()))
    return NS_OK;

  dictFileName = affFileName;
  PRInt32 dotPos = dictFileName.RFindChar('.');
  if (dotPos == -1)
    return NS_ERROR_FAILURE;

  dictFileName.SetLength(dotPos);
  dictFileName.AppendLiteral(".dic");

  // SetDictionary can be called multiple times, so we might have a
  // valid mHunspell instance which needs cleaned up.
  delete mHunspell;

  mDictionary = aDictionary;
  mAffixFileName = affFileName;

  mHunspell = new Hunspell(affFileName.get(),
                         dictFileName.get());
  if (!mHunspell)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ccm->GetUnicodeDecoder(mHunspell->get_dic_encoding(),
                              getter_AddRefs(mDecoder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ccm->GetUnicodeEncoder(mHunspell->get_dic_encoding(),
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

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nsnull,
                         SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION,
                         nsnull);
  }

  return NS_OK;
}

/* readonly attribute wstring language; */
NS_IMETHODIMP mozHunspell::GetLanguage(PRUnichar **aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  if (mDictionary.IsEmpty())
    return NS_ERROR_NOT_INITIALIZED;

  *aLanguage = ToNewUnicode(mLanguage);
  return *aLanguage ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute boolean providesPersonalDictionary; */
NS_IMETHODIMP mozHunspell::GetProvidesPersonalDictionary(PRBool *aProvidesPersonalDictionary)
{
  NS_ENSURE_ARG_POINTER(aProvidesPersonalDictionary);

  *aProvidesPersonalDictionary = PR_FALSE;
  return NS_OK;
}

/* readonly attribute boolean providesWordUtils; */
NS_IMETHODIMP mozHunspell::GetProvidesWordUtils(PRBool *aProvidesWordUtils)
{
  NS_ENSURE_ARG_POINTER(aProvidesWordUtils);

  *aProvidesWordUtils = PR_FALSE;
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP mozHunspell::GetName(PRUnichar * *aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute wstring copyright; */
NS_IMETHODIMP mozHunspell::GetCopyright(PRUnichar * *aCopyright)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute mozIPersonalDictionary personalDictionary; */
NS_IMETHODIMP mozHunspell::GetPersonalDictionary(mozIPersonalDictionary * *aPersonalDictionary)
{
  *aPersonalDictionary = mPersonalDictionary;
  NS_IF_ADDREF(*aPersonalDictionary);
  return NS_OK;
}

NS_IMETHODIMP mozHunspell::SetPersonalDictionary(mozIPersonalDictionary * aPersonalDictionary)
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
AppendNewString(const nsAString& aString, nsIFile* aFile, void* aClosure)
{
  AppendNewStruct *ans = (AppendNewStruct*) aClosure;
  ans->dics[ans->count] = ToNewUnicode(aString);
  if (!ans->dics[ans->count]) {
    ans->failed = PR_TRUE;
    return PL_DHASH_STOP;
  }

  ++ans->count;
  return PL_DHASH_NEXT;
}

/* void GetDictionaryList ([array, size_is (count)] out wstring dictionaries, out PRUint32 count); */
NS_IMETHODIMP mozHunspell::GetDictionaryList(PRUnichar ***aDictionaries,
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

static PLDHashOperator
FindFirstString(const nsAString& aString, nsIFile* aFile, void* aClosure)
{
  nsAString *dic = (nsAString*) aClosure;
  dic->Assign(aString);
  return PL_DHASH_STOP;
}

void
mozHunspell::LoadDictionaryList()
{
  mDictionaries.Clear();

  nsresult rv;

  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return;

  // find built in dictionaries
  nsCOMPtr<nsIFile> dictDir;
  rv = dirSvc->Get(DICTIONARY_SEARCH_DIRECTORY,
                   NS_GET_IID(nsIFile), getter_AddRefs(dictDir));
  if (NS_SUCCEEDED(rv)) {
    LoadDictionariesFromDir(dictDir);
  }
  else {
    // try to load gredir/dictionaries
    nsCOMPtr<nsIFile> greDir;
    rv = dirSvc->Get(NS_GRE_DIR,
                     NS_GET_IID(nsIFile), getter_AddRefs(greDir));
    if (NS_SUCCEEDED(rv)) {
      greDir->AppendNative(NS_LITERAL_CSTRING("dictionaries"));
      LoadDictionariesFromDir(greDir);
    }

    // try to load appdir/dictionaries only if different than gredir
    nsCOMPtr<nsIFile> appDir;
    rv = dirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                     NS_GET_IID(nsIFile), getter_AddRefs(appDir));
    PRBool equals;
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(appDir->Equals(greDir, &equals)) && !equals) {
      appDir->AppendNative(NS_LITERAL_CSTRING("dictionaries"));
      LoadDictionariesFromDir(appDir);
    }
  }

  // find dictionaries from extensions requiring restart
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

  // find dictionaries from restartless extensions
  for (PRUint32 i = 0; i < mDynamicDirectories.Count(); i++) {
    LoadDictionariesFromDir(mDynamicDirectories[i]);
  }

  // Now we have finished updating the list of dictionaries, update the current
  // dictionary and any editors which may use it.
  mozInlineSpellChecker::UpdateCanEnableInlineSpellChecking();

  // Check if the current dictionary is still available.
  // If not, try to replace it with another dictionary of the same language.
  if (!mDictionary.IsEmpty()) {
    rv = SetDictionary(mDictionary.get());
    if (NS_SUCCEEDED(rv))
      return;
  }

  // If the current dictionary has gone, and we don't have a good replacement,
  // set no current dictionary.
  if (!mDictionary.IsEmpty()) {
    SetDictionary(EmptyString().get());
  }
}

NS_IMETHODIMP
mozHunspell::LoadDictionariesFromDir(nsIFile* aDir)
{
  nsresult rv;

  PRBool check = PR_FALSE;
  rv = aDir->Exists(&check);
  if (NS_FAILED(rv) || !check)
    return NS_ERROR_UNEXPECTED;

  rv = aDir->IsDirectory(&check);
  if (NS_FAILED(rv) || !check)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(e));
  if (NS_FAILED(rv))
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIDirectoryEnumerator> files(do_QueryInterface(e));
  if (!files)
    return NS_ERROR_UNEXPECTED;

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

    mDictionaries.Put(dict, file);
  }

  return NS_OK;
}

nsresult mozHunspell::ConvertCharset(const PRUnichar* aStr, char ** aDst)
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
NS_IMETHODIMP mozHunspell::Check(const PRUnichar *aWord, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mHunspell, NS_ERROR_FAILURE);

  nsXPIDLCString charsetWord;
  nsresult rv = ConvertCharset(aWord, getter_Copies(charsetWord));
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = !!mHunspell->spell(charsetWord);


  if (!*aResult && mPersonalDictionary)
    rv = mPersonalDictionary->Check(aWord, mLanguage.get(), aResult);

  return rv;
}

/* void Suggest (in wstring word, [array, size_is (count)] out wstring suggestions, out PRUint32 count); */
NS_IMETHODIMP mozHunspell::Suggest(const PRUnichar *aWord, PRUnichar ***aSuggestions, PRUint32 *aSuggestionCount)
{
  NS_ENSURE_ARG_POINTER(aSuggestions);
  NS_ENSURE_ARG_POINTER(aSuggestionCount);
  NS_ENSURE_TRUE(mHunspell, NS_ERROR_FAILURE);

  nsresult rv;
  *aSuggestionCount = 0;

  nsXPIDLCString charsetWord;
  rv = ConvertCharset(aWord, getter_Copies(charsetWord));
  NS_ENSURE_SUCCESS(rv, rv);

  char ** wlst;
  *aSuggestionCount = mHunspell->suggest(&wlst, charsetWord);

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
mozHunspell::Observe(nsISupports* aSubj, const char *aTopic,
                    const PRUnichar *aData)
{
  NS_ASSERTION(!strcmp(aTopic, "profile-do-change"),
               "Unexpected observer topic");

  LoadDictionaryList();

  return NS_OK;
}

/* void addDirectory(in nsIFile dir); */
NS_IMETHODIMP mozHunspell::AddDirectory(nsIFile *aDir)
{
  mDynamicDirectories.AppendObject(aDir);
  LoadDictionaryList();
  return NS_OK;
}

/* void removeDirectory(in nsIFile dir); */
NS_IMETHODIMP mozHunspell::RemoveDirectory(nsIFile *aDir)
{
  mDynamicDirectories.RemoveObject(aDir);
  LoadDictionaryList();
  return NS_OK;
}

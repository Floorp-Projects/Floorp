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
#include "nsUnicharUtilCIID.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "mozInlineSpellChecker.h"
#include "mozilla/Services.h"
#include <stdlib.h>
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/ContentParent.h"

using mozilla::dom::ContentParent;
using mozilla::dom::EncodingUtils;

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozHunspell)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozHunspell)

NS_INTERFACE_MAP_BEGIN(mozHunspell)
  NS_INTERFACE_MAP_ENTRY(mozISpellCheckingEngine)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIMemoryReporter)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozISpellCheckingEngine)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozHunspell)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(mozHunspell,
                         mPersonalDictionary,
                         mEncoder,
                         mDecoder)

template<> mozilla::Atomic<size_t> mozilla::CountingAllocatorBase<HunspellAllocator>::sAmount(0);

mozHunspell::mozHunspell()
  : mHunspell(nullptr)
{
#ifdef DEBUG
  // There must be only one instance of this class: it reports memory based on
  // a single static count in HunspellAllocator.
  static bool hasRun = false;
  MOZ_ASSERT(!hasRun);
  hasRun = true;
#endif
}

nsresult
mozHunspell::Init()
{
  LoadDictionaryList(false);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "profile-do-change", true);
    obs->AddObserver(this, "profile-after-change", true);
  }

  mozilla::RegisterWeakMemoryReporter(this);

  return NS_OK;
}

mozHunspell::~mozHunspell()
{
  mozilla::UnregisterWeakMemoryReporter(this);

  mPersonalDictionary = nullptr;
  delete mHunspell;
}

/* attribute wstring dictionary; */
NS_IMETHODIMP mozHunspell::GetDictionary(char16_t **aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  *aDictionary = ToNewUnicode(mDictionary);
  return *aDictionary ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* set the Dictionary.
 * This also Loads the dictionary and initializes the converter using the dictionaries converter
 */
NS_IMETHODIMP mozHunspell::SetDictionary(const char16_t *aDictionary)
{
  NS_ENSURE_ARG_POINTER(aDictionary);

  if (nsDependentString(aDictionary).IsEmpty()) {
    delete mHunspell;
    mHunspell = nullptr;
    mDictionary.Truncate();
    mAffixFileName.Truncate();
    mLanguage.Truncate();
    mDecoder = nullptr;
    mEncoder = nullptr;

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr,
                           SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION,
                           nullptr);
    }
    return NS_OK;
  }

  nsIFile* affFile = mDictionaries.GetWeak(nsDependentString(aDictionary));
  if (!affFile)
    return NS_ERROR_FILE_NOT_FOUND;

  nsAutoCString dictFileName, affFileName;

  // XXX This isn't really good. nsIFile->NativePath isn't safe for all
  // character sets on Windows.
  // A better way would be to QI to nsIFile, and get a filehandle
  // from there. Only problem is that hunspell wants a path

  nsresult rv = affFile->GetNativePath(affFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mAffixFileName.Equals(affFileName.get()))
    return NS_OK;

  dictFileName = affFileName;
  int32_t dotPos = dictFileName.RFindChar('.');
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

  nsDependentCString label(mHunspell->get_dic_encoding());
  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(label, encoding)) {
    return NS_ERROR_UCONV_NOCONV;
  }
  mEncoder = EncodingUtils::EncoderForEncoding(encoding);
  mDecoder = EncodingUtils::DecoderForEncoding(encoding);

  if (mEncoder)
    mEncoder->SetOutputErrorBehavior(mEncoder->kOnError_Signal, nullptr, '?');

  int32_t pos = mDictionary.FindChar('-');
  if (pos == -1)
    pos = mDictionary.FindChar('_');

  if (pos == -1)
    mLanguage.Assign(mDictionary);
  else
    mLanguage = Substring(mDictionary, 0, pos);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr,
                         SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION,
                         nullptr);
  }

  return NS_OK;
}

/* readonly attribute wstring language; */
NS_IMETHODIMP mozHunspell::GetLanguage(char16_t **aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  if (mDictionary.IsEmpty())
    return NS_ERROR_NOT_INITIALIZED;

  *aLanguage = ToNewUnicode(mLanguage);
  return *aLanguage ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute boolean providesPersonalDictionary; */
NS_IMETHODIMP mozHunspell::GetProvidesPersonalDictionary(bool *aProvidesPersonalDictionary)
{
  NS_ENSURE_ARG_POINTER(aProvidesPersonalDictionary);

  *aProvidesPersonalDictionary = false;
  return NS_OK;
}

/* readonly attribute boolean providesWordUtils; */
NS_IMETHODIMP mozHunspell::GetProvidesWordUtils(bool *aProvidesWordUtils)
{
  NS_ENSURE_ARG_POINTER(aProvidesWordUtils);

  *aProvidesWordUtils = false;
  return NS_OK;
}

/* readonly attribute wstring name; */
NS_IMETHODIMP mozHunspell::GetName(char16_t * *aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute wstring copyright; */
NS_IMETHODIMP mozHunspell::GetCopyright(char16_t * *aCopyright)
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
  char16_t **dics;
  uint32_t count;
  bool failed;
};

static PLDHashOperator
AppendNewString(const nsAString& aString, nsIFile* aFile, void* aClosure)
{
  AppendNewStruct *ans = (AppendNewStruct*) aClosure;
  ans->dics[ans->count] = ToNewUnicode(aString);
  if (!ans->dics[ans->count]) {
    ans->failed = true;
    return PL_DHASH_STOP;
  }

  ++ans->count;
  return PL_DHASH_NEXT;
}

/* void GetDictionaryList ([array, size_is (count)] out wstring dictionaries, out uint32_t count); */
NS_IMETHODIMP mozHunspell::GetDictionaryList(char16_t ***aDictionaries,
                                            uint32_t *aCount)
{
  if (!aDictionaries || !aCount)
    return NS_ERROR_NULL_POINTER;

  AppendNewStruct ans = {
    (char16_t**) NS_Alloc(sizeof(char16_t*) * mDictionaries.Count()),
    0,
    false
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
mozHunspell::LoadDictionaryList(bool aNotifyChildProcesses)
{
  mDictionaries.Clear();

  nsresult rv;

  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return;

  // find built in dictionaries, or dictionaries specified in
  // spellchecker.dictionary_path in prefs
  nsCOMPtr<nsIFile> dictDir;

  // check preferences first
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    nsCString extDictPath;
    rv = prefs->GetCharPref("spellchecker.dictionary_path", getter_Copies(extDictPath));
    if (NS_SUCCEEDED(rv)) {
      // set the spellchecker.dictionary_path
      rv = NS_NewNativeLocalFile(extDictPath, true, getter_AddRefs(dictDir));
    }
  }
  if (!dictDir) {
    // spellcheck.dictionary_path not found, set internal path
    rv = dirSvc->Get(DICTIONARY_SEARCH_DIRECTORY,
                     NS_GET_IID(nsIFile), getter_AddRefs(dictDir));
  }
  if (dictDir) {
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
    bool equals;
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

  bool hasMore;
  while (NS_SUCCEEDED(dictDirs->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    dictDirs->GetNext(getter_AddRefs(elem));

    dictDir = do_QueryInterface(elem);
    if (dictDir)
      LoadDictionariesFromDir(dictDir);
  }

  // find dictionaries from restartless extensions
  for (int32_t i = 0; i < mDynamicDirectories.Count(); i++) {
    LoadDictionariesFromDir(mDynamicDirectories[i]);
  }

  // Now we have finished updating the list of dictionaries, update the current
  // dictionary and any editors which may use it.
  mozInlineSpellChecker::UpdateCanEnableInlineSpellChecking();

  if (aNotifyChildProcesses) {
    ContentParent::NotifyUpdatedDictionaries();
  }

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

  bool check = false;
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

nsresult mozHunspell::ConvertCharset(const char16_t* aStr, char ** aDst)
{
  NS_ENSURE_ARG_POINTER(aDst);
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_NULL_POINTER);

  int32_t outLength;
  int32_t inLength = NS_strlen(aStr);
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
NS_IMETHODIMP mozHunspell::Check(const char16_t *aWord, bool *aResult)
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

/* void Suggest (in wstring word, [array, size_is (count)] out wstring suggestions, out uint32_t count); */
NS_IMETHODIMP mozHunspell::Suggest(const char16_t *aWord, char16_t ***aSuggestions, uint32_t *aSuggestionCount)
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
    *aSuggestions  = (char16_t **)nsMemory::Alloc(*aSuggestionCount * sizeof(char16_t *));
    if (*aSuggestions) {
      uint32_t index = 0;
      for (index = 0; index < *aSuggestionCount && NS_SUCCEEDED(rv); ++index) {
        // Convert the suggestion to utf16
        int32_t inLength = strlen(wlst[index]);
        int32_t outLength;
        rv = mDecoder->GetMaxLength(wlst[index], inLength, &outLength);
        if (NS_SUCCEEDED(rv))
        {
          (*aSuggestions)[index] = (char16_t *) nsMemory::Alloc(sizeof(char16_t) * (outLength+1));
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
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(index, *aSuggestions); // free the char16_t strings up to the point at which the error occurred
    }
    else // if (*aSuggestions)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(*aSuggestionCount, wlst);
  return rv;
}

NS_IMETHODIMP
mozHunspell::Observe(nsISupports* aSubj, const char *aTopic,
                    const char16_t *aData)
{
  NS_ASSERTION(!strcmp(aTopic, "profile-do-change")
               || !strcmp(aTopic, "profile-after-change"),
               "Unexpected observer topic");

  LoadDictionaryList(false);

  return NS_OK;
}

/* void addDirectory(in nsIFile dir); */
NS_IMETHODIMP mozHunspell::AddDirectory(nsIFile *aDir)
{
  mDynamicDirectories.AppendObject(aDir);
  LoadDictionaryList(true);
  return NS_OK;
}

/* void removeDirectory(in nsIFile dir); */
NS_IMETHODIMP mozHunspell::RemoveDirectory(nsIFile *aDir)
{
  mDynamicDirectories.RemoveObject(aDir);
  LoadDictionaryList(true);
  return NS_OK;
}

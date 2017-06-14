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
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "mozInlineSpellChecker.h"
#include "mozilla/Services.h"
#include <stdlib.h>
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "mozilla/dom/ContentParent.h"

using mozilla::dom::ContentParent;
using namespace mozilla;

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

NS_IMPL_CYCLE_COLLECTION(mozHunspell, mPersonalDictionary)

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

  auto encoding =
    Encoding::ForLabelNoReplacement(mHunspell->get_dict_encoding());
  if (!encoding) {
    return NS_ERROR_UCONV_NOCONV;
  }
  mEncoder = encoding->NewEncoder();
  mDecoder = encoding->NewDecoderWithoutBOMHandling();

  int32_t pos = mDictionary.FindChar('-');
  if (pos == -1)
    pos = mDictionary.FindChar('_');

  if (pos == -1)
    mLanguage.Assign(mDictionary);
  else
    mLanguage = Substring(mDictionary, 0, pos);

  return NS_OK;
}

NS_IMETHODIMP mozHunspell::GetLanguage(char16_t **aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  if (mDictionary.IsEmpty())
    return NS_ERROR_NOT_INITIALIZED;

  *aLanguage = ToNewUnicode(mLanguage);
  return *aLanguage ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP mozHunspell::GetProvidesPersonalDictionary(bool *aProvidesPersonalDictionary)
{
  NS_ENSURE_ARG_POINTER(aProvidesPersonalDictionary);

  *aProvidesPersonalDictionary = false;
  return NS_OK;
}

NS_IMETHODIMP mozHunspell::GetProvidesWordUtils(bool *aProvidesWordUtils)
{
  NS_ENSURE_ARG_POINTER(aProvidesWordUtils);

  *aProvidesWordUtils = false;
  return NS_OK;
}

NS_IMETHODIMP mozHunspell::GetName(char16_t * *aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP mozHunspell::GetCopyright(char16_t * *aCopyright)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

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

NS_IMETHODIMP mozHunspell::GetDictionaryList(char16_t ***aDictionaries,
                                            uint32_t *aCount)
{
  if (!aDictionaries || !aCount)
    return NS_ERROR_NULL_POINTER;

  uint32_t count = 0;
  char16_t** dicts =
    (char16_t**) moz_xmalloc(sizeof(char16_t*) * mDictionaries.Count());

  for (auto iter = mDictionaries.Iter(); !iter.Done(); iter.Next()) {
    dicts[count] = ToNewUnicode(iter.Key());
    if (!dicts[count]) {
      while (count) {
        --count;
        free(dicts[count]);
      }
      free(dicts);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    ++count;
  }

  *aDictionaries = dicts;
  *aCount = count;

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

  // find dictionaries in DICPATH
  char* dicEnv = PR_GetEnv("DICPATH");
  if (dicEnv) {
    // do a two-pass dance so dictionaries are loaded right-to-left as preference
    nsTArray<nsCOMPtr<nsIFile>> dirs;
    nsAutoCString env(dicEnv); // assume dicEnv is UTF-8

    char* currPath = nullptr;
    char* nextPaths = env.BeginWriting();
    while ((currPath = NS_strtok(":", &nextPaths))) {
      nsCOMPtr<nsIFile> dir;
      rv = NS_NewNativeLocalFile(nsCString(currPath), true, getter_AddRefs(dir));
      if (NS_SUCCEEDED(rv)) {
        dirs.AppendElement(dir);
      }
    }

    // load them in reverse order so they override each other properly
    for (int32_t i = dirs.Length() - 1; i >= 0; i--) {
      LoadDictionariesFromDir(dirs[i]);
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

    // Replace '_' separator with '-'
    dict.ReplaceChar("_", '-');

    mDictionaries.Put(dict, file);
  }

  return NS_OK;
}

nsresult
mozHunspell::ConvertCharset(const char16_t* aStr, std::string* aDst)
{
  NS_ENSURE_ARG_POINTER(aDst);
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_NULL_POINTER);

  auto src = MakeStringSpan(aStr);
  CheckedInt<size_t> needed =
    mEncoder->MaxBufferLengthFromUTF16WithoutReplacement(src.Length());
  if (!needed.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aDst->resize(needed.value());

  char* dstPtr = &aDst->operator[](0);
  auto dst = MakeSpan(reinterpret_cast<uint8_t*>(dstPtr), needed.value());

  uint32_t result;
  size_t read;
  size_t written;
  Tie(result, read, written) =
    mEncoder->EncodeFromUTF16WithoutReplacement(src, dst, true);
  Unused << read;
  MOZ_ASSERT(result != kOutputFull);
  if (result != kInputEmpty) {
    return NS_ERROR_UENC_NOMAPPING;
  }
  aDst->resize(written);
  mEncoder->Encoding()->NewEncoderInto(*mEncoder);
  return NS_OK;
}

NS_IMETHODIMP
mozHunspell::CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/spell-check", KIND_HEAP, UNITS_BYTES,
    HunspellAllocator::MemoryAllocated(),
    "Memory used by the spell-checking engine.");

  return NS_OK;
}

NS_IMETHODIMP mozHunspell::Check(const char16_t *aWord, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aWord);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mHunspell, NS_ERROR_FAILURE);

  std::string charsetWord;
  nsresult rv = ConvertCharset(aWord, &charsetWord);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = mHunspell->spell(charsetWord);

  if (!*aResult && mPersonalDictionary)
    rv = mPersonalDictionary->Check(aWord, mLanguage.get(), aResult);

  return rv;
}

NS_IMETHODIMP mozHunspell::Suggest(const char16_t *aWord, char16_t ***aSuggestions, uint32_t *aSuggestionCount)
{
  NS_ENSURE_ARG_POINTER(aSuggestions);
  NS_ENSURE_ARG_POINTER(aSuggestionCount);
  NS_ENSURE_TRUE(mHunspell, NS_ERROR_FAILURE);

  nsresult rv;
  *aSuggestionCount = 0;

  std::string charsetWord;
  rv = ConvertCharset(aWord, &charsetWord);
  NS_ENSURE_SUCCESS(rv, rv);

  std::vector<std::string> suggestions = mHunspell->suggest(charsetWord);
  *aSuggestionCount = static_cast<uint32_t>(suggestions.size());

  if (*aSuggestionCount) {
    *aSuggestions  = (char16_t **)moz_xmalloc(*aSuggestionCount * sizeof(char16_t *));
    if (*aSuggestions) {
      uint32_t index = 0;
      for (index = 0; index < *aSuggestionCount && NS_SUCCEEDED(rv); ++index) {
        // If the IDL used an array of AString, we could use
        // Encoding::DecodeWithoutBOMHandling() here.
        // Convert the suggestion to utf16
        Span<const char> charSrc(suggestions[index]);
        auto src = AsBytes(charSrc);
        CheckedInt<size_t> needed =
          mDecoder->MaxUTF16BufferLength(src.Length());
        if (!needed.isValid()) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        size_t dstLen = needed.value();
        needed += 1;
        needed *= sizeof(char16_t);
        if (!needed.isValid()) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        (*aSuggestions)[index] = (char16_t*)moz_xmalloc(needed.value());
        if (!((*aSuggestions)[index])) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        auto dst = MakeSpan((*aSuggestions)[index], dstLen);
        uint32_t result;
        size_t read;
        size_t written;
        bool hadErrors;
        Tie(result, read, written, hadErrors) =
          mDecoder->DecodeToUTF16(src, dst, true);
        MOZ_ASSERT(result == kInputEmpty);
        MOZ_ASSERT(read == src.Length());
        MOZ_ASSERT(written <= dstLen);
        Unused << hadErrors;
        (*aSuggestions)[index][written] = 0;
        mDecoder->Encoding()->NewDecoderWithoutBOMHandlingInto(*mDecoder);
      }

      if (NS_FAILED(rv))
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(index, *aSuggestions); // free the char16_t strings up to the point at which the error occurred
    }
    else // if (*aSuggestions)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

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

NS_IMETHODIMP mozHunspell::AddDirectory(nsIFile *aDir)
{
  mDynamicDirectories.AppendObject(aDir);
  LoadDictionaryList(true);
  return NS_OK;
}

NS_IMETHODIMP mozHunspell::RemoveDirectory(nsIFile *aDir)
{
  mDynamicDirectories.RemoveObject(aDir);
  LoadDictionaryList(true);

#ifdef MOZ_THUNDERBIRD
  /*
   * This notification is needed for Thunderbird. Thunderbird derives the dictionary
   * from the document's "lang" attribute. If a dictionary is removed,
   * we need to change the "lang" attribute.
   */
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr,
                         SPELLCHECK_DICTIONARY_REMOVE_NOTIFICATION,
                         nullptr);
  }
#endif
  return NS_OK;
}

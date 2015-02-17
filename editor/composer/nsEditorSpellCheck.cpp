/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>                     // for getenv

#include "mozilla/Attributes.h"         // for MOZ_FINAL
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/Services.h"           // for GetXULChromeRegistryService
#include "mozilla/dom/Element.h"        // for Element
#include "mozilla/dom/Selection.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAString.h"                  // for nsAString_internal::IsEmpty, etc
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsDependentSubstring.h"       // for Substring
#include "nsEditorSpellCheck.h"
#include "nsError.h"                    // for NS_ERROR_NOT_INITIALIZED, etc
#include "nsIChromeRegistry.h"          // for nsIXULChromeRegistry
#include "nsIContent.h"                 // for nsIContent
#include "nsIContentPrefService.h"      // for nsIContentPrefService, etc
#include "nsIContentPrefService2.h"     // for nsIContentPrefService2, etc
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDocument.h"                // for nsIDocument
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIHTMLEditor.h"              // for nsIHTMLEditor
#include "nsILoadContext.h"
#include "nsISelection.h"               // for nsISelection
#include "nsISpellChecker.h"            // for nsISpellChecker, etc
#include "nsISupportsBase.h"            // for nsISupports
#include "nsISupportsUtils.h"           // for NS_ADDREF
#include "nsITextServicesDocument.h"    // for nsITextServicesDocument
#include "nsITextServicesFilter.h"      // for nsITextServicesFilter
#include "nsIURI.h"                     // for nsIURI
#include "nsIVariant.h"                 // for nsIWritableVariant, etc
#include "nsLiteralString.h"            // for NS_LITERAL_STRING, etc
#include "nsMemory.h"                   // for nsMemory
#include "nsRange.h"
#include "nsReadableUtils.h"            // for ToNewUnicode, EmptyString, etc
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsStringFwd.h"                // for nsAFlatString
#include "nsStyleUtil.h"                // for nsStyleUtil
#include "nsXULAppAPI.h"                // for XRE_GetProcessType

using namespace mozilla;
using namespace mozilla::dom;

class UpdateDictionaryHolder {
  private:
    nsEditorSpellCheck* mSpellCheck;
  public:
    explicit UpdateDictionaryHolder(nsEditorSpellCheck* esc): mSpellCheck(esc) {
      if (mSpellCheck) {
        mSpellCheck->BeginUpdateDictionary();
      }
    }
    ~UpdateDictionaryHolder() {
      if (mSpellCheck) {
        mSpellCheck->EndUpdateDictionary();
      }
    }
};

#define CPS_PREF_NAME NS_LITERAL_STRING("spellcheck.lang")

/**
 * Gets the URI of aEditor's document.
 */
static nsresult
GetDocumentURI(nsIEditor* aEditor, nsIURI * *aURI)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIDOMDocument> domDoc;
  aEditor->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> docUri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(docUri, NS_ERROR_FAILURE);

  *aURI = docUri;
  NS_ADDREF(*aURI);
  return NS_OK;
}

static already_AddRefed<nsILoadContext>
GetLoadContext(nsIEditor* aEditor)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aEditor->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, nullptr);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, nullptr);

  nsCOMPtr<nsILoadContext> loadContext = doc->GetLoadContext();
  return loadContext.forget();
}

/**
 * Fetches the dictionary stored in content prefs and maintains state during the
 * fetch, which is asynchronous.
 */
class DictionaryFetcher MOZ_FINAL : public nsIContentPrefCallback2
{
public:
  NS_DECL_ISUPPORTS

  DictionaryFetcher(nsEditorSpellCheck* aSpellCheck,
                    nsIEditorSpellCheckCallback* aCallback,
                    uint32_t aGroup)
    : mCallback(aCallback), mGroup(aGroup), mSpellCheck(aSpellCheck) {}

  NS_IMETHOD Fetch(nsIEditor* aEditor);

  NS_IMETHOD HandleResult(nsIContentPref* aPref) MOZ_OVERRIDE
  {
    nsCOMPtr<nsIVariant> value;
    nsresult rv = aPref->GetValue(getter_AddRefs(value));
    NS_ENSURE_SUCCESS(rv, rv);
    value->GetAsAString(mDictionary);
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t reason) MOZ_OVERRIDE
  {
    mSpellCheck->DictionaryFetched(this);
    return NS_OK;
  }

  NS_IMETHOD HandleError(nsresult error) MOZ_OVERRIDE
  {
    return NS_OK;
  }

  nsCOMPtr<nsIEditorSpellCheckCallback> mCallback;
  uint32_t mGroup;
  nsString mRootContentLang;
  nsString mRootDocContentLang;
  nsString mDictionary;

private:
  ~DictionaryFetcher() {}

  nsRefPtr<nsEditorSpellCheck> mSpellCheck;
};
NS_IMPL_ISUPPORTS(DictionaryFetcher, nsIContentPrefCallback2)

NS_IMETHODIMP
DictionaryFetcher::Fetch(nsIEditor* aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  nsresult rv;

  nsCOMPtr<nsIURI> docUri;
  rv = GetDocumentURI(aEditor, getter_AddRefs(docUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString docUriSpec;
  rv = docUri->GetSpec(docUriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsILoadContext> loadContext = GetLoadContext(aEditor);
  rv = contentPrefService->GetByDomainAndName(NS_ConvertUTF8toUTF16(docUriSpec),
                                              CPS_PREF_NAME, loadContext,
                                              this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * Stores the current dictionary for aEditor's document URL.
 */
static nsresult
StoreCurrentDictionary(nsIEditor* aEditor, const nsAString& aDictionary)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  nsresult rv;

  nsCOMPtr<nsIURI> docUri;
  rv = GetDocumentURI(aEditor, getter_AddRefs(docUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString docUriSpec;
  rv = docUri->GetSpec(docUriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritableVariant> prefValue = do_CreateInstance(NS_VARIANT_CONTRACTID);
  NS_ENSURE_TRUE(prefValue, NS_ERROR_OUT_OF_MEMORY);
  prefValue->SetAsAString(aDictionary);

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsILoadContext> loadContext = GetLoadContext(aEditor);
  return contentPrefService->Set(NS_ConvertUTF8toUTF16(docUriSpec),
                                 CPS_PREF_NAME, prefValue, loadContext,
                                 nullptr);
}

/**
 * Forgets the current dictionary stored for aEditor's document URL.
 */
static nsresult
ClearCurrentDictionary(nsIEditor* aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  nsresult rv;

  nsCOMPtr<nsIURI> docUri;
  rv = GetDocumentURI(aEditor, getter_AddRefs(docUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString docUriSpec;
  rv = docUri->GetSpec(docUriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsILoadContext> loadContext = GetLoadContext(aEditor);
  return contentPrefService->RemoveByDomainAndName(
    NS_ConvertUTF8toUTF16(docUriSpec), CPS_PREF_NAME, loadContext, nullptr);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEditorSpellCheck)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEditorSpellCheck)

NS_INTERFACE_MAP_BEGIN(nsEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRY(nsIEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsEditorSpellCheck)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsEditorSpellCheck,
                         mEditor,
                         mSpellChecker,
                         mTxtSrvFilter)

nsEditorSpellCheck::nsEditorSpellCheck()
  : mSuggestedWordIndex(0)
  , mDictionaryIndex(0)
  , mEditor(nullptr)
  , mDictionaryFetcherGroup(0)
  , mUpdateDictionaryRunning(false)
{
}

nsEditorSpellCheck::~nsEditorSpellCheck()
{
  // Make sure we blow the spellchecker away, just in
  // case it hasn't been destroyed already.
  mSpellChecker = nullptr;
}

// The problem is that if the spell checker does not exist, we can not tell
// which dictionaries are installed. This function works around the problem,
// allowing callers to ask if we can spell check without actually doing so (and
// enabling or disabling UI as necessary). This just creates a spellcheck
// object if needed and asks it for the dictionary list.
NS_IMETHODIMP
nsEditorSpellCheck::CanSpellCheck(bool* _retval)
{
  nsresult rv;
  nsCOMPtr<nsISpellChecker> spellChecker;
  if (! mSpellChecker) {
    spellChecker = do_CreateInstance(NS_SPELLCHECKER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    spellChecker = mSpellChecker;
  }
  nsTArray<nsString> dictList;
  rv = spellChecker->GetDictionaryList(&dictList);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = (dictList.Length() > 0);
  return NS_OK;
}

// Instances of this class can be used as either runnables or RAII helpers.
class CallbackCaller MOZ_FINAL : public nsRunnable
{
public:
  explicit CallbackCaller(nsIEditorSpellCheckCallback* aCallback)
    : mCallback(aCallback) {}

  ~CallbackCaller()
  {
    Run();
  }

  NS_IMETHOD Run()
  {
    if (mCallback) {
      mCallback->EditorSpellCheckDone();
      mCallback = nullptr;
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIEditorSpellCheckCallback> mCallback;
};

NS_IMETHODIMP    
nsEditorSpellCheck::InitSpellChecker(nsIEditor* aEditor, bool aEnableSelectionChecking, nsIEditorSpellCheckCallback* aCallback)
{
  NS_ENSURE_TRUE(aEditor, NS_ERROR_NULL_POINTER);
  mEditor = aEditor;

  nsresult rv;

  // We can spell check with any editor type
  nsCOMPtr<nsITextServicesDocument>tsDoc =
     do_CreateInstance("@mozilla.org/textservices/textservicesdocument;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(tsDoc, NS_ERROR_NULL_POINTER);

  tsDoc->SetFilter(mTxtSrvFilter);

  // Pass the editor to the text services document
  rv = tsDoc->InitWithEditor(aEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aEnableSelectionChecking) {
    // Find out if the section is collapsed or not.
    // If it isn't, we want to spellcheck just the selection.

    nsCOMPtr<nsISelection> domSelection;
    aEditor->GetSelection(getter_AddRefs(domSelection));
    nsRefPtr<Selection> selection = static_cast<Selection*>(domSelection.get());
    NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

    int32_t count = 0;

    rv = selection->GetRangeCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    if (count > 0) {
      nsRefPtr<nsRange> range = selection->GetRangeAt(0);
      NS_ENSURE_STATE(range);

      bool collapsed = false;
      rv = range->GetCollapsed(&collapsed);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!collapsed) {
        // We don't want to touch the range in the selection,
        // so create a new copy of it.

        nsRefPtr<nsRange> rangeBounds = range->CloneRange();

        // Make sure the new range spans complete words.

        rv = tsDoc->ExpandRangeToWordBoundaries(rangeBounds);
        NS_ENSURE_SUCCESS(rv, rv);

        // Now tell the text services that you only want
        // to iterate over the text in this range.

        rv = tsDoc->SetExtent(rangeBounds);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  mSpellChecker = do_CreateInstance(NS_SPELLCHECKER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NULL_POINTER);

  rv = mSpellChecker->SetDocument(tsDoc, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // do not fail if UpdateCurrentDictionary fails because this method may
  // succeed later.
  rv = UpdateCurrentDictionary(aCallback);
  if (NS_FAILED(rv) && aCallback) {
    // However, if it does fail, we still need to call the callback since we
    // discard the failure.  Do it asynchronously so that the caller is always
    // guaranteed async behavior.
    nsRefPtr<CallbackCaller> caller = new CallbackCaller(aCallback);
    NS_ENSURE_STATE(caller);
    rv = NS_DispatchToMainThread(caller);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetNextMisspelledWord(char16_t **aNextMisspelledWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  nsAutoString nextMisspelledWord;
  
  DeleteSuggestedWordList();
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsresult rv = mSpellChecker->NextMisspelledWord(nextMisspelledWord,
                                                  &mSuggestedWordList);

  *aNextMisspelledWord = ToNewUnicode(nextMisspelledWord);
  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetSuggestedWord(char16_t **aSuggestedWord)
{
  nsAutoString word;
  if ( mSuggestedWordIndex < int32_t(mSuggestedWordList.Length()))
  {
    *aSuggestedWord = ToNewUnicode(mSuggestedWordList[mSuggestedWordIndex]);
    mSuggestedWordIndex++;
  } else {
    // A blank string signals that there are no more strings
    *aSuggestedWord = ToNewUnicode(EmptyString());
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::CheckCurrentWord(const char16_t *aSuggestedWord,
                                     bool *aIsMisspelled)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  DeleteSuggestedWordList();
  return mSpellChecker->CheckWord(nsDependentString(aSuggestedWord),
                                  aIsMisspelled, &mSuggestedWordList);
}

NS_IMETHODIMP    
nsEditorSpellCheck::CheckCurrentWordNoSuggest(const char16_t *aSuggestedWord,
                                              bool *aIsMisspelled)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->CheckWord(nsDependentString(aSuggestedWord),
                                  aIsMisspelled, nullptr);
}

NS_IMETHODIMP    
nsEditorSpellCheck::ReplaceWord(const char16_t *aMisspelledWord,
                                const char16_t *aReplaceWord,
                                bool             allOccurrences)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->Replace(nsDependentString(aMisspelledWord),
                                nsDependentString(aReplaceWord), allOccurrences);
}

NS_IMETHODIMP    
nsEditorSpellCheck::IgnoreWordAllOccurrences(const char16_t *aWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->IgnoreAll(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetPersonalDictionary()
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

   // We can spell check with any editor type
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  return mSpellChecker->GetPersonalDictionary(&mDictionaryList);
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetPersonalDictionaryWord(char16_t **aDictionaryWord)
{
  if ( mDictionaryIndex < int32_t( mDictionaryList.Length()))
  {
    *aDictionaryWord = ToNewUnicode(mDictionaryList[mDictionaryIndex]);
    mDictionaryIndex++;
  } else {
    // A blank string signals that there are no more strings
    *aDictionaryWord = ToNewUnicode(EmptyString());
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::AddWordToDictionary(const char16_t *aWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->AddWordToPersonalDictionary(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::RemoveWordFromDictionary(const char16_t *aWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->RemoveWordFromPersonalDictionary(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetDictionaryList(char16_t ***aDictionaryList, uint32_t *aCount)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(aDictionaryList && aCount, NS_ERROR_NULL_POINTER);

  *aDictionaryList = 0;
  *aCount          = 0;

  nsTArray<nsString> dictList;

  nsresult rv = mSpellChecker->GetDictionaryList(&dictList);

  NS_ENSURE_SUCCESS(rv, rv);

  char16_t **tmpPtr = 0;

  if (dictList.Length() < 1)
  {
    // If there are no dictionaries, return an array containing
    // one element and a count of one.

    tmpPtr = (char16_t **)nsMemory::Alloc(sizeof(char16_t *));

    NS_ENSURE_TRUE(tmpPtr, NS_ERROR_OUT_OF_MEMORY);

    *tmpPtr          = 0;
    *aDictionaryList = tmpPtr;
    *aCount          = 0;

    return NS_OK;
  }

  tmpPtr = (char16_t **)nsMemory::Alloc(sizeof(char16_t *) * dictList.Length());

  NS_ENSURE_TRUE(tmpPtr, NS_ERROR_OUT_OF_MEMORY);

  *aDictionaryList = tmpPtr;
  *aCount          = dictList.Length();

  uint32_t i;

  for (i = 0; i < *aCount; i++)
  {
    tmpPtr[i] = ToNewUnicode(dictList[i]);
  }

  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetCurrentDictionary(nsAString& aDictionary)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->GetCurrentDictionary(aDictionary);
}

NS_IMETHODIMP    
nsEditorSpellCheck::SetCurrentDictionary(const nsAString& aDictionary)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  nsRefPtr<nsEditorSpellCheck> kungFuDeathGrip = this;

  // The purpose of mUpdateDictionaryRunning is to avoid doing all of this if
  // UpdateCurrentDictionary's helper method DictionaryFetched, which calls us,
  // is on the stack.
  if (!mUpdateDictionaryRunning) {

    // Ignore pending dictionary fetchers by increasing this number.
    mDictionaryFetcherGroup++;

    nsDefaultStringComparator comparator;
    nsAutoString langCode;
    int32_t dashIdx = aDictionary.FindChar('-');
    if (dashIdx != -1) {
      langCode.Assign(Substring(aDictionary, 0, dashIdx));
    } else {
      langCode.Assign(aDictionary);
    }
    if (mPreferredLang.IsEmpty() ||
        !nsStyleUtil::DashMatchCompare(mPreferredLang, langCode, comparator)) {
      // When user sets dictionary manually, we store this value associated
      // with editor url.
      StoreCurrentDictionary(mEditor, aDictionary);
    } else {
      // If user sets a dictionary matching (even partially), lang defined by
      // document, we consider content pref has been canceled, and we clear it.
      ClearCurrentDictionary(mEditor);
    }

    // Also store it in as a preference. It will be used as a default value
    // when everything else fails.
    Preferences::SetString("spellchecker.dictionary", aDictionary);
  }
  return mSpellChecker->SetCurrentDictionary(aDictionary);
}

NS_IMETHODIMP
nsEditorSpellCheck::CheckCurrentDictionary()
{
  mSpellChecker->CheckCurrentDictionary();

  // Check if our current dictionary is still available.
  nsAutoString currentDictionary;
  nsresult rv = GetCurrentDictionary(currentDictionary);
  if (NS_SUCCEEDED(rv) && !currentDictionary.IsEmpty()) {
    return NS_OK;
  }

  // If our preferred current dictionary has gone, pick another one.
  nsTArray<nsString> dictList;
  rv = mSpellChecker->GetDictionaryList(&dictList);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dictList.Length() > 0) {
    rv = SetCurrentDictionary(dictList[0]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::UninitSpellChecker()
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  // Cleanup - kill the spell checker
  DeleteSuggestedWordList();
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  mSpellChecker = 0;
  return NS_OK;
}


/* void setFilter (in nsITextServicesFilter filter); */
NS_IMETHODIMP 
nsEditorSpellCheck::SetFilter(nsITextServicesFilter *filter)
{
  mTxtSrvFilter = filter;
  return NS_OK;
}

nsresult    
nsEditorSpellCheck::DeleteSuggestedWordList()
{
  mSuggestedWordList.Clear();
  mSuggestedWordIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsEditorSpellCheck::UpdateCurrentDictionary(nsIEditorSpellCheckCallback* aCallback)
{
  nsresult rv;

  nsRefPtr<nsEditorSpellCheck> kungFuDeathGrip = this;

  // Get language with html5 algorithm
  nsCOMPtr<nsIContent> rootContent;
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor) {
    rootContent = htmlEditor->GetActiveEditingHost();
  } else {
    nsCOMPtr<nsIDOMElement> rootElement;
    rv = mEditor->GetRootElement(getter_AddRefs(rootElement));
    NS_ENSURE_SUCCESS(rv, rv);
    rootContent = do_QueryInterface(rootElement);
  }
  NS_ENSURE_TRUE(rootContent, NS_ERROR_FAILURE);

  nsRefPtr<DictionaryFetcher> fetcher =
    new DictionaryFetcher(this, aCallback, mDictionaryFetcherGroup);
  rootContent->GetLang(fetcher->mRootContentLang);
  nsCOMPtr<nsIDocument> doc = rootContent->GetCurrentDoc();
  NS_ENSURE_STATE(doc);
  doc->GetContentLanguage(fetcher->mRootDocContentLang);

  rv = fetcher->Fetch(mEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsEditorSpellCheck::DictionaryFetched(DictionaryFetcher* aFetcher)
{
  MOZ_ASSERT(aFetcher);
  nsRefPtr<nsEditorSpellCheck> kungFuDeathGrip = this;

  // Important: declare the holder after the callback caller so that the former
  // is destructed first so that it's not active when the callback is called.
  CallbackCaller callbackCaller(aFetcher->mCallback);
  UpdateDictionaryHolder holder(this);

  if (aFetcher->mGroup < mDictionaryFetcherGroup) {
    // SetCurrentDictionary was called after the fetch started.  Don't overwrite
    // that dictionary with the fetched one.
    return NS_OK;
  }

  mPreferredLang.Assign(aFetcher->mRootContentLang);

  // If we successfully fetched a dictionary from content prefs, do not go
  // further. Use this exact dictionary.
  nsAutoString dictName(aFetcher->mDictionary);
  if (!dictName.IsEmpty()) {
    if (NS_FAILED(SetCurrentDictionary(dictName))) {
      // may be dictionary was uninstalled ?
      ClearCurrentDictionary(mEditor);
    }
    return NS_OK;
  }

  if (mPreferredLang.IsEmpty()) {
    mPreferredLang.Assign(aFetcher->mRootDocContentLang);
  }

  // Then, try to use language computed from element
  if (!mPreferredLang.IsEmpty()) {
    dictName.Assign(mPreferredLang);
  }

  // otherwise, get language from preferences
  nsAutoString preferedDict(Preferences::GetLocalizedString("spellchecker.dictionary"));
  if (dictName.IsEmpty()) {
    dictName.Assign(preferedDict);
  }

  nsresult rv = NS_OK;
  if (dictName.IsEmpty()) {
    // Prefs didn't give us a dictionary name, so just get the current
    // locale and use that as the default dictionary name!

    nsCOMPtr<nsIXULChromeRegistry> packageRegistry =
      mozilla::services::GetXULChromeRegistryService();

    if (packageRegistry) {
      nsAutoCString utf8DictName;
      rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global"),
                                              utf8DictName);
      AppendUTF8toUTF16(utf8DictName, dictName);
    }
  }

  if (NS_SUCCEEDED(rv) && !dictName.IsEmpty()) {
    rv = SetCurrentDictionary(dictName);
    if (NS_FAILED(rv)) {
      // required dictionary was not available. Try to get a dictionary
      // matching at least language part of dictName: 

      nsAutoString langCode;
      int32_t dashIdx = dictName.FindChar('-');
      if (dashIdx != -1) {
        langCode.Assign(Substring(dictName, 0, dashIdx));
      } else {
        langCode.Assign(dictName);
      }

      nsDefaultStringComparator comparator;

      // try dictionary.spellchecker preference if it starts with langCode (and
      // if we haven't tried it already)
      if (!preferedDict.IsEmpty() && !dictName.Equals(preferedDict) &&
          nsStyleUtil::DashMatchCompare(preferedDict, langCode, comparator)) {
        rv = SetCurrentDictionary(preferedDict);
      }

      // Otherwise, try langCode (if we haven't tried it already)
      if (NS_FAILED(rv)) {
        if (!dictName.Equals(langCode) && !preferedDict.Equals(langCode)) {
          rv = SetCurrentDictionary(langCode);
        }
      }

      // Otherwise, try any available dictionary aa-XX
      if (NS_FAILED(rv)) {
        // loop over avaible dictionaries; if we find one with required
        // language, use it
        nsTArray<nsString> dictList;
        rv = mSpellChecker->GetDictionaryList(&dictList);
        NS_ENSURE_SUCCESS(rv, rv);
        int32_t i, count = dictList.Length();
        for (i = 0; i < count; i++) {
          nsAutoString dictStr(dictList.ElementAt(i));

          if (dictStr.Equals(dictName) ||
              dictStr.Equals(preferedDict) ||
              dictStr.Equals(langCode)) {
            // We have already tried it
            continue;
          }
          if (nsStyleUtil::DashMatchCompare(dictStr, langCode, comparator) &&
              NS_SUCCEEDED(SetCurrentDictionary(dictStr))) {
              break;
          }
        }
      }
    }
  }

  // If we have not set dictionary, and the editable element doesn't have a
  // lang attribute, we try to get a dictionary. First try LANG environment variable,
  // then en-US. If it does not work, pick the first one.
  if (mPreferredLang.IsEmpty()) {
    nsAutoString currentDictionary;
    rv = GetCurrentDictionary(currentDictionary);
    if (NS_FAILED(rv) || currentDictionary.IsEmpty()) {
      // Try to get current dictionary from environment variable LANG
      char* env_lang = getenv("LANG");
      if (env_lang != nullptr) {
        nsString lang = NS_ConvertUTF8toUTF16(env_lang);
        // Strip trailing charset if there is any
        int32_t dot_pos = lang.FindChar('.');
        if (dot_pos != -1) {
          lang = Substring(lang, 0, dot_pos);
        }
        if (NS_FAILED(rv)) {
          int32_t underScore = lang.FindChar('_');
          if (underScore != -1) {
            lang.Replace(underScore, 1, '-');
            rv = SetCurrentDictionary(lang);
          }
        }
      }
      if (NS_FAILED(rv)) {
        rv = SetCurrentDictionary(NS_LITERAL_STRING("en-US"));
        if (NS_FAILED(rv)) {
          nsTArray<nsString> dictList;
          rv = mSpellChecker->GetDictionaryList(&dictList);
          if (NS_SUCCEEDED(rv) && dictList.Length() > 0) {
            SetCurrentDictionary(dictList[0]);
          }
        }
      }
    }
  }

  // If an error was thrown while setting the dictionary, just
  // fail silently so that the spellchecker dialog is allowed to come
  // up. The user can manually reset the language to their choice on
  // the dialog if it is wrong.

  DeleteSuggestedWordList();

  return NS_OK;
}

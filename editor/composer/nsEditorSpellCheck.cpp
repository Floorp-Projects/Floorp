/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>                     // for getenv

#include "mozilla/Attributes.h"         // for final
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
#include "nsVariant.h"                  // for nsIWritableVariant, etc
#include "nsLiteralString.h"            // for NS_LITERAL_STRING, etc
#include "nsMemory.h"                   // for nsMemory
#include "nsRange.h"
#include "nsReadableUtils.h"            // for ToNewUnicode, EmptyString, etc
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsStringFwd.h"                // for nsAFlatString
#include "nsStyleUtil.h"                // for nsStyleUtil
#include "nsXULAppAPI.h"                // for XRE_GetProcessType
#include "nsIPlaintextEditor.h"         // for editor flags

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
class DictionaryFetcher final : public nsIContentPrefCallback2
{
public:
  NS_DECL_ISUPPORTS

  DictionaryFetcher(nsEditorSpellCheck* aSpellCheck,
                    nsIEditorSpellCheckCallback* aCallback,
                    uint32_t aGroup)
    : mCallback(aCallback), mGroup(aGroup), mSpellCheck(aSpellCheck) {}

  NS_IMETHOD Fetch(nsIEditor* aEditor);

  NS_IMETHOD HandleResult(nsIContentPref* aPref) override
  {
    nsCOMPtr<nsIVariant> value;
    nsresult rv = aPref->GetValue(getter_AddRefs(value));
    NS_ENSURE_SUCCESS(rv, rv);
    value->GetAsAString(mDictionary);
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t reason) override
  {
    mSpellCheck->DictionaryFetched(this);
    return NS_OK;
  }

  NS_IMETHOD HandleError(nsresult error) override
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

  RefPtr<nsEditorSpellCheck> mSpellCheck;
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

  RefPtr<nsVariant> prefValue = new nsVariant();
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
class CallbackCaller final : public Runnable
{
public:
  explicit CallbackCaller(nsIEditorSpellCheckCallback* aCallback)
    : mCallback(aCallback) {}

  ~CallbackCaller()
  {
    Run();
  }

  NS_IMETHOD Run() override
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
    if (NS_WARN_IF(!domSelection)) {
      return NS_ERROR_FAILURE;
    }
    RefPtr<Selection> selection = domSelection->AsSelection();

    int32_t count = 0;

    rv = selection->GetRangeCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    if (count > 0) {
      RefPtr<nsRange> range = selection->GetRangeAt(0);
      NS_ENSURE_STATE(range);

      bool collapsed = false;
      rv = range->GetCollapsed(&collapsed);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!collapsed) {
        // We don't want to touch the range in the selection,
        // so create a new copy of it.

        RefPtr<nsRange> rangeBounds = range->CloneRange();

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
    RefPtr<CallbackCaller> caller = new CallbackCaller(aCallback);
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
  // XXX This is buggy if mSuggestedWordList.Length() is over INT32_MAX.
  if (mSuggestedWordIndex < static_cast<int32_t>(mSuggestedWordList.Length())) {
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
  // XXX This is buggy if mDictionaryList.Length() is over INT32_MAX.
  if (mDictionaryIndex < static_cast<int32_t>(mDictionaryList.Length())) {
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

  if (dictList.IsEmpty()) {
    // If there are no dictionaries, return an array containing
    // one element and a count of one.

    tmpPtr = (char16_t **)moz_xmalloc(sizeof(char16_t *));

    NS_ENSURE_TRUE(tmpPtr, NS_ERROR_OUT_OF_MEMORY);

    *tmpPtr          = 0;
    *aDictionaryList = tmpPtr;
    *aCount          = 0;

    return NS_OK;
  }

  tmpPtr = (char16_t **)moz_xmalloc(sizeof(char16_t *) * dictList.Length());

  NS_ENSURE_TRUE(tmpPtr, NS_ERROR_OUT_OF_MEMORY);

  *aDictionaryList = tmpPtr;
  *aCount          = dictList.Length();

  for (uint32_t i = 0; i < *aCount; i++) {
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

  RefPtr<nsEditorSpellCheck> kungFuDeathGrip = this;

  // The purpose of mUpdateDictionaryRunning is to avoid doing all of this if
  // UpdateCurrentDictionary's helper method DictionaryFetched, which calls us,
  // is on the stack. In other words: Only do this, if the user manually selected a
  // dictionary to use.
  if (!mUpdateDictionaryRunning) {

    // Ignore pending dictionary fetchers by increasing this number.
    mDictionaryFetcherGroup++;

    uint32_t flags = 0;
    mEditor->GetFlags(&flags);
    if (!(flags & nsIPlaintextEditor::eEditorMailMask)) {
      if (!aDictionary.IsEmpty() && (mPreferredLang.IsEmpty() ||
          !mPreferredLang.Equals(aDictionary,
                                 nsCaseInsensitiveStringComparator()))) {
        // When user sets dictionary manually, we store this value associated
        // with editor url, if it doesn't match the document language exactly.
        // For example on "en" sites, we need to store "en-GB", otherwise
        // the language might jump back to en-US although the user explicitly
        // chose otherwise.
        StoreCurrentDictionary(mEditor, aDictionary);
#ifdef DEBUG_DICT
        printf("***** Writing content preferences for |%s|\n",
               NS_ConvertUTF16toUTF8(aDictionary).get());
#endif
      } else {
        // If user sets a dictionary matching the language defined by
        // document, we consider content pref has been canceled, and we clear it.
        ClearCurrentDictionary(mEditor);
#ifdef DEBUG_DICT
        printf("***** Clearing content preferences for |%s|\n",
               NS_ConvertUTF16toUTF8(aDictionary).get());
#endif
      }

      // Also store it in as a preference, so we can use it as a fallback.
      // We don't want this for mail composer because it uses
      // "spellchecker.dictionary" as a preference.
      Preferences::SetString("spellchecker.dictionary", aDictionary);
#ifdef DEBUG_DICT
      printf("***** Storing spellchecker.dictionary |%s|\n",
             NS_ConvertUTF16toUTF8(aDictionary).get());
#endif
    }
  }
  return mSpellChecker->SetCurrentDictionary(aDictionary);
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

  RefPtr<nsEditorSpellCheck> kungFuDeathGrip = this;

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

  // Try to get topmost document's document element for embedded mail editor.
  uint32_t flags = 0;
  mEditor->GetFlags(&flags);
  if (flags & nsIPlaintextEditor::eEditorMailMask) {
    nsCOMPtr<nsIDocument> ownerDoc = rootContent->OwnerDoc();
    NS_ENSURE_TRUE(ownerDoc, NS_ERROR_FAILURE);
    nsIDocument* parentDoc = ownerDoc->GetParentDocument();
    if (parentDoc) {
      rootContent = do_QueryInterface(parentDoc->GetDocumentElement());
    }
  }

  if (!rootContent) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DictionaryFetcher> fetcher =
    new DictionaryFetcher(this, aCallback, mDictionaryFetcherGroup);
  rootContent->GetLang(fetcher->mRootContentLang);
  nsCOMPtr<nsIDocument> doc = rootContent->GetUncomposedDoc();
  NS_ENSURE_STATE(doc);
  doc->GetContentLanguage(fetcher->mRootDocContentLang);

  rv = fetcher->Fetch(mEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Helper function that iterates over the list of dictionaries and sets the one
// that matches based on a given comparison type.
nsresult
nsEditorSpellCheck::TryDictionary(const nsAString& aDictName,
                                  nsTArray<nsString>& aDictList,
                                  enum dictCompare aCompareType)
{
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  for (uint32_t i = 0; i < aDictList.Length(); i++) {
    nsAutoString dictStr(aDictList.ElementAt(i));
    bool equals = false;
    switch (aCompareType) {
      case DICT_NORMAL_COMPARE:
        equals = aDictName.Equals(dictStr);
        break;
      case DICT_COMPARE_CASE_INSENSITIVE:
        equals = aDictName.Equals(dictStr, nsCaseInsensitiveStringComparator());
        break;
      case DICT_COMPARE_DASHMATCH:
        equals = nsStyleUtil::DashMatchCompare(dictStr, aDictName, nsCaseInsensitiveStringComparator());
        break;
    }
    if (equals) {
      rv = mSpellChecker->SetCurrentDictionary(dictStr);
#ifdef DEBUG_DICT
      if (NS_SUCCEEDED(rv))
        printf("***** Set |%s|.\n", NS_ConvertUTF16toUTF8(dictStr).get());
#endif
      // We always break here. We tried to set the dictionary to an existing
      // dictionary from the list. This must work, if it doesn't, there is
      // no point trying another one.
      break;
    }
  }
  return rv;
}

nsresult
nsEditorSpellCheck::DictionaryFetched(DictionaryFetcher* aFetcher)
{
  MOZ_ASSERT(aFetcher);
  RefPtr<nsEditorSpellCheck> kungFuDeathGrip = this;

  // Important: declare the holder after the callback caller so that the former
  // is destructed first so that it's not active when the callback is called.
  CallbackCaller callbackCaller(aFetcher->mCallback);
  UpdateDictionaryHolder holder(this);

  if (aFetcher->mGroup < mDictionaryFetcherGroup) {
    // SetCurrentDictionary was called after the fetch started.  Don't overwrite
    // that dictionary with the fetched one.
    return NS_OK;
  }

  /*
   * We try to derive the dictionary to use based on the following priorities:
   * 1) Content preference, so the language the user set for the site before.
   *    (Introduced in bug 678842 and corrected in bug 717433.)
   * 2) Language set by the website, or any other dictionary that partly
   *    matches that. (Introduced in bug 338427.)
   *    Eg. if the website is "en-GB", a user who only has "en-US" will get
   *    that. If the website is generic "en", the user will get one of the
   *    "en-*" installed, (almost) at random.
   *    However, we prefer what is stored in "spellchecker.dictionary",
   *    so if the user chose "en-AU" before, they will get "en-AU" on a plain
   *    "en" site. (Introduced in bug 682564.)
   * 3) The value of "spellchecker.dictionary" which reflects a previous
   *    language choice of the user (on another site).
   *    (This was the original behaviour before the aforementioned bugs
   *    landed).
   * 4) The user's locale.
   * 5) Use the current dictionary that is currently set.
   * 6) The content of the "LANG" environment variable (if set).
   * 7) The first spell check dictionary installed.
   */

  // Get the language from the element or its closest parent according to:
  // https://html.spec.whatwg.org/#attr-lang
  // This is used in SetCurrentDictionary.
  mPreferredLang.Assign(aFetcher->mRootContentLang);
#ifdef DEBUG_DICT
  printf("***** mPreferredLang (element) |%s|\n",
         NS_ConvertUTF16toUTF8(mPreferredLang).get());
#endif

  // If no luck, try the "Content-Language" header.
  if (mPreferredLang.IsEmpty()) {
    mPreferredLang.Assign(aFetcher->mRootDocContentLang);
#ifdef DEBUG_DICT
    printf("***** mPreferredLang (content-language) |%s|\n",
           NS_ConvertUTF16toUTF8(mPreferredLang).get());
#endif
  }

  // Auxiliary status.
  nsresult rv2;

  // We obtain a list of available dictionaries.
  nsTArray<nsString> dictList;
  rv2 = mSpellChecker->GetDictionaryList(&dictList);
  NS_ENSURE_SUCCESS(rv2, rv2);

  // Priority 1:
  // If we successfully fetched a dictionary from content prefs, do not go
  // further. Use this exact dictionary.
  // Don't use content preferences for editor with eEditorMailMask flag.
  nsAutoString dictName;
  uint32_t flags;
  mEditor->GetFlags(&flags);
  if (!(flags & nsIPlaintextEditor::eEditorMailMask)) {
    dictName.Assign(aFetcher->mDictionary);
    if (!dictName.IsEmpty()) {
      if (NS_SUCCEEDED(TryDictionary(dictName, dictList, DICT_NORMAL_COMPARE))) {
#ifdef DEBUG_DICT
        printf("***** Assigned from content preferences |%s|\n",
               NS_ConvertUTF16toUTF8(dictName).get());
#endif

        // We take an early exit here, so let's not forget to clear the word
        // list.
        DeleteSuggestedWordList();
        return NS_OK;
      }
      // May be dictionary was uninstalled ?
      // Clear the content preference and continue.
      ClearCurrentDictionary(mEditor);
    }
  }

  // Priority 2:
  // After checking the content preferences, we use the language of the element
  // or document.
  dictName.Assign(mPreferredLang);
#ifdef DEBUG_DICT
  printf("***** Assigned from element/doc |%s|\n",
         NS_ConvertUTF16toUTF8(dictName).get());
#endif

  // Get the preference value.
  nsAutoString preferredDict;
  preferredDict = Preferences::GetLocalizedString("spellchecker.dictionary");

  // The following will be driven by this status. Once we were able to set a
  // dictionary successfully, we're done. So we start with a "failed" status.
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  if (!dictName.IsEmpty()) {
    // RFC 5646 explicitly states that matches should be case-insensitive.
    rv = TryDictionary (dictName, dictList, DICT_COMPARE_CASE_INSENSITIVE);

    if (NS_FAILED(rv)) {
#ifdef DEBUG_DICT
      printf("***** Setting of |%s| failed (or it wasn't available)\n",
             NS_ConvertUTF16toUTF8(dictName).get());
#endif

      // Required dictionary was not available. Try to get a dictionary
      // matching at least language part of dictName.
      nsAutoString langCode;
      int32_t dashIdx = dictName.FindChar('-');
      if (dashIdx != -1) {
        langCode.Assign(Substring(dictName, 0, dashIdx));
      } else {
        langCode.Assign(dictName);
      }

      // Try dictionary.spellchecker preference, if it starts with langCode,
      // so we don't just get any random dictionary matching the language.
      if (!preferredDict.IsEmpty() &&
          nsStyleUtil::DashMatchCompare(preferredDict, langCode, nsDefaultStringComparator())) {
#ifdef DEBUG_DICT
        printf("***** Trying preference value |%s| since it matches language code\n",
               NS_ConvertUTF16toUTF8(preferredDict).get());
#endif
        rv = TryDictionary (preferredDict, dictList,
                            DICT_COMPARE_CASE_INSENSITIVE);
      }

      if (NS_FAILED(rv)) {
        // Use any dictionary with the required language.
#ifdef DEBUG_DICT
        printf("***** Trying to find match for language code |%s|\n",
               NS_ConvertUTF16toUTF8(langCode).get());
#endif
        rv = TryDictionary (langCode, dictList, DICT_COMPARE_DASHMATCH);
      }
    }
  }

  // Priority 3:
  // If the document didn't supply a dictionary or the setting failed,
  // try the user preference next.
  if (NS_FAILED(rv)) {
    if (!preferredDict.IsEmpty()) {
#ifdef DEBUG_DICT
      printf("***** Trying preference value |%s|\n",
             NS_ConvertUTF16toUTF8(preferredDict).get());
#endif
      rv = TryDictionary (preferredDict, dictList, DICT_NORMAL_COMPARE);
    }
  }

  // Priority 4:
  // As next fallback, try the current locale.
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIXULChromeRegistry> packageRegistry =
      mozilla::services::GetXULChromeRegistryService();

    if (packageRegistry) {
      nsAutoCString utf8DictName;
      rv2 = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global"),
                                               false, utf8DictName);
      dictName.Assign(EmptyString());
      AppendUTF8toUTF16(utf8DictName, dictName);
#ifdef DEBUG_DICT
      printf("***** Trying locale |%s|\n",
             NS_ConvertUTF16toUTF8(dictName).get());
#endif
      rv = TryDictionary (dictName, dictList, DICT_COMPARE_CASE_INSENSITIVE);
    }
  }

  if (NS_FAILED(rv)) {
  // Still no success.

  // Priority 5:
  // If we have a current dictionary, don't try anything else.
    nsAutoString currentDictionary;
    rv2 = GetCurrentDictionary(currentDictionary);
#ifdef DEBUG_DICT
    if (NS_SUCCEEDED(rv2)) {
        printf("***** Retrieved current dict |%s|\n",
               NS_ConvertUTF16toUTF8(currentDictionary).get());
    }
#endif

    if (NS_FAILED(rv2) || currentDictionary.IsEmpty()) {
      // Priority 6:
      // Try to get current dictionary from environment variable LANG.
      // LANG = language[_territory][.charset]
      char* env_lang = getenv("LANG");
      if (env_lang) {
        nsString lang = NS_ConvertUTF8toUTF16(env_lang);
        // Strip trailing charset, if there is any.
        int32_t dot_pos = lang.FindChar('.');
        if (dot_pos != -1) {
          lang = Substring(lang, 0, dot_pos);
        }

        int32_t underScore = lang.FindChar('_');
        if (underScore != -1) {
          lang.Replace(underScore, 1, '-');
#ifdef DEBUG_DICT
          printf("***** Trying LANG from environment |%s|\n",
                 NS_ConvertUTF16toUTF8(lang).get());
#endif
          nsAutoString lang2;
          lang2.Assign(lang);
          rv = TryDictionary(lang2, dictList, DICT_COMPARE_CASE_INSENSITIVE);
        }
      }

      // Priority 7:
      // If it does not work, pick the first one.
      if (NS_FAILED(rv) && !dictList.IsEmpty()) {
        nsAutoString firstInList;
        firstInList.Assign(dictList[0]);
        rv = TryDictionary(firstInList, dictList, DICT_NORMAL_COMPARE);
#ifdef DEBUG_DICT
        printf("***** Trying first of list |%s|\n",
               NS_ConvertUTF16toUTF8(dictList[0]).get());
        if (NS_SUCCEEDED(rv)) {
          printf ("***** Setting worked.\n");
        }
#endif
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorSpellCheck.h"

#include "mozilla/Attributes.h"   // for final
#include "mozilla/EditorBase.h"   // for EditorBase
#include "mozilla/HTMLEditor.h"   // for HTMLEditor
#include "mozilla/dom/Element.h"  // for Element
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "mozilla/intl/LocaleService.h"    // for retrieving app locale
#include "mozilla/mozalloc.h"              // for operator delete, etc
#include "mozilla/mozSpellChecker.h"       // for mozSpellChecker
#include "mozilla/Preferences.h"           // for Preferences
#include "mozilla/TextServicesDocument.h"  // for TextServicesDocument
#include "nsAString.h"                     // for nsAString::IsEmpty, etc
#include "nsComponentManagerUtils.h"       // for do_CreateInstance
#include "nsDebug.h"                       // for NS_ENSURE_TRUE, etc
#include "nsDependentSubstring.h"          // for Substring
#include "nsError.h"                       // for NS_ERROR_NOT_INITIALIZED, etc
#include "nsIContent.h"                    // for nsIContent
#include "nsIContentPrefService2.h"        // for nsIContentPrefService2, etc
#include "mozilla/dom/Document.h"          // for Document
#include "nsIEditor.h"                     // for nsIEditor
#include "nsILoadContext.h"
#include "nsISupportsBase.h"   // for nsISupports
#include "nsISupportsUtils.h"  // for NS_ADDREF
#include "nsIURI.h"            // for nsIURI
#include "nsThreadUtils.h"     // for GetMainThreadSerialEventTarget
#include "nsVariant.h"         // for nsIWritableVariant, etc
#include "nsLiteralString.h"   // for NS_LITERAL_STRING, etc
#include "nsMemory.h"          // for nsMemory
#include "nsRange.h"
#include "nsReadableUtils.h"        // for ToNewUnicode, EmptyString, etc
#include "nsServiceManagerUtils.h"  // for do_GetService
#include "nsString.h"               // for nsAutoString, nsString, etc
#include "nsStringFwd.h"            // for nsAFlatString
#include "nsStyleUtil.h"            // for nsStyleUtil
#include "nsXULAppAPI.h"            // for XRE_GetProcessType

namespace mozilla {

using namespace dom;
using intl::LocaleService;

class UpdateDictionaryHolder {
 private:
  EditorSpellCheck* mSpellCheck;

 public:
  explicit UpdateDictionaryHolder(EditorSpellCheck* esc) : mSpellCheck(esc) {
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
static nsIURI* GetDocumentURI(EditorBase* aEditor) {
  MOZ_ASSERT(aEditor);

  Document* doc = aEditor->AsEditorBase()->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }

  return doc->GetDocumentURI();
}

static nsILoadContext* GetLoadContext(nsIEditor* aEditor) {
  Document* doc = aEditor->AsEditorBase()->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }

  return doc->GetLoadContext();
}

/**
 * Fetches the dictionary stored in content prefs and maintains state during the
 * fetch, which is asynchronous.
 */
class DictionaryFetcher final : public nsIContentPrefCallback2 {
 public:
  NS_DECL_ISUPPORTS

  DictionaryFetcher(EditorSpellCheck* aSpellCheck,
                    nsIEditorSpellCheckCallback* aCallback, uint32_t aGroup)
      : mCallback(aCallback), mGroup(aGroup), mSpellCheck(aSpellCheck) {}

  NS_IMETHOD Fetch(nsIEditor* aEditor);

  NS_IMETHOD HandleResult(nsIContentPref* aPref) override {
    nsCOMPtr<nsIVariant> value;
    nsresult rv = aPref->GetValue(getter_AddRefs(value));
    NS_ENSURE_SUCCESS(rv, rv);
    value->GetAsAString(mDictionary);
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t reason) override {
    mSpellCheck->DictionaryFetched(this);
    return NS_OK;
  }

  NS_IMETHOD HandleError(nsresult error) override { return NS_OK; }

  nsCOMPtr<nsIEditorSpellCheckCallback> mCallback;
  uint32_t mGroup;
  nsString mRootContentLang;
  nsString mRootDocContentLang;
  nsString mDictionary;

 private:
  ~DictionaryFetcher() {}

  RefPtr<EditorSpellCheck> mSpellCheck;
};

NS_IMPL_ISUPPORTS(DictionaryFetcher, nsIContentPrefCallback2)

class ContentPrefInitializerRunnable final : public Runnable {
 public:
  ContentPrefInitializerRunnable(nsIEditor* aEditor,
                                 nsIContentPrefCallback2* aCallback)
      : Runnable("ContentPrefInitializerRunnable"),
        mEditorBase(aEditor->AsEditorBase()),
        mCallback(aCallback) {}

  NS_IMETHOD Run() override {
    if (mEditorBase->Destroyed()) {
      mCallback->HandleError(NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }

    nsCOMPtr<nsIContentPrefService2> contentPrefService =
        do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
    if (NS_WARN_IF(!contentPrefService)) {
      mCallback->HandleError(NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> docUri = GetDocumentURI(mEditorBase);
    if (NS_WARN_IF(!docUri)) {
      mCallback->HandleError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    nsAutoCString docUriSpec;
    nsresult rv = docUri->GetSpec(docUriSpec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCallback->HandleError(rv);
      return NS_OK;
    }

    rv = contentPrefService->GetByDomainAndName(
        NS_ConvertUTF8toUTF16(docUriSpec), CPS_PREF_NAME,
        GetLoadContext(mEditorBase), mCallback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCallback->HandleError(rv);
      return NS_OK;
    }
    return NS_OK;
  }

 private:
  RefPtr<EditorBase> mEditorBase;
  nsCOMPtr<nsIContentPrefCallback2> mCallback;
};

NS_IMETHODIMP
DictionaryFetcher::Fetch(nsIEditor* aEditor) {
  NS_ENSURE_ARG_POINTER(aEditor);

  nsCOMPtr<nsIRunnable> runnable =
      new ContentPrefInitializerRunnable(aEditor, this);
  NS_DispatchToCurrentThreadQueue(runnable.forget(), 1000,
                                  EventQueuePriority::Idle);

  return NS_OK;
}

/**
 * Stores the current dictionary for aEditor's document URL.
 */
static nsresult StoreCurrentDictionary(EditorBase* aEditorBase,
                                       const nsAString& aDictionary) {
  NS_ENSURE_ARG_POINTER(aEditorBase);

  nsresult rv;

  nsCOMPtr<nsIURI> docUri = GetDocumentURI(aEditorBase);
  if (NS_WARN_IF(!docUri)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString docUriSpec;
  rv = docUri->GetSpec(docUriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsVariant> prefValue = new nsVariant();
  prefValue->SetAsAString(aDictionary);

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_NOT_INITIALIZED);

  return contentPrefService->Set(NS_ConvertUTF8toUTF16(docUriSpec),
                                 CPS_PREF_NAME, prefValue,
                                 GetLoadContext(aEditorBase), nullptr);
}

/**
 * Forgets the current dictionary stored for aEditor's document URL.
 */
static nsresult ClearCurrentDictionary(EditorBase* aEditorBase) {
  NS_ENSURE_ARG_POINTER(aEditorBase);

  nsresult rv;

  nsCOMPtr<nsIURI> docUri = GetDocumentURI(aEditorBase);
  if (NS_WARN_IF(!docUri)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString docUriSpec;
  rv = docUri->GetSpec(docUriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(contentPrefService, NS_ERROR_NOT_INITIALIZED);

  return contentPrefService->RemoveByDomainAndName(
      NS_ConvertUTF8toUTF16(docUriSpec), CPS_PREF_NAME,
      GetLoadContext(aEditorBase), nullptr);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(EditorSpellCheck)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EditorSpellCheck)

NS_INTERFACE_MAP_BEGIN(EditorSpellCheck)
  NS_INTERFACE_MAP_ENTRY(nsIEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(EditorSpellCheck)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(EditorSpellCheck, mEditor, mSpellChecker)

EditorSpellCheck::EditorSpellCheck()
    : mTxtSrvFilterType(0),
      mSuggestedWordIndex(0),
      mDictionaryIndex(0),
      mDictionaryFetcherGroup(0),
      mUpdateDictionaryRunning(false) {}

EditorSpellCheck::~EditorSpellCheck() {
  // Make sure we blow the spellchecker away, just in
  // case it hasn't been destroyed already.
  mSpellChecker = nullptr;
}

mozSpellChecker* EditorSpellCheck::GetSpellChecker() { return mSpellChecker; }

// The problem is that if the spell checker does not exist, we can not tell
// which dictionaries are installed. This function works around the problem,
// allowing callers to ask if we can spell check without actually doing so (and
// enabling or disabling UI as necessary). This just creates a spellcheck
// object if needed and asks it for the dictionary list.
NS_IMETHODIMP
EditorSpellCheck::CanSpellCheck(bool* aCanSpellCheck) {
  RefPtr<mozSpellChecker> spellChecker = mSpellChecker;
  if (!spellChecker) {
    spellChecker = mozSpellChecker::Create();
    MOZ_ASSERT(spellChecker);
  }
  nsTArray<nsString> dictList;
  nsresult rv = spellChecker->GetDictionaryList(&dictList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aCanSpellCheck = !dictList.IsEmpty();
  return NS_OK;
}

// Instances of this class can be used as either runnables or RAII helpers.
class CallbackCaller final : public Runnable {
 public:
  explicit CallbackCaller(nsIEditorSpellCheckCallback* aCallback)
      : mozilla::Runnable("CallbackCaller"), mCallback(aCallback) {}

  ~CallbackCaller() { Run(); }

  NS_IMETHOD Run() override {
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
EditorSpellCheck::InitSpellChecker(nsIEditor* aEditor,
                                   bool aEnableSelectionChecking,
                                   nsIEditorSpellCheckCallback* aCallback) {
  NS_ENSURE_TRUE(aEditor, NS_ERROR_NULL_POINTER);
  mEditor = aEditor->AsEditorBase();

  RefPtr<Document> doc = mEditor->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  // We can spell check with any editor type
  RefPtr<TextServicesDocument> textServicesDocument =
      new TextServicesDocument();
  textServicesDocument->SetFilterType(mTxtSrvFilterType);

  // EditorBase::AddEditActionListener() needs to access mSpellChecker and
  // mSpellChecker->GetTextServicesDocument().  Therefore, we need to
  // initialize them before calling TextServicesDocument::InitWithEditor()
  // since it calls EditorBase::AddEditActionListener().
  mSpellChecker = mozSpellChecker::Create();
  MOZ_ASSERT(mSpellChecker);
  rv = mSpellChecker->SetDocument(textServicesDocument, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Pass the editor to the text services document
  rv = textServicesDocument->InitWithEditor(aEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aEnableSelectionChecking) {
    // Find out if the section is collapsed or not.
    // If it isn't, we want to spellcheck just the selection.

    RefPtr<Selection> selection;
    aEditor->GetSelection(getter_AddRefs(selection));
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }

    if (selection->RangeCount()) {
      RefPtr<const nsRange> range = selection->GetRangeAt(0);
      NS_ENSURE_STATE(range);

      if (!range->Collapsed()) {
        // We don't want to touch the range in the selection,
        // so create a new copy of it.
        RefPtr<StaticRange> staticRange =
            StaticRange::Create(range, IgnoreErrors());
        if (NS_WARN_IF(!staticRange)) {
          return NS_ERROR_FAILURE;
        }

        // Make sure the new range spans complete words.
        rv = textServicesDocument->ExpandRangeToWordBoundaries(staticRange);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        // Now tell the text services that you only want
        // to iterate over the text in this range.
        rv = textServicesDocument->SetExtent(staticRange);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }
  // do not fail if UpdateCurrentDictionary fails because this method may
  // succeed later.
  rv = UpdateCurrentDictionary(aCallback);
  if (NS_FAILED(rv) && aCallback) {
    // However, if it does fail, we still need to call the callback since we
    // discard the failure.  Do it asynchronously so that the caller is always
    // guaranteed async behavior.
    RefPtr<CallbackCaller> caller = new CallbackCaller(aCallback);
    rv = doc->Dispatch(TaskCategory::Other, caller.forget());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorSpellCheck::GetNextMisspelledWord(nsAString& aNextMisspelledWord) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  DeleteSuggestedWordList();
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  RefPtr<mozSpellChecker> spellChecker(mSpellChecker);
  return spellChecker->NextMisspelledWord(aNextMisspelledWord,
                                          mSuggestedWordList);
}

NS_IMETHODIMP
EditorSpellCheck::GetSuggestedWord(nsAString& aSuggestedWord) {
  // XXX This is buggy if mSuggestedWordList.Length() is over INT32_MAX.
  if (mSuggestedWordIndex < static_cast<int32_t>(mSuggestedWordList.Length())) {
    aSuggestedWord = mSuggestedWordList[mSuggestedWordIndex];
    mSuggestedWordIndex++;
  } else {
    // A blank string signals that there are no more strings
    aSuggestedWord.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
EditorSpellCheck::CheckCurrentWord(const nsAString& aSuggestedWord,
                                   bool* aIsMisspelled) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  DeleteSuggestedWordList();
  return mSpellChecker->CheckWord(aSuggestedWord, aIsMisspelled,
                                  &mSuggestedWordList);
}

RefPtr<CheckWordPromise> EditorSpellCheck::CheckCurrentWordsNoSuggest(
    const nsTArray<nsString>& aSuggestedWords) {
  if (NS_WARN_IF(!mSpellChecker)) {
    return CheckWordPromise::CreateAndReject(NS_ERROR_NOT_INITIALIZED,
                                             __func__);
  }

  return mSpellChecker->CheckWords(aSuggestedWords);
}

NS_IMETHODIMP
EditorSpellCheck::ReplaceWord(const nsAString& aMisspelledWord,
                              const nsAString& aReplaceWord,
                              bool aAllOccurrences) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  RefPtr<mozSpellChecker> spellChecker(mSpellChecker);
  return spellChecker->Replace(aMisspelledWord, aReplaceWord, aAllOccurrences);
}

NS_IMETHODIMP
EditorSpellCheck::IgnoreWordAllOccurrences(const nsAString& aWord) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->IgnoreAll(aWord);
}

NS_IMETHODIMP
EditorSpellCheck::GetPersonalDictionary() {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  // We can spell check with any editor type
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  return mSpellChecker->GetPersonalDictionary(&mDictionaryList);
}

NS_IMETHODIMP
EditorSpellCheck::GetPersonalDictionaryWord(nsAString& aDictionaryWord) {
  // XXX This is buggy if mDictionaryList.Length() is over INT32_MAX.
  if (mDictionaryIndex < static_cast<int32_t>(mDictionaryList.Length())) {
    aDictionaryWord = mDictionaryList[mDictionaryIndex];
    mDictionaryIndex++;
  } else {
    // A blank string signals that there are no more strings
    aDictionaryWord.Truncate();
  }

  return NS_OK;
}

NS_IMETHODIMP
EditorSpellCheck::AddWordToDictionary(const nsAString& aWord) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->AddWordToPersonalDictionary(aWord);
}

NS_IMETHODIMP
EditorSpellCheck::RemoveWordFromDictionary(const nsAString& aWord) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->RemoveWordFromPersonalDictionary(aWord);
}

NS_IMETHODIMP
EditorSpellCheck::GetDictionaryList(nsTArray<nsString>& aList) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->GetDictionaryList(&aList);
}

NS_IMETHODIMP
EditorSpellCheck::GetCurrentDictionary(nsAString& aDictionary) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->GetCurrentDictionary(aDictionary);
}

NS_IMETHODIMP
EditorSpellCheck::SetCurrentDictionary(const nsAString& aDictionary) {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  RefPtr<EditorSpellCheck> kungFuDeathGrip = this;

  // The purpose of mUpdateDictionaryRunning is to avoid doing all of this if
  // UpdateCurrentDictionary's helper method DictionaryFetched, which calls us,
  // is on the stack. In other words: Only do this, if the user manually
  // selected a dictionary to use.
  if (!mUpdateDictionaryRunning) {
    // Ignore pending dictionary fetchers by increasing this number.
    mDictionaryFetcherGroup++;

    uint32_t flags = 0;
    mEditor->GetFlags(&flags);
    if (!(flags & nsIEditor::eEditorMailMask)) {
      if (!aDictionary.IsEmpty() &&
          (mPreferredLang.IsEmpty() ||
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
        // document, we consider content pref has been canceled, and we clear
        // it.
        ClearCurrentDictionary(mEditor);
#ifdef DEBUG_DICT
        printf("***** Clearing content preferences for |%s|\n",
               NS_ConvertUTF16toUTF8(aDictionary).get());
#endif
      }

      // Also store it in as a preference, so we can use it as a fallback.
      // We don't want this for mail composer because it uses
      // "spellchecker.dictionary" as a preference.
      //
      // XXX: Prefs can only be set in the parent process, so this condition is
      // necessary to stop libpref from throwing errors. But this should
      // probably be handled in a better way.
      if (XRE_IsParentProcess()) {
        Preferences::SetString("spellchecker.dictionary", aDictionary);
#ifdef DEBUG_DICT
        printf("***** Possibly storing spellchecker.dictionary |%s|\n",
               NS_ConvertUTF16toUTF8(aDictionary).get());
#endif
      }
    }
  }
  return mSpellChecker->SetCurrentDictionary(aDictionary);
}

NS_IMETHODIMP
EditorSpellCheck::UninitSpellChecker() {
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  // Cleanup - kill the spell checker
  DeleteSuggestedWordList();
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  mDictionaryFetcherGroup++;
  mSpellChecker = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
EditorSpellCheck::SetFilterType(uint32_t aFilterType) {
  mTxtSrvFilterType = aFilterType;
  return NS_OK;
}

nsresult EditorSpellCheck::DeleteSuggestedWordList() {
  mSuggestedWordList.Clear();
  mSuggestedWordIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP
EditorSpellCheck::UpdateCurrentDictionary(
    nsIEditorSpellCheckCallback* aCallback) {
  if (NS_WARN_IF(!mSpellChecker)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv;

  RefPtr<EditorSpellCheck> kungFuDeathGrip = this;
  uint32_t flags = 0;
  mEditor->GetFlags(&flags);

  // Get language with html5 algorithm
  nsCOMPtr<nsIContent> rootContent;
  HTMLEditor* htmlEditor = mEditor->AsHTMLEditor();
  if (htmlEditor) {
    if (flags & nsIEditor::eEditorMailMask) {
      // Always determine the root content for a mail editor,
      // even if not focused, to enable further processing below.
      rootContent = htmlEditor->GetActiveEditingHost();
    } else {
      rootContent = htmlEditor->GetFocusedContent();
    }
  } else {
    rootContent = mEditor->GetRoot();
  }

  if (!rootContent) {
    return NS_ERROR_FAILURE;
  }

  // Try to get topmost document's document element for embedded mail editor.
  if (flags & nsIEditor::eEditorMailMask) {
    RefPtr<Document> ownerDoc = rootContent->OwnerDoc();
    Document* parentDoc = ownerDoc->GetInProcessParentDocument();
    if (parentDoc) {
      rootContent = parentDoc->GetDocumentElement();
      if (!rootContent) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  RefPtr<DictionaryFetcher> fetcher =
      new DictionaryFetcher(this, aCallback, mDictionaryFetcherGroup);
  rootContent->GetLang(fetcher->mRootContentLang);
  RefPtr<Document> doc = rootContent->GetComposedDoc();
  NS_ENSURE_STATE(doc);
  doc->GetContentLanguage(fetcher->mRootDocContentLang);

  rv = fetcher->Fetch(mEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Helper function that iterates over the list of dictionaries and sets the one
// that matches based on a given comparison type.
void EditorSpellCheck::BuildDictionaryList(const nsAString& aDictName,
                                           const nsTArray<nsString>& aDictList,
                                           enum dictCompare aCompareType,
                                           nsTArray<nsString>& aOutList) {
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
        equals = nsStyleUtil::DashMatchCompare(
            dictStr, aDictName, nsCaseInsensitiveStringComparator());
        break;
    }
    if (equals) {
      aOutList.AppendElement(dictStr);
#ifdef DEBUG_DICT
      if (NS_SUCCEEDED(rv)) {
        printf("***** Trying |%s|.\n", NS_ConvertUTF16toUTF8(dictStr).get());
      }
#endif
      // We always break here. We tried to set the dictionary to an existing
      // dictionary from the list. This must work, if it doesn't, there is
      // no point trying another one.
      return;
    }
  }
}

nsresult EditorSpellCheck::DictionaryFetched(DictionaryFetcher* aFetcher) {
  MOZ_ASSERT(aFetcher);
  RefPtr<EditorSpellCheck> kungFuDeathGrip = this;

  BeginUpdateDictionary();

  if (aFetcher->mGroup < mDictionaryFetcherGroup) {
    // SetCurrentDictionary was called after the fetch started.  Don't overwrite
    // that dictionary with the fetched one.
    EndUpdateDictionary();
    if (aFetcher->mCallback) {
      aFetcher->mCallback->EditorSpellCheckDone();
    }
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

  // We obtain a list of available dictionaries.
  AutoTArray<nsString, 8> dictList;
  nsresult rv = mSpellChecker->GetDictionaryList(&dictList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    EndUpdateDictionary();
    if (aFetcher->mCallback) {
      aFetcher->mCallback->EditorSpellCheckDone();
    }
    return rv;
  }

  // Priority 1:
  // If we successfully fetched a dictionary from content prefs, do not go
  // further. Use this exact dictionary.
  // Don't use content preferences for editor with eEditorMailMask flag.
  nsAutoString dictName;
  uint32_t flags;
  mEditor->GetFlags(&flags);
  if (!(flags & nsIEditor::eEditorMailMask)) {
    dictName.Assign(aFetcher->mDictionary);
    if (!dictName.IsEmpty()) {
      AutoTArray<nsString, 1> tryDictList;
      BuildDictionaryList(dictName, dictList, DICT_NORMAL_COMPARE, tryDictList);

      RefPtr<EditorSpellCheck> self = this;
      RefPtr<DictionaryFetcher> fetcher = aFetcher;
      mSpellChecker->SetCurrentDictionaryFromList(tryDictList)
          ->Then(
              GetMainThreadSerialEventTarget(), __func__,
              [self, fetcher]() {
#ifdef DEBUG_DICT
                printf("***** Assigned from content preferences |%s|\n",
                       NS_ConvertUTF16toUTF8(dictName).get());
#endif
                // We take an early exit here, so let's not forget to clear
                // the word list.
                self->DeleteSuggestedWordList();

                self->EndUpdateDictionary();
                if (fetcher->mCallback) {
                  fetcher->mCallback->EditorSpellCheckDone();
                }
              },
              [self, fetcher](nsresult aError) {
                if (aError == NS_ERROR_ABORT) {
                  return;
                }
                // May be dictionary was uninstalled ?
                // Clear the content preference and continue.
                ClearCurrentDictionary(self->mEditor);

                // Priority 2 or later will handled by the following
                self->SetFallbackDictionary(fetcher);
              });
      return NS_OK;
    }
  }
  SetFallbackDictionary(aFetcher);
  return NS_OK;
}

void EditorSpellCheck::SetFallbackDictionary(DictionaryFetcher* aFetcher) {
  MOZ_ASSERT(mUpdateDictionaryRunning);

  AutoTArray<nsString, 6> tryDictList;

  // We obtain a list of available dictionaries.
  AutoTArray<nsString, 8> dictList;
  nsresult rv = mSpellChecker->GetDictionaryList(&dictList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    EndUpdateDictionary();
    if (aFetcher->mCallback) {
      aFetcher->mCallback->EditorSpellCheckDone();
    }
    return;
  }

  // Priority 2:
  // After checking the content preferences, we use the language of the element
  // or document.
  nsAutoString dictName(mPreferredLang);
#ifdef DEBUG_DICT
  printf("***** Assigned from element/doc |%s|\n",
         NS_ConvertUTF16toUTF8(dictName).get());
#endif

  // Get the preference value.
  nsAutoString preferredDict;
  Preferences::GetLocalizedString("spellchecker.dictionary", preferredDict);

  if (!dictName.IsEmpty()) {
    // RFC 5646 explicitly states that matches should be case-insensitive.
    BuildDictionaryList(dictName, dictList, DICT_COMPARE_CASE_INSENSITIVE,
                        tryDictList);

#ifdef DEBUG_DICT
    printf("***** Trying from element/doc |%s| \n",
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
        nsStyleUtil::DashMatchCompare(preferredDict, langCode,
                                      nsDefaultStringComparator())) {
#ifdef DEBUG_DICT
      printf(
          "***** Trying preference value |%s| since it matches language code\n",
          NS_ConvertUTF16toUTF8(preferredDict).get());
#endif
      BuildDictionaryList(preferredDict, dictList,
                          DICT_COMPARE_CASE_INSENSITIVE, tryDictList);
    }

    // Use any dictionary with the required language.
#ifdef DEBUG_DICT
    printf("***** Trying to find match for language code |%s|\n",
           NS_ConvertUTF16toUTF8(langCode).get());
#endif
    BuildDictionaryList(langCode, dictList, DICT_COMPARE_DASHMATCH,
                        tryDictList);
  }

  // Priority 3:
  // If the document didn't supply a dictionary or the setting failed,
  // try the user preference next.
  if (!preferredDict.IsEmpty()) {
#ifdef DEBUG_DICT
    printf("***** Trying preference value |%s|\n",
           NS_ConvertUTF16toUTF8(preferredDict).get());
#endif
    BuildDictionaryList(preferredDict, dictList, DICT_NORMAL_COMPARE,
                        tryDictList);
  }

  // Priority 4:
  // As next fallback, try the current locale.
  nsAutoCString utf8DictName;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(utf8DictName);

  CopyUTF8toUTF16(utf8DictName, dictName);
#ifdef DEBUG_DICT
  printf("***** Trying locale |%s|\n", NS_ConvertUTF16toUTF8(dictName).get());
#endif
  BuildDictionaryList(dictName, dictList, DICT_COMPARE_CASE_INSENSITIVE,
                      tryDictList);

  // Priority 5:
  // If we have a current dictionary and we don't have no item in try list,
  // don't try anything else.
  nsAutoString currentDictionary;
  GetCurrentDictionary(currentDictionary);
  if (!currentDictionary.IsEmpty() && tryDictList.IsEmpty()) {
#ifdef DEBUG_DICT
    printf("***** Retrieved current dict |%s|\n",
           NS_ConvertUTF16toUTF8(currentDictionary).get());
#endif
    EndUpdateDictionary();
    if (aFetcher->mCallback) {
      aFetcher->mCallback->EditorSpellCheckDone();
    }
    return;
  }

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
      BuildDictionaryList(lang, dictList, DICT_COMPARE_CASE_INSENSITIVE,
                          tryDictList);
    }
  }

  // Priority 7:
  // If it does not work, pick the first one.
  if (!dictList.IsEmpty()) {
    BuildDictionaryList(dictList[0], dictList, DICT_NORMAL_COMPARE,
                        tryDictList);
#ifdef DEBUG_DICT
    printf("***** Trying first of list |%s|\n",
           NS_ConvertUTF16toUTF8(dictList[0]).get());
#endif
  }

  RefPtr<EditorSpellCheck> self = this;
  RefPtr<DictionaryFetcher> fetcher = aFetcher;
  mSpellChecker->SetCurrentDictionaryFromList(tryDictList)
      ->Then(GetMainThreadSerialEventTarget(), __func__, [self, fetcher]() {
        // If an error was thrown while setting the dictionary, just
        // fail silently so that the spellchecker dialog is allowed to come
        // up. The user can manually reset the language to their choice on
        // the dialog if it is wrong.
        self->DeleteSuggestedWordList();
        self->EndUpdateDictionary();
        if (fetcher->mCallback) {
          fetcher->mCallback->EditorSpellCheckDone();
        }
      });
}

}  // namespace mozilla

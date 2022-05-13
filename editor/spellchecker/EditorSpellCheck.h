/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorSpellCheck_h
#define mozilla_EditorSpellCheck_h

#include "mozilla/mozSpellChecker.h"  // for mozilla::CheckWordPromise
#include "nsCOMPtr.h"                 // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIEditorSpellCheck.h"  // for NS_DECL_NSIEDITORSPELLCHECK, etc
#include "nsISupportsImpl.h"
#include "nsString.h"  // for nsString
#include "nsTArray.h"  // for nsTArray
#include "nscore.h"    // for nsresult

class mozSpellChecker;
class nsIEditor;

namespace mozilla {

class DictionaryFetcher;
class EditorBase;

enum dictCompare {
  DICT_NORMAL_COMPARE,
  DICT_COMPARE_CASE_INSENSITIVE,
  DICT_COMPARE_DASHMATCH
};

class EditorSpellCheck final : public nsIEditorSpellCheck {
  friend class DictionaryFetcher;

 public:
  EditorSpellCheck();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(EditorSpellCheck)

  /* Declare all methods in the nsIEditorSpellCheck interface */
  NS_DECL_NSIEDITORSPELLCHECK

  mozSpellChecker* GetSpellChecker();

  /**
   * Like CheckCurrentWord, checks the word you give it, returning true via
   * promise if it's misspelled.
   * This is faster than CheckCurrentWord because it does not compute
   * any suggestions.
   *
   * Watch out: this does not clear any suggestions left over from previous
   * calls to CheckCurrentWord, so there may be suggestions, but they will be
   * invalid.
   */
  RefPtr<mozilla::CheckWordPromise> CheckCurrentWordsNoSuggest(
      const nsTArray<nsString>& aSuggestedWords);

 protected:
  virtual ~EditorSpellCheck();

  RefPtr<mozSpellChecker> mSpellChecker;
  RefPtr<EditorBase> mEditor;

  nsTArray<nsString> mSuggestedWordList;

  // these are the words in the current personal dictionary,
  // GetPersonalDictionary must be called to load them.
  nsTArray<nsString> mDictionaryList;

  nsTArray<nsCString> mPreferredLangs;

  uint32_t mTxtSrvFilterType;
  int32_t mSuggestedWordIndex;
  int32_t mDictionaryIndex;
  uint32_t mDictionaryFetcherGroup;

  bool mUpdateDictionaryRunning;

  nsresult DeleteSuggestedWordList();

  bool BuildDictionaryList(const nsACString& aDictName,
                           const nsTArray<nsCString>& aDictList,
                           enum dictCompare aCompareType,
                           nsTArray<nsCString>& aOutList);

  nsresult DictionaryFetched(DictionaryFetcher* aFetchState);

  void SetDictionarySucceeded(DictionaryFetcher* aFetcher);
  void SetFallbackDictionary(DictionaryFetcher* aFetcher);

 public:
  void BeginUpdateDictionary() { mUpdateDictionaryRunning = true; }
  void EndUpdateDictionary() { mUpdateDictionaryRunning = false; }
};

}  // namespace mozilla

#endif  // mozilla_EditorSpellCheck_h

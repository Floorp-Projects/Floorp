/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorSpellCheck_h
#define mozilla_EditorSpellCheck_h


#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIEditorSpellCheck.h"        // for NS_DECL_NSIEDITORSPELLCHECK, etc
#include "nsISupportsImpl.h"
#include "nsString.h"                   // for nsString
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsresult

class mozSpellChecker;
class nsIEditor;
class nsISpellChecker;
class nsITextServicesFilter;

#define NS_EDITORSPELLCHECK_CID                     \
{ /* {75656ad9-bd13-4c5d-939a-ec6351eea0cc} */        \
    0x75656ad9, 0xbd13, 0x4c5d,                       \
    { 0x93, 0x9a, 0xec, 0x63, 0x51, 0xee, 0xa0, 0xcc }\
}

namespace mozilla {

class DictionaryFetcher;

enum dictCompare
{
  DICT_NORMAL_COMPARE,
  DICT_COMPARE_CASE_INSENSITIVE,
  DICT_COMPARE_DASHMATCH
};

class EditorSpellCheck final : public nsIEditorSpellCheck
{
  friend class DictionaryFetcher;

public:
  EditorSpellCheck();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(EditorSpellCheck)

  /* Declare all methods in the nsIEditorSpellCheck interface */
  NS_DECL_NSIEDITORSPELLCHECK

protected:
  virtual ~EditorSpellCheck();

  RefPtr<mozSpellChecker> mSpellChecker;
  nsCOMPtr<nsITextServicesFilter> mTxtSrvFilter;
  nsCOMPtr<nsIEditor> mEditor;

  nsTArray<nsString> mSuggestedWordList;

  // these are the words in the current personal dictionary,
  // GetPersonalDictionary must be called to load them.
  nsTArray<nsString>  mDictionaryList;

  nsString mPreferredLang;

  int32_t mSuggestedWordIndex;
  int32_t mDictionaryIndex;
  uint32_t mDictionaryFetcherGroup;

  bool mUpdateDictionaryRunning;

  nsresult DeleteSuggestedWordList();

  void BuildDictionaryList(const nsAString& aDictName,
                           const nsTArray<nsString>& aDictList,
                           enum dictCompare aCompareType,
                           nsTArray<nsString>& aTryList);

  nsresult DictionaryFetched(DictionaryFetcher* aFetchState);

  void SetFallbackDictionary(DictionaryFetcher* aFetcher);


public:
  void BeginUpdateDictionary() { mUpdateDictionaryRunning = true ;}
  void EndUpdateDictionary() { mUpdateDictionaryRunning = false ;}
};

} // namespace mozilla

#endif // mozilla_EditorSpellCheck_h

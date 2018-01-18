/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEditorSpellCheck_h___
#define nsEditorSpellCheck_h___


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

class DictionaryFetcher;

enum dictCompare {
  DICT_NORMAL_COMPARE,
  DICT_COMPARE_CASE_INSENSITIVE,
  DICT_COMPARE_DASHMATCH
};

class nsEditorSpellCheck final : public nsIEditorSpellCheck
{
  friend class DictionaryFetcher;

public:
  nsEditorSpellCheck();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsEditorSpellCheck)

  /* Declare all methods in the nsIEditorSpellCheck interface */
  NS_DECL_NSIEDITORSPELLCHECK

protected:
  virtual ~nsEditorSpellCheck();

  RefPtr<mozSpellChecker> mSpellChecker;

  nsTArray<nsString>  mSuggestedWordList;
  int32_t        mSuggestedWordIndex;

  // these are the words in the current personal dictionary,
  // GetPersonalDictionary must be called to load them.
  nsTArray<nsString>  mDictionaryList;
  int32_t        mDictionaryIndex;

  nsresult       DeleteSuggestedWordList();

  nsCOMPtr<nsITextServicesFilter> mTxtSrvFilter;
  nsCOMPtr<nsIEditor> mEditor;

  nsString mPreferredLang;

  uint32_t mDictionaryFetcherGroup;

  bool mUpdateDictionaryRunning;

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

#endif // nsEditorSpellCheck_h___



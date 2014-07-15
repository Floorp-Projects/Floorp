/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozSpellChecker_h__
#define mozSpellChecker_h__

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsISpellChecker.h"
#include "nsString.h"
#include "nsITextServicesDocument.h"
#include "mozIPersonalDictionary.h"
#include "mozISpellCheckingEngine.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "mozISpellI18NUtil.h"
#include "nsCycleCollectionParticipant.h"
#include "RemoteSpellCheckEngineChild.h"

namespace mozilla {
class PRemoteSpellcheckEngineChild;
class RemoteSpellcheckEngineChild;
}

class mozSpellChecker : public nsISpellChecker
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(mozSpellChecker)

  mozSpellChecker();

  nsresult Init();

  // nsISpellChecker
  NS_IMETHOD SetDocument(nsITextServicesDocument *aDoc, bool aFromStartofDoc);
  NS_IMETHOD NextMisspelledWord(nsAString &aWord, nsTArray<nsString> *aSuggestions);
  NS_IMETHOD CheckWord(const nsAString &aWord, bool *aIsMisspelled, nsTArray<nsString> *aSuggestions);
  NS_IMETHOD Replace(const nsAString &aOldWord, const nsAString &aNewWord, bool aAllOccurrences);
  NS_IMETHOD IgnoreAll(const nsAString &aWord);

  NS_IMETHOD AddWordToPersonalDictionary(const nsAString &aWord);
  NS_IMETHOD RemoveWordFromPersonalDictionary(const nsAString &aWord);
  NS_IMETHOD GetPersonalDictionary(nsTArray<nsString> *aWordList);

  NS_IMETHOD GetDictionaryList(nsTArray<nsString> *aDictionaryList);
  NS_IMETHOD GetCurrentDictionary(nsAString &aDictionary);
  NS_IMETHOD SetCurrentDictionary(const nsAString &aDictionary);
  NS_IMETHOD CheckCurrentDictionary();
  void DeleteRemoteEngine();

protected:
  virtual ~mozSpellChecker();

  nsCOMPtr<mozISpellI18NUtil> mConverter;
  nsCOMPtr<nsITextServicesDocument> mTsDoc;
  nsCOMPtr<mozIPersonalDictionary> mPersonalDictionary;

  nsCOMPtr<mozISpellCheckingEngine>  mSpellCheckingEngine;
  bool mFromStart;

  nsresult SetupDoc(int32_t *outBlockOffset);

  nsresult GetCurrentBlockIndex(nsITextServicesDocument *aDoc, int32_t *outBlockIndex);

  nsresult GetEngineList(nsCOMArray<mozISpellCheckingEngine> *aDictionaryList);

  mozilla::PRemoteSpellcheckEngineChild *mEngine;
};
#endif // mozSpellChecker_h__

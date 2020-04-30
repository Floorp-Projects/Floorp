/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozSpellChecker_h__
#define mozSpellChecker_h__

#include "mozilla/MozPromise.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "mozIPersonalDictionary.h"
#include "mozISpellCheckingEngine.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"

class mozEnglishWordUtils;

namespace mozilla {
class RemoteSpellcheckEngineChild;
class TextServicesDocument;
typedef MozPromise<CopyableTArray<bool>, nsresult, false> CheckWordPromise;
}  // namespace mozilla

class mozSpellChecker final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(mozSpellChecker)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(mozSpellChecker)

  static already_AddRefed<mozSpellChecker> Create() {
    RefPtr<mozSpellChecker> spellChecker = new mozSpellChecker();
    nsresult rv = spellChecker->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);
    return spellChecker.forget();
  }

  /**
   * Tells the spellchecker what document to check.
   * @param aDoc is the document to check.
   * @param aFromStartOfDoc If true, start check from beginning of document,
   * if false, start check from current cursor position.
   */
  nsresult SetDocument(mozilla::TextServicesDocument* aTextServicesDocument,
                       bool aFromStartofDoc);

  /**
   * Selects (hilites) the next misspelled word in the document.
   * @param aWord will contain the misspelled word.
   * @param aSuggestions is an array of nsStrings, that represent the
   * suggested replacements for the misspelled word.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult NextMisspelledWord(nsAString& aWord,
                              nsTArray<nsString>& aSuggestions);

  /**
   * Checks if a word is misspelled. No document is required to use this method.
   * @param aWord is the word to check.
   * @param aIsMisspelled will be set to true if the word is misspelled.
   * @param aSuggestions is an array of nsStrings which represent the
   * suggested replacements for the misspelled word. The array will be empty
   * in chrome process if there aren't any suggestions. If suggestions is
   * unnecessary, use CheckWords of async version.
   */
  nsresult CheckWord(const nsAString& aWord, bool* aIsMisspelled,
                     nsTArray<nsString>* aSuggestions);

  /**
   * This is a flavor of CheckWord, is async version of CheckWord.
   * @Param aWords is array of words to check
   */
  RefPtr<mozilla::CheckWordPromise> CheckWords(
      const nsTArray<nsString>& aWords);

  /**
   * Replaces the old word with the specified new word.
   * @param aOldWord is the word to be replaced.
   * @param aNewWord is the word that is to replace old word.
   * @param aAllOccurrences will replace all occurrences of old
   * word, in the document, with new word when it is true. If
   * false, it will replace the 1st occurrence only!
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult Replace(const nsAString& aOldWord, const nsAString& aNewWord,
                   bool aAllOccurrences);

  /**
   * Ignores all occurrences of the specified word in the document.
   * @param aWord is the word to ignore.
   */
  nsresult IgnoreAll(const nsAString& aWord);

  /**
   * Add a word to the user's personal dictionary.
   * @param aWord is the word to add.
   */
  nsresult AddWordToPersonalDictionary(const nsAString& aWord);

  /**
   * Remove a word from the user's personal dictionary.
   * @param aWord is the word to remove.
   */
  nsresult RemoveWordFromPersonalDictionary(const nsAString& aWord);

  /**
   * Returns the list of words in the user's personal dictionary.
   * @param aWordList is an array of nsStrings that represent the
   * list of words in the user's personal dictionary.
   */
  nsresult GetPersonalDictionary(nsTArray<nsString>* aWordList);

  /**
   * Returns the list of strings representing the dictionaries
   * the spellchecker supports. It was suggested that the strings
   * returned be in the RFC 1766 format. This format looks something
   * like <ISO 639 language code>-<ISO 3166 country code>.
   * For example: en-US
   * @param aDictionaryList is an array of nsStrings that represent the
   * dictionaries supported by the spellchecker.
   */
  nsresult GetDictionaryList(nsTArray<nsString>* aDictionaryList);

  /**
   * Returns a string representing the current dictionary.
   * @param aDictionary will contain the name of the dictionary.
   * This name is the same string that is in the list returned
   * by GetDictionaryList().
   */
  nsresult GetCurrentDictionary(nsAString& aDictionary);

  /**
   * Tells the spellchecker to use a specific dictionary.
   * @param aDictionary a string that is in the list returned
   * by GetDictionaryList() or an empty string. If aDictionary is
   * empty string, spellchecker will be disabled.
   */
  nsresult SetCurrentDictionary(const nsAString& aDictionary);

  /**
   * Tells the spellchecker to use a specific dictionary from list.
   * @param aList  a preferred dictionary list
   */
  RefPtr<mozilla::GenericPromise> SetCurrentDictionaryFromList(
      const nsTArray<nsString>& aList);

  void DeleteRemoteEngine() { mEngine = nullptr; }

  mozilla::TextServicesDocument* GetTextServicesDocument();

 protected:
  mozSpellChecker();
  virtual ~mozSpellChecker();

  nsresult Init();

  RefPtr<mozEnglishWordUtils> mConverter;
  RefPtr<mozilla::TextServicesDocument> mTextServicesDocument;
  nsCOMPtr<mozIPersonalDictionary> mPersonalDictionary;

  nsCOMPtr<mozISpellCheckingEngine> mSpellCheckingEngine;
  bool mFromStart;

  nsString mCurrentDictionary;

  MOZ_CAN_RUN_SCRIPT
  nsresult SetupDoc(int32_t* outBlockOffset);

  nsresult GetCurrentBlockIndex(
      mozilla::TextServicesDocument* aTextServicesDocument,
      int32_t* aOutBlockIndex);

  nsresult GetEngineList(nsCOMArray<mozISpellCheckingEngine>* aDictionaryList);

  mozilla::RemoteSpellcheckEngineChild* mEngine;

  friend class mozilla::RemoteSpellcheckEngineChild;
};
#endif  // mozSpellChecker_h__

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISpellChecker_h__
#define nsISpellChecker_h__

#include "nsISupports.h"
#include "nsTArray.h"

#define NS_SPELLCHECKER_CONTRACTID "@mozilla.org/spellchecker;1"

#define NS_ISPELLCHECKER_IID                    \
{ /* 27bff957-b486-40ae-9f5d-af0cdd211868 */    \
0x27bff957, 0xb486, 0x40ae, \
  { 0x9f, 0x5d, 0xaf, 0x0c, 0xdd, 0x21, 0x18, 0x68 } }

class nsITextServicesDocument;
class nsString;

/**
 * A generic interface for a spelling checker.
 */
class nsISpellChecker  : public nsISupports{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISPELLCHECKER_IID)

  /**
   * Tells the spellchecker what document to check.
   * @param aDoc is the document to check.
   * @param aFromStartOfDoc If true, start check from beginning of document,
   * if false, start check from current cursor position.
   */
  NS_IMETHOD SetDocument(nsITextServicesDocument *aDoc, bool aFromStartofDoc) = 0;

  /**
   * Selects (hilites) the next misspelled word in the document.
   * @param aWord will contain the misspelled word.
   * @param aSuggestions is an array of nsStrings, that represent the
   * suggested replacements for the misspelled word.
   */
  NS_IMETHOD NextMisspelledWord(nsAString &aWord, nsTArray<nsString> *aSuggestions) = 0;

  /**
   * Checks if a word is misspelled. No document is required to use this method.
   * @param aWord is the word to check.
   * @param aIsMisspelled will be set to true if the word is misspelled.
   * @param aSuggestions is an array of nsStrings which represent the
   * suggested replacements for the misspelled word. The array will be empty
   * if there aren't any suggestions.
   */
  NS_IMETHOD CheckWord(const nsAString &aWord, bool *aIsMisspelled, nsTArray<nsString> *aSuggestions) = 0;

  /**
   * Replaces the old word with the specified new word.
   * @param aOldWord is the word to be replaced.
   * @param aNewWord is the word that is to replace old word.
   * @param aAllOccurrences will replace all occurrences of old
   * word, in the document, with new word when it is true. If
   * false, it will replace the 1st occurrence only!
   */
  NS_IMETHOD Replace(const nsAString &aOldWord, const nsAString &aNewWord, bool aAllOccurrences) = 0;

  /**
   * Ignores all occurrences of the specified word in the document.
   * @param aWord is the word to ignore.
   */
  NS_IMETHOD IgnoreAll(const nsAString &aWord) = 0;

  /**
   * Add a word to the user's personal dictionary.
   * @param aWord is the word to add.
   */
  NS_IMETHOD AddWordToPersonalDictionary(const nsAString &aWord) = 0;

  /**
   * Remove a word from the user's personal dictionary.
   * @param aWord is the word to remove.
   */
  NS_IMETHOD RemoveWordFromPersonalDictionary(const nsAString &aWord) = 0;

  /**
   * Returns the list of words in the user's personal dictionary.
   * @param aWordList is an array of nsStrings that represent the
   * list of words in the user's personal dictionary.
   */
  NS_IMETHOD GetPersonalDictionary(nsTArray<nsString> *aWordList) = 0;

  /**
   * Returns the list of strings representing the dictionaries
   * the spellchecker supports. It was suggested that the strings
   * returned be in the RFC 1766 format. This format looks something
   * like <ISO 639 language code>-<ISO 3166 country code>.
   * For example: en-US
   * @param aDictionaryList is an array of nsStrings that represent the
   * dictionaries supported by the spellchecker.
   */
  NS_IMETHOD GetDictionaryList(nsTArray<nsString> *aDictionaryList) = 0;

  /**
   * Returns a string representing the current dictionary.
   * @param aDictionary will contain the name of the dictionary.
   * This name is the same string that is in the list returned
   * by GetDictionaryList().
   */
  NS_IMETHOD GetCurrentDictionary(nsAString &aDictionary) = 0;

  /**
   * Tells the spellchecker to use a specific dictionary.
   * @param aDictionary a string that is in the list returned
   * by GetDictionaryList() or an empty string. If aDictionary is 
   * empty string, spellchecker will be disabled.
   */
  NS_IMETHOD SetCurrentDictionary(const nsAString &aDictionary) = 0;

  /**
   * Call this on any change in installed dictionaries to ensure that the spell
   * checker is not using a current dictionary which is no longer available.
   */
  NS_IMETHOD CheckCurrentDictionary() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISpellChecker, NS_ISPELLCHECKER_IID)

#endif // nsISpellChecker_h__


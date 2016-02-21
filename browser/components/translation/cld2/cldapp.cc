/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "public/compact_lang_det.h"

#define MAX_RESULTS 3

class Language {
public:
  Language(CLD2::Language lang) : mLang(lang) {}

  const char* getLanguageCode() const
  {
    return CLD2::LanguageCode(mLang);
  }

private:
  const CLD2::Language mLang;
};

class LanguageGuess : public Language {
public:
  LanguageGuess(CLD2::Language lang, char percent) :
    Language(lang), mPercent(percent) {}

  char getPercent() const
  {
    return mPercent;
  }

private:
  const char mPercent;
};


class LanguageInfo : public Language {
public:
  static LanguageInfo* detectLanguage(const char* buffer, bool isPlainText)
  {
    CLD2::Language languages[MAX_RESULTS] = {};
    int percentages[MAX_RESULTS] = {};
    bool isReliable = false;

    // This is ignored.
    int textBytes;

    CLD2::Language bestGuess = DetectLanguageSummary(
      buffer, strlen(buffer), isPlainText,
      languages, percentages, &textBytes,
      &isReliable);

    return new LanguageInfo(isReliable, bestGuess, languages, percentages);
  }

  static LanguageInfo* detectLanguage(const char* buffer, bool isPlainText,
                                      const char* tldHint, int encodingHint,
                                      const char* languageHint)
  {
    CLD2::CLDHints hints = {languageHint, tldHint, encodingHint, CLD2::UNKNOWN_LANGUAGE};

    CLD2::Language languages[MAX_RESULTS] = {};
    int percentages[MAX_RESULTS] = {};
    bool isReliable = false;

    // These are ignored.
    double scores[MAX_RESULTS];
    int textBytes;

    CLD2::Language bestGuess = ExtDetectLanguageSummary(
      buffer, strlen(buffer), isPlainText,
      &hints, 0,
      languages, percentages, scores,
      nullptr, &textBytes, &isReliable);

    return new LanguageInfo(isReliable, bestGuess, languages, percentages);
  }

  ~LanguageInfo()
  {
    for (int i = 0; i < MAX_RESULTS; i++) {
      delete languages[i];
    }
  }

  bool getIsReliable() const
  {
    return mIsReliable;
  }

  const LanguageGuess* languages[MAX_RESULTS];

private:
  LanguageInfo(bool isReliable, CLD2::Language bestGuess,
               CLD2::Language languageIDs[MAX_RESULTS],
               int percentages[MAX_RESULTS]) :
    Language(bestGuess), mIsReliable(isReliable)
  {
    for (int i = 0; i < MAX_RESULTS; i++) {
      languages[i] = new LanguageGuess(languageIDs[i], percentages[i]);
    }
  }

  const bool mIsReliable;
};

#include "cld.cpp"

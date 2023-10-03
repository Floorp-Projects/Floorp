/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_intl_WordBreaker_h__
#define mozilla_intl_WordBreaker_h__

#include "nsStringFwd.h"
#include <cstdint>

#define NS_WORDBREAKER_NEED_MORE_TEXT -1

namespace mozilla {
namespace intl {

struct WordRange {
  uint32_t mBegin;
  uint32_t mEnd;
};

class WordBreaker final {
 public:
  // WordBreaker is a utility class with only static methods. No need to
  // instantiate it.
  WordBreaker() = delete;
  ~WordBreaker() = delete;

  // Find the word boundary by scanning forward and backward from aPos.
  //
  // @return WordRange where mBegin equals to the offset to first character in
  // the word and mEnd equals to the offset to the last character plus 1. mEnd
  // can be aText.Lengh() if the desired word is at the end of aText.
  //
  // If aPos is already at the end of aText or beyond, both mBegin and mEnd
  // equals to aText.Length().
  //
  // If setting StopAtPunctuation, even if using UAX#29 word segmenter rule,
  // there will be break opportunities on characters with punctuation class.
  enum class FindWordOptions { None, StopAtPunctuation };

  static WordRange FindWord(
      const nsAString& aText, uint32_t aPos,
      const FindWordOptions aOptions = FindWordOptions::None);

  // Find the next word break opportunity starting from aPos + 1. It can return
  // aLen if there's no break opportunity between [aPos + 1, aLen - 1].
  //
  // If aPos is already at the end of aText or beyond, i.e. aPos >= aLen, return
  // NS_WORDBREAKER_NEED_MORE_TEXT.
  //
  // DEPRECATED: Use WordBreakIteratorUtf16 instead.
  static int32_t Next(const char16_t* aText, uint32_t aLen, uint32_t aPos);

 private:
  enum WordBreakClass : uint8_t {
    kWbClassSpace = 0,
    kWbClassAlphaLetter,
    kWbClassPunct,
    kWbClassHanLetter,
    kWbClassKatakanaLetter,
    kWbClassHiraganaLetter,
    kWbClassHWKatakanaLetter,
    kWbClassScriptioContinua
  };

  static WordBreakClass GetClass(char16_t aChar);
};

}  // namespace intl
}  // namespace mozilla

#endif /* mozilla_intl_WordBreaker_h__ */

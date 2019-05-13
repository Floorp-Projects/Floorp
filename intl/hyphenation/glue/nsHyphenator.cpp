/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHyphenator.h"
#include "nsIFile.h"
#include "nsUTF8Utils.h"
#include "nsUnicodeProperties.h"
#include "nsIURI.h"

#include "hyphen.h"

nsHyphenator::nsHyphenator(nsIURI* aURI, bool aHyphenateCapitalized)
    : mDict(nullptr), mHyphenateCapitalized(aHyphenateCapitalized) {
  nsCString uriSpec;
  nsresult rv = aURI->GetSpec(uriSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  mDict = hnj_hyphen_load(uriSpec.get());
#ifdef DEBUG
  if (mDict) {
    printf("loaded hyphenation patterns from %s\n", uriSpec.get());
  }
#endif
}

nsHyphenator::~nsHyphenator() {
  if (mDict != nullptr) {
    hnj_hyphen_free((HyphenDict*)mDict);
    mDict = nullptr;
  }
}

bool nsHyphenator::IsValid() { return (mDict != nullptr); }

nsresult nsHyphenator::Hyphenate(const nsAString& aString,
                                 nsTArray<bool>& aHyphens) {
  if (!aHyphens.SetLength(aString.Length(), mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(aHyphens.Elements(), false, aHyphens.Length() * sizeof(bool));

  bool inWord = false;
  uint32_t wordStart = 0, wordLimit = 0;
  uint32_t chLen;
  for (uint32_t i = 0; i < aString.Length(); i += chLen) {
    uint32_t ch = aString[i];
    chLen = 1;

    if (NS_IS_HIGH_SURROGATE(ch)) {
      if (i + 1 < aString.Length() && NS_IS_LOW_SURROGATE(aString[i + 1])) {
        ch = SURROGATE_TO_UCS4(ch, aString[i + 1]);
        chLen = 2;
      } else {
        NS_WARNING("unpaired surrogate found during hyphenation");
      }
    }

    nsUGenCategory cat = mozilla::unicode::GetGenCategory(ch);
    if (cat == nsUGenCategory::kLetter || cat == nsUGenCategory::kMark) {
      if (!inWord) {
        inWord = true;
        wordStart = i;
      }
      wordLimit = i + chLen;
      if (i + chLen < aString.Length()) {
        continue;
      }
    }

    if (inWord) {
      HyphenateWord(aString, wordStart, wordLimit, aHyphens);
      inWord = false;
    }
  }

  return NS_OK;
}

void nsHyphenator::HyphenateWord(const nsAString& aString, uint32_t aStart,
                                 uint32_t aLimit, nsTArray<bool>& aHyphens) {
  // Convert word from aStart and aLimit in aString to utf-8 for libhyphen,
  // lowercasing it as we go so that it will match the (lowercased) patterns
  // (bug 1105644).
  nsAutoCString utf8;
  const char16_t* const begin = aString.BeginReading();
  const char16_t* cur = begin + aStart;
  const char16_t* end = begin + aLimit;
  bool firstLetter = true;
  while (cur < end) {
    uint32_t ch = *cur++;

    if (NS_IS_HIGH_SURROGATE(ch)) {
      if (cur < end && NS_IS_LOW_SURROGATE(*cur)) {
        ch = SURROGATE_TO_UCS4(ch, *cur++);
      } else {
        ch = 0xfffd;  // unpaired surrogate, treat as REPLACEMENT CHAR
      }
    } else if (NS_IS_LOW_SURROGATE(ch)) {
      ch = 0xfffd;  // unpaired surrogate
    }

    // XXX What about language-specific casing? Consider Turkish I/i...
    // In practice, it looks like the current patterns will not be
    // affected by this, as they treat dotted and undotted i similarly.
    uint32_t origCh = ch;
    ch = ToLowerCase(ch);

    // Avoid hyphenating capitalized words (bug 1550532) unless explicitly
    // allowed by prefs for the language in use.
    if (firstLetter) {
      if (!mHyphenateCapitalized && ch != origCh) {
        return;
      }
      firstLetter = false;
    }

    if (ch < 0x80) {  // U+0000 - U+007F
      utf8.Append(ch);
    } else if (ch < 0x0800) {  // U+0100 - U+07FF
      utf8.Append(0xC0 | (ch >> 6));
      utf8.Append(0x80 | (0x003F & ch));
    } else if (ch < 0x10000) {  // U+0800 - U+D7FF,U+E000 - U+FFFF
      utf8.Append(0xE0 | (ch >> 12));
      utf8.Append(0x80 | (0x003F & (ch >> 6)));
      utf8.Append(0x80 | (0x003F & ch));
    } else {
      utf8.Append(0xF0 | (ch >> 18));
      utf8.Append(0x80 | (0x003F & (ch >> 12)));
      utf8.Append(0x80 | (0x003F & (ch >> 6)));
      utf8.Append(0x80 | (0x003F & ch));
    }
  }

  AutoTArray<char, 200> utf8hyphens;
  utf8hyphens.SetLength(utf8.Length() + 5);
  char** rep = nullptr;
  int* pos = nullptr;
  int* cut = nullptr;
  int err = hnj_hyphen_hyphenate2((HyphenDict*)mDict, utf8.BeginReading(),
                                  utf8.Length(), utf8hyphens.Elements(),
                                  nullptr, &rep, &pos, &cut);
  if (!err) {
    // Surprisingly, hnj_hyphen_hyphenate2 converts the 'hyphens' buffer
    // from utf8 code unit indexing (which would match the utf8 input
    // string directly) to Unicode character indexing.
    // We then need to convert this to utf16 code unit offsets for Gecko.
    const char* hyphPtr = utf8hyphens.Elements();
    const char16_t* cur = begin + aStart;
    const char16_t* end = begin + aLimit;
    while (cur < end) {
      if (*hyphPtr & 0x01) {
        aHyphens[cur - begin] = true;
      }
      cur++;
      if (cur < end && NS_IS_LOW_SURROGATE(*cur) &&
          NS_IS_HIGH_SURROGATE(*(cur - 1))) {
        cur++;
      }
      hyphPtr++;
    }
  }
}

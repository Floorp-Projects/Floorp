/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHyphenator.h"
#include "nsIFile.h"
#include "nsUTF8Utils.h"
#include "nsUnicodeProperties.h"
#include "nsUnicharUtilCIID.h"
#include "nsIURI.h"

#include "hyphen.h"

nsHyphenator::nsHyphenator(nsIURI *aURI)
  : mDict(nullptr)
{
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

nsHyphenator::~nsHyphenator()
{
  if (mDict != nullptr) {
    hnj_hyphen_free((HyphenDict*)mDict);
    mDict = nullptr;
  }
}

bool
nsHyphenator::IsValid()
{
  return (mDict != nullptr);
}

nsresult
nsHyphenator::Hyphenate(const nsAString& aString,
                        nsTArray<bool>& aHyphens)
{
  if (!aHyphens.SetLength(aString.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(aHyphens.Elements(), false, aHyphens.Length());

  bool inWord = false;
  uint32_t wordStart = 0, wordLimit = 0;
  uint32_t chLen;
  for (uint32_t i = 0; i < aString.Length(); i += chLen) {
    uint32_t ch = aString[i];
    chLen = 1;

    if (NS_IS_HIGH_SURROGATE(ch)) {
      if (i + 1 < aString.Length() && NS_IS_LOW_SURROGATE(aString[i+1])) {
        ch = SURROGATE_TO_UCS4(ch, aString[i+1]);
        chLen = 2;
      } else {
        NS_WARNING("unpaired surrogate found during hyphenation");
      }
    }

    nsIUGenCategory::nsUGenCategory cat = mozilla::unicode::GetGenCategory(ch);
    if (cat == nsIUGenCategory::kLetter || cat == nsIUGenCategory::kMark) {
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
      const PRUnichar *begin = aString.BeginReading();
      NS_ConvertUTF16toUTF8 utf8(begin + wordStart,
                                 wordLimit - wordStart);
      nsAutoTArray<char,200> utf8hyphens;
      utf8hyphens.SetLength(utf8.Length() + 5);
      char **rep = nullptr;
      int *pos = nullptr;
      int *cut = nullptr;
      int err = hnj_hyphen_hyphenate2((HyphenDict*)mDict,
                                      utf8.BeginReading(), utf8.Length(),
                                      utf8hyphens.Elements(), nullptr,
                                      &rep, &pos, &cut);
      if (!err) {
        // Surprisingly, hnj_hyphen_hyphenate2 converts the 'hyphens' buffer
        // from utf8 code unit indexing (which would match the utf8 input
        // string directly) to Unicode character indexing.
        // We then need to convert this to utf16 code unit offsets for Gecko.
        const char *hyphPtr = utf8hyphens.Elements();
        const PRUnichar *cur = begin + wordStart;
        const PRUnichar *end = begin + wordLimit;
        while (cur < end) {
          if (*hyphPtr & 0x01) {
            aHyphens[cur - begin] = true;
          }
          cur++;
          if (cur < end && NS_IS_LOW_SURROGATE(*cur) &&
              NS_IS_HIGH_SURROGATE(*(cur-1)))
          {
            cur++;
          }
          hyphPtr++;
        }
      }
    }
    
    inWord = false;
  }

  return NS_OK;
}

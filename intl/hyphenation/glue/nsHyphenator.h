/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHyphenator_h__
#define nsHyphenator_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIURI;

class nsHyphenator {
 public:
  nsHyphenator(nsIURI* aURI, bool aHyphenateCapitalized);

  NS_INLINE_DECL_REFCOUNTING(nsHyphenator)

  bool IsValid();

  nsresult Hyphenate(const nsAString& aText, nsTArray<bool>& aHyphens);

 private:
  ~nsHyphenator();

  void HyphenateWord(const nsAString& aString, uint32_t aStart, uint32_t aLimit,
                     nsTArray<bool>& aHyphens);

  const void* mDict;  // If mDictSize > 0, this points to a raw byte buffer
                      // containing the hyphenation dictionary data (in the
                      // memory-mapped omnijar, or owned by us if mOwnsDict);
                      // if mDictSize == 0, it's a HyphDic reference created
                      // by mapped_hyph_load_dictionary() and must be released
                      // by calling mapped_hyph_free_dictionary().
  uint32_t mDictSize;
  bool mOwnsDict;
  bool mHyphenateCapitalized;
};

#endif  // nsHyphenator_h__

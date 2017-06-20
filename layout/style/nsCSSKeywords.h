/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* keywords used within CSS property values */

#ifndef nsCSSKeywords_h___
#define nsCSSKeywords_h___

#include "nsStringFwd.h"

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eCSSKeyword_foo" (where foo is the keyword)

   To change the list of keywords, see nsCSSKeywordList.h

 */
#define CSS_KEY(_name,_id) eCSSKeyword_##_id,
enum nsCSSKeyword : int16_t {
  eCSSKeyword_UNKNOWN = -1,
#include "nsCSSKeywordList.h"
  eCSSKeyword_COUNT
};
#undef CSS_KEY


class nsCSSKeywords {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given a keyword string, return the enum value
  static nsCSSKeyword LookupKeyword(const nsACString& aKeyword);
  static nsCSSKeyword LookupKeyword(const nsAString& aKeyword);

  // Given a keyword enum, get the string value
  static const nsCString& GetStringValue(nsCSSKeyword aKeyword);
};

#endif /* nsCSSKeywords_h___ */

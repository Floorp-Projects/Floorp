/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsCSSKeywords_h___
#define nsCSSKeywords_h___

#include "nslayout.h"

struct nsStr;
class nsCString;

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eCSSKeyword_foo" (where foo is the keyword)

   To change the list of keywords, see nsCSSKeywordList.h

 */
#define CSS_KEY(_key) eCSSKeyword_##_key,
enum nsCSSKeyword {
  eCSSKeyword_UNKNOWN = -1,
#include "nsCSSKeywordList.h"
  eCSSKeyword_COUNT
};
#undef CSS_KEY


class NS_LAYOUT nsCSSKeywords {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given a keyword string, return the enum value
  static nsCSSKeyword LookupKeyword(const nsStr& aKeyword);

  // Given a keyword enum, get the string value
  static const nsCString& GetStringValue(nsCSSKeyword aKeyword);
};

#endif /* nsCSSKeywords_h___ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsCSSKeywords_h___
#define nsCSSKeywords_h___

#include "nslayout.h"

class nsString;
class nsCString;

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eCSSKeyword_foo" (where foo is the keyword)

   To change the list of keywords, see nsCSSKeywordList.h

 */
#define CSS_KEY(_name,_id) eCSSKeyword_##_id,
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
  static nsCSSKeyword LookupKeyword(const nsCString& aKeyword);
  static nsCSSKeyword LookupKeyword(const nsString& aKeyword);

  // Given a keyword enum, get the string value
  static const nsCString& GetStringValue(nsCSSKeyword aKeyword);
};

#endif /* nsCSSKeywords_h___ */

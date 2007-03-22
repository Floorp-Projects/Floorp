/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
enum nsCSSKeyword {
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
  static const nsAFlatCString& GetStringValue(nsCSSKeyword aKeyword);
};

#endif /* nsCSSKeywords_h___ */

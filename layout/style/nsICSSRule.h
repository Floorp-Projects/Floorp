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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsICSSRule_h___
#define nsICSSRule_h___

#include "nslayout.h"
#include "nsIStyleRule.h"

class nsICSSStyleSheet;
class nsICSSGroupRule;

// IID for the nsICSSRule interface {b9791e20-1a04-11d3-805a-006008159b5a}
#define NS_ICSS_RULE_IID     \
{0xb9791e20, 0x1a04, 0x11d3, {0x80, 0x5a, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}


class nsICSSRule : public nsIStyleRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_RULE_IID; return iid; }
  enum {
    UNKNOWN_RULE = 0,
    STYLE_RULE = 1,
    IMPORT_RULE = 2,
    MEDIA_RULE = 3,
    FONT_FACE_RULE = 4,
    PAGE_RULE = 5,
    CHARSET_RULE = 6,
    NAMESPACE_RULE = 7
  };

  NS_IMETHOD GetType(PRInt32& aType) const = 0;

  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet) = 0;
  NS_IMETHOD SetParentRule(nsICSSGroupRule* aRule) = 0;

  NS_IMETHOD Clone(nsICSSRule*& aClone) const = 0;
};

#endif /* nsICSSRule_h___ */

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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/* internal interface for all rule types in a CSS style sheet */

#ifndef nsICSSRule_h
#define nsICSSRule_h

#include "nsIStyleRule.h"
#include "nsIDOMCSSRule.h"

class nsCSSStyleSheet;
class nsICSSGroupRule;
class nsAString;
template<class T> struct already_AddRefed;
class nsIStyleSheet;

// IID for the nsICSSRule interface
#define NS_ICSS_RULE_IID \
{ 0x1f560b20, 0xa829, 0x4b99, \
  { 0x87, 0xbd, 0x8c, 0x87, 0x95, 0x2b, 0x3b, 0xb6 } }


// inheriting from nsIStyleRule is only for style rules, not other rule types
class nsICSSRule : public nsIStyleRule {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_RULE_IID)
  // The constants in this list must maintain the following invariants:
  //   If a rule of type N must appear before a rule of type M in stylesheets
  //   then N < M
  // Note that nsCSSStyleSheet::RebuildChildList assumes that no other kinds of
  // rules can come between two rules of type IMPORT_RULE.
  enum {
    UNKNOWN_RULE = 0,
    CHARSET_RULE,
    IMPORT_RULE,
    NAMESPACE_RULE,
    STYLE_RULE,
    MEDIA_RULE,
    FONT_FACE_RULE,
    PAGE_RULE,
    DOCUMENT_RULE
  };

  virtual PRInt32 GetType() const = 0;

  virtual already_AddRefed<nsIStyleSheet> GetStyleSheet() const = 0;
  virtual void SetStyleSheet(nsCSSStyleSheet* aSheet) = 0;
  virtual void SetParentRule(nsICSSGroupRule* aRule) = 0;

  /**
   * Clones |this|. Never returns NULL.
   */
  virtual already_AddRefed<nsICSSRule> Clone() const = 0;

  // Note that this returns null for inline style rules since they aren't
  // supposed to have a DOM rule representation (and our code wouldn't work).
  nsresult GetDOMRule(nsIDOMCSSRule** aDOMRule)
  {
    nsresult rv;
    NS_IF_ADDREF(*aDOMRule = GetDOMRuleWeak(&rv));
    return rv;
  }
  virtual nsIDOMCSSRule* GetDOMRuleWeak(nsresult* aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSRule, NS_ICSS_RULE_IID)

/* Define global NS_New* functions for rules that don't need their own
   interfaces here */
nsresult
NS_NewCSSCharsetRule(nsICSSRule** aInstancePtrResult,
                     const nsAString& aEncoding);

#endif /* nsICSSRule_h */

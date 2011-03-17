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

/* class for CSS @namespace rules */

#ifndef mozilla_css_NameSpaceRule_h__
#define mozilla_css_NameSpaceRule_h__

#include "nsICSSRule.h"
#include "nsCSSRule.h"
#include "nsIDOMCSSRule.h"

class nsIAtom;

// IID for the nsCSSNameSpaceRule class {f0b0dbe1-5031-4a21-b06a-dc141ef2af98}
#define NS_CSS_NAMESPACE_RULE_IMPL_CID     \
{0xf0b0dbe1, 0x5031, 0x4a21, {0xb0, 0x6a, 0xdc, 0x14, 0x1e, 0xf2, 0xaf, 0x98}}


namespace mozilla {
namespace css {

class NS_FINAL_CLASS NameSpaceRule : public nsCSSRule,
                                     public nsICSSRule,
                                     public nsIDOMCSSRule
{
public:
  NameSpaceRule();
private:
  // for |Clone|
  NameSpaceRule(const NameSpaceRule& aCopy);
  ~NameSpaceRule();
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_NAMESPACE_RULE_IMPL_CID)

  NS_DECL_ISUPPORTS

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<nsICSSRule> Clone() const;

  NS_IMETHOD  GetPrefix(nsIAtom*& aPrefix) const;
  NS_IMETHOD  SetPrefix(nsIAtom* aPrefix);

  NS_IMETHOD  GetURLSpec(nsString& aURLSpec) const;
  NS_IMETHOD  SetURLSpec(const nsString& aURLSpec);

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

private:
  nsIAtom*          mPrefix;
  nsString          mURLSpec;
};

} // namespace css
} // namespace mozilla

NS_DEFINE_STATIC_IID_ACCESSOR(mozilla::css::NameSpaceRule, NS_CSS_NAMESPACE_RULE_IMPL_CID)

nsresult
NS_NewCSSNameSpaceRule(mozilla::css::NameSpaceRule** aInstancePtrResult,
                       nsIAtom* aPrefix, const nsString& aURLSpec);

#endif /* mozilla_css_NameSpaceRule_h__ */

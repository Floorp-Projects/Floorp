/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class for CSS @namespace rules */

#ifndef mozilla_css_NameSpaceRule_h__
#define mozilla_css_NameSpaceRule_h__

#include "mozilla/Attributes.h"
#include "mozilla/css/Rule.h"

#include "nsIDOMCSSRule.h"

class nsIAtom;

// IID for the NameSpaceRule class {f0b0dbe1-5031-4a21-b06a-dc141ef2af98}
#define NS_CSS_NAMESPACE_RULE_IMPL_CID     \
{0xf0b0dbe1, 0x5031, 0x4a21, {0xb0, 0x6a, 0xdc, 0x14, 0x1e, 0xf2, 0xaf, 0x98}}


namespace mozilla {
namespace css {

class NameSpaceRule MOZ_FINAL : public Rule,
                                public nsIDOMCSSRule
{
public:
  NameSpaceRule(nsIAtom* aPrefix, const nsString& aURLSpec);
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
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif

  // Rule methods
  virtual int32_t GetType() const;
  virtual already_AddRefed<Rule> Clone() const;

  nsIAtom* GetPrefix() const { return mPrefix; }

  void GetURLSpec(nsString& aURLSpec) const { aURLSpec = mURLSpec; }

  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
    MOZ_MUST_OVERRIDE;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

private:
  nsCOMPtr<nsIAtom> mPrefix;
  nsString          mURLSpec;
};

} // namespace css
} // namespace mozilla

NS_DEFINE_STATIC_IID_ACCESSOR(mozilla::css::NameSpaceRule, NS_CSS_NAMESPACE_RULE_IMPL_CID)

#endif /* mozilla_css_NameSpaceRule_h__ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class for CSS @namespace rules */

#ifndef mozilla_css_NameSpaceRule_h__
#define mozilla_css_NameSpaceRule_h__

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/CSSNamespaceRule.h"

#include "nsIDOMCSSRule.h"

class nsIAtom;

// IID for the NameSpaceRule class {f0b0dbe1-5031-4a21-b06a-dc141ef2af98}
#define NS_CSS_NAMESPACE_RULE_IMPL_CID     \
{0xf0b0dbe1, 0x5031, 0x4a21, {0xb0, 0x6a, 0xdc, 0x14, 0x1e, 0xf2, 0xaf, 0x98}}


namespace mozilla {
namespace css {

class NameSpaceRule final : public dom::CSSNamespaceRule
{
public:
  NameSpaceRule(nsIAtom* aPrefix, const nsString& aURLSpec,
                uint32_t aLineNumber, uint32_t aColumnNumber);
private:
  // for |Clone|
  NameSpaceRule(const NameSpaceRule& aCopy);
  ~NameSpaceRule();
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_NAMESPACE_RULE_IMPL_CID)

  NS_DECL_ISUPPORTS_INHERITED

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual already_AddRefed<Rule> Clone() const override;

  nsIAtom* GetPrefix() const final { return mPrefix; }
  void GetURLSpec(nsString& aURLSpec) const final { aURLSpec = mURLSpec; }

  // WebIDL interface
  void GetCssTextImpl(nsAString& aCssText) const override;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

private:
  nsCOMPtr<nsIAtom> mPrefix;
  nsString          mURLSpec;
};

NS_DEFINE_STATIC_IID_ACCESSOR(NameSpaceRule, NS_CSS_NAMESPACE_RULE_IMPL_CID)

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_NameSpaceRule_h__ */

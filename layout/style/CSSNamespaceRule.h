/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSNamespaceRule_h
#define mozilla_dom_CSSNamespaceRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/dom/CSSNamespaceRuleBinding.h"
#include "mozilla/ServoBindingTypes.h"

class nsAtom;

namespace mozilla {
namespace dom {

class CSSNamespaceRule final : public css::Rule
{
public:
  CSSNamespaceRule(already_AddRefed<RawServoNamespaceRule> aRule,
                   StyleSheet* aSheet,
                   css::Rule* aParentRule,
                   uint32_t aLine,
                   uint32_t aColumn)
    : css::Rule(aSheet, aParentRule, aLine, aColumn)
    , mRawRule(std::move(aRule))
  {
  }

  bool IsCCLeaf() const final {
    return Rule::IsCCLeaf();
  }

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  nsAtom* GetPrefix() const;
  void GetURLSpec(nsString& aURLSpec) const;

  // WebIDL interface
  void GetCssText(nsAString& aCssText) const final;

  // WebIDL interfaces
  uint16_t Type() const final { return CSSRule_Binding::NAMESPACE_RULE; }

  void GetNamespaceURI(nsString& aNamespaceURI) {
    GetURLSpec(aNamespaceURI);
  }

  void GetPrefix(DOMString& aPrefix) {
    aPrefix.SetKnownLiveAtom(GetPrefix(), DOMString::eTreatNullAsEmpty);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) final {
    return CSSNamespaceRule_Binding::Wrap(aCx, this, aGivenProto);
  }

private:
  ~CSSNamespaceRule();
  RefPtr<RawServoNamespaceRule> mRawRule;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSNamespaceRule_h

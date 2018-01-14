/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSNamespaceRule_h
#define mozilla_dom_CSSNamespaceRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/dom/CSSNamespaceRuleBinding.h"

class nsAtom;

namespace mozilla {
namespace dom {

class CSSNamespaceRule : public css::Rule
{
protected:
  using Rule::Rule;

public:
  bool IsCCLeaf() const final override {
    return Rule::IsCCLeaf();
  }
  int32_t GetType() const final override {
    return Rule::NAMESPACE_RULE;
  }

  virtual nsAtom* GetPrefix() const = 0;
  virtual void GetURLSpec(nsString& aURLSpec) const = 0;

  // WebIDL interfaces
  uint16_t Type() const final override { return CSSRuleBinding::NAMESPACE_RULE; }
  void GetNamespaceURI(nsString& aNamespaceURI) {
    GetURLSpec(aNamespaceURI);
  }
  void GetPrefix(DOMString& aPrefix) {
    aPrefix.SetKnownLiveAtom(GetPrefix(), DOMString::eTreatNullAsEmpty);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override = 0;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) final override {
    return CSSNamespaceRuleBinding::Wrap(aCx, this, aGivenProto);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSNamespaceRule_h

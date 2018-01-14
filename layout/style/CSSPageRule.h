/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSPageRule_h
#define mozilla_dom_CSSPageRule_h

#include "mozilla/css/Rule.h"

#include "nsICSSDeclaration.h"

namespace mozilla {
namespace dom {

class CSSPageRule : public css::Rule
{
protected:
  using Rule::Rule;
  virtual ~CSSPageRule() {};

public:
  virtual bool IsCCLeaf() const override = 0;

  int32_t GetType() const final override { return Rule::PAGE_RULE; }

  // WebIDL interfaces
  uint16_t Type() const final override { return CSSRuleBinding::PAGE_RULE; }
  virtual void GetCssTextImpl(nsAString& aCssText) const override = 0;
  virtual nsICSSDeclaration* Style() = 0;

  virtual size_t
  SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override = 0;

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSPageRule_h

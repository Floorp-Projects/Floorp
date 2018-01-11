/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSImportRule_h
#define mozilla_dom_CSSImportRule_h

#include "mozilla/css/Rule.h"

namespace mozilla {
namespace dom {

class CSSImportRule : public css::Rule
{
protected:
  using Rule::Rule;
  virtual ~CSSImportRule() {}

public:
  bool IsCCLeaf() const final;

  int32_t GetType() const final { return css::Rule::IMPORT_RULE; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override = 0;

  // WebIDL interface
  uint16_t Type() const final { return CSSRuleBinding::IMPORT_RULE; }
  virtual void GetHref(nsAString& aHref) const = 0;
  virtual dom::MediaList* GetMedia() const = 0;
  virtual StyleSheet* GetStyleSheet() const = 0;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSImportRule_h

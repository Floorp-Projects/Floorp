/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSImportRule_h
#define mozilla_dom_CSSImportRule_h

#include "mozilla/css/Rule.h"
#include "nsIDOMCSSImportRule.h"

namespace mozilla {
namespace dom {

class CSSImportRule : public css::Rule
                    , public nsIDOMCSSImportRule
{
protected:
  using Rule::Rule;
  virtual ~CSSImportRule() {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  bool IsCCLeaf() const final;

  int32_t GetType() const final { return css::Rule::IMPORT_RULE; }
  using Rule::GetType;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override = 0;

  // nsIDOMCSSImportRule interface
  NS_IMETHOD GetMedia(nsIDOMMediaList** aMedia) final;
  NS_IMETHOD GetStyleSheet(nsIDOMCSSStyleSheet** aStyleSheet) final;

  // WebIDL interface
  uint16_t Type() const final { return nsIDOMCSSRule::IMPORT_RULE; }
  // The XPCOM GetHref is fine, since it never fails.
  virtual dom::MediaList* GetMedia() const = 0;
  virtual StyleSheet* GetStyleSheet() const = 0;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSImportRule_h

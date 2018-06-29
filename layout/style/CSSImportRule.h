/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSImportRule_h
#define mozilla_dom_CSSImportRule_h

#include "mozilla/css/Rule.h"

namespace mozilla {

class StyleSheet;

namespace dom {

class CSSImportRule final : public css::Rule
{
public:
  CSSImportRule(RefPtr<RawServoImportRule> aRawRule,
                StyleSheet* aSheet,
                css::Rule* aParentRule,
                uint32_t aLine,
                uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CSSImportRule, css::Rule)

  bool IsCCLeaf() const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override;

  // WebIDL interface
  uint16_t Type() const final { return CSSRule_Binding::IMPORT_RULE; }
  void GetCssText(nsAString& aCssText) const override;
  void GetHref(nsAString& aHref) const;
  dom::MediaList* GetMedia() const;
  StyleSheet* GetStyleSheet() const { return mChildSheet; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~CSSImportRule();

  RefPtr<RawServoImportRule> mRawRule;
  RefPtr<StyleSheet> mChildSheet;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSImportRule_h

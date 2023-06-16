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

class CSSImportRule final : public css::Rule {
 public:
  CSSImportRule(RefPtr<StyleLockedImportRule> aRawRule, StyleSheet* aSheet,
                css::Rule* aParentRule, uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CSSImportRule, css::Rule)

  bool IsCCLeaf() const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  size_t SizeOfIncludingThis(MallocSizeOf) const override;

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const override;
  void GetHref(nsAString& aHref) const;
  dom::MediaList* GetMedia();
  StyleSheet* GetStyleSheet() const { return mChildSheet; }
  StyleSheet* GetStyleSheetForBindings();
  void GetLayerName(nsACString&) const;
  void GetSupportsText(nsACString&) const;

  // Clear the mSheet pointer on this rule and descendants.
  void DropSheetReference() final;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  const StyleLockedImportRule* Raw() const { return mRawRule.get(); }
  void SetRawAfterClone(RefPtr<StyleLockedImportRule>);

 private:
  ~CSSImportRule();

  RefPtr<StyleLockedImportRule> mRawRule;
  RefPtr<StyleSheet> mChildSheet;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CSSImportRule_h

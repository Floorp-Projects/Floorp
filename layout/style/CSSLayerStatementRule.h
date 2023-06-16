/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSLayerStatementRule_h
#define mozilla_dom_CSSLayerStatementRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla::dom {

class CSSLayerStatementRule final : public css::Rule {
 public:
  CSSLayerStatementRule(RefPtr<StyleLayerStatementRule> aRawRule,
                        StyleSheet* aSheet, css::Rule* aParentRule,
                        uint32_t aLine, uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

  bool IsCCLeaf() const final { return css::Rule::IsCCLeaf(); }

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  StyleLayerStatementRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleLayerStatementRule>);

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;

  void GetNameList(nsTArray<nsCString>&) const;

  size_t SizeOfIncludingThis(MallocSizeOf) const override;
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;

 private:
  ~CSSLayerStatementRule() = default;

  RefPtr<StyleLayerStatementRule> mRawRule;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSLayerStatementRule_h

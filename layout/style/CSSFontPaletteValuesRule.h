/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSFontPaletteValuesRule_h
#define mozilla_dom_CSSFontPaletteValuesRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

#include "nsICSSDeclaration.h"

namespace mozilla::dom {

class CSSFontPaletteValuesRule final : public css::Rule {
 public:
  CSSFontPaletteValuesRule(RefPtr<StyleFontPaletteValuesRule> aRawRule,
                           StyleSheet* aSheet, css::Rule* aParentRule,
                           uint32_t aLine, uint32_t aColumn)
      : css::Rule(aSheet, aParentRule, aLine, aColumn),
        mRawRule(std::move(aRawRule)) {}

  bool IsCCLeaf() const final;

  StyleFontPaletteValuesRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<StyleFontPaletteValuesRule> aRaw);

  // WebIDL interfaces
  StyleCssRuleType Type() const final;

  void GetCssText(nsACString& aCssText) const final;

  void GetFontFamily(nsACString& aFamily) const;

  void GetName(nsACString& aName) const;

  void GetBasePalette(nsACString& aPalette) const;

  void GetOverrideColors(nsACString& aColors) const;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

 private:
  ~CSSFontPaletteValuesRule() = default;

  RefPtr<StyleFontPaletteValuesRule> mRawRule;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSFontPaletteValuesRule_h

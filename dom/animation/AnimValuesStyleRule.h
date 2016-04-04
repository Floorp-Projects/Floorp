/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimValuesStyleRule_h
#define mozilla_AnimValuesStyleRule_h

#include "mozilla/StyleAnimationValue.h"
#include "nsCSSProperty.h"
#include "nsCSSPropertySet.h"
#include "nsIStyleRule.h"
#include "nsISupportsImpl.h" // For NS_DECL_ISUPPORTS
#include "nsRuleNode.h" // For nsCachedStyleData
#include "nsTArray.h" // For nsTArray

namespace mozilla {

/**
 * A style rule that maps property-StyleAnimationValue pairs.
 */
class AnimValuesStyleRule final : public nsIStyleRule
{
public:
  AnimValuesStyleRule()
    : mStyleBits(0) {}

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // nsIStyleRule implementation
  void MapRuleInfoInto(nsRuleData* aRuleData) override;
  bool MightMapInheritedStyleData() override;
#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  void AddValue(nsCSSProperty aProperty, StyleAnimationValue &aStartValue)
  {
    PropertyValuePair v = { aProperty, aStartValue };
    mPropertyValuePairs.AppendElement(v);
    mStyleBits |=
      nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[aProperty]);
  }

  // Caller must fill in returned value.
  StyleAnimationValue* AddEmptyValue(nsCSSProperty aProperty)
  {
    PropertyValuePair *p = mPropertyValuePairs.AppendElement();
    p->mProperty = aProperty;
    mStyleBits |=
      nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[aProperty]);
    return &p->mValue;
  }

  struct PropertyValuePair {
    nsCSSProperty mProperty;
    StyleAnimationValue mValue;
  };

private:
  ~AnimValuesStyleRule() {}

  InfallibleTArray<PropertyValuePair> mPropertyValuePairs;
  uint32_t mStyleBits;
};

} // namespace mozilla

#endif // mozilla_AnimValuesStyleRule_h

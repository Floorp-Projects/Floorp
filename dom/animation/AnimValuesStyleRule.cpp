/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimValuesStyleRule.h"
#include "nsRuleData.h"
#include "nsStyleContext.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(AnimValuesStyleRule, nsIStyleRule)

void
AnimValuesStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  nsStyleContext *contextParent = aRuleData->mStyleContext->GetParent();
  if (contextParent && contextParent->HasPseudoElementData()) {
    // Don't apply transitions or animations to things inside of
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.

    // Prevent structs from being cached on the rule node since we're inside
    // a pseudo-element, as we could determine cacheability differently
    // when walking the rule tree for a style context that is not inside
    // a pseudo-element.  Note that nsRuleNode::GetStyle##name_ and GetStyleData
    // will never look at cached structs when we're animating things inside
    // a pseduo-element, so that we don't incorrectly return a struct that
    // is only appropriate for non-pseudo-elements.
    aRuleData->mConditions.SetUncacheable();
    return;
  }

  for (auto iter = mAnimationValues.ConstIter(); !iter.Done(); iter.Next()) {
    nsCSSPropertyID property = static_cast<nsCSSPropertyID>(iter.Key());
    if (aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(
                             nsCSSProps::kSIDTable[property])) {
      nsCSSValue *prop = aRuleData->ValueFor(property);
      if (prop->GetUnit() == eCSSUnit_Null) {
        DebugOnly<bool> ok =
          StyleAnimationValue::UncomputeValue(property, iter.Data(),
                                              *prop);
        MOZ_ASSERT(ok, "could not store computed value");
      }
    }
  }
}

bool
AnimValuesStyleRule::MightMapInheritedStyleData()
{
  return mStyleBits & NS_STYLE_INHERITED_STRUCT_MASK;
}

bool
AnimValuesStyleRule::GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                                   nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
}

void
AnimValuesStyleRule::AddValue(nsCSSPropertyID aProperty,
                              const StyleAnimationValue &aValue)
{
  MOZ_ASSERT(aProperty != eCSSProperty_UNKNOWN,
             "Unexpected css property");
  mAnimationValues.Put(aProperty, aValue);
  mStyleBits |=
    nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[aProperty]);
}

void
AnimValuesStyleRule::AddValue(nsCSSPropertyID aProperty,
                              StyleAnimationValue&& aValue)
{
  MOZ_ASSERT(aProperty != eCSSProperty_UNKNOWN,
             "Unexpected css property");
  mAnimationValues.Put(aProperty, Move(aValue));
  mStyleBits |=
    nsCachedStyleData::GetBitForSID(nsCSSProps::kSIDTable[aProperty]);
}

bool
AnimValuesStyleRule::GetValue(nsCSSPropertyID aProperty,
                              StyleAnimationValue& aValue) const
{
  return mAnimationValues.Get(aProperty, &aValue);
}

#ifdef DEBUG
void
AnimValuesStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }
  str.AppendLiteral("[anim values] { ");
  for (auto iter = mAnimationValues.ConstIter(); !iter.Done(); iter.Next()) {
    nsCSSPropertyID property = static_cast<nsCSSPropertyID>(iter.Key());
    str.Append(nsCSSProps::GetStringValue(property));
    str.AppendLiteral(": ");
    nsAutoString value;
    Unused <<
      StyleAnimationValue::UncomputeValue(property, iter.Data(), value);
    AppendUTF16toUTF8(value, str);
    str.AppendLiteral("; ");
  }
  str.AppendLiteral("}\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

} // namespace mozilla

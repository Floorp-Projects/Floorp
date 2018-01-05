/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * temporary (expanded) representation of property-value pairs used to
 * hold data from matched rules during style data computation.
 */

#ifndef nsRuleData_h_
#define nsRuleData_h_

#include "mozilla/CSSVariableDeclarations.h"
#include "mozilla/GenericSpecifiedValues.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "mozilla/SheetType.h"
#include "nsAutoPtr.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsStyleStructFwd.h"

class nsPresContext;
struct nsRuleData;

namespace mozilla {
class GeckoStyleContext;
} // namespace mozilla

typedef void (*nsPostResolveFunc)(void* aStyleStruct, nsRuleData* aData);

struct nsRuleData final : mozilla::GenericSpecifiedValues
{
  mozilla::RuleNodeCacheConditions mConditions;
  bool mIsImportantRule;
  mozilla::SheetType mLevel;
  mozilla::GeckoStyleContext* const mStyleContext;
  nsPresContext* const mPresContext;

  // We store nsCSSValues needed to compute the data for one or more
  // style structs (specified by the bitfield mSIDs).  These are stored
  // in a single array allocation (which our caller allocates; see
  // AutoCSSValueArray)   The offset of each property |prop| in
  // mValueStorage is the sum of
  // mValueOffsets[nsCSSProps::kSIDTable[prop]] and
  // nsCSSProps::PropertyIndexInStruct(prop).  The only place we gather
  // more than one style struct's data at a time is
  // nsRuleNode::HasAuthorSpecifiedRules; therefore some code that we
  // know is not called from HasAuthorSpecifiedRules assumes that the
  // mValueOffsets for the one struct in mSIDs is zero.
  nsCSSValue* const mValueStorage; // our user owns this array
  size_t mValueOffsets[nsStyleStructID_Length];

  nsAutoPtr<mozilla::CSSVariableDeclarations> mVariables;

  nsRuleData(uint32_t aSIDs,
             nsCSSValue* aValueStorage,
             mozilla::GeckoStyleContext* aStyleContext);

#ifdef DEBUG
  ~nsRuleData();
#else
  ~nsRuleData() {}
#endif

  /**
   * Return a pointer to the value object within |this| corresponding
   * to property |aProperty|.
   *
   * This function must only be called if the given property is in
   * mSIDs.
   */
  nsCSSValue* ValueFor(nsCSSPropertyID aProperty)
  {
    MOZ_ASSERT(aProperty < eCSSProperty_COUNT_no_shorthands,
               "invalid or shorthand property");

    nsStyleStructID sid = nsCSSProps::kSIDTable[aProperty];
    size_t indexInStruct = nsCSSProps::PropertyIndexInStruct(aProperty);

    // This should really be nsCachedStyleData::GetBitForSID, but we can't
    // include that here since it includes us.
    MOZ_ASSERT(mSIDs & (1 << sid),
               "calling nsRuleData::ValueFor on property not in mSIDs");
    MOZ_ASSERT(indexInStruct != size_t(-1), "logical property");

    return mValueStorage + mValueOffsets[sid] + indexInStruct;
  }

  const nsCSSValue* ValueFor(nsCSSPropertyID aProperty) const
  {
    return const_cast<nsRuleData*>(this)->ValueFor(aProperty);
  }

  /**
   * Getters like ValueFor(aProperty), but for each property by name
   * (ValueForBackgroundColor, etc.), and more efficient than ValueFor.
   * These use the names used for the property on DOM interfaces (the
   * 'method' field in nsCSSPropList.h).
   *
   * Like ValueFor(), the caller must check that the property is within
   * mSIDs.
   */
  #define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) privatename_
  #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,        \
                   kwtable_, stylestruct_, stylestructoffset_, animtype_)    \
    nsCSSValue* ValueFor##method_() {                                        \
      MOZ_ASSERT(mSIDs & NS_STYLE_INHERIT_BIT(stylestruct_),                 \
                 "Calling nsRuleData::ValueFor" #method_ " without "         \
                 "NS_STYLE_INHERIT_BIT(" #stylestruct_ " in mSIDs.");        \
      nsStyleStructID sid = eStyleStruct_##stylestruct_;                     \
      size_t indexInStruct =                                                 \
        nsCSSProps::PropertyIndexInStruct(eCSSProperty_##id_);               \
      MOZ_ASSERT(indexInStruct != size_t(-1),                                \
                 "logical property");                                        \
      return mValueStorage + mValueOffsets[sid] + indexInStruct;             \
    }                                                                        \
    const nsCSSValue* ValueFor##method_() const {                            \
      return const_cast<nsRuleData*>(this)->ValueFor##method_();             \
    }
  #define CSS_PROP_LIST_EXCLUDE_LOGICAL
  #include "nsCSSPropList.h"
  #undef CSS_PROP_LIST_EXCLUDE_LOGICAL
  #undef CSS_PROP
  #undef CSS_PROP_PUBLIC_OR_PRIVATE

  // GenericSpecifiedValues overrides
  bool PropertyIsSet(nsCSSPropertyID aId)
  {
    return ValueFor(aId)->GetUnit() != eCSSUnit_Null;
  }

  void SetIdentStringValue(nsCSSPropertyID aId, const nsString& aValue)
  {
    ValueFor(aId)->SetStringValue(aValue, eCSSUnit_Ident);
  }

  void SetIdentAtomValue(nsCSSPropertyID aId, nsAtom* aValue)
  {
    RefPtr<nsAtom> atom = aValue;
    ValueFor(aId)->SetAtomIdentValue(atom.forget());
  }

  void SetKeywordValue(nsCSSPropertyID aId, int32_t aValue)
  {
    ValueFor(aId)->SetIntValue(aValue, eCSSUnit_Enumerated);
  }

  void SetIntValue(nsCSSPropertyID aId, int32_t aValue)
  {
    ValueFor(aId)->SetIntValue(aValue, eCSSUnit_Integer);
  }

  void SetPixelValue(nsCSSPropertyID aId, float aValue)
  {
    ValueFor(aId)->SetFloatValue(aValue, eCSSUnit_Pixel);
  }

  void SetLengthValue(nsCSSPropertyID aId, nsCSSValue aValue)
  {
    nsCSSValue* val = ValueFor(aId);
    *val = aValue;
  }

  void SetNumberValue(nsCSSPropertyID aId, float aValue)
  {
    ValueFor(aId)->SetFloatValue(aValue, eCSSUnit_Number);
  }

  void SetPercentValue(nsCSSPropertyID aId, float aValue)
  {
    ValueFor(aId)->SetPercentValue(aValue);
  }

  void SetAutoValue(nsCSSPropertyID aId) {
    ValueFor(aId)->SetAutoValue();
  }

  void SetCurrentColor(nsCSSPropertyID aId)
  {
    ValueFor(aId)->SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
  }

  void SetColorValue(nsCSSPropertyID aId, nscolor aValue)
  {
    ValueFor(aId)->SetColorValue(aValue);
  }

  void SetFontFamily(const nsString& aValue);
  void SetTextDecorationColorOverride();
  void SetBackgroundImage(nsAttrValue& aValue);

private:
  inline size_t GetPoisonOffset();
};

#endif

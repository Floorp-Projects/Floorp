/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * temporary (expanded) representation of property-value pairs used to
 * hold data from matched rules during style data computation.
 */

#ifndef nsRuleData_h_
#define nsRuleData_h_

#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsStyleStructFwd.h"

class nsPresContext;
class nsStyleContext;
struct nsRuleData;

typedef void (*nsPostResolveFunc)(void* aStyleStruct, nsRuleData* aData);

struct nsRuleData
{
  const uint32_t mSIDs;
  bool mCanStoreInRuleTree;
  bool mIsImportantRule;
  uint16_t mLevel; // an nsStyleSet::sheetType
  nsPresContext* const mPresContext;
  nsStyleContext* const mStyleContext;

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

  nsRuleData(uint32_t aSIDs, nsCSSValue* aValueStorage,
             nsPresContext* aContext, nsStyleContext* aStyleContext);

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
  nsCSSValue* ValueFor(nsCSSProperty aProperty)
  {
    NS_ABORT_IF_FALSE(aProperty < eCSSProperty_COUNT_no_shorthands,
                      "invalid or shorthand property");

    nsStyleStructID sid = nsCSSProps::kSIDTable[aProperty];
    size_t indexInStruct = nsCSSProps::PropertyIndexInStruct(aProperty);

    // This should really be nsCachedStyleData::GetBitForSID, but we can't
    // include that here since it includes us.
    NS_ABORT_IF_FALSE(mSIDs & (1 << sid),
                      "calling nsRuleData::ValueFor on property not in mSIDs");
    NS_ABORT_IF_FALSE(sid != eStyleStruct_BackendOnly &&
                      indexInStruct != size_t(-1),
                      "backend-only property");

    return mValueStorage + mValueOffsets[sid] + indexInStruct;
  }

  const nsCSSValue* ValueFor(nsCSSProperty aProperty) const {
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
      NS_ABORT_IF_FALSE(mSIDs & NS_STYLE_INHERIT_BIT(stylestruct_),          \
                        "Calling nsRuleData::ValueFor" #method_ " without "  \
                        "NS_STYLE_INHERIT_BIT(" #stylestruct_ " in mSIDs."); \
      nsStyleStructID sid = eStyleStruct_##stylestruct_;                     \
      size_t indexInStruct =                                                 \
        nsCSSProps::PropertyIndexInStruct(eCSSProperty_##id_);               \
      NS_ABORT_IF_FALSE(sid != eStyleStruct_BackendOnly &&                   \
                        indexInStruct != size_t(-1),                         \
                        "backend-only property");                            \
      return mValueStorage + mValueOffsets[sid] + indexInStruct;             \
    }                                                                        \
    const nsCSSValue* ValueFor##method_() const {                            \
      return const_cast<nsRuleData*>(this)->ValueFor##method_();             \
    }
  #define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, pref_,           \
                             parsevariant_, kwtable_)                        \
    /* empty; backend-only structs are not in nsRuleData  */
  #include "nsCSSPropList.h"
  #undef CSS_PROP
  #undef CSS_PROP_PUBLIC_OR_PRIVATE
  #undef CSS_PROP_BACKENDONLY

private:
  inline size_t GetPoisonOffset();

};

#endif

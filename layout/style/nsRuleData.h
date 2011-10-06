/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * temporary (expanded) representation of property-value pairs used to
 * hold data from matched rules during style data computation.
 */

#ifndef nsRuleData_h_
#define nsRuleData_h_

#include "nsCSSProps.h"
#include "nsStyleStructFwd.h"

class nsPresContext;
class nsStyleContext;
struct nsRuleData;

typedef void (*nsPostResolveFunc)(void* aStyleStruct, nsRuleData* aData);

struct nsRuleData
{
  const PRUint32 mSIDs;
  bool mCanStoreInRuleTree;
  bool mIsImportantRule;
  PRUint8 mLevel; // an nsStyleSet::sheetType
  nsPresContext* const mPresContext;
  nsStyleContext* const mStyleContext;
  const nsPostResolveFunc mPostResolveCallback;

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

  nsRuleData(PRUint32 aSIDs, nsCSSValue* aValueStorage,
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
  #define CSS_PROP_DOMPROP_PREFIXED(prop_) prop_
  #define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,     \
                   stylestruct_, stylestructoffset_, animtype_)              \
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
  #define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, parsevariant_,   \
                               kwtable_)                                     \
    /* empty; backend-only structs are not in nsRuleData  */
  #include "nsCSSPropList.h"
  #undef CSS_PROP
  #undef CSS_PROP_DOMPROP_PREFIXED
  #undef CSS_PROP_BACKENDONLY

private:
  inline size_t GetPoisonOffset();

};

#endif

/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the features that media queries can test */

#ifndef nsMediaFeatures_h_
#define nsMediaFeatures_h_

#include "nsCSSProps.h"

class nsIAtom;
class nsPresContext;
class nsCSSValue;

struct nsMediaFeature;
typedef nsresult
(* nsMediaFeatureValueGetter)(nsPresContext* aPresContext,
                              const nsMediaFeature* aFeature,
                              nsCSSValue& aResult);

struct nsMediaFeature {
    nsIAtom **mName; // extra indirection to point to nsGkAtoms members

    enum RangeType { eMinMaxAllowed, eMinMaxNotAllowed };
    RangeType mRangeType;

    enum ValueType {
        // All value types allow eCSSUnit_Null to indicate that no value
        // was given (in addition to the types listed below).
        eLength,     // values are such that nsCSSValue::IsLengthUnit() is true
        eInteger,    // values are eCSSUnit_Integer
        eFloat,      // values are eCSSUnit_Number
        eBoolInteger,// values are eCSSUnit_Integer (0, -0, or 1 only)
        eIntRatio,   // values are eCSSUnit_Array of two eCSSUnit_Integer
        eResolution, // values are in eCSSUnit_Inch (for dpi),
                     //   eCSSUnit_Pixel (for dppx), or
                     //   eCSSUnit_Centimeter (for dpcm)
        eEnumerated, // values are eCSSUnit_Enumerated (uses keyword table)
        eIdent       // values are eCSSUnit_Ident
        // Note that a number of pieces of code (both for parsing and
        // for matching of valueless expressions) assume that all numeric
        // value types cannot be negative.  The parsing code also does
        // not allow zeros in eIntRatio types.
    };
    ValueType mValueType;

    union {
      // In static arrays, it's the first member that's initialized.  We
      // need that to be void* so we can initialize both other types.
      // This member should never be accessed by name.
      const void* mInitializer_;
      // If mValueType == eEnumerated:  const int32_t*: keyword table in
      //   the same format as the keyword tables in nsCSSProps.
      const nsCSSProps::KTableValue* mKeywordTable;
      // If mGetter == GetSystemMetric (which implies mValueType ==
      //   eBoolInteger): nsIAtom * const *, for the system metric.
      nsIAtom * const * mMetric;
    } mData;

    // A function that returns the current value for this feature for a
    // given presentation.  If it returns eCSSUnit_Null, the feature is
    // not present.
    nsMediaFeatureValueGetter mGetter;
};

class nsMediaFeatures {
public:
    // Terminated with an entry whose mName is null.
    static const nsMediaFeature features[];
};

#endif /* !defined(nsMediaFeatures_h_) */

/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsMediaFeatures.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* the features that media queries can test */

#ifndef nsMediaFeatures_h_
#define nsMediaFeatures_h_

#include "nscore.h"

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
        eResolution, // values are in eCSSUnit_Inch (for dpi) or
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
      // If mValueType == eEnumerated:  const PRInt32*: keyword table in
      //   the same format as the keyword tables in nsCSSProps.
      const PRInt32* mKeywordTable;
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

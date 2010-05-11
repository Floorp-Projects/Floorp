/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is CSSCalc.h.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
#ifndef CSSCalc_h_
#define CSSCalc_h_

#include "nsCSSValue.h"
#include <math.h>

namespace mozilla {

namespace css {

/**
 * ComputeCalc computes the result of a calc() expression tree.
 *
 * It is templatized over a CalcOps class that is expected to provide:
 *
 *   typedef ... result_type;
 *
 *   static result_type
 *   MergeAdditive(nsCSSUnit aCalcFunction,
 *                 result_type aValue1, result_type aValue2);
 *
 *   static result_type
 *   MergeMultiplicativeL(nsCSSUnit aCalcFunction,
 *                        float aValue1, result_type aValue2);
 *
 *   static result_type
 *   MergeMultiplicativeR(nsCSSUnit aCalcFunction,
 *                        result_type aValue1, float aValue2);
 *
 *   struct ComputeData { ... };
 *
 *   static result_type ComputeLeafValue(const nsCSSValue& aValue,
 *                                       const ComputeData& aClosure);
 *
 *   static float ComputeNumber(const nsCSSValue& aValue);
 *
 * The CalcOps methods might compute the calc() expression down to a
 * number, reduce some parts of it to a number but replicate other
 * parts, or produce a tree with a different data structure (for
 * example, nsCSS* for specified values vs nsStyle* for computed
 * values).
 *
 * For each leaf in the calc() expression, ComputeCalc will call either
 * ComputeNumber (when the leaf is the left side of a Times_L or the
 * right side of a Times_R or Divided) or ComputeLeafValue (otherwise).
 * (The CalcOps in the CSS parser that reduces purely numeric
 * expressions in turn calls ComputeCalc on numbers; other ops can
 * presume that expressions in the number positions have already been
 * normalized to a single numeric value and derive from
 * NumbersAlreadyNormalizedCalcOps.)
 *
 * For non-leaves, one of the Merge functions will be called:
 *   MergeAdditive for Plus, Minus, Minimum, and Maximum
 *   MergeMultiplicativeL for Times_L (number * value)
 *   MergeMultiplicativeR for Times_R (value * number) and Divided
 */
template <class CalcOps>
static typename CalcOps::result_type
ComputeCalc(const nsCSSValue& aValue,
            const typename CalcOps::ComputeData &aClosure)
{
  switch (aValue.GetUnit()) {
    case eCSSUnit_Calc: {
      nsCSSValue::Array *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 1, "unexpected length");
      return ComputeCalc<CalcOps>(arr->Item(0), aClosure);
    }
    case eCSSUnit_Calc_Plus:
    case eCSSUnit_Calc_Minus: {
      nsCSSValue::Array *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 2, "unexpected length");
      typename CalcOps::result_type
        lhs = ComputeCalc<CalcOps>(arr->Item(0), aClosure),
        rhs = ComputeCalc<CalcOps>(arr->Item(1), aClosure);
      return CalcOps::MergeAdditive(aValue.GetUnit(), lhs, rhs);
    }
    case eCSSUnit_Calc_Times_L: {
      nsCSSValue::Array *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 2, "unexpected length");
      float lhs = CalcOps::ComputeNumber(arr->Item(0));
      typename CalcOps::result_type rhs =
        ComputeCalc<CalcOps>(arr->Item(1), aClosure);
      return CalcOps::MergeMultiplicativeL(aValue.GetUnit(), lhs, rhs);
    }
    case eCSSUnit_Calc_Times_R:
    case eCSSUnit_Calc_Divided: {
      nsCSSValue::Array *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 2, "unexpected length");
      typename CalcOps::result_type lhs =
        ComputeCalc<CalcOps>(arr->Item(0), aClosure);
      float rhs = CalcOps::ComputeNumber(arr->Item(1));
      return CalcOps::MergeMultiplicativeR(aValue.GetUnit(), lhs, rhs);
    }
    case eCSSUnit_Calc_Minimum:
    case eCSSUnit_Calc_Maximum: {
      nsCSSValue::Array *arr = aValue.GetArrayValue();
      typename CalcOps::result_type result =
        ComputeCalc<CalcOps>(arr->Item(0), aClosure);
      for (PRUint32 i = 1, i_end = arr->Count(); i < i_end; ++i) {
        typename CalcOps::result_type tmp =
          ComputeCalc<CalcOps>(arr->Item(i), aClosure);
        result = CalcOps::MergeAdditive(aValue.GetUnit(), result, tmp);
      }
      return result;
    }
    default: {
      return CalcOps::ComputeLeafValue(aValue, aClosure);
    }
  }
}

/**
 * Basic*CalcOps provide a partial implementation of the CalcOps
 * template parameter to ComputeCalc, for those callers whose merging
 * just consists of mathematics (rather than tree construction).
 */
template <typename T>
struct BasicCalcOpsAdditive
{
  typedef T result_type;

  static result_type
  MergeAdditive(nsCSSUnit aCalcFunction,
                result_type aValue1, result_type aValue2)
  {
    if (aCalcFunction == eCSSUnit_Calc_Plus) {
      return aValue1 + aValue2;
    }
    if (aCalcFunction == eCSSUnit_Calc_Minus) {
      return aValue1 - aValue2;
    }
    if (aCalcFunction == eCSSUnit_Calc_Minimum) {
      return NS_MIN(aValue1, aValue2);
    }
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Maximum,
                      "unexpected unit");
    return NS_MAX(aValue1, aValue2);
  }
};

struct BasicCoordCalcOps : public BasicCalcOpsAdditive<nscoord>
{
  static result_type
  MergeMultiplicativeL(nsCSSUnit aCalcFunction,
                       float aValue1, result_type aValue2)
  {
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Times_L,
                      "unexpected unit");
    return NSToCoordRound(aValue1 * aValue2);
  }

  static result_type
  MergeMultiplicativeR(nsCSSUnit aCalcFunction,
                       result_type aValue1, float aValue2)
  {
    if (aCalcFunction == eCSSUnit_Calc_Times_R) {
      return NSToCoordRound(aValue1 * aValue2);
    }
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Divided,
                      "unexpected unit");
    return NSToCoordRound(aValue1 / aValue2);
  }
};

struct BasicFloatCalcOps : public BasicCalcOpsAdditive<float>
{
  static result_type
  MergeMultiplicativeL(nsCSSUnit aCalcFunction,
                       float aValue1, result_type aValue2)
  {
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Times_L,
                      "unexpected unit");
    return aValue1 * aValue2;
  }

  static result_type
  MergeMultiplicativeR(nsCSSUnit aCalcFunction,
                       result_type aValue1, float aValue2)
  {
    if (aCalcFunction == eCSSUnit_Calc_Times_R) {
      return aValue1 * aValue2;
    }
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Divided,
                      "unexpected unit");
    return aValue1 / aValue2;
  }
};

/**
 * A ComputeNumber implementation for callers that can assume numbers
 * are already normalized (i.e., anything past the parser).
 */
struct NumbersAlreadyNormalizedOps
{
  static float ComputeNumber(const nsCSSValue& aValue)
  {
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Number, "unexpected unit");
    return aValue.GetFloatValue();
  }
};

}

}

#endif /* !defined(CSSCalc_h_) */

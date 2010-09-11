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
#include "nsStyleCoord.h"
#include <math.h>

namespace mozilla {

namespace css {

/**
 * ComputeCalc computes the result of a calc() expression tree.
 *
 * It is templatized over a CalcOps class that is expected to provide:
 *
 *   // input_type and input_array_type have a bunch of very specific
 *   // expectations (which happen to be met by two classes (nsCSSValue
 *   // and nsStyleCoord).  There must be methods (roughly):
 *   //   input_array_type* input_type::GetArrayValue();
 *   //   PRUint32 input_array_type::Count() const;
 *   //   input_type& input_array_type::Item(PRUint32);
 *   typedef ... input_type;
 *   typedef ... input_array_type;
 *
 *   typedef ... result_type;
 *
 *   // GetUnit(avalue) must return the correct nsCSSUnit for any
 *   // value that represents a calc tree node (eCSSUnit_Calc*).  For
 *   // other nodes, it may return any non eCSSUnit_Calc* unit.
 *   static nsCSSUnit GetUnit(const input_type& aValue);
 *
 *   result_type
 *   MergeAdditive(nsCSSUnit aCalcFunction,
 *                 result_type aValue1, result_type aValue2);
 *
 *   result_type
 *   MergeMultiplicativeL(nsCSSUnit aCalcFunction,
 *                        float aValue1, result_type aValue2);
 *
 *   result_type
 *   MergeMultiplicativeR(nsCSSUnit aCalcFunction,
 *                        result_type aValue1, float aValue2);
 *
 *   result_type
 *   ComputeLeafValue(const input_type& aValue);
 *
 *   float
 *   ComputeNumber(const input_type& aValue);
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
 *   MergeAdditive for Plus and Minus
 *   MergeMultiplicativeL for Times_L (number * value)
 *   MergeMultiplicativeR for Times_R (value * number) and Divided
 */
template <class CalcOps>
static typename CalcOps::result_type
ComputeCalc(const typename CalcOps::input_type& aValue, CalcOps &aOps)
{
  switch (CalcOps::GetUnit(aValue)) {
    case eCSSUnit_Calc: {
      typename CalcOps::input_array_type *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 1, "unexpected length");
      return ComputeCalc(arr->Item(0), aOps);
    }
    case eCSSUnit_Calc_Plus:
    case eCSSUnit_Calc_Minus: {
      typename CalcOps::input_array_type *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 2, "unexpected length");
      typename CalcOps::result_type lhs = ComputeCalc(arr->Item(0), aOps),
                                    rhs = ComputeCalc(arr->Item(1), aOps);
      return aOps.MergeAdditive(CalcOps::GetUnit(aValue), lhs, rhs);
    }
    case eCSSUnit_Calc_Times_L: {
      typename CalcOps::input_array_type *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 2, "unexpected length");
      float lhs = aOps.ComputeNumber(arr->Item(0));
      typename CalcOps::result_type rhs = ComputeCalc(arr->Item(1), aOps);
      return aOps.MergeMultiplicativeL(CalcOps::GetUnit(aValue), lhs, rhs);
    }
    case eCSSUnit_Calc_Times_R:
    case eCSSUnit_Calc_Divided: {
      typename CalcOps::input_array_type *arr = aValue.GetArrayValue();
      NS_ABORT_IF_FALSE(arr->Count() == 2, "unexpected length");
      typename CalcOps::result_type lhs = ComputeCalc(arr->Item(0), aOps);
      float rhs = aOps.ComputeNumber(arr->Item(1));
      return aOps.MergeMultiplicativeR(CalcOps::GetUnit(aValue), lhs, rhs);
    }
    default: {
      return aOps.ComputeLeafValue(aValue);
    }
  }
}

#define CHECK_UNIT(u_)                                                        \
  PR_STATIC_ASSERT(int(eCSSUnit_##u_) + 14 == int(eStyleUnit_##u_));          \
  PR_STATIC_ASSERT(eCSSUnit_##u_ >= eCSSUnit_Calc);                           \
  PR_STATIC_ASSERT(eCSSUnit_##u_ <= eCSSUnit_Calc_Divided);

CHECK_UNIT(Calc)
CHECK_UNIT(Calc_Plus)
CHECK_UNIT(Calc_Minus)
CHECK_UNIT(Calc_Times_L)
CHECK_UNIT(Calc_Times_R)
CHECK_UNIT(Calc_Divided)

#undef CHECK_UNIT

inline nsStyleUnit
ConvertCalcUnit(nsCSSUnit aUnit)
{
  NS_ABORT_IF_FALSE(eCSSUnit_Calc <= aUnit &&
                    aUnit <= eCSSUnit_Calc_Divided, "out of range");
  return nsStyleUnit(aUnit + 14);
}

inline nsCSSUnit
ConvertCalcUnit(nsStyleUnit aUnit)
{
  NS_ABORT_IF_FALSE(eStyleUnit_Calc <= aUnit &&
                    aUnit <= eStyleUnit_Calc_Divided, "out of range");
  return nsCSSUnit(aUnit - 14);
}

/**
 * The input unit operation for input_type being nsCSSValue.
 */
struct CSSValueInputCalcOps
{
  typedef nsCSSValue input_type;
  typedef nsCSSValue::Array input_array_type;

  static nsCSSUnit GetUnit(const nsCSSValue& aValue)
  {
    return aValue.GetUnit();
  }

};

/**
 * The input unit operation for input_type being nsStyleCoord
 */
struct StyleCoordInputCalcOps
{
  typedef nsStyleCoord input_type;
  typedef nsStyleCoord::Array input_array_type;

  static nsCSSUnit GetUnit(const nsStyleCoord& aValue)
  {
    if (aValue.IsCalcUnit()) {
      return css::ConvertCalcUnit(aValue.GetUnit());
    }
    return eCSSUnit_Null;
  }

  float ComputeNumber(const nsStyleCoord& aValue)
  {
    NS_ABORT_IF_FALSE(PR_FALSE, "SpecifiedToComputedCalcOps should not "
                                "leave numbers in structure");
    return 0.0f;
  }
};

/**
 * Basic*CalcOps provide a partial implementation of the CalcOps
 * template parameter to ComputeCalc, for those callers whose merging
 * just consists of mathematics (rather than tree construction).
 */

struct BasicCoordCalcOps
{
  typedef nscoord result_type;

  result_type
  MergeAdditive(nsCSSUnit aCalcFunction,
                result_type aValue1, result_type aValue2)
  {
    if (aCalcFunction == eCSSUnit_Calc_Plus) {
      return NSCoordSaturatingAdd(aValue1, aValue2);
    }
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Minus,
                      "unexpected unit");
    return NSCoordSaturatingSubtract(aValue1, aValue2, 0);
  }

  result_type
  MergeMultiplicativeL(nsCSSUnit aCalcFunction,
                       float aValue1, result_type aValue2)
  {
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Times_L,
                      "unexpected unit");
    return NSCoordSaturatingMultiply(aValue2, aValue1);
  }

  result_type
  MergeMultiplicativeR(nsCSSUnit aCalcFunction,
                       result_type aValue1, float aValue2)
  {
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Times_R ||
                      aCalcFunction == eCSSUnit_Calc_Divided,
                      "unexpected unit");
    if (aCalcFunction == eCSSUnit_Calc_Divided) {
      aValue2 = 1.0f / aValue2;
    }
    return NSCoordSaturatingMultiply(aValue1, aValue2);
  }
};

struct BasicFloatCalcOps
{
  typedef float result_type;

  result_type
  MergeAdditive(nsCSSUnit aCalcFunction,
                result_type aValue1, result_type aValue2)
  {
    if (aCalcFunction == eCSSUnit_Calc_Plus) {
      return aValue1 + aValue2;
    }
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Minus,
                      "unexpected unit");
    return aValue1 - aValue2;
  }

  result_type
  MergeMultiplicativeL(nsCSSUnit aCalcFunction,
                       float aValue1, result_type aValue2)
  {
    NS_ABORT_IF_FALSE(aCalcFunction == eCSSUnit_Calc_Times_L,
                      "unexpected unit");
    return aValue1 * aValue2;
  }

  result_type
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
struct NumbersAlreadyNormalizedOps : public CSSValueInputCalcOps
{
  float ComputeNumber(const nsCSSValue& aValue)
  {
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Number, "unexpected unit");
    return aValue.GetFloatValue();
  }
};

/**
 * SerializeCalc appends the serialization of aValue to a string.
 *
 * It is templatized over a CalcOps class that is expected to provide:
 *
 *   // input_type and input_array_type have a bunch of very specific
 *   // expectations (which happen to be met by two classes (nsCSSValue
 *   // and nsStyleCoord).  There must be methods (roughly):
 *   //   input_array_type* input_type::GetArrayValue();
 *   //   PRUint32 input_array_type::Count() const;
 *   //   input_type& input_array_type::Item(PRUint32);
 *   typedef ... input_type;
 *   typedef ... input_array_type;
 *
 *   static nsCSSUnit GetUnit(const input_type& aValue);
 *
 *   void Append(const char* aString);
 *   void AppendLeafValue(const input_type& aValue);
 *   void AppendNumber(const input_type& aValue);
 *
 * Data structures given may or may not have a toplevel eCSSUnit_Calc
 * node representing a calc whose toplevel is not min() or max().
 */

template <class CalcOps>
static void
SerializeCalcInternal(const typename CalcOps::input_type& aValue, CalcOps &aOps);

// Serialize the toplevel value in a calc() tree.  See big comment
// above.
template <class CalcOps>
static void
SerializeCalc(const typename CalcOps::input_type& aValue, CalcOps &aOps)
{
  aOps.Append("-moz-calc(");
  nsCSSUnit unit = CalcOps::GetUnit(aValue);
  if (unit == eCSSUnit_Calc) {
    const typename CalcOps::input_array_type *array = aValue.GetArrayValue();
    NS_ABORT_IF_FALSE(array->Count() == 1, "unexpected length");
    SerializeCalcInternal(array->Item(0), aOps);
  } else {
    SerializeCalcInternal(aValue, aOps);
  }
  aOps.Append(")");
}

static inline PRBool
IsCalcAdditiveUnit(nsCSSUnit aUnit)
{
  return aUnit == eCSSUnit_Calc_Plus ||
         aUnit == eCSSUnit_Calc_Minus;
}

static inline PRBool
IsCalcMultiplicativeUnit(nsCSSUnit aUnit)
{
  return aUnit == eCSSUnit_Calc_Times_L ||
         aUnit == eCSSUnit_Calc_Times_R ||
         aUnit == eCSSUnit_Calc_Divided;
}

// Serialize a non-toplevel value in a calc() tree.  See big comment
// above.
template <class CalcOps>
/* static */ void
SerializeCalcInternal(const typename CalcOps::input_type& aValue, CalcOps &aOps)
{
  nsCSSUnit unit = CalcOps::GetUnit(aValue);
  if (IsCalcAdditiveUnit(unit)) {
    const typename CalcOps::input_array_type *array = aValue.GetArrayValue();
    NS_ABORT_IF_FALSE(array->Count() == 2, "unexpected length");

    SerializeCalcInternal(array->Item(0), aOps);

    if (eCSSUnit_Calc_Plus == unit) {
      aOps.Append(" + ");
    } else {
      NS_ABORT_IF_FALSE(eCSSUnit_Calc_Minus == unit, "unexpected unit");
      aOps.Append(" - ");
    }

    PRBool needParens = IsCalcAdditiveUnit(CalcOps::GetUnit(array->Item(1)));
    if (needParens) {
      aOps.Append("(");
    }
    SerializeCalcInternal(array->Item(1), aOps);
    if (needParens) {
      aOps.Append(")");
    }
  } else if (IsCalcMultiplicativeUnit(unit)) {
    const typename CalcOps::input_array_type *array = aValue.GetArrayValue();
    NS_ABORT_IF_FALSE(array->Count() == 2, "unexpected length");

    PRBool needParens = IsCalcAdditiveUnit(CalcOps::GetUnit(array->Item(0)));
    if (needParens) {
      aOps.Append("(");
    }
    if (unit == eCSSUnit_Calc_Times_L) {
      aOps.AppendNumber(array->Item(0));
    } else {
      SerializeCalcInternal(array->Item(0), aOps);
    }
    if (needParens) {
      aOps.Append(")");
    }

    if (eCSSUnit_Calc_Times_L == unit || eCSSUnit_Calc_Times_R == unit) {
      aOps.Append(" * ");
    } else {
      NS_ABORT_IF_FALSE(eCSSUnit_Calc_Divided == unit, "unexpected unit");
      aOps.Append(" / ");
    }

    nsCSSUnit subUnit = CalcOps::GetUnit(array->Item(1));
    needParens = IsCalcAdditiveUnit(subUnit) ||
                 IsCalcMultiplicativeUnit(subUnit);
    if (needParens) {
      aOps.Append("(");
    }
    if (unit == eCSSUnit_Calc_Times_L) {
      SerializeCalcInternal(array->Item(1), aOps);
    } else {
      aOps.AppendNumber(array->Item(1));
    }
    if (needParens) {
      aOps.Append(")");
    }
  } else {
    aOps.AppendLeafValue(aValue);
  }
}

}

}

#endif /* !defined(CSSCalc_h_) */

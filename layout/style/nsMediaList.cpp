/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of media lists used when linking to style sheets or by
 * @media rules
 */

#include "nsMediaList.h"

#include "nsCSSParser.h"
#include "nsCSSRules.h"
#include "nsMediaFeatures.h"
#include "nsRuleNode.h"

using namespace mozilla;

template <class Numeric>
int32_t DoCompare(Numeric a, Numeric b)
{
  if (a == b)
    return 0;
  if (a < b)
    return -1;
  return 1;
}

bool
nsMediaExpression::Matches(nsPresContext *aPresContext,
                           const nsCSSValue& aActualValue) const
{
  const nsCSSValue& actual = aActualValue;
  const nsCSSValue& required = mValue;

  // If we don't have the feature, the match fails.
  if (actual.GetUnit() == eCSSUnit_Null) {
    return false;
  }

  // If the expression had no value to match, the match succeeds,
  // unless the value is an integer 0 or a zero length.
  if (required.GetUnit() == eCSSUnit_Null) {
    if (actual.GetUnit() == eCSSUnit_Integer)
      return actual.GetIntValue() != 0;
    if (actual.IsLengthUnit())
      return actual.GetFloatValue() != 0;
    return true;
  }

  NS_ASSERTION(mFeature->mRangeType == nsMediaFeature::eMinMaxAllowed ||
               mRange == nsMediaExpression::eEqual, "yikes");
  int32_t cmp; // -1 (actual < required)
               //  0 (actual == required)
               //  1 (actual > required)
  switch (mFeature->mValueType) {
    case nsMediaFeature::eLength:
      {
        NS_ASSERTION(actual.IsLengthUnit(), "bad actual value");
        NS_ASSERTION(required.IsLengthUnit(), "bad required value");
        nscoord actualCoord = nsRuleNode::CalcLengthWithInitialFont(
                                aPresContext, actual);
        nscoord requiredCoord = nsRuleNode::CalcLengthWithInitialFont(
                                  aPresContext, required);
        cmp = DoCompare(actualCoord, requiredCoord);
      }
      break;
    case nsMediaFeature::eInteger:
    case nsMediaFeature::eBoolInteger:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Integer,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Integer,
                     "bad required value");
        NS_ASSERTION(mFeature->mValueType != nsMediaFeature::eBoolInteger ||
                     actual.GetIntValue() == 0 || actual.GetIntValue() == 1,
                     "bad actual bool integer value");
        NS_ASSERTION(mFeature->mValueType != nsMediaFeature::eBoolInteger ||
                     required.GetIntValue() == 0 || required.GetIntValue() == 1,
                     "bad required bool integer value");
        cmp = DoCompare(actual.GetIntValue(), required.GetIntValue());
      }
      break;
    case nsMediaFeature::eFloat:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Number,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Number,
                     "bad required value");
        cmp = DoCompare(actual.GetFloatValue(), required.GetFloatValue());
      }
      break;
    case nsMediaFeature::eIntRatio:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Array &&
                     actual.GetArrayValue()->Count() == 2 &&
                     actual.GetArrayValue()->Item(0).GetUnit() ==
                       eCSSUnit_Integer &&
                     actual.GetArrayValue()->Item(1).GetUnit() ==
                       eCSSUnit_Integer,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Array &&
                     required.GetArrayValue()->Count() == 2 &&
                     required.GetArrayValue()->Item(0).GetUnit() ==
                       eCSSUnit_Integer &&
                     required.GetArrayValue()->Item(1).GetUnit() ==
                       eCSSUnit_Integer,
                     "bad required value");
        // Convert to int64_t so we can multiply without worry.  Note
        // that while the spec requires that both halves of |required|
        // be positive, the numerator or denominator of |actual| might
        // be zero (e.g., when testing 'aspect-ratio' on a 0-width or
        // 0-height iframe).
        int64_t actualNum = actual.GetArrayValue()->Item(0).GetIntValue(),
                actualDen = actual.GetArrayValue()->Item(1).GetIntValue(),
                requiredNum = required.GetArrayValue()->Item(0).GetIntValue(),
                requiredDen = required.GetArrayValue()->Item(1).GetIntValue();
        cmp = DoCompare(actualNum * requiredDen, requiredNum * actualDen);
      }
      break;
    case nsMediaFeature::eResolution:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Inch ||
                     actual.GetUnit() == eCSSUnit_Pixel ||
                     actual.GetUnit() == eCSSUnit_Centimeter,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Inch ||
                     required.GetUnit() == eCSSUnit_Pixel ||
                     required.GetUnit() == eCSSUnit_Centimeter,
                     "bad required value");
        float actualDPI = actual.GetFloatValue();
        float overrideDPPX = aPresContext->GetOverrideDPPX();

        if (overrideDPPX > 0) {
          actualDPI = overrideDPPX * 96.0f;
        } else if (actual.GetUnit() == eCSSUnit_Centimeter) {
          actualDPI = actualDPI * 2.54f;
        } else if (actual.GetUnit() == eCSSUnit_Pixel) {
          actualDPI = actualDPI * 96.0f;
        }
        float requiredDPI = required.GetFloatValue();
        if (required.GetUnit() == eCSSUnit_Centimeter) {
          requiredDPI = requiredDPI * 2.54f;
        } else if (required.GetUnit() == eCSSUnit_Pixel) {
          requiredDPI = requiredDPI * 96.0f;
        }
        cmp = DoCompare(actualDPI, requiredDPI);
      }
      break;
    case nsMediaFeature::eEnumerated:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Enumerated,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Enumerated,
                     "bad required value");
        NS_ASSERTION(mFeature->mRangeType == nsMediaFeature::eMinMaxNotAllowed,
                     "bad range"); // we asserted above about mRange
        // We don't really need DoCompare, but it doesn't hurt (and
        // maybe the compiler will condense this case with eInteger).
        cmp = DoCompare(actual.GetIntValue(), required.GetIntValue());
      }
      break;
    case nsMediaFeature::eIdent:
      {
        NS_ASSERTION(actual.GetUnit() == eCSSUnit_Ident,
                     "bad actual value");
        NS_ASSERTION(required.GetUnit() == eCSSUnit_Ident,
                     "bad required value");
        NS_ASSERTION(mFeature->mRangeType == nsMediaFeature::eMinMaxNotAllowed,
                     "bad range");
        cmp = !(actual == required); // string comparison
      }
      break;
  }
  switch (mRange) {
    case nsMediaExpression::eMin:
      return cmp != -1;
    case nsMediaExpression::eMax:
      return cmp != 1;
    case nsMediaExpression::eEqual:
      return cmp == 0;
  }
  NS_NOTREACHED("unexpected mRange");
  return false;
}

void
nsMediaQueryResultCacheKey::AddExpression(const nsMediaExpression* aExpression,
                                          bool aExpressionMatches)
{
  const nsMediaFeature *feature = aExpression->mFeature;
  FeatureEntry *entry = nullptr;
  for (uint32_t i = 0; i < mFeatureCache.Length(); ++i) {
    if (mFeatureCache[i].mFeature == feature) {
      entry = &mFeatureCache[i];
      break;
    }
  }
  if (!entry) {
    entry = mFeatureCache.AppendElement();
    if (!entry) {
      return; /* out of memory */
    }
    entry->mFeature = feature;
  }

  ExpressionEntry eentry = { *aExpression, aExpressionMatches };
  entry->mExpressions.AppendElement(eentry);
}

bool
nsMediaQueryResultCacheKey::Matches(nsPresContext* aPresContext) const
{
  if (aPresContext->Medium() != mMedium) {
    return false;
  }

  for (uint32_t i = 0; i < mFeatureCache.Length(); ++i) {
    const FeatureEntry *entry = &mFeatureCache[i];
    nsCSSValue actual;

    entry->mFeature->mGetter(aPresContext, entry->mFeature, actual);

    for (uint32_t j = 0; j < entry->mExpressions.Length(); ++j) {
      const ExpressionEntry &eentry = entry->mExpressions[j];
      if (eentry.mExpression.Matches(aPresContext, actual) !=
          eentry.mExpressionMatches) {
        return false;
      }
    }
  }

  return true;
}

bool
nsDocumentRuleResultCacheKey::AddMatchingRule(css::DocumentRule* aRule)
{
  MOZ_ASSERT(!mFinalized);
  return mMatchingRules.AppendElement(aRule);
}

void
nsDocumentRuleResultCacheKey::Finalize()
{
  mMatchingRules.Sort();
#ifdef DEBUG
  mFinalized = true;
#endif
}

#ifdef DEBUG
static bool
ArrayIsSorted(const nsTArray<css::DocumentRule*>& aRules)
{
  for (size_t i = 1; i < aRules.Length(); i++) {
    if (aRules[i - 1] > aRules[i]) {
      return false;
    }
  }
  return true;
}
#endif

bool
nsDocumentRuleResultCacheKey::Matches(
                       nsPresContext* aPresContext,
                       const nsTArray<css::DocumentRule*>& aRules) const
{
  MOZ_ASSERT(mFinalized);
  MOZ_ASSERT(ArrayIsSorted(mMatchingRules));
  MOZ_ASSERT(ArrayIsSorted(aRules));

#ifdef DEBUG
  for (css::DocumentRule* rule : mMatchingRules) {
    MOZ_ASSERT(aRules.BinaryIndexOf(rule) != aRules.NoIndex,
               "aRules must contain all rules in mMatchingRules");
  }
#endif

  // First check that aPresContext matches all the rules listed in
  // mMatchingRules.
  for (css::DocumentRule* rule : mMatchingRules) {
    if (!rule->UseForPresentation(aPresContext)) {
      return false;
    }
  }

  // Then check that all the rules in aRules that aren't also in
  // mMatchingRules do not match.

  // pointer to matching rules
  auto pm     = mMatchingRules.begin();
  auto pm_end = mMatchingRules.end();

  // pointer to all rules
  auto pr     = aRules.begin();
  auto pr_end = aRules.end();

  // mMatchingRules and aRules are both sorted by their pointer values,
  // so we can iterate over the two lists simultaneously.
  while (pr < pr_end) {
    while (pm < pm_end && *pm < *pr) {
      ++pm;
    }
    if (pm >= pm_end || *pm != *pr) {
      if ((*pr)->UseForPresentation(aPresContext)) {
        return false;
      }
    }
    ++pr;
  }
  return true;
}

#ifdef DEBUG
void
nsDocumentRuleResultCacheKey::List(FILE* aOut, int32_t aIndent) const
{
  for (css::DocumentRule* rule : mMatchingRules) {
    nsCString str;

    for (int32_t i = 0; i < aIndent; i++) {
      str.AppendLiteral("  ");
    }
    str.AppendLiteral("{ ");

    nsString condition;
    rule->GetConditionText(condition);
    AppendUTF16toUTF8(condition, str);

    str.AppendLiteral(" }\n");
    fprintf_stderr(aOut, "%s", str.get());
  }
}
#endif

size_t
nsDocumentRuleResultCacheKey::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += mMatchingRules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

void
nsMediaQuery::AppendToString(nsAString& aString) const
{
  if (mHadUnknownExpression) {
    aString.AppendLiteral("not all");
    return;
  }

  NS_ASSERTION(!mNegated || !mHasOnly, "can't have not and only");
  NS_ASSERTION(!mTypeOmitted || (!mNegated && !mHasOnly),
               "can't have not or only when type is omitted");
  if (!mTypeOmitted) {
    if (mNegated) {
      aString.AppendLiteral("not ");
    } else if (mHasOnly) {
      aString.AppendLiteral("only ");
    }
    aString.Append(nsDependentAtomString(mMediaType));
  }

  for (uint32_t i = 0, i_end = mExpressions.Length(); i < i_end; ++i) {
    if (i > 0 || !mTypeOmitted)
      aString.AppendLiteral(" and ");
    aString.Append('(');

    const nsMediaExpression &expr = mExpressions[i];
    const nsMediaFeature *feature = expr.mFeature;
    if (feature->mReqFlags & nsMediaFeature::eHasWebkitPrefix) {
      aString.AppendLiteral("-webkit-");
    }
    if (expr.mRange == nsMediaExpression::eMin) {
      aString.AppendLiteral("min-");
    } else if (expr.mRange == nsMediaExpression::eMax) {
      aString.AppendLiteral("max-");
    }

    aString.Append(nsDependentAtomString(*feature->mName));

    if (expr.mValue.GetUnit() != eCSSUnit_Null) {
      aString.AppendLiteral(": ");
      switch (feature->mValueType) {
        case nsMediaFeature::eLength:
          NS_ASSERTION(expr.mValue.IsLengthUnit(), "bad unit");
          // Use 'width' as a property that takes length values
          // written in the normal way.
          expr.mValue.AppendToString(eCSSProperty_width, aString,
                                     nsCSSValue::eNormalized);
          break;
        case nsMediaFeature::eInteger:
        case nsMediaFeature::eBoolInteger:
          NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Integer,
                       "bad unit");
          // Use 'z-index' as a property that takes integer values
          // written without anything extra.
          expr.mValue.AppendToString(eCSSProperty_z_index, aString,
                                     nsCSSValue::eNormalized);
          break;
        case nsMediaFeature::eFloat:
          {
            NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Number,
                         "bad unit");
            // Use 'line-height' as a property that takes float values
            // written in the normal way.
            expr.mValue.AppendToString(eCSSProperty_line_height, aString,
                                       nsCSSValue::eNormalized);
          }
          break;
        case nsMediaFeature::eIntRatio:
          {
            NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Array,
                         "bad unit");
            nsCSSValue::Array *array = expr.mValue.GetArrayValue();
            NS_ASSERTION(array->Count() == 2, "unexpected length");
            NS_ASSERTION(array->Item(0).GetUnit() == eCSSUnit_Integer,
                         "bad unit");
            NS_ASSERTION(array->Item(1).GetUnit() == eCSSUnit_Integer,
                         "bad unit");
            array->Item(0).AppendToString(eCSSProperty_z_index, aString,
                                          nsCSSValue::eNormalized);
            aString.Append('/');
            array->Item(1).AppendToString(eCSSProperty_z_index, aString,
                                          nsCSSValue::eNormalized);
          }
          break;
        case nsMediaFeature::eResolution:
          {
            aString.AppendFloat(expr.mValue.GetFloatValue());
            if (expr.mValue.GetUnit() == eCSSUnit_Inch) {
              aString.AppendLiteral("dpi");
            } else if (expr.mValue.GetUnit() == eCSSUnit_Pixel) {
              aString.AppendLiteral("dppx");
            } else {
              NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Centimeter,
                           "bad unit");
              aString.AppendLiteral("dpcm");
            }
          }
          break;
        case nsMediaFeature::eEnumerated:
          NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Enumerated,
                       "bad unit");
          AppendASCIItoUTF16(
              nsCSSProps::ValueToKeyword(expr.mValue.GetIntValue(),
                                         feature->mData.mKeywordTable),
              aString);
          break;
        case nsMediaFeature::eIdent:
          NS_ASSERTION(expr.mValue.GetUnit() == eCSSUnit_Ident,
                       "bad unit");
          aString.Append(expr.mValue.GetStringBufferValue());
          break;
      }
    }

    aString.Append(')');
  }
}

nsMediaQuery*
nsMediaQuery::Clone() const
{
  return new nsMediaQuery(*this);
}

bool
nsMediaQuery::Matches(nsPresContext* aPresContext,
                      nsMediaQueryResultCacheKey* aKey) const
{
  if (mHadUnknownExpression)
    return false;

  bool match =
    mMediaType == aPresContext->Medium() || mMediaType == nsGkAtoms::all;
  for (uint32_t i = 0, i_end = mExpressions.Length(); match && i < i_end; ++i) {
    const nsMediaExpression &expr = mExpressions[i];
    nsCSSValue actual;
    expr.mFeature->mGetter(aPresContext, expr.mFeature, actual);

    match = expr.Matches(aPresContext, actual);
    if (aKey) {
      aKey->AddExpression(&expr, match);
    }
  }

  return match == !mNegated;
}

nsMediaList::nsMediaList()
{
}

nsMediaList::~nsMediaList()
{
}

void
nsMediaList::GetText(nsAString& aMediaText)
{
  aMediaText.Truncate();

  for (int32_t i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    nsMediaQuery* query = mArray[i];

    query->AppendToString(aMediaText);

    if (i + 1 < i_end) {
      aMediaText.AppendLiteral(", ");
    }
  }
}

// XXXbz this is so ill-defined in the spec, it's not clear quite what
// it should be doing....
void
nsMediaList::SetText(const nsAString& aMediaText)
{
  nsCSSParser parser;
  parser.ParseMediaList(aMediaText, nullptr, 0, this);
}

bool
nsMediaList::Matches(nsPresContext* aPresContext,
                     nsMediaQueryResultCacheKey* aKey)
{
  for (int32_t i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    if (mArray[i]->Matches(aPresContext, aKey)) {
      return true;
    }
  }
  return mArray.IsEmpty();
}

already_AddRefed<MediaList>
nsMediaList::Clone()
{
  RefPtr<nsMediaList> result = new nsMediaList();
  result->mArray.AppendElements(mArray.Length());
  for (uint32_t i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    result->mArray[i] = mArray[i]->Clone();
    MOZ_ASSERT(result->mArray[i]);
  }
  return result.forget();
}

void
nsMediaList::IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aReturn)
{
  if (aIndex < Length()) {
    aFound = true;
    aReturn.Truncate();
    mArray[aIndex]->AppendToString(aReturn);
  } else {
    aFound = false;
    SetDOMStringToNull(aReturn);
  }
}

nsresult
nsMediaList::Delete(const nsAString& aOldMedium)
{
  if (aOldMedium.IsEmpty())
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  for (int32_t i = 0, i_end = mArray.Length(); i < i_end; ++i) {
    nsMediaQuery* query = mArray[i];

    nsAutoString buf;
    query->AppendToString(buf);
    if (buf == aOldMedium) {
      mArray.RemoveElementAt(i);
      return NS_OK;
    }
  }

  return NS_ERROR_DOM_NOT_FOUND_ERR;
}

nsresult
nsMediaList::Append(const nsAString& aNewMedium)
{
  if (aNewMedium.IsEmpty())
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  Delete(aNewMedium);

  nsresult rv = NS_OK;
  nsTArray<nsAutoPtr<nsMediaQuery> > buf;
  mArray.SwapElements(buf);
  SetText(aNewMedium);
  if (mArray.Length() == 1) {
    nsMediaQuery *query = mArray[0].forget();
    if (!buf.AppendElement(query)) {
      delete query;
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mArray.SwapElements(buf);
  return rv;
}

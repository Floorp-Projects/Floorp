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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *   Chris Double  <chris.double@double.co.nz>
 *   Daniel Holbert <dholbert@cs.stanford.edu>
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

#include "nsSMILAnimationFunction.h"
#include "nsISMILAttr.h"
#include "nsSMILParserUtils.h"
#include "nsSMILNullType.h"
#include "nsISMILAnimationElement.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include <math.h>

//----------------------------------------------------------------------
// Static members

nsAttrValue::EnumTable nsSMILAnimationFunction::sAccumulateTable[] = {
      {"none", PR_FALSE},
      {"sum", PR_TRUE},
      {nsnull, 0}
};

nsAttrValue::EnumTable nsSMILAnimationFunction::sAdditiveTable[] = {
      {"replace", PR_FALSE},
      {"sum", PR_TRUE},
      {nsnull, 0}
};

nsAttrValue::EnumTable nsSMILAnimationFunction::sCalcModeTable[] = {
      {"linear", CALC_LINEAR},
      {"discrete", CALC_DISCRETE},
      {"paced", CALC_PACED},
      {"spline", CALC_SPLINE},
      {nsnull, 0}
};

// Bits for attributes that are parsed the same regardless of animated type
#define BF_ACCUMULATE  0
#define BF_ADDITIVE    1
#define BF_CALC_MODE   2
#define BF_KEY_TIMES   3
#define BF_KEY_SPLINES 4

// Any negative number should be fine as a sentinel here,
// because valid distances are non-negative.
#define COMPUTE_DISTANCE_ERROR (-1)

// Based on GET/SET_BOOLBIT in nsHTMLInputElement.cpp
#define GET_FLAG(bitfield, field) (((bitfield) & (0x01 << (field))) \
                                     ? PR_TRUE : PR_FALSE)
#define SET_FLAG(bitfield, field, b) ((b) \
                                     ? ((bitfield) |=  (0x01 << (field))) \
                                     : ((bitfield) &= ~(0x01 << (field))))

//----------------------------------------------------------------------
// Constructors etc.

nsSMILAnimationFunction::nsSMILAnimationFunction()
  : mIsActive(PR_FALSE),
    mIsFrozen(PR_FALSE),
    mSampleTime(-1),
    mRepeatIteration(0),
    mLastValue(PR_FALSE),
    mHasChanged(PR_TRUE),
    mBeginTime(LL_MININT),
    mAnimationElement(nsnull),
    mErrorFlags(0)
{
}

void
nsSMILAnimationFunction::SetAnimationElement(
    nsISMILAnimationElement* aAnimationElement)
{
  mAnimationElement = aAnimationElement;
}

PRBool
nsSMILAnimationFunction::SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                                 nsAttrValue& aResult, nsresult* aParseResult)
{
  PRBool foundMatch = PR_TRUE;
  nsresult parseResult = NS_OK;

  // The attributes 'by', 'from', 'to', and 'values' may be parsed differently
  // depending on the element & attribute we're animating.  So instead of
  // parsing them now we re-parse them at every sample.
  if (aAttribute == nsGkAtoms::by ||
      aAttribute == nsGkAtoms::from ||
      aAttribute == nsGkAtoms::to ||
      aAttribute == nsGkAtoms::values) {
    // We parse to, from, by, values at sample time.
    // XXX Need to flag which attribute has changed and then when we parse it at
    // sample time, report any errors and reset the flag
    mHasChanged = PR_TRUE;
    aResult.SetTo(aValue);
  } else if (aAttribute == nsGkAtoms::accumulate) {
    parseResult = SetAccumulate(aValue, aResult);
  } else if (aAttribute == nsGkAtoms::additive) {
    parseResult = SetAdditive(aValue, aResult);
  } else if (aAttribute == nsGkAtoms::calcMode) {
    parseResult = SetCalcMode(aValue, aResult);
  } else if (aAttribute == nsGkAtoms::keyTimes) {
    parseResult = SetKeyTimes(aValue, aResult);
  } else if (aAttribute == nsGkAtoms::keySplines) {
    parseResult = SetKeySplines(aValue, aResult);
  } else {
    foundMatch = PR_FALSE;
  }

  if (foundMatch && aParseResult) {
    *aParseResult = parseResult;
  }

  return foundMatch;
}

PRBool
nsSMILAnimationFunction::UnsetAttr(nsIAtom* aAttribute)
{
  PRBool foundMatch = PR_TRUE;

  if (aAttribute == nsGkAtoms::by ||
      aAttribute == nsGkAtoms::from ||
      aAttribute == nsGkAtoms::to ||
      aAttribute == nsGkAtoms::values) {
    mHasChanged = PR_TRUE;
  } else if (aAttribute == nsGkAtoms::accumulate) {
    UnsetAccumulate();
  } else if (aAttribute == nsGkAtoms::additive) {
    UnsetAdditive();
  } else if (aAttribute == nsGkAtoms::calcMode) {
    UnsetCalcMode();
  } else if (aAttribute == nsGkAtoms::keyTimes) {
    UnsetKeyTimes();
  } else if (aAttribute == nsGkAtoms::keySplines) {
    UnsetKeySplines();
  } else {
    foundMatch = PR_FALSE;
  }

  return foundMatch;
}

void
nsSMILAnimationFunction::SampleAt(nsSMILTime aSampleTime,
                                  const nsSMILTimeValue& aSimpleDuration,
                                  PRUint32 aRepeatIteration)
{
  if (mHasChanged || mLastValue || mSampleTime != aSampleTime || 
      mSimpleDuration.CompareTo(aSimpleDuration) || 
      mRepeatIteration != aRepeatIteration) {
    mHasChanged = PR_TRUE;
  }

  mSampleTime       = aSampleTime;
  mSimpleDuration   = aSimpleDuration;
  mRepeatIteration  = aRepeatIteration;
  mLastValue        = PR_FALSE;
}

void
nsSMILAnimationFunction::SampleLastValue(PRUint32 aRepeatIteration)
{
  if (mHasChanged || !mLastValue || mRepeatIteration != aRepeatIteration) {
    mHasChanged = PR_TRUE;
  }

  mRepeatIteration  = aRepeatIteration;
  mLastValue        = PR_TRUE;
}

void
nsSMILAnimationFunction::Activate(nsSMILTime aBeginTime)
{
  mBeginTime = aBeginTime;
  mIsActive = PR_TRUE;
  mIsFrozen = PR_FALSE;
  mFrozenValue = nsSMILValue();
}

void
nsSMILAnimationFunction::Inactivate(PRBool aIsFrozen)
{
  mIsActive = PR_FALSE;
  mIsFrozen = aIsFrozen;
  mFrozenValue = nsSMILValue();
  mHasChanged = PR_TRUE;
}

void
nsSMILAnimationFunction::ComposeResult(const nsISMILAttr& aSMILAttr,
                                       nsSMILValue& aResult)
{
  mHasChanged = PR_FALSE;

  // Skip animations that are inactive or in error
  if (!IsActive() || mErrorFlags != 0)
    return;

  // Get the animation values
  nsSMILValueArray values;
  nsresult rv = GetValues(aSMILAttr, values);
  if (NS_FAILED(rv))
    return;

  // If this interval is active, we must have a non-negative
  // mSampleTime and a resolved or indefinite mSimpleDuration.
  // (Otherwise, we're probably just frozen.)
  if (mIsActive) {
    NS_ENSURE_TRUE(mSampleTime >= 0,);
    NS_ENSURE_TRUE(mSimpleDuration.IsResolved() || 
                   mSimpleDuration.IsIndefinite(),);
  }

  nsSMILValue result(aResult.mType);
  
  if (mSimpleDuration.IsIndefinite() ||
      (HasAttr(nsGkAtoms::values) && values.Length() == 1)) {

    // Indefinite duration or only one value set: Always set the first value
    result = values[0];

  } else if (mLastValue) {

    // Sampling last value
    nsSMILValue last(values[values.Length() - 1]);
    result = last;

    // See comment in AccumulateResult: to-animation does not accumulate
    if (!IsToAnimation() && GetAccumulate() && mRepeatIteration) {
      // If the target attribute type doesn't support addition Add will
      // fail leaving result = last
      result.Add(last, mRepeatIteration);
    }

  } else if (!mFrozenValue.IsNull() && !mHasChanged) {

    // Frozen to animation
    result = mFrozenValue;

  } else {

    // Interpolation
    NS_ENSURE_SUCCESS(InterpolateResult(values, result, aResult),);
    NS_ENSURE_SUCCESS(AccumulateResult(values, result),);

    if (IsToAnimation() && mIsFrozen) {
      mFrozenValue = result;
    }
  }

  // If additive animation isn't required or isn't supported, set the value.
  if (!IsAdditive() || NS_FAILED(aResult.Add(result)))
    aResult = result;
}

PRInt8
nsSMILAnimationFunction::CompareTo(const nsSMILAnimationFunction* aOther) const
{
  NS_ENSURE_TRUE(aOther, 0);

  NS_ASSERTION(aOther != this, "Trying to compare to self.");

  // Inactive animations sort first
  if (!IsActive() && aOther->IsActive())
    return -1;

  if (IsActive() && !aOther->IsActive())
    return 1;

  // Sort based on begin time
  if (mBeginTime != aOther->GetBeginTime())
    return mBeginTime > aOther->GetBeginTime() ? 1 : -1;

  // XXX When syncbase timing is implemented, we next need to sort based on
  // dependencies

  // Animations that appear later in the document sort after those earlier in
  // the document
  nsIContent &thisElement = mAnimationElement->Content();
  nsIContent &otherElement = aOther->mAnimationElement->Content();

  NS_ASSERTION(&thisElement != &otherElement,
             "Two animations cannot have the same animation content element!");

  return (nsContentUtils::PositionIsBefore(&thisElement, &otherElement))
          ? -1 : 1;
}

PRBool
nsSMILAnimationFunction::WillReplace() const
{
  /*
   * In IsAdditive() we don't consider to-animation to be additive as it is
   * a special case that is dealt with differently in the compositing method but
   * here we return false for to animation as it builds on the underlying value
   * unless its a frozen to animation.
   */
  return !(IsAdditive() || IsToAnimation()) || 
    (IsToAnimation() && mIsFrozen && !mHasChanged);
}

PRBool
nsSMILAnimationFunction::HasChanged() const
{
  return mHasChanged;
}

//----------------------------------------------------------------------
// Implementation helpers

nsresult
nsSMILAnimationFunction::InterpolateResult(const nsSMILValueArray& aValues,
                                           nsSMILValue& aResult,
                                           nsSMILValue& aBaseValue)
{
  nsresult rv = NS_OK;
  const nsSMILValue* from = nsnull;
  const nsSMILValue* to = nsnull;
  const nsSMILTime& dur = mSimpleDuration.GetMillis();

  // Sanity Checks
  NS_ASSERTION(mSampleTime >= 0.0f, "Sample time should not be negative...");
  NS_ASSERTION(dur  >= 0.0f, "Simple duration should not be negative...");

  if (mSampleTime >= dur || mSampleTime < 0) {
    NS_ERROR("Animation sampled outside interval.");
    return NS_ERROR_FAILURE;
  }

  if ((!IsToAnimation() && aValues.Length() < 2) || 
      (IsToAnimation()  && aValues.Length() != 1)) {
    NS_ERROR("Unexpected number of values.");
    return NS_ERROR_FAILURE;
  }
  // End Sanity Checks

  double fTime = double(mSampleTime);
  double fDur = double(dur);

  // Get the normalised progress through the simple duration
  double simpleProgress = (fDur > 0.0) ? fTime / fDur : 0.0;

  // Handle bad keytimes (where first != 0 and/or last != 1)
  // See http://brian.sol1.net/svg/range-for-keytimes for more info.
  if (HasAttr(nsGkAtoms::keyTimes)) {
    double first = mKeyTimes[0];
    if (first > 0.0 && simpleProgress < first) {
      if (!IsToAnimation())
        aResult = aValues[0];
      return rv;
    }
    double last = mKeyTimes[mKeyTimes.Length() - 1];
    if (last < 1.0 && simpleProgress >= last) {
      if (IsToAnimation())
        aResult = aValues[0];
      else
        aResult = aValues[aValues.Length() - 1];
      return rv;
    }
  }

  ScaleSimpleProgress(simpleProgress);

  // Handle CALC_DISCRETE separately, because it's simple.
  if (GetCalcMode() == CALC_DISCRETE) {
    PRUint32 index = IsToAnimation() ? 0 :
      (PRUint32) floor(simpleProgress * (aValues.Length()));
    aResult = aValues[index];
    return NS_OK;
  } 

  // Get the normalised progress between adjacent values
  double intervalProgress;
  if (IsToAnimation()) {
    // Note: Don't need to do any special-casing for CALC_PACED here,
    // because To-Animation doesn't use a values list, by definition.
    from = &aBaseValue;
    to = &aValues[0];
    intervalProgress = simpleProgress;
    ScaleIntervalProgress(intervalProgress, 0, 1);
  } else {
    if (GetCalcMode() == CALC_PACED) { 
      rv = ComputePacedPosition(aValues, simpleProgress, intervalProgress,
                                from, to);
      NS_ENSURE_SUCCESS(rv,rv);
    } else { // GetCalcMode() == CALC_LINEAR or GetCalcMode() == CALC_SPLINE
      PRUint32 index = (PRUint32)floor(simpleProgress * (aValues.Length() - 1));
      from = &aValues[index];
      to = &aValues[index + 1];
      intervalProgress = simpleProgress * (aValues.Length() - 1) - index;
      ScaleIntervalProgress(intervalProgress, index, aValues.Length() - 1);
    }
  }
  NS_ASSERTION(from, "NULL from-value during interpolation.");
  NS_ASSERTION(to, "NULL to-value during interpolation.");

  return from->Interpolate(*to, intervalProgress, aResult);
}

nsresult
nsSMILAnimationFunction::AccumulateResult(const nsSMILValueArray& aValues,
                                          nsSMILValue& aResult)
{
  if (!IsToAnimation() && GetAccumulate() && mRepeatIteration)
  {
    nsSMILValue lastValue = aValues[aValues.Length() - 1];

    // If the target attribute type doesn't support addition, Add will
    // fail and we leave aResult untouched.
    aResult.Add(lastValue, mRepeatIteration);
  }

  return NS_OK;
}

/*
 * Given the simple progress for a paced animation, this method:
 *  - determines which two elements of the values array we're in between
 *    (returned as aFrom and aTo)
 *  - determines where we are between them
 *    (returned as aIntervalProgress)
 *
 * Returns NS_OK, unless there's an error computing distances.
 */
nsresult
nsSMILAnimationFunction::ComputePacedPosition(const nsSMILValueArray& aValues,
                                              double aSimpleProgress,
                                              double& aIntervalProgress,
                                              const nsSMILValue*& aFrom,
                                              const nsSMILValue*& aTo)
{
  NS_ASSERTION(0.0f <= aSimpleProgress && aSimpleProgress < 1.0f,
               "aSimpleProgress is out of bounds.");
  NS_ASSERTION(GetCalcMode() == CALC_PACED,
               "Calling paced-specific function, but not in paced mode");

  double totalDistance = ComputePacedTotalDistance(aValues);
  if (totalDistance == COMPUTE_DISTANCE_ERROR)
    return NS_ERROR_FAILURE;

  // total distance we should have moved at this point in time.
  // (called 'remainingDist' due to how it's used in loop below)
  double remainingDist = aSimpleProgress * totalDistance;

  // Must be satisfied, because totalDistance is a sum of (non-negative)
  // distances, and aSimpleProgress is non-negative
  NS_ASSERTION(remainingDist >= 0, "distance values must be non-negative");

  // Find where remainingDist puts us in the list of values
  // Note: We could optimize this next loop by caching the
  // interval-distances in an array, but maybe that's excessive.
  for (PRUint32 i = 0; i < aValues.Length() - 1; i++) {
    // Note: The following assertion is valid because remainingDist should
    // start out non-negative, and this loop never shaves off more than its
    // current value.
    NS_ASSERTION(remainingDist >= 0, "distance values must be non-negative");

    double curIntervalDist;
    nsresult tmpRv = aValues[i].ComputeDistance(aValues[i+1], curIntervalDist);
    NS_ASSERTION(NS_SUCCEEDED(tmpRv), "ComputeDistance failed...?");
    NS_ASSERTION(curIntervalDist >= 0, "distance values must be non-negative");
    // Clamp distance value at 0, just in case ComputeDistance is evil.
    curIntervalDist = PR_MAX(curIntervalDist, 0.0f);

    if (remainingDist >= curIntervalDist) {
      remainingDist -= curIntervalDist;
    } else {
      // NOTE: If we get here, then curIntervalDist necessarily is not 0. Why?
      // Because this clause is only hit when remainingDist < curIntervalDist,
      // and if curIntervalDist were 0, that would mean remainingDist would
      // have to be < 0.  But that can't happen, because remainingDist (as
      // a distance) is non-negative by definition.
      NS_ASSERTION(curIntervalDist != 0,
                   "We should never get here with this set to 0...");

      // We found the right spot -- an interpolated position between
      // values i and i+1.
      aFrom = &aValues[i];
      aTo = &aValues[i+1];
      aIntervalProgress = remainingDist / curIntervalDist;
      return NS_OK;
    }
  }

  NS_NOTREACHED("shouldn't complete loop & get here -- if we do, "
                "then aSimpleProgress was probably out of bounds.");
  return NS_ERROR_FAILURE;
}

/*
 * Computes & caches the total distance to be travelled by a paced animation.
 *
 * Returns NS_OK, unless there's an error computing distance.
 */
double
nsSMILAnimationFunction::ComputePacedTotalDistance(
    const nsSMILValueArray& aValues) const
{
  NS_ASSERTION(GetCalcMode() == CALC_PACED,
               "Calling paced-specific function, but not in paced mode");

  double totalDistance = 0.0;
  for (PRUint32 i = 0; i < aValues.Length() - 1; i++) {
    double tmpDist;
    nsresult rv = aValues[i].ComputeDistance(aValues[i+1], tmpDist);
    if (!NS_SUCCEEDED(rv)) {
      NS_NOTREACHED("ComputeDistance failed...?");
      return COMPUTE_DISTANCE_ERROR;
    }

    // Clamp distance value at 0, just in case ComputeDistance is evil.
    NS_ASSERTION(tmpDist >= 0, "distance values must be non-negative");
    tmpDist = PR_MAX(tmpDist, 0.0f);

    totalDistance += tmpDist;
  }

  return totalDistance;
}

/*
 * Scale the simple progress, taking into account any keyTimes.
 */
void
nsSMILAnimationFunction::ScaleSimpleProgress(double& aProgress)
{
  if (!HasAttr(nsGkAtoms::keyTimes))
    return;

  PRUint32 numTimes = mKeyTimes.Length();

  if (numTimes < 2)
    return;

  PRUint32 i = 0;
  for (; i < numTimes - 2 && aProgress >= mKeyTimes[i+1]; ++i);

  double& intervalStart = mKeyTimes[i];
  double& intervalEnd   = mKeyTimes[i+1];

  double intervalLength = intervalEnd - intervalStart;
  if (intervalLength <= 0.0) {
    aProgress = intervalStart;
    return;
  }

  aProgress = (i + (aProgress - intervalStart) / intervalLength) * 
         1.0 / double(numTimes - 1);
}

/*
 * Scale the interval progress, taking into account any keySplines
 * or discrete methods.
 */
void
nsSMILAnimationFunction::ScaleIntervalProgress(double& aProgress,
                                               PRUint32   aIntervalIndex,
                                               PRUint32   aNumIntervals)
{
  if (GetCalcMode() != CALC_SPLINE)
    return;

  if (!HasAttr(nsGkAtoms::keySplines))
    return;

  NS_ASSERTION(aIntervalIndex >= 0 && 
               aIntervalIndex < (PRUint32)mKeySplines.Length(),
               "Invalid interval index.");
  NS_ASSERTION(aNumIntervals >= 1, "Invalid number of intervals.");

  if (aIntervalIndex < 0 ||
      aIntervalIndex >= (PRUint32)mKeySplines.Length() ||
      aNumIntervals < 1)
    return;

  nsSMILKeySpline const &spline = mKeySplines[aIntervalIndex];
  aProgress = spline.GetSplineValue(aProgress);
}

PRBool
nsSMILAnimationFunction::HasAttr(nsIAtom* aAttName) const
{
  return mAnimationElement->HasAnimAttr(aAttName);
}

const nsAttrValue*
nsSMILAnimationFunction::GetAttr(nsIAtom* aAttName) const
{
  return mAnimationElement->GetAnimAttr(aAttName);
}

PRBool
nsSMILAnimationFunction::GetAttr(nsIAtom* aAttName, nsAString& aResult) const
{
  return mAnimationElement->GetAnimAttr(aAttName, aResult);
}

/*
 * A utility function to make querying an attribute that corresponds to an
 * nsSMILValue a little neater.
 *
 * @param aAttName    The attribute name (in the global namespace)
 * @param aSMILAttr   The SMIL attribute to perform the parsing
 * @param aResult     The resulting nsSMILValue
 *
 * Returns PR_FALSE if a parse error occurred, otherwise returns PR_TRUE.
 */
PRBool
nsSMILAnimationFunction::ParseAttr(nsIAtom* aAttName,
                                   const nsISMILAttr& aSMILAttr,
                                   nsSMILValue& aResult) const
{
  nsAutoString attValue;
  if (GetAttr(aAttName, attValue)) {
    nsresult rv = 
      aSMILAttr.ValueFromString(attValue, mAnimationElement, aResult);
    if (NS_FAILED(rv))
      return PR_FALSE;
  }
  return PR_TRUE;
}

/*
 * SMILANIM specifies the following rules for animation function values:
 *
 * (1) if values is set, it overrides everything
 * (2) for from/to/by animation at least to or by must be specified, from on its
 *     own (or nothing) is an error--which we will ignore
 * (3) if both by and to are specified only to will be used, by will be ignored
 * (4) if by is specified without from (by animation), forces additive behaviour
 * (5) if to is specified without from (to animation), special care needs to be
 *     taken when compositing animation as such animations are composited last.
 *
 * This helper method applies these rules to fill in the values list and to set
 * some internal state.
 */
nsresult
nsSMILAnimationFunction::GetValues(const nsISMILAttr& aSMILAttr,
                                   nsSMILValueArray& aResult)
{
  if (!mAnimationElement)
    return NS_ERROR_FAILURE;

  nsSMILValueArray result;

  // If "values" is set, use it
  if (HasAttr(nsGkAtoms::values)) {
    nsAutoString attValue;
    GetAttr(nsGkAtoms::values, attValue);
    nsresult rv = nsSMILParserUtils::ParseValues(attValue, mAnimationElement,
                                                 aSMILAttr, result);
    if (NS_FAILED(rv))
      return rv;

  // Else try to/from/by
  } else {

    PRBool parseOk = PR_TRUE;
    nsSMILValue to, from, by;
    parseOk &= ParseAttr(nsGkAtoms::to,   aSMILAttr, to);
    parseOk &= ParseAttr(nsGkAtoms::from, aSMILAttr, from);
    parseOk &= ParseAttr(nsGkAtoms::by,   aSMILAttr, by);

    if (!parseOk)
      return NS_ERROR_FAILURE;

    result.SetCapacity(2);
    if (!to.IsNull()) {
      if (!from.IsNull()) {
        result.AppendElement(from);
        result.AppendElement(to);
      } else {
        result.AppendElement(to);
      }
    } else if (!by.IsNull()) {
      nsSMILValue effectiveFrom(by.mType);
      if (!from.IsNull())
        effectiveFrom = from;
      // Set values to 'from; from + by'
      result.AppendElement(effectiveFrom);
      nsSMILValue effectiveTo(effectiveFrom);
      if (!effectiveTo.IsNull() && NS_SUCCEEDED(effectiveTo.Add(by))) {
        result.AppendElement(effectiveTo);
      } else {
        // Using by-animation with non-additive type or bad base-value
        return NS_ERROR_FAILURE;
      }
    } else {
      // No values, no to, no by -- call it a day
      return NS_ERROR_FAILURE;
    }
  }

  // Check that we have the right number of keySplines and keyTimes
  CheckKeyTimes(result.Length());
  CheckKeySplines(result.Length());

  result.SwapElements(aResult);
  
  return NS_OK;
}

inline PRBool
nsSMILAnimationFunction::IsToAnimation() const
{
  return !HasAttr(nsGkAtoms::values) && 
         HasAttr(nsGkAtoms::to) &&
         !HasAttr(nsGkAtoms::from);
}

inline PRBool
nsSMILAnimationFunction::IsAdditive() const
{
  /*
   * Animation is additive if:
   *
   * (1) additive = "sum" (GetAdditive() == true), or
   * (2) it is 'by animation' (by is set, from and values are not)
   *
   * Although animation is not additive if it is 'to animation'
   */
  PRBool isByAnimation = (!HasAttr(nsGkAtoms::values)
                       &&  HasAttr(nsGkAtoms::by)
                       && !HasAttr(nsGkAtoms::from));
  return !IsToAnimation() && (GetAdditive() || isByAnimation);
}

/**
 * Performs checks for the keyTimes attribute required by the SMIL spec but
 * which depend on other attributes and therefore needs to be updated as
 * dependent attributes are set.
 */
void
nsSMILAnimationFunction::CheckKeyTimes(PRUint32 aNumValues)
{
  if (!HasAttr(nsGkAtoms::keyTimes))
    return;

  // attribute is ignored for calcMode = paced
  if (GetCalcMode() == CALC_PACED) {
    SET_FLAG(mErrorFlags, BF_KEY_TIMES, PR_FALSE);
    return;
  }

  if (mKeyTimes.Length() < 1) {
    // keyTimes isn't set or failed preliminary checks
    SET_FLAG(mErrorFlags, BF_KEY_TIMES, PR_TRUE);
    return;
  }

  // no. keyTimes == no. values
  if ((mKeyTimes.Length() != aNumValues && !IsToAnimation()) ||
      (IsToAnimation() && mKeyTimes.Length() != 2)) {
    SET_FLAG(mErrorFlags, BF_KEY_TIMES, PR_TRUE);
    return;
  }

  // special handling if there is only one keyTime. The spec doesn't say what to
  // do in this case so we allow the keyTime to be either 0 or 1.
  if (mKeyTimes.Length() == 1) {
    double time = mKeyTimes[0];
    SET_FLAG(mErrorFlags, BF_KEY_TIMES, !(time == 0.0 || time == 1.0));
    return;
  }

  // According to the spec, the first value should be 0 and for linear or spline
  // calcMode's the last value should be 1, but then an example is give with
  // a spline calcMode and keyTimes "0.0; 0.7". So we don't bother checking
  // the end-values here but just allow bad specs.

  SET_FLAG(mErrorFlags, BF_KEY_TIMES, PR_FALSE);
}

void
nsSMILAnimationFunction::CheckKeySplines(PRUint32 aNumValues)
{
  // attribute is ignored if calc mode is not spline
  if (GetCalcMode() != CALC_SPLINE) {
    SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_FALSE);
    return;
  }

  // calc mode is spline but the attribute is not set
  if (!HasAttr(nsGkAtoms::keySplines)) {
    SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_FALSE);
    return;
  }

  if (mKeySplines.Length() < 1) {
    // keyTimes isn't set or failed preliminary checks
    SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_TRUE);
    return;
  }

  // ignore splines if there's only one value
  if (aNumValues == 1 && !IsToAnimation()) {
    SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_FALSE);
    return;
  }

  // no. keySpline specs == no. values - 1
  PRUint32 splineSpecs = mKeySplines.Length();
  if ((splineSpecs != aNumValues - 1 && !IsToAnimation()) ||
      (IsToAnimation() && splineSpecs != 1)) {
    SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_TRUE);
    return;
  }

  SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_FALSE);
}

//----------------------------------------------------------------------
// Property getters

PRBool
nsSMILAnimationFunction::GetAccumulate() const
{
  const nsAttrValue* value = GetAttr(nsGkAtoms::accumulate);
  if (!value)
    return PR_FALSE;

  return (value->GetEnumValue() == PR_TRUE);
}

PRBool
nsSMILAnimationFunction::GetAdditive() const
{
  const nsAttrValue* value = GetAttr(nsGkAtoms::additive);
  if (!value)
    return PR_FALSE;

  return (value->GetEnumValue() == PR_TRUE);
}

nsSMILAnimationFunction::nsSMILCalcMode
nsSMILAnimationFunction::GetCalcMode() const
{
  const nsAttrValue* value = GetAttr(nsGkAtoms::calcMode);
  if (!value)
    return CALC_LINEAR;

  return nsSMILCalcMode(value->GetEnumValue());
}

//----------------------------------------------------------------------
// Property setters / un-setters:

nsresult
nsSMILAnimationFunction::SetAccumulate(const nsAString& aAccumulate,
                                       nsAttrValue& aResult)
{
  mHasChanged = PR_TRUE;
  PRBool parseResult = 
    aResult.ParseEnumValue(aAccumulate, sAccumulateTable, PR_TRUE);
  SET_FLAG(mErrorFlags, BF_ACCUMULATE, !parseResult);
  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void
nsSMILAnimationFunction::UnsetAccumulate()
{
  SET_FLAG(mErrorFlags, BF_ACCUMULATE, PR_FALSE);
  mHasChanged = PR_TRUE;
}

nsresult
nsSMILAnimationFunction::SetAdditive(const nsAString& aAdditive,
                                     nsAttrValue& aResult)
{
  mHasChanged = PR_TRUE;
  PRBool parseResult 
    = aResult.ParseEnumValue(aAdditive, sAdditiveTable, PR_TRUE);
  SET_FLAG(mErrorFlags, BF_ADDITIVE, !parseResult);
  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void
nsSMILAnimationFunction::UnsetAdditive()
{
  SET_FLAG(mErrorFlags, BF_ADDITIVE, PR_FALSE);
  mHasChanged = PR_TRUE;
}

nsresult
nsSMILAnimationFunction::SetCalcMode(const nsAString& aCalcMode,
                                     nsAttrValue& aResult)
{
  mHasChanged = PR_TRUE;
  PRBool parseResult 
    = aResult.ParseEnumValue(aCalcMode, sCalcModeTable, PR_TRUE);
  SET_FLAG(mErrorFlags, BF_CALC_MODE, !parseResult);
  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void
nsSMILAnimationFunction::UnsetCalcMode()
{
  SET_FLAG(mErrorFlags, BF_CALC_MODE, PR_FALSE);
  mHasChanged = PR_TRUE;
}

nsresult
nsSMILAnimationFunction::SetKeySplines(const nsAString& aKeySplines,
                                       nsAttrValue& aResult)
{
  mKeySplines.Clear();
  aResult.SetTo(aKeySplines);

  nsTArray<double> keySplines;
  nsresult rv = nsSMILParserUtils::ParseKeySplines(aKeySplines, keySplines);

  if (keySplines.Length() < 1 || keySplines.Length() % 4)
    rv = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(rv))
  {
    mKeySplines.SetCapacity(keySplines.Length() % 4);
    for (PRUint32 i = 0; i < keySplines.Length() && NS_SUCCEEDED(rv); i += 4)
    {
      if (!mKeySplines.AppendElement(nsSMILKeySpline(keySplines[i],
                                                     keySplines[i+1],
                                                     keySplines[i+2],
                                                     keySplines[i+3]))) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  mHasChanged = PR_TRUE;

  return rv;
}

void
nsSMILAnimationFunction::UnsetKeySplines()
{
  mKeySplines.Clear();
  SET_FLAG(mErrorFlags, BF_KEY_SPLINES, PR_FALSE);
  mHasChanged = PR_TRUE;
}

nsresult
nsSMILAnimationFunction::SetKeyTimes(const nsAString& aKeyTimes,
                                     nsAttrValue& aResult)
{
  mKeyTimes.Clear();
  aResult.SetTo(aKeyTimes);

  nsresult rv = nsSMILParserUtils::ParseKeyTimes(aKeyTimes, mKeyTimes);

  if (NS_SUCCEEDED(rv) && mKeyTimes.Length() < 1)
    rv = NS_ERROR_FAILURE;

  if (NS_FAILED(rv))
    mKeyTimes.Clear();

  mHasChanged = PR_TRUE;

  return NS_OK;
}

void
nsSMILAnimationFunction::UnsetKeyTimes()
{
  mKeyTimes.Clear();
  SET_FLAG(mErrorFlags, BF_KEY_TIMES, PR_FALSE);
  mHasChanged = PR_TRUE;
}

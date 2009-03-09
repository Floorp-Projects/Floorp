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
 *   Daniel Holbert <dholbert@mozilla.com>
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

#ifndef NS_SMILANIMATIONFUNCTION_H_
#define NS_SMILANIMATIONFUNCTION_H_

#include "nsISMILAttr.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsSMILTimeValue.h"
#include "nsSMILKeySpline.h"
#include "nsSMILValue.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsAttrValue.h"
#include "nsSMILTypes.h"

class nsISMILAnimationElement;

//----------------------------------------------------------------------
// nsSMILAnimationFunction
//
// The animation function calculates animation values. It it is provided with
// time parameters (sample time, repeat iteration etc.) and it uses this to
// build an appropriate animation value by performing interpolation and
// addition operations.
//
// It is responsible for implementing the animation parameters of an animation
// element (e.g. from, by, to, values, calcMode, additive, accumulate, keyTimes,
// keySplines)
//
class nsSMILAnimationFunction
{
public:
  nsSMILAnimationFunction();

  /*
   * Sets the owning animation element which this class uses to query attribute
   * values and compare document positions.
   */
  void SetAnimationElement(nsISMILAnimationElement* aAnimationElement);

  /*
   * Sets animation-specific attributes (or marks them dirty, in the case
   * of from/to/by/values).
   *
   * @param aAttribute The attribute being set
   * @param aValue     The updated value of the attribute.
   * @param aResult    The nsAttrValue object that may be used for storing the
   *                   parsed result.
   * @param aParseResult  Outparam used for reporting parse errors. Will be set
   *                      to NS_OK if everything succeeds.
   * @returns PR_TRUE if aAttribute is a recognized animation-related
   *          attribute; PR_FALSE otherwise.
   */
  virtual PRBool SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                         nsAttrValue& aResult, nsresult* aParseResult = nsnull);

  /*
   * Unsets the given attribute.
   *
   * @returns PR_TRUE if aAttribute is a recognized animation-related
   *          attribute; PR_FALSE otherwise.
   */
  virtual PRBool UnsetAttr(nsIAtom* aAttribute);

  /**
   * Indicate a new sample has occurred.
   *
   * @param aSampleTime The sample time for this timed element expressed in
   *                    simple time.
   * @param aSimpleDuration The simple duration for this timed element.
   * @param aRepeatIteration  The repeat iteration for this sample. The first
   *                          iteration has a value of 0.
   */
  void SampleAt(nsSMILTime aSampleTime,
                const nsSMILTimeValue& aSimpleDuration,
                PRUint32 aRepeatIteration);

  /**
   * Indicate to sample using the last value defined for the animation function.
   * This value is not normally sampled due to the end-point exclusive timing
   * model but only occurs when the fill mode is "freeze" and the active
   * duration is an even multiple of the simple duration.
   *
   * @param aRepeatIteration  The repeat iteration for this sample. The first
   *                          iteration has a value of 0.
   */
  void SampleLastValue(PRUint32 aRepeatIteration);

  /**
   * Indicate that this animation is now active. This is used to instruct the
   * animation function that it should now add its result to the animation
   * sandwich. The begin time is also provided for proper prioritization of
   * animation functions, and for this reason, this method must be called
   * before either of the Sample methods.
   *
   * @param aBeginTime The begin time for the newly active interval.
   */
  void Activate(nsSMILTime aBeginTime);

  /**
   * Indicate that this animation is no longer active. This is used to instruct
   * the animation function that it should no longer add its result to the
   * animation sandwich.
   *
   * @param aIsFrozen True if this animation should continue to contribute to
   *                  the animation sandwich using the most recent sample
   *                  parameters.
   */
  void Inactivate(PRBool aIsFrozen);

  /**
   * Combines the result of this animation function for the last sample with the
   * specified value.
   *
   * @param aSMILAttr This animation's target attribute. Used here for
   *                  doing attribute-specific parsing of from/to/by/values.
   *
   * @param aResult   The value to compose with.
   */
  void ComposeResult(const nsISMILAttr& aSMILAttr, nsSMILValue& aResult);

  /**
   * Returns the relative priority of this animation to another. The priority is
   * used for determining the position of the animation in the animation
   * sandwich -- higher priority animations are applied on top of lower
   * priority animations.
   *
   * @return  -1 if this animation has lower priority or 1 if this animation has
   *          higher priority
   *
   * This method should never return any other value, including 0.
   */
  PRInt8 CompareTo(const nsSMILAnimationFunction* aOther) const;

  /*
   * The following methods are provided so that the compositor can optimize its
   * operations by only composing those animation that will affect the final
   * result.
   */
 
  /**
   * Indicates if the animation is currently active. Inactive animations will
   * not contribute to the composed result.
   *
   * @return  True if the animation active, false otherwise.
   */
  PRBool IsActive() const
  {
    /* 
     * - Frozen animations should be considered active for the purposes of
     * compositing.
     * - This function does not assume that our nsSMILValues (by/from/to/values)
     * have already been parsed.
     */
    return (mIsActive || mIsFrozen);
  }

  /**
   * Indicates if this animation will replace the passed in result rather than
   * adding to it. Animations that replace the underlying value may be called
   * without first calling lower priority animations.
   *
   * @return  True if the animation will replace, false if it will add or
   *          otherwise build on the passed in value.
   */
  PRBool WillReplace() const;

  /**
   * Indicates if the parameters for this animation have changed since the last
   * time it was composited. This allows rendering to be performed only when
   * necessary, particularly when no animations are active.
   *
   * Note that the caller is responsible for determining if the animation target
   * has changed.
   *
   * @return  True if the animation parameters have changed, false otherwise.
   */
  PRBool HasChanged() const;

  // Comparator utility class, used for sorting nsSMILAnimationFunctions
  class Comparator {
    public:
      PRBool Equals(const nsSMILAnimationFunction* aElem1,
                    const nsSMILAnimationFunction* aElem2) const {
        return (aElem1->CompareTo(aElem2) == 0);
      }
      PRBool LessThan(const nsSMILAnimationFunction* aElem1,
                      const nsSMILAnimationFunction* aElem2) const {
        return (aElem1->CompareTo(aElem2) < 0);
      }
  };

protected:
  // Typedefs
  typedef nsTArray<nsSMILValue> nsSMILValueArray;

  // Types
  enum nsSMILCalcMode
  {
    CALC_LINEAR,
    CALC_DISCRETE,
    CALC_PACED,
    CALC_SPLINE
  };

  // Used for sorting nsSMILAnimationFunctions
  nsSMILTime GetBeginTime() const { return mBeginTime; }

  // Property getters
  PRBool                 GetAccumulate() const;
  PRBool                 GetAdditive() const;
  virtual nsSMILCalcMode GetCalcMode() const;

  // Property setters
  nsresult SetAccumulate(const nsAString& aAccumulate, nsAttrValue& aResult);
  nsresult SetAdditive(const nsAString& aAdditive, nsAttrValue& aResult);
  nsresult SetCalcMode(const nsAString& aCalcMode, nsAttrValue& aResult);
  nsresult SetKeyTimes(const nsAString& aKeyTimes, nsAttrValue& aResult);
  nsresult SetKeySplines(const nsAString& aKeySplines, nsAttrValue& aResult);

  // Property un-setters
  void     UnsetAccumulate();
  void     UnsetAdditive();
  void     UnsetCalcMode();
  void     UnsetKeyTimes();
  void     UnsetKeySplines();

  // Helpers
  nsresult InterpolateResult(const nsSMILValueArray& aValues,
                             nsSMILValue& aResult,
                             nsSMILValue& aBaseValue);
  nsresult AccumulateResult(const nsSMILValueArray& aValues,
                            nsSMILValue& aResult);

  nsresult ComputePacedPosition(const nsSMILValueArray& aValues,
                                double aSimpleProgress,
                                double& aIntervalProgress,
                                const nsSMILValue*& aFrom,
                                const nsSMILValue*& aTo);
  double   ComputePacedTotalDistance(const nsSMILValueArray& aValues) const;

  void     ScaleSimpleProgress(double& aProgress);
  void     ScaleIntervalProgress(double& aProgress, PRUint32 aIntervalIndex,
                                 PRUint32 aNumIntervals);

  // Convenience attribute getters -- use these instead of querying
  // mAnimationElement as these may need to be overridden by subclasses
  virtual PRBool             HasAttr(nsIAtom* aAttName) const;
  virtual const nsAttrValue* GetAttr(nsIAtom* aAttName) const;
  virtual PRBool             GetAttr(nsIAtom* aAttName,
                                     nsAString& aResult) const;

  PRBool   ParseAttr(nsIAtom* aAttName, const nsISMILAttr& aSMILAttr,
                     nsSMILValue& aResult) const;
  nsresult GetValues(const nsISMILAttr& aSMILAttr,
                     nsSMILValueArray& aResult);
  void     UpdateValuesArray();
  PRBool   IsToAnimation() const;
  PRBool   IsAdditive() const;
  void     CheckKeyTimes(PRUint32 aNumValues);
  void     CheckKeySplines(PRUint32 aNumValues);


  // Members
  // -------

  static nsAttrValue::EnumTable sAdditiveTable[];
  static nsAttrValue::EnumTable sCalcModeTable[];
  static nsAttrValue::EnumTable sAccumulateTable[];

  nsTArray<double>              mKeyTimes;
  nsTArray<nsSMILKeySpline>     mKeySplines;

  PRPackedBool                  mIsActive;
  PRPackedBool                  mIsFrozen;

  // These are the parameters provided by the previous sample. Currently we
  // perform lazy calculation. That is, we only calculate the result if and when
  // instructed by the compositor. This allows us to apply the result directly
  // to the animation value and allows the compositor to filter out functions
  // that it determines will not contribute to the final result.
  nsSMILTime                    mSampleTime; // sample time within simple dur
  nsSMILTimeValue               mSimpleDuration;
  PRUint32                      mRepeatIteration;
  PRPackedBool                  mLastValue;
  PRPackedBool                  mHasChanged;

  nsSMILTime                    mBeginTime; // document time
  
  // The owning animation element. This is used for sorting based on document
  // position and for fetching attribute values stored in the element.
  // Raw pointer is OK here, because this nsSMILAnimationFunction can't outlive
  // its owning animation element.
  nsISMILAnimationElement*      mAnimationElement;

  // Which attributes have been set but have had errors. This is not used for
  // all attributes but only those which have specified error behaviour
  // associated with them.
  PRUint16                      mErrorFlags;

  // This is for the very specific case where we have a 'to' animation that is
  // frozen part way through the simple duration and there are other active
  // lower-priority animations targetting the same attribute. In this case
  // SMILANIM 3.3.6 says:
  //
  //   The value for F(t) when a to-animation is frozen (at the end of the
  //   simple duration) is just the to value. If a to-animation is frozen
  //   anywhere within the simple duration (e.g., using a repeatCount of "2.5"),
  //   the value for F(t) when the animation is frozen is the value computed for
  //   the end of the active duration. Even if other, lower priority animations
  //   are active while a to-animation is frozen, the value for F(t) does not
  //   change.
  //
  // To implement this properly we'd need to force a resample of all the lower
  // priority animations at the active end of this animation--something which
  // would introduce unwanted coupling between the timing and animation model.
  // Instead we just save the value calculated when this animation is frozen (in
  // which case this animation will be sampled at the active end and the lower
  // priority animations should be sampled at a time pretty close to this,
  // provided we have a reasonable frame rate and we aren't seeking).
  //
  // @see
  // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#FromToByAndAdditive
  nsSMILValue                   mFrozenValue;
};

#endif // NS_SMILANIMATIONFUNCTION_H_

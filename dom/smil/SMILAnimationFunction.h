/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILANIMATIONFUNCTION_H_
#define DOM_SMIL_SMILANIMATIONFUNCTION_H_

#include "mozilla/SMILAttr.h"
#include "mozilla/SMILKeySpline.h"
#include "mozilla/SMILTargetIdentifier.h"
#include "mozilla/SMILTimeValue.h"
#include "mozilla/SMILTypes.h"
#include "mozilla/SMILValue.h"
#include "nsAttrValue.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}  // namespace dom

//----------------------------------------------------------------------
// SMILAnimationFunction
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
class SMILAnimationFunction {
 public:
  SMILAnimationFunction();

  /*
   * Sets the owning animation element which this class uses to query attribute
   * values and compare document positions.
   */
  void SetAnimationElement(
      mozilla::dom::SVGAnimationElement* aAnimationElement);

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
   * @return  true if aAttribute is a recognized animation-related
   *          attribute; false otherwise.
   */
  virtual bool SetAttr(nsAtom* aAttribute, const nsAString& aValue,
                       nsAttrValue& aResult, nsresult* aParseResult = nullptr);

  /*
   * Unsets the given attribute.
   *
   * @returns true if aAttribute is a recognized animation-related
   *          attribute; false otherwise.
   */
  virtual bool UnsetAttr(nsAtom* aAttribute);

  /**
   * Indicate a new sample has occurred.
   *
   * @param aSampleTime The sample time for this timed element expressed in
   *                    simple time.
   * @param aSimpleDuration The simple duration for this timed element.
   * @param aRepeatIteration  The repeat iteration for this sample. The first
   *                          iteration has a value of 0.
   */
  void SampleAt(SMILTime aSampleTime, const SMILTimeValue& aSimpleDuration,
                uint32_t aRepeatIteration);

  /**
   * Indicate to sample using the last value defined for the animation function.
   * This value is not normally sampled due to the end-point exclusive timing
   * model but only occurs when the fill mode is "freeze" and the active
   * duration is an even multiple of the simple duration.
   *
   * @param aRepeatIteration  The repeat iteration for this sample. The first
   *                          iteration has a value of 0.
   */
  void SampleLastValue(uint32_t aRepeatIteration);

  /**
   * Indicate that this animation is now active. This is used to instruct the
   * animation function that it should now add its result to the animation
   * sandwich. The begin time is also provided for proper prioritization of
   * animation functions, and for this reason, this method must be called
   * before either of the Sample methods.
   *
   * @param aBeginTime The begin time for the newly active interval.
   */
  void Activate(SMILTime aBeginTime);

  /**
   * Indicate that this animation is no longer active. This is used to instruct
   * the animation function that it should no longer add its result to the
   * animation sandwich.
   *
   * @param aIsFrozen true if this animation should continue to contribute
   *                  to the animation sandwich using the most recent sample
   *                  parameters.
   */
  void Inactivate(bool aIsFrozen);

  /**
   * Combines the result of this animation function for the last sample with the
   * specified value.
   *
   * @param aSMILAttr This animation's target attribute. Used here for
   *                  doing attribute-specific parsing of from/to/by/values.
   *
   * @param aResult   The value to compose with.
   */
  void ComposeResult(const SMILAttr& aSMILAttr, SMILValue& aResult);

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
  int8_t CompareTo(const SMILAnimationFunction* aOther) const;

  /*
   * The following methods are provided so that the compositor can optimize its
   * operations by only composing those animation that will affect the final
   * result.
   */

  /**
   * Indicates if the animation is currently active or frozen. Inactive
   * animations will not contribute to the composed result.
   *
   * @return  true if the animation is active or frozen, false otherwise.
   */
  bool IsActiveOrFrozen() const {
    /*
     * - Frozen animations should be considered active for the purposes of
     * compositing.
     * - This function does not assume that our SMILValues (by/from/to/values)
     * have already been parsed.
     */
    return (mIsActive || mIsFrozen);
  }

  /**
   * Indicates if the animation is active.
   *
   * @return  true if the animation is active, false otherwise.
   */
  bool IsActive() const { return mIsActive; }

  /**
   * Indicates if this animation will replace the passed in result rather than
   * adding to it. Animations that replace the underlying value may be called
   * without first calling lower priority animations.
   *
   * @return  True if the animation will replace, false if it will add or
   *          otherwise build on the passed in value.
   */
  virtual bool WillReplace() const;

  /**
   * Indicates if the parameters for this animation have changed since the last
   * time it was composited. This allows rendering to be performed only when
   * necessary, particularly when no animations are active.
   *
   * Note that the caller is responsible for determining if the animation
   * target has changed (with help from my UpdateCachedTarget() method).
   *
   * @return  true if the animation parameters have changed, false
   *          otherwise.
   */
  bool HasChanged() const;

  /**
   * This method lets us clear the 'HasChanged' flag for inactive animations
   * after we've reacted to their change to the 'inactive' state, so that we
   * won't needlessly recompose their targets in every sample.
   *
   * This should only be called on an animation function that is inactive and
   * that returns true from HasChanged().
   */
  void ClearHasChanged() {
    MOZ_ASSERT(HasChanged(),
               "clearing mHasChanged flag, when it's already false");
    MOZ_ASSERT(!IsActiveOrFrozen(),
               "clearing mHasChanged flag for active animation");
    mHasChanged = false;
  }

  /**
   * Updates the cached record of our animation target, and returns a boolean
   * that indicates whether the target has changed since the last call to this
   * function. (This lets SMILCompositor check whether its animation
   * functions have changed value or target since the last sample.  If none of
   * them have, then the compositor doesn't need to do anything.)
   *
   * @param aNewTarget A SMILTargetIdentifier representing the animation
   *                   target of this function for this sample.
   * @return  true if |aNewTarget| is different from the old cached value;
   *          otherwise, false.
   */
  bool UpdateCachedTarget(const SMILTargetIdentifier& aNewTarget);

  /**
   * Returns true if this function was skipped in the previous sample (because
   * there was a higher-priority non-additive animation). If a skipped animation
   * function is later used, then the animation sandwich must be recomposited.
   */
  bool WasSkippedInPrevSample() const { return mWasSkippedInPrevSample; }

  /**
   * Mark this animation function as having been skipped. By marking the
   * function as skipped, if it is used in a subsequent sample we'll know to
   * recomposite the sandwich.
   */
  void SetWasSkipped() { mWasSkippedInPrevSample = true; }

  /**
   * Returns true if we need to recalculate the animation value on every sample.
   * (e.g. because it depends on context like the font-size)
   */
  bool ValueNeedsReparsingEverySample() const {
    return mValueNeedsReparsingEverySample;
  }

  // Comparator utility class, used for sorting SMILAnimationFunctions
  class Comparator {
   public:
    bool Equals(const SMILAnimationFunction* aElem1,
                const SMILAnimationFunction* aElem2) const {
      return (aElem1->CompareTo(aElem2) == 0);
    }
    bool LessThan(const SMILAnimationFunction* aElem1,
                  const SMILAnimationFunction* aElem2) const {
      return (aElem1->CompareTo(aElem2) < 0);
    }
  };

 protected:
  // alias declarations
  using SMILValueArray = FallibleTArray<SMILValue>;

  // Types
  enum SMILCalcMode : uint8_t {
    CALC_LINEAR,
    CALC_DISCRETE,
    CALC_PACED,
    CALC_SPLINE
  };

  // Used for sorting SMILAnimationFunctions
  SMILTime GetBeginTime() const { return mBeginTime; }

  // Property getters
  bool GetAccumulate() const;
  bool GetAdditive() const;
  virtual SMILCalcMode GetCalcMode() const;

  // Property setters
  nsresult SetAccumulate(const nsAString& aAccumulate, nsAttrValue& aResult);
  nsresult SetAdditive(const nsAString& aAdditive, nsAttrValue& aResult);
  nsresult SetCalcMode(const nsAString& aCalcMode, nsAttrValue& aResult);
  nsresult SetKeyTimes(const nsAString& aKeyTimes, nsAttrValue& aResult);
  nsresult SetKeySplines(const nsAString& aKeySplines, nsAttrValue& aResult);

  // Property un-setters
  void UnsetAccumulate();
  void UnsetAdditive();
  void UnsetCalcMode();
  void UnsetKeyTimes();
  void UnsetKeySplines();

  // Helpers
  virtual nsresult InterpolateResult(const SMILValueArray& aValues,
                                     SMILValue& aResult, SMILValue& aBaseValue);
  nsresult AccumulateResult(const SMILValueArray& aValues, SMILValue& aResult);

  nsresult ComputePacedPosition(const SMILValueArray& aValues,
                                double aSimpleProgress,
                                double& aIntervalProgress,
                                const SMILValue*& aFrom, const SMILValue*& aTo);
  double ComputePacedTotalDistance(const SMILValueArray& aValues) const;

  /**
   * Adjust the simple progress, that is, the point within the simple duration,
   * by applying any keyTimes.
   */
  double ScaleSimpleProgress(double aProgress, SMILCalcMode aCalcMode);
  /**
   * Adjust the progress within an interval, that is, between two animation
   * values, by applying any keySplines.
   */
  double ScaleIntervalProgress(double aProgress, uint32_t aIntervalIndex);

  // Convenience attribute getters -- use these instead of querying
  // mAnimationElement as these may need to be overridden by subclasses
  virtual bool HasAttr(nsAtom* aAttName) const;
  virtual const nsAttrValue* GetAttr(nsAtom* aAttName) const;
  virtual bool GetAttr(nsAtom* aAttName, nsAString& aResult) const;

  bool ParseAttr(nsAtom* aAttName, const SMILAttr& aSMILAttr,
                 SMILValue& aResult, bool& aPreventCachingOfSandwich) const;

  virtual nsresult GetValues(const SMILAttr& aSMILAttr,
                             SMILValueArray& aResult);

  virtual void CheckValueListDependentAttrs(uint32_t aNumValues);
  void CheckKeyTimes(uint32_t aNumValues);
  void CheckKeySplines(uint32_t aNumValues);

  virtual bool IsToAnimation() const {
    return !HasAttr(nsGkAtoms::values) && HasAttr(nsGkAtoms::to) &&
           !HasAttr(nsGkAtoms::from);
  }

  // Returns true if we know our composited value won't change over the
  // simple duration of this animation (for a fixed base value).
  virtual bool IsValueFixedForSimpleDuration() const;

  inline bool IsAdditive() const {
    /*
     * Animation is additive if:
     *
     * (1) additive = "sum" (GetAdditive() == true), or
     * (2) it is 'by animation' (by is set, from and values are not)
     *
     * Although animation is not additive if it is 'to animation'
     */
    bool isByAnimation = (!HasAttr(nsGkAtoms::values) &&
                          HasAttr(nsGkAtoms::by) && !HasAttr(nsGkAtoms::from));
    return !IsToAnimation() && (GetAdditive() || isByAnimation);
  }

  // Setters for error flags
  // These correspond to bit-indices in mErrorFlags, for tracking parse errors
  // in these attributes, when those parse errors should block us from doing
  // animation.
  enum AnimationAttributeIdx {
    BF_ACCUMULATE = 0,
    BF_ADDITIVE = 1,
    BF_CALC_MODE = 2,
    BF_KEY_TIMES = 3,
    BF_KEY_SPLINES = 4,
    BF_KEY_POINTS = 5  // <animateMotion> only
  };

  inline void SetAccumulateErrorFlag(bool aNewValue) {
    SetErrorFlag(BF_ACCUMULATE, aNewValue);
  }
  inline void SetAdditiveErrorFlag(bool aNewValue) {
    SetErrorFlag(BF_ADDITIVE, aNewValue);
  }
  inline void SetCalcModeErrorFlag(bool aNewValue) {
    SetErrorFlag(BF_CALC_MODE, aNewValue);
  }
  inline void SetKeyTimesErrorFlag(bool aNewValue) {
    SetErrorFlag(BF_KEY_TIMES, aNewValue);
  }
  inline void SetKeySplinesErrorFlag(bool aNewValue) {
    SetErrorFlag(BF_KEY_SPLINES, aNewValue);
  }
  inline void SetKeyPointsErrorFlag(bool aNewValue) {
    SetErrorFlag(BF_KEY_POINTS, aNewValue);
  }
  inline void SetErrorFlag(AnimationAttributeIdx aField, bool aValue) {
    if (aValue) {
      mErrorFlags |= (0x01 << aField);
    } else {
      mErrorFlags &= ~(0x01 << aField);
    }
  }

  // Members
  // -------

  static nsAttrValue::EnumTable sAdditiveTable[];
  static nsAttrValue::EnumTable sCalcModeTable[];
  static nsAttrValue::EnumTable sAccumulateTable[];

  FallibleTArray<double> mKeyTimes;
  FallibleTArray<SMILKeySpline> mKeySplines;

  // These are the parameters provided by the previous sample. Currently we
  // perform lazy calculation. That is, we only calculate the result if and when
  // instructed by the compositor. This allows us to apply the result directly
  // to the animation value and allows the compositor to filter out functions
  // that it determines will not contribute to the final result.
  SMILTime mSampleTime;  // sample time within simple dur
  SMILTimeValue mSimpleDuration;
  uint32_t mRepeatIteration;

  SMILTime mBeginTime;  // document time

  // The owning animation element. This is used for sorting based on document
  // position and for fetching attribute values stored in the element.
  // Raw pointer is OK here, because this SMILAnimationFunction can't outlive
  // its owning animation element.
  mozilla::dom::SVGAnimationElement* mAnimationElement;

  // Which attributes have been set but have had errors. This is not used for
  // all attributes but only those which have specified error behaviour
  // associated with them.
  uint16_t mErrorFlags;

  // Allows us to check whether an animation function has changed target from
  // sample to sample (because if neither target nor animated value have
  // changed, we don't have to do anything).
  SMILWeakTargetIdentifier mLastTarget;

  // Boolean flags
  bool mIsActive : 1;
  bool mIsFrozen : 1;
  bool mLastValue : 1;
  bool mHasChanged : 1;
  bool mValueNeedsReparsingEverySample : 1;
  bool mPrevSampleWasSingleValueAnimation : 1;
  bool mWasSkippedInPrevSample : 1;
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILANIMATIONFUNCTION_H_

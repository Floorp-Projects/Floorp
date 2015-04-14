/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCSSPseudoElements.h"
#include "nsIDocument.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/StickyTimeDuration.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Nullable.h"
#include "nsSMILKeySpline.h"
#include "nsStyleStruct.h" // for nsTimingFunction

struct JSContext;
class nsCSSPropertySet;

namespace mozilla {
namespace css {
class AnimValuesStyleRule;
} // namespace css

/**
 * Input timing parameters.
 *
 * Eventually this will represent all the input timing parameters specified
 * by content but for now it encapsulates just the subset of those
 * parameters passed to GetPositionInIteration.
 */
struct AnimationTiming
{
  TimeDuration mIterationDuration;
  TimeDuration mDelay;
  float mIterationCount; // mozilla::PositiveInfinity<float>() means infinite
  uint8_t mDirection;
  uint8_t mFillMode;

  bool FillsForwards() const {
    return mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BOTH ||
           mFillMode == NS_STYLE_ANIMATION_FILL_MODE_FORWARDS;
  }
  bool FillsBackwards() const {
    return mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BOTH ||
           mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS;
  }
  bool operator==(const AnimationTiming& aOther) const {
    return mIterationDuration == aOther.mIterationDuration &&
           mDelay == aOther.mDelay &&
           mIterationCount == aOther.mIterationCount &&
           mDirection == aOther.mDirection &&
           mFillMode == aOther.mFillMode;
  }
  bool operator!=(const AnimationTiming& aOther) const {
    return !(*this == aOther);
  }
};

/**
 * Stores the results of calculating the timing properties of an animation
 * at a given sample time.
 */
struct ComputedTiming
{
  ComputedTiming()
    : mTimeFraction(kNullTimeFraction)
    , mCurrentIteration(0)
    , mPhase(AnimationPhase_Null)
  { }

  static const double kNullTimeFraction;

  // The total duration of the animation including all iterations.
  // Will equal StickyTimeDuration::Forever() if the animation repeats
  // indefinitely.
  StickyTimeDuration mActiveDuration;

  // Will be kNullTimeFraction if the animation is neither animating nor
  // filling at the sampled time.
  double mTimeFraction;

  // Zero-based iteration index (meaningless if mTimeFraction is
  // kNullTimeFraction).
  uint64_t mCurrentIteration;

  enum {
    // Not sampled (null sample time)
    AnimationPhase_Null,
    // Sampled prior to the start of the active interval
    AnimationPhase_Before,
    // Sampled within the active interval
    AnimationPhase_Active,
    // Sampled after (or at) the end of the active interval
    AnimationPhase_After
  } mPhase;
};

class ComputedTimingFunction
{
public:
  typedef nsTimingFunction::Type Type;
  void Init(const nsTimingFunction &aFunction);
  double GetValue(double aPortion) const;
  const nsSMILKeySpline* GetFunction() const {
    NS_ASSERTION(mType == nsTimingFunction::Function, "Type mismatch");
    return &mTimingFunction;
  }
  Type GetType() const { return mType; }
  uint32_t GetSteps() const { return mSteps; }
  bool operator==(const ComputedTimingFunction& aOther) const {
    return mType == aOther.mType &&
           (mType == nsTimingFunction::Function ?
	    mTimingFunction == aOther.mTimingFunction :
	    mSteps == aOther.mSteps);
  }
  bool operator!=(const ComputedTimingFunction& aOther) const {
    return !(*this == aOther);
  }

private:
  Type mType;
  nsSMILKeySpline mTimingFunction;
  uint32_t mSteps;
};

struct AnimationPropertySegment
{
  float mFromKey, mToKey;
  StyleAnimationValue mFromValue, mToValue;
  ComputedTimingFunction mTimingFunction;

  bool operator==(const AnimationPropertySegment& aOther) const {
    return mFromKey == aOther.mFromKey &&
           mToKey == aOther.mToKey &&
           mFromValue == aOther.mFromValue &&
           mToValue == aOther.mToValue &&
           mTimingFunction == aOther.mTimingFunction;
  }
  bool operator!=(const AnimationPropertySegment& aOther) const {
    return !(*this == aOther);
  }
};

struct AnimationProperty
{
  nsCSSProperty mProperty;

  // Does this property win in the CSS Cascade?
  //
  // For CSS transitions, this is true as long as a CSS animation on the
  // same property and element is not running, in which case we set this
  // to false so that the animation (lower in the cascade) can win.  We
  // then use this to decide whether to apply the style both in the CSS
  // cascade and for OMTA.
  //
  // For CSS Animations, which are overridden by !important rules in the
  // cascade, we actually determine this from the CSS cascade
  // computations, and then use it for OMTA.
  // **NOTE**: For CSS animations, we only bother setting mWinsInCascade
  // accurately for properties that we can animate on the compositor.
  // For other properties, we make it always be true.
  bool mWinsInCascade;

  InfallibleTArray<AnimationPropertySegment> mSegments;

  bool operator==(const AnimationProperty& aOther) const {
    return mProperty == aOther.mProperty &&
           mWinsInCascade == aOther.mWinsInCascade &&
           mSegments == aOther.mSegments;
  }
  bool operator!=(const AnimationProperty& aOther) const {
    return !(*this == aOther);
  }
};

struct ElementPropertyTransition;

namespace dom {

class AnimationEffect;

class Animation : public nsWrapperCache
{
public:
  Animation(nsIDocument* aDocument,
            Element* aTarget,
            nsCSSPseudoElements::Type aPseudoType,
            const AnimationTiming &aTiming,
            const nsSubstring& aName)
    : mDocument(aDocument)
    , mTarget(aTarget)
    , mTiming(aTiming)
    , mName(aName)
    , mIsFinishedTransition(false)
    , mPseudoType(aPseudoType)
  {
    MOZ_ASSERT(aTarget, "null animation target is not yet supported");
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Animation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Animation)

  nsIDocument* GetParentObject() const { return mDocument; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // FIXME: If we succeed in moving transition-specific code to a type of
  // AnimationEffect (as per the Web Animations API) we should remove these
  // virtual methods.
  virtual ElementPropertyTransition* AsTransition() { return nullptr; }
  virtual const ElementPropertyTransition* AsTransition() const {
    return nullptr;
  }

  // Animation interface
  // This currently returns a new object each time when used from C++ but is
  // cached when used from JS.
  already_AddRefed<AnimationEffect> GetEffect();
  Element* GetTarget() const {
    // Currently we only implement Element.getAnimations() which only
    // returns animations targetting Elements so this should never
    // be called for an animation that targets a pseudo-element.
    MOZ_ASSERT(mPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement,
               "Requesting the target of an Animation that targets a"
               " pseudo-element is not yet supported.");
    return mTarget;
  }

  // Temporary workaround to return both the target element and pseudo-type
  // until we implement PseudoElement.
  void GetTarget(Element*& aTarget,
                 nsCSSPseudoElements::Type& aPseudoType) const {
    aTarget = mTarget;
    aPseudoType = mPseudoType;
  }

  void SetParentTime(Nullable<TimeDuration> aParentTime);

  const AnimationTiming& Timing() const {
    return mTiming;
  }
  AnimationTiming& Timing() {
    return mTiming;
  }

  virtual const nsString& Name() const {
    return mName;
  }

  // Return the duration from the start the active interval to the point where
  // the animation begins playback. This is zero unless the animation has
  // a negative delay in which case it is the absolute value of the delay.
  // This is used for setting the elapsedTime member of CSS AnimationEvents.
  TimeDuration InitialAdvance() const {
    return std::max(TimeDuration(), mTiming.mDelay * -1);
  }

  Nullable<TimeDuration> GetLocalTime() const {
    // Since the *animation* start time is currently always zero, the local
    // time is equal to the parent time.
    return mParentTime;
  }

  // This function takes as input the timing parameters of an animation and
  // returns the computed timing at the specified local time.
  //
  // The local time may be null in which case only static parameters such as the
  // active duration are calculated. All other members of the returned object
  // are given a null/initial value.
  //
  // This function returns ComputedTiming::kNullTimeFraction for the
  // mTimeFraction member of the return value if the animation should not be
  // run (because it is not currently active and is not filling at this time).
  static ComputedTiming
  GetComputedTimingAt(const Nullable<TimeDuration>& aLocalTime,
                      const AnimationTiming& aTiming);

  // Shortcut for that gets the computed timing using the current local time as
  // calculated from the timeline time.
  ComputedTiming GetComputedTiming(const AnimationTiming* aTiming
                                     = nullptr) const {
    return GetComputedTimingAt(GetLocalTime(), aTiming ? *aTiming : mTiming);
  }

  // Return the duration of the active interval for the given timing parameters.
  static StickyTimeDuration
  ActiveDuration(const AnimationTiming& aTiming);

  // After transitions finish they need to be retained in order to
  // address the issue described in
  // https://lists.w3.org/Archives/Public/www-style/2015Jan/0444.html .
  // However, finished transitions are ignored for many purposes.
  bool IsFinishedTransition() const {
    return mIsFinishedTransition;
  }

  void SetIsFinishedTransition() {
    MOZ_ASSERT(AsTransition(),
               "Calling SetIsFinishedTransition but it's not a transition");
    mIsFinishedTransition = true;
  }

  bool IsInPlay(const AnimationPlayer& aPlayer) const;
  bool IsCurrent(const AnimationPlayer& aPlayer) const;
  bool IsInEffect() const;

  const AnimationProperty*
  GetAnimationOfProperty(nsCSSProperty aProperty) const;
  bool HasAnimationOfProperty(nsCSSProperty aProperty) const {
    return GetAnimationOfProperty(aProperty) != nullptr;
  }
  bool HasAnimationOfProperties(const nsCSSProperty* aProperties,
                                size_t aPropertyCount) const;
  const InfallibleTArray<AnimationProperty>& Properties() const {
    return mProperties;
  }
  InfallibleTArray<AnimationProperty>& Properties() {
    return mProperties;
  }

  // Updates |aStyleRule| with the animation values produced by this
  // Animation for the current time except any properties already contained
  // in |aSetProperties|.
  // Any updated properties are added to |aSetProperties|.
  void ComposeStyle(nsRefPtr<css::AnimValuesStyleRule>& aStyleRule,
                    nsCSSPropertySet& aSetProperties);

protected:
  virtual ~Animation() { }

  // We use a document for a parent object since the other likely candidate,
  // the target element, can be empty.
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<Element> mTarget;
  Nullable<TimeDuration> mParentTime;

  AnimationTiming mTiming;
  nsString mName;
  // A flag to mark transitions that have finished and are due to
  // be removed on the next throttle-able cycle.
  bool mIsFinishedTransition;
  nsCSSPseudoElements::Type mPseudoType;

  InfallibleTArray<AnimationProperty> mProperties;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Animation_h

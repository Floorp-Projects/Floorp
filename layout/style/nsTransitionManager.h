/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#ifndef nsTransitionManager_h_
#define nsTransitionManager_h_

#include "mozilla/ContentEvents.h"
#include "mozilla/EffectCompositor.h" // For EffectCompositor::CascadeLevel
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "AnimationCommon.h"
#include "nsCSSProps.h"

class nsIGlobalObject;
class nsStyleContext;
class nsPresContext;
class nsCSSPropertyIDSet;

namespace mozilla {
enum class CSSPseudoElementType : uint8_t;
struct Keyframe;
struct StyleTransition;
} // namespace mozilla

/*****************************************************************************
 * Per-Element data                                                          *
 *****************************************************************************/

namespace mozilla {

struct ElementPropertyTransition : public dom::KeyframeEffectReadOnly
{
  ElementPropertyTransition(nsIDocument* aDocument,
                            Maybe<OwningAnimationTarget>& aTarget,
                            const TimingParams &aTiming,
                            StyleAnimationValue aStartForReversingTest,
                            double aReversePortion,
                            const KeyframeEffectParams& aEffectOptions)
    : dom::KeyframeEffectReadOnly(aDocument, aTarget, aTiming, aEffectOptions)
    , mStartForReversingTest(aStartForReversingTest)
    , mReversePortion(aReversePortion)
  { }

  ElementPropertyTransition* AsTransition() override { return this; }
  const ElementPropertyTransition* AsTransition() const override
  {
    return this;
  }

  nsCSSPropertyID TransitionProperty() const {
    MOZ_ASSERT(mKeyframes.Length() == 2,
               "Transitions should have exactly two animation keyframes. "
               "Perhaps we are using an un-initialized transition?");
    MOZ_ASSERT(mKeyframes[0].mPropertyValues.Length() == 1,
               "Transitions should have exactly one property in their first "
               "frame");
    return mKeyframes[0].mPropertyValues[0].mProperty;
  }

  StyleAnimationValue ToValue() const {
    // If we failed to generate properties from the transition frames,
    // return a null value but also show a warning since we should be
    // detecting that kind of situation in advance and not generating a
    // transition in the first place.
    if (mProperties.Length() < 1 ||
        mProperties[0].mSegments.Length() < 1) {
      NS_WARNING("Failed to generate transition property values");
      return StyleAnimationValue();
    }
    return mProperties[0].mSegments[0].mToValue;
  }

  // This is the start value to be used for a check for whether a
  // transition is being reversed.  Normally the same as
  // mProperties[0].mSegments[0].mFromValue, except when this transition
  // started as the reversal of another in-progress transition.
  // Needed so we can handle two reverses in a row.
  StyleAnimationValue mStartForReversingTest;
  // Likewise, the portion (in value space) of the "full" reversed
  // transition that we're actually covering.  For example, if a :hover
  // effect has a transition that moves the element 10px to the right
  // (by changing 'left' from 0px to 10px), and the mouse moves in to
  // the element (starting the transition) but then moves out after the
  // transition has advanced 4px, the second transition (from 10px/4px
  // to 0px) will have mReversePortion of 0.4.  (If the mouse then moves
  // in again when the transition is back to 2px, the mReversePortion
  // for the third transition (from 0px/2px to 10px) will be 0.8.
  double mReversePortion;

  // Compute the portion of the *value* space that we should be through
  // at the current time.  (The input to the transition timing function
  // has time units, the output has value units.)
  double CurrentValuePortion() const;

  // For a new transition interrupting an existing transition on the
  // compositor, update the start value to match the value of the replaced
  // transitions at the current time.
  void UpdateStartValueFromReplacedTransition();

  struct ReplacedTransitionProperties {
    TimeDuration mStartTime;
    double mPlaybackRate;
    TimingParams mTiming;
    Maybe<ComputedTimingFunction> mTimingFunction;
    StyleAnimationValue mFromValue, mToValue;
  };
  Maybe<ReplacedTransitionProperties> mReplacedTransition;
};

namespace dom {

class CSSTransition final : public Animation
{
public:
 explicit CSSTransition(nsIGlobalObject* aGlobal)
    : dom::Animation(aGlobal)
    , mWasFinishedOnLastTick(false)
    , mNeedsNewAnimationIndexWhenRun(false)
    , mTransitionProperty(eCSSProperty_UNKNOWN)
  {
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  CSSTransition* AsCSSTransition() override { return this; }
  const CSSTransition* AsCSSTransition() const override { return this; }

  // CSSTransition interface
  void GetTransitionProperty(nsString& aRetVal) const;

  // Animation interface overrides
  virtual AnimationPlayState PlayStateFromJS() const override;
  virtual void PlayFromJS(ErrorResult& aRv) override;

  // A variant of Play() that avoids posting style updates since this method
  // is expected to be called whilst already updating style.
  void PlayFromStyle()
  {
    ErrorResult rv;
    PlayNoUpdate(rv, Animation::LimitBehavior::Continue);
    // play() should not throw when LimitBehavior is Continue
    MOZ_ASSERT(!rv.Failed(), "Unexpected exception playing transition");
  }

  void CancelFromStyle() override
  {
    // The animation index to use for compositing will be established when
    // this transition next transitions out of the idle state but we still
    // update it now so that the sort order of this transition remains
    // defined until that moment.
    //
    // See longer explanation in CSSAnimation::CancelFromStyle.
    mAnimationIndex = sNextAnimationIndex++;
    mNeedsNewAnimationIndexWhenRun = true;

    Animation::CancelFromStyle();

    // It is important we do this *after* calling CancelFromStyle().
    // This is because CancelFromStyle() will end up posting a restyle and
    // that restyle should target the *transitions* level of the cascade.
    // However, once we clear the owning element, CascadeLevel() will begin
    // returning CascadeLevel::Animations.
    mOwningElement = OwningElementRef();
  }

  void SetEffectFromStyle(AnimationEffectReadOnly* aEffect);

  void Tick() override;

  nsCSSPropertyID TransitionProperty() const;
  StyleAnimationValue ToValue() const;

  bool HasLowerCompositeOrderThan(const CSSTransition& aOther) const;
  EffectCompositor::CascadeLevel CascadeLevel() const override
  {
    return IsTiedToMarkup() ?
           EffectCompositor::CascadeLevel::Transitions :
           EffectCompositor::CascadeLevel::Animations;
  }

  void SetCreationSequence(uint64_t aIndex)
  {
    MOZ_ASSERT(IsTiedToMarkup());
    mAnimationIndex = aIndex;
  }

  // Sets the owning element which is used for determining the composite
  // oder of CSSTransition objects generated from CSS markup.
  //
  // @see mOwningElement
  void SetOwningElement(const OwningElementRef& aElement)
  {
    mOwningElement = aElement;
  }
  // True for transitions that are generated from CSS markup and continue to
  // reflect changes to that markup.
  bool IsTiedToMarkup() const { return mOwningElement.IsSet(); }

  // Return the animation current time based on a given TimeStamp, a given
  // start time and a given playbackRate on a given timeline.  This is useful
  // when we estimate the current animated value running on the compositor
  // because the animation on the compositor may be running ahead while
  // main-thread is busy.
  static Nullable<TimeDuration> GetCurrentTimeAt(
      const DocumentTimeline& aTimeline,
      const TimeStamp& aBaseTime,
      const TimeDuration& aStartTime,
      double aPlaybackRate);

protected:
  virtual ~CSSTransition()
  {
    MOZ_ASSERT(!mOwningElement.IsSet(), "Owning element should be cleared "
                                        "before a CSS transition is destroyed");
  }

  // Animation overrides
  void UpdateTiming(SeekFlag aSeekFlag,
                    SyncNotifyFlag aSyncNotifyFlag) override;

  void QueueEvents();

  // The (pseudo-)element whose computed transition-property refers to this
  // transition (if any).
  //
  // This is used for determining the relative composite order of transitions
  // generated from CSS markup.
  //
  // Typically this will be the same as the target element of the keyframe
  // effect associated with this transition. However, it can differ in the
  // following circumstances:
  //
  // a) If script removes or replaces the effect of this transition,
  // b) If this transition is cancelled (e.g. by updating the
  //    transition-property or removing the owning element from the document),
  // c) If this object is generated from script using the CSSTransition
  //    constructor.
  //
  // For (b) and (c) the owning element will return !IsSet().
  OwningElementRef mOwningElement;

  bool mWasFinishedOnLastTick;

  // When true, indicates that when this transition next leaves the idle state,
  // its animation index should be updated.
  bool mNeedsNewAnimationIndexWhenRun;

  // Store the transition property and to-value here since we need that
  // information in order to determine if there is an existing transition
  // for a given style change. We can't store that information on the
  // ElementPropertyTransition (effect) however since it can be replaced
  // using the Web Animations API.
  nsCSSPropertyID mTransitionProperty;
  StyleAnimationValue mTransitionToValue;
};

} // namespace dom

template <>
struct AnimationTypeTraits<dom::CSSTransition>
{
  static nsIAtom* ElementPropertyAtom()
  {
    return nsGkAtoms::transitionsProperty;
  }
  static nsIAtom* BeforePropertyAtom()
  {
    return nsGkAtoms::transitionsOfBeforeProperty;
  }
  static nsIAtom* AfterPropertyAtom()
  {
    return nsGkAtoms::transitionsOfAfterProperty;
  }
};

struct TransitionEventInfo {
  RefPtr<dom::Element> mElement;
  RefPtr<dom::Animation> mAnimation;
  InternalTransitionEvent mEvent;
  TimeStamp mTimeStamp;

  TransitionEventInfo(dom::Element* aElement,
                      CSSPseudoElementType aPseudoType,
                      nsCSSPropertyID aProperty,
                      StickyTimeDuration aDuration,
                      const TimeStamp& aTimeStamp,
                      dom::Animation* aAnimation)
    : mElement(aElement)
    , mAnimation(aAnimation)
    , mEvent(true, eTransitionEnd)
    , mTimeStamp(aTimeStamp)
  {
    // XXX Looks like nobody initialize WidgetEvent::time
    mEvent.mPropertyName =
      NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty));
    mEvent.mElapsedTime = aDuration.ToSeconds();
    mEvent.mPseudoElement =
      AnimationCollection<dom::CSSTransition>::PseudoTypeAsString(aPseudoType);
  }

  // InternalTransitionEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  TransitionEventInfo(const TransitionEventInfo& aOther)
    : mElement(aOther.mElement)
    , mAnimation(aOther.mAnimation)
    , mEvent(true, eTransitionEnd)
    , mTimeStamp(aOther.mTimeStamp)
  {
    mEvent.AssignTransitionEventData(aOther.mEvent, false);
  }
};

} // namespace mozilla

class nsTransitionManager final
  : public mozilla::CommonAnimationManager<mozilla::dom::CSSTransition>
{
public:
  explicit nsTransitionManager(nsPresContext *aPresContext)
    : mozilla::CommonAnimationManager<mozilla::dom::CSSTransition>(aPresContext)
    , mInAnimationOnlyStyleUpdate(false)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsTransitionManager)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsTransitionManager)

  typedef mozilla::AnimationCollection<mozilla::dom::CSSTransition>
    CSSTransitionCollection;

  /**
   * StyleContextChanged
   *
   * To be called from RestyleManager::TryStartingTransition when the
   * style of an element has changed, to initiate transitions from
   * that style change.  For style contexts with :before and :after
   * pseudos, aElement is expected to be the generated before/after
   * element.
   *
   * It may modify the new style context (by replacing
   * *aNewStyleContext) to cover up some of the changes for the duration
   * of the restyling of descendants.  If it does, this function will
   * take care of causing the necessary restyle afterwards.
   */
  void StyleContextChanged(mozilla::dom::Element *aElement,
                           nsStyleContext *aOldStyleContext,
                           RefPtr<nsStyleContext>* aNewStyleContext /* inout */);

  /**
   * When we're resolving style for an element that previously didn't have
   * style, we might have some old finished transitions for it, if,
   * say, it was display:none for a while, but previously displayed.
   *
   * This method removes any finished transitions that don't match the
   * new style.
   */
  void PruneCompletedTransitions(mozilla::dom::Element* aElement,
                                 mozilla::CSSPseudoElementType aPseudoType,
                                 nsStyleContext* aNewStyleContext);

  void SetInAnimationOnlyStyleUpdate(bool aInAnimationOnlyUpdate) {
    mInAnimationOnlyStyleUpdate = aInAnimationOnlyUpdate;
  }

  bool InAnimationOnlyStyleUpdate() const {
    return mInAnimationOnlyStyleUpdate;
  }

  void QueueEvent(mozilla::TransitionEventInfo&& aEventInfo)
  {
    mEventDispatcher.QueueEvent(
      mozilla::Forward<mozilla::TransitionEventInfo>(aEventInfo));
  }

  void DispatchEvents()
  {
    RefPtr<nsTransitionManager> kungFuDeathGrip(this);
    mEventDispatcher.DispatchEvents(mPresContext);
  }
  void SortEvents()      { mEventDispatcher.SortEvents(); }
  void ClearEventQueue() { mEventDispatcher.ClearEventQueue(); }

  // Stop transitions on the element. This method takes the real element
  // rather than the element for the generated content for transitions on
  // ::before and ::after.
  void StopTransitionsForElement(mozilla::dom::Element* aElement,
                                 mozilla::CSSPseudoElementType aPseudoType);

protected:
  virtual ~nsTransitionManager() {}

  typedef nsTArray<RefPtr<mozilla::dom::CSSTransition>>
    OwningCSSTransitionPtrArray;

  // Update the transitions. It'd start new, replace, or stop current
  // transitions if need. aDisp and aElement shouldn't be nullptr.
  // aElementTransitions is the collection of current transitions, and it
  // could be a nullptr if we don't have any transitions.
  bool
  UpdateTransitions(const nsStyleDisplay* aDisp,
                    mozilla::dom::Element* aElement,
                    CSSTransitionCollection*& aElementTransitions,
                    nsStyleContext* aOldStyleContext,
                    nsStyleContext* aNewStyleContext);

  void
  ConsiderStartingTransition(nsCSSPropertyID aProperty,
                             const mozilla::StyleTransition& aTransition,
                             mozilla::dom::Element* aElement,
                             CSSTransitionCollection*& aElementTransitions,
                             nsStyleContext* aOldStyleContext,
                             nsStyleContext* aNewStyleContext,
                             bool* aStartedAny,
                             nsCSSPropertyIDSet* aWhichStarted);

  nsTArray<mozilla::Keyframe> GetTransitionKeyframes(
    nsStyleContext* aStyleContext,
    nsCSSPropertyID aProperty,
    mozilla::StyleAnimationValue&& aStartValue,
    mozilla::StyleAnimationValue&& aEndValue,
    const nsTimingFunction& aTimingFunction);

  bool mInAnimationOnlyStyleUpdate;

  mozilla::DelayedEventDispatcher<mozilla::TransitionEventInfo>
      mEventDispatcher;
};

#endif /* !defined(nsTransitionManager_h_) */

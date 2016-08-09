/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectCompositor_h
#define mozilla_EffectCompositor_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/PseudoElementHashEntry.h"
#include "mozilla/RefPtr.h"
#include "nsCSSPropertyID.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsIStyleRuleProcessor.h"
#include "nsTArray.h"

class nsCSSPropertySet;
class nsIFrame;
class nsIStyleRule;
class nsPresContext;
class nsStyleContext;

namespace mozilla {

class EffectSet;
class RestyleTracker;
struct AnimationPerformanceWarning;
struct NonOwningAnimationTarget;

namespace dom {
class Animation;
class Element;
}

class EffectCompositor
{
public:
  explicit EffectCompositor(nsPresContext* aPresContext)
    : mPresContext(aPresContext)
  {
    for (size_t i = 0; i < kCascadeLevelCount; i++) {
      CascadeLevel cascadeLevel = CascadeLevel(i);
      mRuleProcessors[cascadeLevel] =
        new AnimationStyleRuleProcessor(this, cascadeLevel);
    }
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(EffectCompositor)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(EffectCompositor)

  void Disconnect() {
    mPresContext = nullptr;
  }

  // Animations can be applied at two different levels in the CSS cascade:
  enum class CascadeLevel {
    // The animations sheet (CSS animations, script-generated animations,
    // and CSS transitions that are no longer tied to CSS markup)
    Animations,
    // The transitions sheet (CSS transitions that are tied to CSS markup)
    Transitions
  };
  // We don't define this as part of CascadeLevel as then we'd have to add
  // explicit checks for the Count enum value everywhere CascadeLevel is used.
  static const size_t kCascadeLevelCount =
    static_cast<size_t>(CascadeLevel::Transitions) + 1;

  // NOTE: This can return null after Disconnect().
  nsPresContext* PresContext() const { return mPresContext; }

  enum class RestyleType {
    // Animation style has changed but the compositor is applying the same
    // change so we might be able to defer updating the main thread until it
    // becomes necessary.
    Throttled,
    // Animation style has changed and needs to be updated on the main thread.
    Standard,
    // Animation style has changed and needs to be updated on the main thread
    // as well as forcing animations on layers to be updated.
    // This is needed in cases such as when an animation becomes paused or has
    // its playback rate changed. In such cases, although the computed style
    // and refresh driver time might not change, we still need to ensure the
    // corresponding animations on layers are updated to reflect the new
    // configuration of the animation.
    Layer
  };

  // Notifies the compositor that the animation rule for the specified
  // (pseudo-)element at the specified cascade level needs to be updated.
  // The specified steps taken to update the animation rule depend on
  // |aRestyleType| whose values are described above.
  void RequestRestyle(dom::Element* aElement,
                      CSSPseudoElementType aPseudoType,
                      RestyleType aRestyleType,
                      CascadeLevel aCascadeLevel);

  // Schedule an animation restyle. This is called automatically by
  // RequestRestyle when necessary. However, it is exposed here since we also
  // need to perform this step when triggering transitions *without* also
  // invalidating the animation style rule (which RequestRestyle would do).
  void PostRestyleForAnimation(dom::Element* aElement,
                               CSSPseudoElementType aPseudoType,
                               CascadeLevel aCascadeLevel);

  // Posts an animation restyle for any elements whose animation style rule
  // is out of date but for which an animation restyle has not yet been
  // posted because updates on the main thread are throttled.
  void PostRestyleForThrottledAnimations();

  // Called when the style context on the specified (pseudo-) element might
  // have changed so that any context-sensitive values stored within
  // animation effects (e.g. em-based endpoints used in keyframe effects)
  // can be re-resolved to computed values.
  void UpdateEffectProperties(nsStyleContext* aStyleContext,
                              dom::Element* aElement,
                              CSSPseudoElementType aPseudoType);

  // Updates the animation rule stored on the EffectSet for the
  // specified (pseudo-)element for cascade level |aLevel|.
  // If the animation rule is not marked as needing an update,
  // no work is done.
  // |aStyleContext| is used for UpdateCascadingResults.
  // |aStyleContext| can be nullptr if style context, which is associated with
  // the primary frame of the specified (pseudo-)element, is the current style
  // context.
  // If we are resolving a new style context, we shoud pass the newly created
  // style context, otherwise we may use an old style context, it will result
  // unexpected cascading results.
  void MaybeUpdateAnimationRule(dom::Element* aElement,
                                CSSPseudoElementType aPseudoType,
                                CascadeLevel aCascadeLevel,
                                nsStyleContext *aStyleContext);

  // We need to pass the newly resolved style context as |aStyleContext| when
  // we call this function during resolving style context because this function
  // calls UpdateCascadingResults with a style context if necessary, at the
  // time, we end up using the previous style context if we don't pass the new
  // style context.
  // When we are not resolving style context, |aStyleContext| can be nullptr, we
  // will use a style context associated with the primary frame of the specified
  // (pseudo-)element.
  nsIStyleRule* GetAnimationRule(dom::Element* aElement,
                                 CSSPseudoElementType aPseudoType,
                                 CascadeLevel aCascadeLevel,
                                 nsStyleContext* aStyleContext);

  bool HasPendingStyleUpdates() const;
  bool HasThrottledStyleUpdates() const;

  // Tell the restyle tracker about all the animated styles that have
  // pending updates so that it can update the animation rule for these
  // elements.
  void AddStyleUpdatesTo(RestyleTracker& aTracker);

  nsIStyleRuleProcessor* RuleProcessor(CascadeLevel aCascadeLevel) const
  {
    return mRuleProcessors[aCascadeLevel];
  }

  static bool HasAnimationsForCompositor(const nsIFrame* aFrame,
                                         nsCSSPropertyID aProperty);

  static nsTArray<RefPtr<dom::Animation>>
  GetAnimationsForCompositor(const nsIFrame* aFrame,
                             nsCSSPropertyID aProperty);

  static void ClearIsRunningOnCompositor(const nsIFrame* aFrame,
                                         nsCSSPropertyID aProperty);

  // Update animation cascade results for the specified (pseudo-)element
  // but only if we have marked the cascade as needing an update due a
  // the change in the set of effects or a change in one of the effects'
  // "in effect" state.
  // |aStyleContext| may be nullptr in which case we will use the
  // nsStyleContext of the primary frame of the specified (pseudo-)element.
  //
  // This method does NOT detect if other styles that apply above the
  // animation level of the cascade have changed.
  static void
  MaybeUpdateCascadeResults(dom::Element* aElement,
                            CSSPseudoElementType aPseudoType,
                            nsStyleContext* aStyleContext);

  // Update the mWinsInCascade member for each property in effects targetting
  // the specified (pseudo-)element.
  //
  // This can be expensive so we should only call it if styles that apply
  // above the animation level of the cascade might have changed. For all
  // other cases we should call MaybeUpdateCascadeResults.
  static void
  UpdateCascadeResults(dom::Element* aElement,
                       CSSPseudoElementType aPseudoType,
                       nsStyleContext* aStyleContext);

  // Helper to fetch the corresponding element and pseudo-type from a frame.
  //
  // For frames corresponding to pseudo-elements, the returned element is the
  // element on which we store the animations (i.e. the EffectSet and/or
  // AnimationCollection), *not* the generated content.
  //
  // Returns an empty result when a suitable element cannot be found including
  // when the frame represents a pseudo-element on which we do not support
  // animations.
  static Maybe<NonOwningAnimationTarget>
  GetAnimationElementAndPseudoForFrame(const nsIFrame* aFrame);

  // Associates a performance warning with effects on |aFrame| that animates
  // |aProperty|.
  static void SetPerformanceWarning(
    const nsIFrame* aFrame,
    nsCSSPropertyID aProperty,
    const AnimationPerformanceWarning& aWarning);

private:
  ~EffectCompositor() = default;

  // Rebuilds the animation rule corresponding to |aCascadeLevel| on the
  // EffectSet associated with the specified (pseudo-)element.
  static void ComposeAnimationRule(dom::Element* aElement,
                                   CSSPseudoElementType aPseudoType,
                                   CascadeLevel aCascadeLevel,
                                   TimeStamp aRefreshTime);

  static dom::Element* GetElementToRestyle(dom::Element* aElement,
                                           CSSPseudoElementType
                                             aPseudoType);

  // Get the properties in |aEffectSet| that we are able to animate on the
  // compositor but which are also specified at a higher level in the cascade
  // than the animations level in |aStyleContext|.
  static void
  GetOverriddenProperties(nsStyleContext* aStyleContext,
                          EffectSet& aEffectSet,
                          nsCSSPropertySet& aPropertiesOverridden);

  static void
  UpdateCascadeResults(EffectSet& aEffectSet,
                       dom::Element* aElement,
                       CSSPseudoElementType aPseudoType,
                       nsStyleContext* aStyleContext);

  static nsPresContext* GetPresContext(dom::Element* aElement);

  nsPresContext* mPresContext;

  // Elements with a pending animation restyle. The associated bool value is
  // true if a pending animation restyle has also been dispatched. For
  // animations that can be throttled, we will add an entry to the hashtable to
  // indicate that the style rule on the element is out of date but without
  // posting a restyle to update it.
  EnumeratedArray<CascadeLevel, CascadeLevel(kCascadeLevelCount),
                  nsDataHashtable<PseudoElementHashEntry, bool>>
                    mElementsToRestyle;

  class AnimationStyleRuleProcessor final : public nsIStyleRuleProcessor
  {
  public:
    AnimationStyleRuleProcessor(EffectCompositor* aCompositor,
                                CascadeLevel aCascadeLevel)
      : mCompositor(aCompositor)
      , mCascadeLevel(aCascadeLevel)
    {
      MOZ_ASSERT(aCompositor);
    }

    NS_DECL_ISUPPORTS

    // nsIStyleRuleProcessor (parts)
    nsRestyleHint HasStateDependentStyle(
                        StateRuleProcessorData* aData) override;
    nsRestyleHint HasStateDependentStyle(
                        PseudoElementStateRuleProcessorData* aData) override;
    bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) override;
    nsRestyleHint HasAttributeDependentStyle(
                        AttributeRuleProcessorData* aData,
                        RestyleHintData& aRestyleHintDataResult) override;
    bool MediumFeaturesChanged(nsPresContext* aPresContext) override;
    void RulesMatching(ElementRuleProcessorData* aData) override;
    void RulesMatching(PseudoElementRuleProcessorData* aData) override;
    void RulesMatching(AnonBoxRuleProcessorData* aData) override;
#ifdef MOZ_XUL
    void RulesMatching(XULTreeRuleProcessorData* aData) override;
#endif
    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf)
      const MOZ_MUST_OVERRIDE override;
    size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
      const MOZ_MUST_OVERRIDE override;

  private:
    ~AnimationStyleRuleProcessor() = default;

    EffectCompositor* mCompositor;
    CascadeLevel      mCascadeLevel;
  };

  EnumeratedArray<CascadeLevel, CascadeLevel(kCascadeLevelCount),
                  OwningNonNull<AnimationStyleRuleProcessor>>
                    mRuleProcessors;
};

} // namespace mozilla

#endif // mozilla_EffectCompositor_h

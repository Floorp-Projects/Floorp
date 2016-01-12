/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectCompositor_h
#define mozilla_EffectCompositor_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/Pair.h"
#include "mozilla/PseudoElementHashEntry.h"
#include "mozilla/RefPtr.h"
#include "nsCSSProperty.h"
#include "nsCSSPseudoElements.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

class nsCSSPropertySet;
class nsIFrame;
class nsIStyleRule;
class nsPresContext;
class nsStyleContext;

namespace mozilla {

class EffectSet;
class RestyleTracker;

namespace dom {
class Animation;
class Element;
}

class EffectCompositor
{
public:
  explicit EffectCompositor(nsPresContext* aPresContext)
    : mPresContext(aPresContext)
  { }

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
                      nsCSSPseudoElements::Type aPseudoType,
                      RestyleType aRestyleType,
                      CascadeLevel aCascadeLevel);

  // Schedule an animation restyle. This is called automatically by
  // RequestRestyle when necessary. However, it is exposed here since we also
  // need to perform this step when triggering transitions *without* also
  // invalidating the animation style rule (which RequestRestyle would do).
  void PostRestyleForAnimation(dom::Element* aElement,
                               nsCSSPseudoElements::Type aPseudoType,
                               CascadeLevel aCascadeLevel);

  // Posts an animation restyle for any elements whose animation style rule
  // is out of date but for which an animation restyle has not yet been
  // posted because updates on the main thread are throttled.
  void PostRestyleForThrottledAnimations();

  // Updates the animation rule stored on the EffectSet for the
  // specified (pseudo-)element for cascade level |aLevel|.
  // If the animation rule is not marked as needing an update,
  // no work is done.
  void MaybeUpdateAnimationRule(dom::Element* aElement,
                                nsCSSPseudoElements::Type aPseudoType,
                                CascadeLevel aCascadeLevel);

  nsIStyleRule* GetAnimationRule(dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType,
                                 CascadeLevel aCascadeLevel);

  bool HasPendingStyleUpdates() const;
  bool HasThrottledStyleUpdates() const;

  // Tell the restyle tracker about all the animated styles that have
  // pending updates so that it can update the animation rule for these
  // elements.
  void AddStyleUpdatesTo(RestyleTracker& aTracker);

  static bool HasAnimationsForCompositor(const nsIFrame* aFrame,
                                         nsCSSProperty aProperty);

  static nsTArray<RefPtr<dom::Animation>>
  GetAnimationsForCompositor(const nsIFrame* aFrame,
                             nsCSSProperty aProperty);


  // Update animation cascade results for the specified (pseudo-)element
  // but only if we have marked the cascade as needing an update due a
  // the change in the set of effects or a change in one of the effects'
  // "in effect" state.
  //
  // This method does NOT detect if other styles that apply above the
  // animation level of the cascade have changed.
  static void
  MaybeUpdateCascadeResults(dom::Element* aElement,
                            nsCSSPseudoElements::Type aPseudoType,
                            nsStyleContext* aStyleContext);

  // An overload of MaybeUpdateCascadeResults that uses the style context
  // of the primary frame of the specified (pseudo-)element, when available.
  static void
  MaybeUpdateCascadeResults(dom::Element* aElement,
                            nsCSSPseudoElements::Type aPseudoType);

  // Update the mWinsInCascade member for each property in effects targetting
  // the specified (pseudo-)element.
  //
  // This can be expensive so we should only call it if styles that apply
  // above the animation level of the cascade might have changed. For all
  // other cases we should call MaybeUpdateCascadeResults.
  static void
  UpdateCascadeResults(dom::Element* aElement,
                       nsCSSPseudoElements::Type aPseudoType,
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
  static Maybe<Pair<dom::Element*, nsCSSPseudoElements::Type>>
  GetAnimationElementAndPseudoForFrame(const nsIFrame* aFrame);

private:
  ~EffectCompositor() = default;

  // Rebuilds the animation rule corresponding to |aCascadeLevel| on the
  // EffectSet associated with the specified (pseudo-)element.
  static void ComposeAnimationRule(dom::Element* aElement,
                                   nsCSSPseudoElements::Type aPseudoType,
                                   CascadeLevel aCascadeLevel,
                                   TimeStamp aRefreshTime);

  static dom::Element* GetElementToRestyle(dom::Element* aElement,
                                           nsCSSPseudoElements::Type
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
                       nsCSSPseudoElements::Type aPseudoType,
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
};

} // namespace mozilla

#endif // mozilla_EffectCompositor_h

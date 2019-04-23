/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectCompositor_h
#define mozilla_EffectCompositor_h

#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/PseudoElementHashEntry.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoTypes.h"
#include "nsCSSPropertyID.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

class nsCSSPropertyIDSet;
class nsAtom;
class nsIFrame;
class nsPresContext;
enum class DisplayItemType : uint8_t;
struct RawServoAnimationValueMap;

namespace mozilla {

class ComputedStyle;
class EffectSet;
class RestyleTracker;
class StyleAnimationValue;
struct AnimationProperty;
struct NonOwningAnimationTarget;

namespace dom {
class Animation;
class Element;
}  // namespace dom

class EffectCompositor {
 public:
  explicit EffectCompositor(nsPresContext* aPresContext)
      : mPresContext(aPresContext) {}

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(EffectCompositor)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(EffectCompositor)

  void Disconnect() { mPresContext = nullptr; }

  // Animations can be applied at two different levels in the CSS cascade:
  enum class CascadeLevel : uint32_t {
    // The animations sheet (CSS animations, script-generated animations,
    // and CSS transitions that are no longer tied to CSS markup)
    Animations = 0,
    // The transitions sheet (CSS transitions that are tied to CSS markup)
    Transitions = 1
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
  void RequestRestyle(dom::Element* aElement, PseudoStyleType aPseudoType,
                      RestyleType aRestyleType, CascadeLevel aCascadeLevel);

  // Schedule an animation restyle. This is called automatically by
  // RequestRestyle when necessary. However, it is exposed here since we also
  // need to perform this step when triggering transitions *without* also
  // invalidating the animation style rule (which RequestRestyle would do).
  void PostRestyleForAnimation(dom::Element* aElement,
                               PseudoStyleType aPseudoType,
                               CascadeLevel aCascadeLevel);

  // Posts an animation restyle for any elements whose animation style rule
  // is out of date but for which an animation restyle has not yet been
  // posted because updates on the main thread are throttled.
  void PostRestyleForThrottledAnimations();

  // Clear all pending restyle requests for the given (pseudo-) element (and its
  // ::before, ::after and ::marker elements if the given element is not
  // pseudo).
  void ClearRestyleRequestsFor(dom::Element* aElement);

  // Called when computed style on the specified (pseudo-) element might
  // have changed so that any context-sensitive values stored within
  // animation effects (e.g. em-based endpoints used in keyframe effects)
  // can be re-resolved to computed values.
  void UpdateEffectProperties(const ComputedStyle* aStyle,
                              dom::Element* aElement,
                              PseudoStyleType aPseudoType);

  // Get animation rule for stylo. This is an equivalent of GetAnimationRule
  // and will be called from servo side.
  // The animation rule is stored in |RawServoAnimationValueMap|.
  // We need to be careful while doing any modification because it may cause
  // some thread-safe issues.
  bool GetServoAnimationRule(const dom::Element* aElement,
                             PseudoStyleType aPseudoType,
                             CascadeLevel aCascadeLevel,
                             RawServoAnimationValueMap* aAnimationValues);

  bool HasPendingStyleUpdates() const;

  static bool HasAnimationsForCompositor(const nsIFrame* aFrame,
                                         DisplayItemType aType);

  static nsTArray<RefPtr<dom::Animation>> GetAnimationsForCompositor(
      const nsIFrame* aFrame, const nsCSSPropertyIDSet& aPropertySet);

  static void ClearIsRunningOnCompositor(const nsIFrame* aFrame,
                                         DisplayItemType aType);

  // Update animation cascade results for the specified (pseudo-)element
  // but only if we have marked the cascade as needing an update due a
  // the change in the set of effects or a change in one of the effects'
  // "in effect" state.
  //
  // This method does NOT detect if other styles that apply above the
  // animation level of the cascade have changed.
  static void MaybeUpdateCascadeResults(dom::Element* aElement,
                                        PseudoStyleType aPseudoType);

  // Update the mPropertiesWithImportantRules and
  // mPropertiesForAnimationsLevel members of the given EffectSet, and also
  // request any restyles required by changes to the cascade result.
  //
  // NOTE: This can be expensive so we should only call it if styles that apply
  // above the animation level of the cascade might have changed. For all
  // other cases we should call MaybeUpdateCascadeResults.
  //
  // This is typically reserved for internal callers but is public here since
  // when we detect changes to the cascade on the Servo side we can't call
  // MarkCascadeNeedsUpdate during the traversal so instead we call this as part
  // of a follow-up sequential task.
  static void UpdateCascadeResults(EffectSet& aEffectSet,
                                   dom::Element* aElement,
                                   PseudoStyleType aPseudoType);

  // Helper to fetch the corresponding element and pseudo-type from a frame.
  //
  // For frames corresponding to pseudo-elements, the returned element is the
  // element on which we store the animations (i.e. the EffectSet and/or
  // AnimationCollection), *not* the generated content.
  //
  // For display:table content, which maintains a distinction between primary
  // frame (table wrapper frame) and style frame (inner table frame), animations
  // are stored on the content associated with the _style_ frame even though
  // some (particularly transform-like animations) may be applied to the
  // _primary_ frame. As a result, callers will typically want to pass the style
  // frame to this function.
  //
  // Returns an empty result when a suitable element cannot be found including
  // when the frame represents a pseudo-element on which we do not support
  // animations.
  static Maybe<NonOwningAnimationTarget> GetAnimationElementAndPseudoForFrame(
      const nsIFrame* aFrame);

  // Associates a performance warning with effects on |aFrame| that animate
  // properties in |aPropertySet|.
  static void SetPerformanceWarning(
      const nsIFrame* aFrame, const nsCSSPropertyIDSet& aPropertySet,
      const AnimationPerformanceWarning& aWarning);

  // Do a bunch of stuff that we should avoid doing during the parallel
  // traversal (e.g. changing member variables) for all elements that we expect
  // to restyle on the next traversal.
  //
  // Returns true if there are elements needing a restyle for animation.
  bool PreTraverse(ServoTraversalFlags aFlags);

  // Similar to the above but for all elements in the subtree rooted
  // at aElement.
  bool PreTraverseInSubtree(ServoTraversalFlags aFlags, dom::Element* aRoot);

  // Returns the target element for restyling.
  //
  // If |aPseudoType| is ::after, ::before or ::marker, returns the generated
  // content element of which |aElement| is the parent. If |aPseudoType| is any
  // other pseudo type (other than PseudoStyleType::NotPseudo) returns nullptr.
  // Otherwise, returns |aElement|.
  static dom::Element* GetElementToRestyle(dom::Element* aElement,
                                           PseudoStyleType aPseudoType);

  // Returns true if any type of compositor animations on |aFrame| allow
  // runnning on the compositor.
  // Sets the reason in |aWarning| if the result is false.
  static bool AllowCompositorAnimationsOnFrame(
      const nsIFrame* aFrame,
      AnimationPerformanceWarning::Type& aWarning /* out */);

 private:
  ~EffectCompositor() = default;

  // Get the properties in |aEffectSet| that we are able to animate on the
  // compositor but which are also specified at a higher level in the cascade
  // than the animations level.
  static nsCSSPropertyIDSet GetOverriddenProperties(
      EffectSet& aEffectSet, dom::Element* aElement,
      PseudoStyleType aPseudoType);

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

  bool mIsInPreTraverse = false;
};

}  // namespace mozilla

#endif  // mozilla_EffectCompositor_h

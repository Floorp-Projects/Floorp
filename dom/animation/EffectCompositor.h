/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectCompositor_h
#define mozilla_EffectCompositor_h

#include "mozilla/Maybe.h"
#include "mozilla/Pair.h"
#include "mozilla/RefPtr.h"
#include "nsCSSProperty.h"
#include "nsCSSPseudoElements.h"
#include "nsTArray.h"

class nsCSSPropertySet;
class nsIFrame;
class nsPresContext;
class nsStyleContext;

namespace mozilla {

class EffectSet;

namespace dom {
class Animation;
class Element;
}

class EffectCompositor
{
public:
  // Animations can be applied at two different levels in the CSS cascade:
  enum class CascadeLevel {
    // The animations sheet (CSS animations, script-generated animations,
    // and CSS transitions that are no longer tied to CSS markup)
    Animations,
    // The transitions sheet (CSS transitions that are tied to CSS markup)
    Transitions
  };

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
};

} // namespace mozilla

#endif // mozilla_EffectCompositor_h

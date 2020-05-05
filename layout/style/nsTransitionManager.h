/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#ifndef nsTransitionManager_h_
#define nsTransitionManager_h_

#include "mozilla/dom/CSSTransition.h"
#include "AnimationCommon.h"
#include "nsISupportsImpl.h"

class nsPresContext;
class nsCSSPropertyIDSet;

namespace mozilla {
class ComputedStyle;
enum class PseudoStyleType : uint8_t;
}  // namespace mozilla

class nsTransitionManager final
    : public mozilla::CommonAnimationManager<mozilla::dom::CSSTransition> {
 public:
  explicit nsTransitionManager(nsPresContext* aPresContext)
      : mozilla::CommonAnimationManager<mozilla::dom::CSSTransition>(
            aPresContext) {}

  ~nsTransitionManager() final = default;

  typedef mozilla::AnimationCollection<mozilla::dom::CSSTransition>
      CSSTransitionCollection;

  /**
   * Update transitions for stylo.
   */
  bool UpdateTransitions(mozilla::dom::Element* aElement,
                         mozilla::PseudoStyleType aPseudoType,
                         const mozilla::ComputedStyle& aOldStyle,
                         const mozilla::ComputedStyle& aNewStyle);

 protected:
  typedef nsTArray<RefPtr<mozilla::dom::CSSTransition>>
      OwningCSSTransitionPtrArray;

  // Update transitions. This will start new transitions,
  // replace existing transitions, and stop existing transitions
  // as needed. aDisp and aElement must be non-null.
  // aElementTransitions is the collection of current transitions, and it
  // could be a nullptr if we don't have any transitions.
  bool DoUpdateTransitions(const nsStyleDisplay& aDisp,
                           mozilla::dom::Element* aElement,
                           mozilla::PseudoStyleType aPseudoType,
                           CSSTransitionCollection*& aElementTransitions,
                           const mozilla::ComputedStyle& aOldStyle,
                           const mozilla::ComputedStyle& aNewStyle);

  // Returns whether the transition actually started.
  bool ConsiderInitiatingTransition(
      nsCSSPropertyID aProperty, const nsStyleDisplay& aStyleDisplay,
      uint32_t transitionIdx, mozilla::dom::Element* aElement,
      mozilla::PseudoStyleType aPseudoType,
      CSSTransitionCollection*& aElementTransitions,
      const mozilla::ComputedStyle& aOldStyle,
      const mozilla::ComputedStyle& aNewStyle,
      nsCSSPropertyIDSet& aPropertiesChecked);
};

#endif /* !defined(nsTransitionManager_h_) */

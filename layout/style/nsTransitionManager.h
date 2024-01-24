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
struct nsStyleUIReset;

namespace mozilla {
class AnimatedPropertyIDSet;
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
  bool DoUpdateTransitions(const nsStyleUIReset& aStyle,
                           mozilla::dom::Element* aElement,
                           mozilla::PseudoStyleType aPseudoType,
                           CSSTransitionCollection*& aElementTransitions,
                           const mozilla::ComputedStyle& aOldStyle,
                           const mozilla::ComputedStyle& aNewStyle);

  // Returns whether the transition actually started.
  bool ConsiderInitiatingTransition(
      const mozilla::AnimatedPropertyID&, const nsStyleUIReset& aStyle,
      uint32_t aTransitionIndex, float aDelay, float aDuration,
      mozilla::dom::Element* aElement, mozilla::PseudoStyleType aPseudoType,
      CSSTransitionCollection*& aElementTransitions,
      const mozilla::ComputedStyle& aOldStyle,
      const mozilla::ComputedStyle& aNewStyle,
      mozilla::AnimatedPropertyIDSet& aPropertiesChecked);

  already_AddRefed<mozilla::dom::CSSTransition> DoCreateTransition(
      const mozilla::AnimatedPropertyID& aProperty,
      mozilla::dom::Element* aElement, mozilla::PseudoStyleType aPseudoType,
      const mozilla::ComputedStyle& aNewStyle,
      CSSTransitionCollection*& aElementTransitions,
      mozilla::TimingParams&& aTiming, mozilla::AnimationValue&& aStartValue,
      mozilla::AnimationValue&& aEndValue,
      mozilla::AnimationValue&& aStartForReversingTest, double aReversePortion);

  void DoCancelTransition(mozilla::dom::Element* aElement,
                          mozilla::PseudoStyleType aPseudoType,
                          CSSTransitionCollection*& aElementTransitions,
                          size_t aIndex);
};

#endif /* !defined(nsTransitionManager_h_) */

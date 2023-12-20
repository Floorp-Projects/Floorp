/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_AnimationCommon_h
#define mozilla_css_AnimationCommon_h

#include "mozilla/AnimationCollection.h"
#include "mozilla/LinkedList.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/BaseKeyframeTypesBinding.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimingParams.h"
#include "mozilla/dom/Nullable.h"
#include "nsContentUtils.h"
#include "nsDOMMutationObserver.h"  // For nsAutoAnimationMutationBatch

class nsPresContext;

namespace mozilla {
enum class PseudoStyleType : uint8_t;

namespace dom {
class Element;
}

template <class AnimationType>
class CommonAnimationManager {
 public:
  explicit CommonAnimationManager(nsPresContext* aPresContext)
      : mPresContext(aPresContext) {}

  // NOTE:  This can return null after Disconnect().
  nsPresContext* PresContext() const { return mPresContext; }

  /**
   * Notify the manager that the pres context is going away.
   */
  void Disconnect() {
    // Content nodes might outlive the transition or animation manager.
    RemoveAllElementCollections();

    mPresContext = nullptr;
  }

  /**
   * Stop animations on the element. This method takes the real element
   * rather than the element for the generated content for animations on
   * ::before, ::after and ::marker.
   */
  void StopAnimationsForElement(dom::Element* aElement,
                                PseudoStyleType aPseudoType) {
    MOZ_ASSERT(aElement);
    auto* collection =
        AnimationCollection<AnimationType>::Get(aElement, aPseudoType);
    if (!collection) {
      return;
    }

    nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());
    collection->Destroy();
  }

 protected:
  virtual ~CommonAnimationManager() {
    MOZ_ASSERT(!mPresContext, "Disconnect should have been called");
  }

  void AddElementCollection(AnimationCollection<AnimationType>* aCollection) {
    mElementCollections.insertBack(aCollection);
  }
  void RemoveAllElementCollections() {
    while (AnimationCollection<AnimationType>* head =
               mElementCollections.getFirst()) {
      head->Destroy();  // Note: this removes 'head' from mElementCollections.
    }
  }

  LinkedList<AnimationCollection<AnimationType>> mElementCollections;
  nsPresContext* mPresContext;  // weak (non-null from ctor to Disconnect)
};

/**
 * Utility class for referencing the element that created a CSS animation or
 * transition. It is non-owning (i.e. it uses a raw pointer) since it is only
 * expected to be set by the owned animation while it actually being managed
 * by the owning element.
 *
 * This class also abstracts the comparison of an element/pseudo-class pair
 * for the sake of composite ordering since this logic is common to both CSS
 * animations and transitions.
 *
 * (We call this OwningElementRef instead of just OwningElement so that we can
 * call the getter on CSSAnimation/CSSTransition OwningElement() without
 * clashing with this object's contructor.)
 */
class OwningElementRef final {
 public:
  OwningElementRef() = default;

  explicit OwningElementRef(const NonOwningAnimationTarget& aTarget)
      : mTarget(aTarget) {}

  OwningElementRef(dom::Element& aElement, PseudoStyleType aPseudoType)
      : mTarget(&aElement, aPseudoType) {}

  bool Equals(const OwningElementRef& aOther) const {
    return mTarget == aOther.mTarget;
  }

  bool LessThan(Maybe<uint32_t>& aChildIndex, const OwningElementRef& aOther,
                Maybe<uint32_t>& aOtherChildIndex) const {
    MOZ_ASSERT(mTarget.mElement && aOther.mTarget.mElement,
               "Elements to compare should not be null");

    if (mTarget.mElement != aOther.mTarget.mElement) {
      return nsContentUtils::PositionIsBefore(mTarget.mElement,
                                              aOther.mTarget.mElement,
                                              &aChildIndex, &aOtherChildIndex);
    }

    return mTarget.mPseudoType == PseudoStyleType::NotPseudo ||
           (mTarget.mPseudoType == PseudoStyleType::before &&
            aOther.mTarget.mPseudoType == PseudoStyleType::after) ||
           (mTarget.mPseudoType == PseudoStyleType::marker &&
            aOther.mTarget.mPseudoType == PseudoStyleType::before) ||
           (mTarget.mPseudoType == PseudoStyleType::marker &&
            aOther.mTarget.mPseudoType == PseudoStyleType::after);
  }

  bool IsSet() const { return !!mTarget.mElement; }

  void GetElement(dom::Element*& aElement, PseudoStyleType& aPseudoType) const {
    aElement = mTarget.mElement;
    aPseudoType = mTarget.mPseudoType;
  }

  const NonOwningAnimationTarget& Target() const { return mTarget; }

  nsPresContext* GetPresContext() const {
    return nsContentUtils::GetContextForContent(mTarget.mElement);
  }

 private:
  NonOwningAnimationTarget mTarget;
};

// Return the TransitionPhase or AnimationPhase to use when the animation
// doesn't have a target effect.
template <typename PhaseType>
PhaseType GetAnimationPhaseWithoutEffect(const dom::Animation& aAnimation) {
  MOZ_ASSERT(!aAnimation.GetEffect(),
             "Should only be called when we do not have an effect");

  dom::Nullable<TimeDuration> currentTime =
      aAnimation.GetCurrentTimeAsDuration();
  if (currentTime.IsNull()) {
    return PhaseType::Idle;
  }

  // If we don't have a target effect, the duration will be zero so the phase is
  // 'before' if the current time is less than zero.
  return currentTime.Value() < TimeDuration() ? PhaseType::Before
                                              : PhaseType::After;
};

inline dom::PlaybackDirection StyleToDom(StyleAnimationDirection aDirection) {
  switch (aDirection) {
    case StyleAnimationDirection::Normal:
      return dom::PlaybackDirection::Normal;
    case StyleAnimationDirection::Reverse:
      return dom::PlaybackDirection::Reverse;
    case StyleAnimationDirection::Alternate:
      return dom::PlaybackDirection::Alternate;
    case StyleAnimationDirection::AlternateReverse:
      return dom::PlaybackDirection::Alternate_reverse;
  }
  MOZ_ASSERT_UNREACHABLE("Wrong style value?");
  return dom::PlaybackDirection::Normal;
}

inline dom::FillMode StyleToDom(StyleAnimationFillMode aFillMode) {
  switch (aFillMode) {
    case StyleAnimationFillMode::None:
      return dom::FillMode::None;
    case StyleAnimationFillMode::Both:
      return dom::FillMode::Both;
    case StyleAnimationFillMode::Forwards:
      return dom::FillMode::Forwards;
    case StyleAnimationFillMode::Backwards:
      return dom::FillMode::Backwards;
  }
  MOZ_ASSERT_UNREACHABLE("Wrong style value?");
  return dom::FillMode::None;
}

inline dom::CompositeOperation StyleToDom(StyleAnimationComposition aStyle) {
  switch (aStyle) {
    case StyleAnimationComposition::Replace:
      return dom::CompositeOperation::Replace;
    case StyleAnimationComposition::Add:
      return dom::CompositeOperation::Add;
    case StyleAnimationComposition::Accumulate:
      return dom::CompositeOperation::Accumulate;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid style composite operation?");
  return dom::CompositeOperation::Replace;
}

inline TimingParams TimingParamsFromCSSParams(
    float aDuration, float aDelay, float aIterationCount,
    StyleAnimationDirection aDirection, StyleAnimationFillMode aFillMode) {
  MOZ_ASSERT(aIterationCount >= 0.0 && !std::isnan(aIterationCount),
             "aIterations should be nonnegative & finite, as ensured by "
             "CSSParser");
  return TimingParams{aDuration, aDelay, aIterationCount,
                      StyleToDom(aDirection), StyleToDom(aFillMode)};
}

}  // namespace mozilla

#endif /* !defined(mozilla_css_AnimationCommon_h) */

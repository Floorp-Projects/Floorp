/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsAnimationManager_h_
#define nsAnimationManager_h_

#include "mozilla/Attributes.h"
#include "AnimationCommon.h"
#include "mozilla/dom/CSSAnimation.h"
#include "mozilla/Keyframe.h"
#include "mozilla/MemoryReporting.h"
#include "nsISupportsImpl.h"
#include "nsTHashSet.h"

class ServoCSSAnimationBuilder;

struct nsStyleUIReset;

namespace mozilla {
class ComputedStyle;

enum class PseudoStyleType : uint8_t;
struct NonOwningAnimationTarget;

} /* namespace mozilla */

class nsAnimationManager final
    : public mozilla::CommonAnimationManager<mozilla::dom::CSSAnimation> {
 public:
  explicit nsAnimationManager(nsPresContext* aPresContext)
      : mozilla::CommonAnimationManager<mozilla::dom::CSSAnimation>(
            aPresContext) {}

  typedef mozilla::AnimationCollection<mozilla::dom::CSSAnimation>
      CSSAnimationCollection;
  typedef nsTArray<RefPtr<mozilla::dom::CSSAnimation>>
      OwningCSSAnimationPtrArray;

  ~nsAnimationManager() override = default;

  /**
   * This function does the same thing as the above UpdateAnimations()
   * but with servo's computed values.
   */
  void UpdateAnimations(mozilla::dom::Element* aElement,
                        mozilla::PseudoStyleType aPseudoType,
                        const mozilla::ComputedStyle* aComputedValues);

  // Utility function to walk through |aIter| to find the Keyframe with
  // matching offset and timing function but stopping as soon as the offset
  // differs from |aOffset| (i.e. it assumes a sorted iterator).
  //
  // If a matching Keyframe is found,
  //   Returns true and sets |aIndex| to the index of the matching Keyframe
  //   within |aIter|.
  //
  // If no matching Keyframe is found,
  //   Returns false and sets |aIndex| to the index in the iterator of the
  //   first Keyframe with an offset differing to |aOffset| or, if the end
  //   of the iterator is reached, sets |aIndex| to the index after the last
  //   Keyframe.
  template <class IterType>
  static bool FindMatchingKeyframe(
      IterType&& aIter, double aOffset,
      const mozilla::StyleComputedTimingFunction& aTimingFunctionToMatch,
      mozilla::dom::CompositeOperationOrAuto aCompositionToMatch,
      size_t& aIndex) {
    aIndex = 0;
    for (mozilla::Keyframe& keyframe : aIter) {
      if (keyframe.mOffset.value() != aOffset) {
        break;
      }
      const bool matches = [&] {
        if (keyframe.mComposite != aCompositionToMatch) {
          return false;
        }
        return keyframe.mTimingFunction
                   ? *keyframe.mTimingFunction == aTimingFunctionToMatch
                   : aTimingFunctionToMatch.IsLinearKeyword();
      }();
      if (matches) {
        return true;
      }
      ++aIndex;
    }
    return false;
  }

  bool AnimationMayBeReferenced(nsAtom* aName) const {
    return mMaybeReferencedAnimations.Contains(aName);
  }

 private:
  // This includes all animation names referenced regardless of whether a
  // corresponding `@keyframes` rule is available.
  //
  // It may contain names which are no longer referenced, but it should always
  // contain names which are currently referenced, so that it is usable for
  // style invalidation.
  nsTHashSet<RefPtr<nsAtom>> mMaybeReferencedAnimations;

  void DoUpdateAnimations(const mozilla::NonOwningAnimationTarget& aTarget,
                          const nsStyleUIReset& aStyle,
                          ServoCSSAnimationBuilder& aBuilder);
};

#endif /* !defined(nsAnimationManager_h_) */

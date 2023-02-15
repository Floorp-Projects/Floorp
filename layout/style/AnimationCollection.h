/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationCollection_h
#define mozilla_AnimationCollection_h

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "nsCSSPseudoElements.h"
#include "nsTArrayForwardDeclare.h"

class nsAtom;
class nsIFrame;
class nsPresContext;

namespace mozilla {
namespace dom {
class Element;
}
enum class PseudoStyleType : uint8_t;

template <class AnimationType>
class AnimationCollection
    : public LinkedListElement<AnimationCollection<AnimationType>> {
  typedef AnimationCollection<AnimationType> SelfType;

 public:
  AnimationCollection(dom::Element& aOwner, PseudoStyleType aPseudoType)
      : mElement(aOwner), mPseudo(aPseudoType) {
    MOZ_COUNT_CTOR(AnimationCollection);
  }

  ~AnimationCollection();

  void Destroy();

  // Given the frame |aFrame| with possibly animated content, finds its
  // associated collection of animations. If |aFrame| is a generated content
  // frame, this function may examine the parent frame to search for such
  // animations.
  static AnimationCollection* Get(const nsIFrame* aFrame);
  static AnimationCollection* Get(const dom::Element* aElement,
                                  PseudoStyleType aPseudoType);

  // The element. Weak reference is fine since it owns us.
  // FIXME(emilio): These are only needed for Destroy(), so maybe remove and
  // rely on the caller clearing us properly?
  dom::Element& mElement;
  const PseudoStyleType mPseudo;

  nsTArray<RefPtr<AnimationType>> mAnimations;

 private:
  // We distinguish between destroying this by calling Destroy() vs directly
  // clearing the collection.
  //
  // The former case represents regular updating due to style changes and should
  // trigger subsequent restyles.
  //
  // The latter case represents document tear-down or other DOM surgery in
  // which case we should not trigger restyles.
  bool mCalledDestroy = false;
};

}  // namespace mozilla

#endif  // mozilla_AnimationCollection_h

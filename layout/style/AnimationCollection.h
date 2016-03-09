/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationCollection_h
#define mozilla_AnimationCollection_h

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "nsCSSPseudoElements.h"
#include "nsDOMMutationObserver.h"
#include "nsTArray.h"

class nsIAtom;
class nsPresContext;

namespace mozilla {

// Traits class to define the specific atoms used when storing specializations
// of AnimationCollection as a property on an Element (e.g. which atom
// to use when storing an AnimationCollection<CSSAnimation> for a ::before
// pseudo-element).
template <class AnimationType>
struct AnimationTypeTraits { };

template <class AnimationType>
class AnimationCollection
  : public LinkedListElement<AnimationCollection<AnimationType>>
{
  typedef AnimationCollection<AnimationType> SelfType;
  typedef AnimationTypeTraits<AnimationType> TraitsType;

  AnimationCollection(dom::Element* aElement, nsIAtom* aElementProperty)
    : mElement(aElement)
    , mElementProperty(aElementProperty)
    , mCheckGeneration(0)
#ifdef DEBUG
    , mCalledPropertyDtor(false)
#endif
  {
    MOZ_COUNT_CTOR(AnimationCollection);
  }

public:
  ~AnimationCollection()
  {
    MOZ_ASSERT(mCalledPropertyDtor,
               "must call destructor through element property dtor");
    MOZ_COUNT_DTOR(AnimationCollection);
    LinkedListElement<SelfType>::remove();
  }

  void Destroy()
  {
    // This will call our destructor.
    mElement->DeleteProperty(mElementProperty);
  }

  static void PropertyDtor(void *aObject, nsIAtom *aPropertyName,
                           void *aPropertyValue, void *aData);

  // Get the collection of animations for the given |aElement| and
  // |aPseudoType|.
  static AnimationCollection<AnimationType>*
    GetAnimationCollection(dom::Element* aElement,
                           CSSPseudoElementType aPseudoType);

  // Given the frame |aFrame| with possibly animated content, finds its
  // associated collection of animations. If |aFrame| is a generated content
  // frame, this function may examine the parent frame to search for such
  // animations.
  static AnimationCollection<AnimationType>* GetAnimationCollection(
    const nsIFrame* aFrame);

  // Get the collection of animations for the given |aElement| and
  // |aPseudoType| or create it if it does not already exist.
  //
  // We'll set the outparam |aCreatedCollection| to true if we have
  // to create the collection and we successfully do so. Otherwise,
  // we'll set it to false.
  static AnimationCollection<AnimationType>*
    GetOrCreateAnimationCollection(dom::Element* aElement,
                                   CSSPseudoElementType aPseudoType,
                                   bool* aCreatedCollection);

  bool IsForElement() const { // rather than for a pseudo-element
    return mElementProperty == TraitsType::ElementPropertyAtom();
  }

  bool IsForBeforePseudo() const {
    return mElementProperty == TraitsType::BeforePropertyAtom();
  }

  bool IsForAfterPseudo() const {
    return mElementProperty == TraitsType::AfterPropertyAtom();
  }

  CSSPseudoElementType PseudoElementType() const
  {
    if (IsForElement()) {
      return CSSPseudoElementType::NotPseudo;
    }
    if (IsForBeforePseudo()) {
      return CSSPseudoElementType::before;
    }
    MOZ_ASSERT(IsForAfterPseudo(),
               "::before & ::after should be the only pseudo-elements here");
    return CSSPseudoElementType::after;
  }

  static nsString PseudoTypeAsString(CSSPseudoElementType aPseudoType);

  dom::Element *mElement;

  // the atom we use in mElement's prop table (must be a static atom,
  // i.e., in an atom list)
  nsIAtom *mElementProperty;

  InfallibleTArray<RefPtr<AnimationType>> mAnimations;

  // For CSS transitions only, we record the most recent generation
  // for which we've done the transition update, so that we avoid doing
  // it more than once per style change.
  // (Note that we also store an animation generation on each EffectSet in
  // order to track when we need to update animations on layers.)
  uint64_t mCheckGeneration;

  // Update mCheckGeneration to RestyleManager's count
  void UpdateCheckGeneration(nsPresContext* aPresContext);

private:
  static nsIAtom* GetPropertyAtomForPseudoType(
    CSSPseudoElementType aPseudoType);

#ifdef DEBUG
  bool mCalledPropertyDtor;
#endif
};

} // namespace mozilla

#endif // mozilla_AnimationCollection_h

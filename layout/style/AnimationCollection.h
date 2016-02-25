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

typedef InfallibleTArray<RefPtr<dom::Animation>> AnimationPtrArray;

struct AnimationCollection : public LinkedListElement<AnimationCollection>
{
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
  ~AnimationCollection()
  {
    MOZ_ASSERT(mCalledPropertyDtor,
               "must call destructor through element property dtor");
    MOZ_COUNT_DTOR(AnimationCollection);
    remove();
  }

  void Destroy()
  {
    // This will call our destructor.
    mElement->DeleteProperty(mElementProperty);
  }

  static void PropertyDtor(void *aObject, nsIAtom *aPropertyName,
                           void *aPropertyValue, void *aData);

public:
  bool IsForElement() const { // rather than for a pseudo-element
    return mElementProperty == nsGkAtoms::animationsProperty ||
           mElementProperty == nsGkAtoms::transitionsProperty;
  }

  bool IsForBeforePseudo() const {
    return mElementProperty == nsGkAtoms::animationsOfBeforeProperty ||
           mElementProperty == nsGkAtoms::transitionsOfBeforeProperty;
  }

  bool IsForAfterPseudo() const {
    return mElementProperty == nsGkAtoms::animationsOfAfterProperty ||
           mElementProperty == nsGkAtoms::transitionsOfAfterProperty;
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

  AnimationPtrArray mAnimations;

  // For CSS transitions only, we record the most recent generation
  // for which we've done the transition update, so that we avoid doing
  // it more than once per style change.
  // (Note that we also store an animation generation on each EffectSet in
  // order to track when we need to update animations on layers.)
  uint64_t mCheckGeneration;

  // Update mCheckGeneration to RestyleManager's count
  void UpdateCheckGeneration(nsPresContext* aPresContext);

private:
#ifdef DEBUG
  bool mCalledPropertyDtor;
#endif
};

} // namespace mozilla

#endif // mozilla_AnimationCollection_h

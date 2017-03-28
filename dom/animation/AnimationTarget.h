/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationTarget_h
#define mozilla_AnimationTarget_h

#include "mozilla/Attributes.h"   // For MOZ_NON_OWNING_REF
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsCSSPseudoElements.h"

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

struct OwningAnimationTarget
{
  OwningAnimationTarget(dom::Element* aElement, CSSPseudoElementType aType)
    : mElement(aElement), mPseudoType(aType) { }

  explicit OwningAnimationTarget(dom::Element* aElement)
    : mElement(aElement) { }

  bool operator==(const OwningAnimationTarget& aOther) const
  {
     return mElement == aOther.mElement &&
            mPseudoType == aOther.mPseudoType;
  }

  // mElement represents the parent element of a pseudo-element, not the
  // generated content element.
  RefPtr<dom::Element> mElement;
  CSSPseudoElementType mPseudoType = CSSPseudoElementType::NotPseudo;
};

struct NonOwningAnimationTarget
{
  NonOwningAnimationTarget() = default;

  NonOwningAnimationTarget(dom::Element* aElement, CSSPseudoElementType aType)
    : mElement(aElement), mPseudoType(aType) { }

  explicit NonOwningAnimationTarget(const OwningAnimationTarget& aOther)
    : mElement(aOther.mElement), mPseudoType(aOther.mPseudoType) { }

  bool operator==(const NonOwningAnimationTarget& aOther) const
  {
    return mElement == aOther.mElement &&
           mPseudoType == aOther.mPseudoType;
  }

  // mElement represents the parent element of a pseudo-element, not the
  // generated content element.
  dom::Element* MOZ_NON_OWNING_REF mElement = nullptr;
  CSSPseudoElementType mPseudoType = CSSPseudoElementType::NotPseudo;
};


// Helper functions for cycle-collecting Maybe<OwningAnimationTarget>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            Maybe<OwningAnimationTarget>& aTarget,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  if (aTarget) {
    ImplCycleCollectionTraverse(aCallback, aTarget->mElement, aName, aFlags);
  }
}

inline void
ImplCycleCollectionUnlink(Maybe<OwningAnimationTarget>& aTarget)
{
  if (aTarget) {
    ImplCycleCollectionUnlink(aTarget->mElement);
  }
}

} // namespace mozilla

#endif // mozilla_AnimationTarget_h

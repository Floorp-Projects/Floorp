/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NonOwningAnimationTarget_h
#define mozilla_NonOwningAnimationTarget_h

#include "mozilla/Attributes.h"   // For MOZ_NON_OWNING_REF
#include "nsCSSPseudoElements.h"

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

struct NonOwningAnimationTarget
{
  NonOwningAnimationTarget(dom::Element* aElement, CSSPseudoElementType aType)
    : mElement(aElement), mPseudoType(aType) { }

  // mElement represents the parent element of a pseudo-element, not the
  // generated content element.
  dom::Element* MOZ_NON_OWNING_REF mElement = nullptr;
  CSSPseudoElementType mPseudoType = CSSPseudoElementType::NotPseudo;
};

} // namespace mozilla

#endif // mozilla_NonOwningAnimationTarget_h

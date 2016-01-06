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
#include "nsCSSPseudoElements.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
class Animation;
class Element;
}

class EffectCompositor
{
public:
  static bool HasAnimationsForCompositor(const nsIFrame* aFrame,
                                         nsCSSProperty aProperty);

  static nsTArray<RefPtr<dom::Animation>>
  GetAnimationsForCompositor(const nsIFrame* aFrame,
                             nsCSSProperty aProperty);

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
};

} // namespace mozilla

#endif // mozilla_EffectCompositor_h

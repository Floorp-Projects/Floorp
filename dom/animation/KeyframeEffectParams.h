/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyframeEffectParams_h
#define mozilla_KeyframeEffectParams_h

#include "mozilla/dom/KeyframeEffectBinding.h"  // IterationCompositeOperation
#include "mozilla/PseudoStyleType.h"            // PseudoStyleType

namespace mozilla {

struct KeyframeEffectParams {
  KeyframeEffectParams() = default;
  KeyframeEffectParams(dom::IterationCompositeOperation aIterationComposite,
                       dom::CompositeOperation aComposite,
                       PseudoStyleType aPseudoType)
      : mIterationComposite(aIterationComposite),
        mComposite(aComposite),
        mPseudoType(aPseudoType) {}
  explicit KeyframeEffectParams(dom::CompositeOperation aComposite)
      : mComposite(aComposite) {}

  dom::IterationCompositeOperation mIterationComposite =
      dom::IterationCompositeOperation::Replace;
  dom::CompositeOperation mComposite = dom::CompositeOperation::Replace;
  PseudoStyleType mPseudoType = PseudoStyleType::NotPseudo;
};

}  // namespace mozilla

#endif  // mozilla_KeyframeEffectParams_h

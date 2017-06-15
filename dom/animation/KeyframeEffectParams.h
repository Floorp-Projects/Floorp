/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyframeEffectParams_h
#define mozilla_KeyframeEffectParams_h

// X11 has a #define for None
#ifdef None
#undef None
#endif
#include "mozilla/dom/KeyframeEffectBinding.h" // IterationCompositeOperation

namespace mozilla {

struct KeyframeEffectParams
{
  dom::IterationCompositeOperation mIterationComposite =
    dom::IterationCompositeOperation::Replace;
  dom::CompositeOperation mComposite = dom::CompositeOperation::Replace;
};

} // namespace mozilla

#endif // mozilla_KeyframeEffectParams_h

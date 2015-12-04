/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectCompositor_h
#define mozilla_EffectCompositor_h

#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {

namespace dom {
class Animation;
}

class EffectCompositor
{
public:
  static nsTArray<RefPtr<dom::Animation>>
  GetAnimationsForCompositor(const nsIFrame* aFrame,
                             nsCSSProperty aProperty);
};

} // namespace mozilla

#endif // mozilla_EffectCompositor_h

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasGradient.h"

#include "mozilla/dom/CanvasRenderingContext2D.h"

namespace mozilla::dom {
CanvasGradient::CanvasGradient(CanvasRenderingContext2D* aContext, Type aType)
    : mContext(aContext), mType(aType) {}

CanvasGradient::~CanvasGradient() = default;
}  // namespace mozilla::dom

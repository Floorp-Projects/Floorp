/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameUniformityData.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/dom/APZTestDataBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

using namespace gfx;

bool FrameUniformityData::ToJS(JS::MutableHandle<JS::Value> aOutValue,
                               JSContext* aContext) {
  dom::FrameUniformityResults results;
  dom::Sequence<dom::FrameUniformity>& layers =
      results.mLayerUniformities.Construct();

  for (const auto& [layerAddr, uniformity] : mUniformities) {
    // FIXME: Make this infallible after bug 968520 is done.
    MOZ_ALWAYS_TRUE(layers.AppendElement(fallible));
    dom::FrameUniformity& entry = layers.LastElement();

    entry.mLayerAddress.Construct() = layerAddr;
    entry.mFrameUniformity.Construct() = uniformity;
  }

  return dom::ToJSValue(aContext, results, aOutValue);
}

}  // namespace layers
}  // namespace mozilla

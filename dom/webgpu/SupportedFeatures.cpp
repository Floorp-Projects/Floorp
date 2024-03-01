/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SupportedFeatures.h"
#include "Adapter.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(SupportedFeatures, mParent)
GPU_IMPL_JS_WRAP(SupportedFeatures)

SupportedFeatures::SupportedFeatures(Adapter* const aParent)
    : ChildOf(aParent) {}

void SupportedFeatures::Add(const dom::GPUFeatureName aFeature,
                            ErrorResult& aRv) {
  const auto u8 = dom::GPUFeatureNameValues::GetString(aFeature);
  const auto u16 = NS_ConvertUTF8toUTF16(u8);
  dom::GPUSupportedFeatures_Binding::SetlikeHelpers::Add(this, u16, aRv);

  mFeatures.insert(aFeature);
}

}  // namespace mozilla::webgpu

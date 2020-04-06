/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "ShaderModule.h"

#include "Device.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(ShaderModule, mParent)
GPU_IMPL_JS_WRAP(ShaderModule)

ShaderModule::ShaderModule(Device* const aParent, RawId aId)
    : ChildOf(aParent), mId(aId) {}

ShaderModule::~ShaderModule() { Cleanup(); }

void ShaderModule::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendShaderModuleDestroy(mId);
    }
  }
}

}  // namespace webgpu
}  // namespace mozilla

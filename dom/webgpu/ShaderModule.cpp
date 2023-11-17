/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/Promise.h"
#include "ShaderModule.h"
#include "CompilationInfo.h"
#include "ipc/WebGPUChild.h"

#include "Device.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(ShaderModule, mParent, mCompilationInfo)
GPU_IMPL_JS_WRAP(ShaderModule)

ShaderModule::ShaderModule(Device* const aParent, RawId aId,
                           const RefPtr<dom::Promise>& aCompilationInfo)
    : ChildOf(aParent), mId(aId), mCompilationInfo(aCompilationInfo) {
  MOZ_RELEASE_ASSERT(aId);
}

ShaderModule::~ShaderModule() { Cleanup(); }

void ShaderModule::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendShaderModuleDrop(mId);
    }
  }
}

already_AddRefed<dom::Promise> ShaderModule::CompilationInfo(ErrorResult& aRv) {
  return GetCompilationInfo(aRv);
}

already_AddRefed<dom::Promise> ShaderModule::GetCompilationInfo(
    ErrorResult& aRv) {
  RefPtr<dom::Promise> tmp = mCompilationInfo;
  return tmp.forget();
}

}  // namespace mozilla::webgpu

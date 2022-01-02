/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompilationInfo.h"
#include "CompilationMessage.h"
#include "ShaderModule.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(CompilationInfo, mParent)
GPU_IMPL_JS_WRAP(CompilationInfo)

CompilationInfo::CompilationInfo(ShaderModule* const aParent)
    : ChildOf(aParent) {}

}  // namespace webgpu
}  // namespace mozilla

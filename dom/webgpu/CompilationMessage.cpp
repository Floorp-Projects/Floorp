/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompilationMessage.h"
#include "CompilationInfo.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(CompilationMessage, mParent)
GPU_IMPL_JS_WRAP(CompilationMessage)

CompilationMessage::CompilationMessage(CompilationInfo* const aParent,
                                       uint64_t aLineNum, uint64_t aLinePos,
                                       uint64_t aOffset, nsString&& aMessage)
    : ChildOf(aParent),
      mLineNum(aLineNum),
      mLinePos(aLinePos),
      mOffset(aOffset),
      mMessage(std::move(aMessage)) {}

}  // namespace mozilla::webgpu

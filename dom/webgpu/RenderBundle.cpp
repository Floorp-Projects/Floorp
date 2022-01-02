/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderBundle.h"

#include "Device.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderBundle, mParent)
GPU_IMPL_JS_WRAP(RenderBundle)

RenderBundle::RenderBundle(Device* const aParent, RawId aId)
    : ChildOf(aParent), mId(aId) {
  // If we happened to finish an encoder twice, the second
  // bundle should be invalid.
  if (!mId) {
    mValid = false;
  }
}

RenderBundle::~RenderBundle() { Cleanup(); }

void RenderBundle::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendRenderBundleDestroy(mId);
    }
  }
}

}  // namespace webgpu
}  // namespace mozilla

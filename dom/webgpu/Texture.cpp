/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Texture.h"

#include "mozilla/webgpu/ffi/wgpu.h"
#include "TextureView.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(Texture, mParent)
GPU_IMPL_JS_WRAP(Texture)

Texture::Texture(Device* const aParent, RawId aId,
                 const dom::GPUTextureDescriptor& aDesc)
    : ChildOf(aParent),
      mId(aId),
      mDefaultViewDescriptor(WebGPUChild::GetDefaultViewDescriptor(aDesc)) {}

Texture::~Texture() { Cleanup(); }

void Texture::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendTextureDestroy(mId);
    }
  }
}

already_AddRefed<TextureView> Texture::CreateView(
    const dom::GPUTextureViewDescriptor& aDesc) {
  RawId id = mParent->GetBridge()->TextureCreateView(mId, aDesc,
                                                     *mDefaultViewDescriptor);
  RefPtr<TextureView> view = new TextureView(this, id);
  return view.forget();
}

}  // namespace webgpu
}  // namespace mozilla

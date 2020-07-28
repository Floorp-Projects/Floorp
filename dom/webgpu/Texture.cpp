/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Texture.h"

#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/dom/HTMLCanvasElement.h"
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

uint8_t Texture::BytesPerTexel() const {
  switch (mDefaultViewDescriptor->format) {
    case ffi::WGPUTextureFormat_R8Unorm:
    case ffi::WGPUTextureFormat_R8Snorm:
    case ffi::WGPUTextureFormat_R8Uint:
    case ffi::WGPUTextureFormat_R8Sint:
      return 1;
    case ffi::WGPUTextureFormat_R16Uint:
    case ffi::WGPUTextureFormat_R16Sint:
    case ffi::WGPUTextureFormat_R16Float:
    case ffi::WGPUTextureFormat_Rg8Unorm:
    case ffi::WGPUTextureFormat_Rg8Snorm:
    case ffi::WGPUTextureFormat_Rg8Uint:
    case ffi::WGPUTextureFormat_Rg8Sint:
      return 2;
    case ffi::WGPUTextureFormat_R32Uint:
    case ffi::WGPUTextureFormat_R32Sint:
    case ffi::WGPUTextureFormat_R32Float:
    case ffi::WGPUTextureFormat_Rg16Uint:
    case ffi::WGPUTextureFormat_Rg16Sint:
    case ffi::WGPUTextureFormat_Rg16Float:
    case ffi::WGPUTextureFormat_Rgba8Unorm:
    case ffi::WGPUTextureFormat_Rgba8UnormSrgb:
    case ffi::WGPUTextureFormat_Rgba8Snorm:
    case ffi::WGPUTextureFormat_Rgba8Uint:
    case ffi::WGPUTextureFormat_Rgba8Sint:
    case ffi::WGPUTextureFormat_Bgra8Unorm:
    case ffi::WGPUTextureFormat_Bgra8UnormSrgb:
    case ffi::WGPUTextureFormat_Rgb10a2Unorm:
    case ffi::WGPUTextureFormat_Rg11b10Float:
      return 4;
    case ffi::WGPUTextureFormat_Rg32Uint:
    case ffi::WGPUTextureFormat_Rg32Sint:
    case ffi::WGPUTextureFormat_Rg32Float:
      return 8;
    case ffi::WGPUTextureFormat_Rgba16Uint:
    case ffi::WGPUTextureFormat_Rgba16Sint:
    case ffi::WGPUTextureFormat_Rgba16Float:
    case ffi::WGPUTextureFormat_Rgba32Uint:
    case ffi::WGPUTextureFormat_Rgba32Sint:
    case ffi::WGPUTextureFormat_Rgba32Float:
      return 16;
    default:
      return 0;
  }
}

already_AddRefed<TextureView> Texture::CreateView(
    const dom::GPUTextureViewDescriptor& aDesc) {
  RawId id = mParent->GetBridge()->TextureCreateView(mId, aDesc,
                                                     *mDefaultViewDescriptor);
  RefPtr<TextureView> view = new TextureView(this, id);
  return view.forget();
}

void Texture::Destroy() {
  // TODO: we don't have to implement it right now, but it's used by the
  // examples
}

}  // namespace webgpu
}  // namespace mozilla

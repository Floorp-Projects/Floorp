/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Texture.h"

#include "ipc/WebGPUChild.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "TextureView.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(Texture, mParent)
GPU_IMPL_JS_WRAP(Texture)

static Maybe<uint8_t> GetBytesPerBlock(dom::GPUTextureFormat format) {
  switch (format) {
    case dom::GPUTextureFormat::R8unorm:
    case dom::GPUTextureFormat::R8snorm:
    case dom::GPUTextureFormat::R8uint:
    case dom::GPUTextureFormat::R8sint:
      return Some<uint8_t>(1u);
    case dom::GPUTextureFormat::R16uint:
    case dom::GPUTextureFormat::R16sint:
    case dom::GPUTextureFormat::R16float:
    case dom::GPUTextureFormat::Rg8unorm:
    case dom::GPUTextureFormat::Rg8snorm:
    case dom::GPUTextureFormat::Rg8uint:
    case dom::GPUTextureFormat::Rg8sint:
      return Some<uint8_t>(2u);
    case dom::GPUTextureFormat::R32uint:
    case dom::GPUTextureFormat::R32sint:
    case dom::GPUTextureFormat::R32float:
    case dom::GPUTextureFormat::Rg16uint:
    case dom::GPUTextureFormat::Rg16sint:
    case dom::GPUTextureFormat::Rg16float:
    case dom::GPUTextureFormat::Rgba8unorm:
    case dom::GPUTextureFormat::Rgba8unorm_srgb:
    case dom::GPUTextureFormat::Rgba8snorm:
    case dom::GPUTextureFormat::Rgba8uint:
    case dom::GPUTextureFormat::Rgba8sint:
    case dom::GPUTextureFormat::Bgra8unorm:
    case dom::GPUTextureFormat::Bgra8unorm_srgb:
    case dom::GPUTextureFormat::Rgb10a2unorm:
    case dom::GPUTextureFormat::Rg11b10float:
      return Some<uint8_t>(4u);
    case dom::GPUTextureFormat::Rg32uint:
    case dom::GPUTextureFormat::Rg32sint:
    case dom::GPUTextureFormat::Rg32float:
    case dom::GPUTextureFormat::Rgba16uint:
    case dom::GPUTextureFormat::Rgba16sint:
    case dom::GPUTextureFormat::Rgba16float:
      return Some<uint8_t>(8u);
    case dom::GPUTextureFormat::Rgba32uint:
    case dom::GPUTextureFormat::Rgba32sint:
    case dom::GPUTextureFormat::Rgba32float:
      return Some<uint8_t>(16u);
    case dom::GPUTextureFormat::Depth32float:
      return Some<uint8_t>(4u);
    case dom::GPUTextureFormat::Bc1_rgba_unorm:
    case dom::GPUTextureFormat::Bc1_rgba_unorm_srgb:
    case dom::GPUTextureFormat::Bc4_r_unorm:
    case dom::GPUTextureFormat::Bc4_r_snorm:
      return Some<uint8_t>(8u);
    case dom::GPUTextureFormat::Bc2_rgba_unorm:
    case dom::GPUTextureFormat::Bc2_rgba_unorm_srgb:
    case dom::GPUTextureFormat::Bc3_rgba_unorm:
    case dom::GPUTextureFormat::Bc3_rgba_unorm_srgb:
    case dom::GPUTextureFormat::Bc5_rg_unorm:
    case dom::GPUTextureFormat::Bc5_rg_snorm:
    case dom::GPUTextureFormat::Bc6h_rgb_ufloat:
    case dom::GPUTextureFormat::Bc6h_rgb_float:
    case dom::GPUTextureFormat::Bc7_rgba_unorm:
    case dom::GPUTextureFormat::Bc7_rgba_unorm_srgb:
      return Some<uint8_t>(16u);
    case dom::GPUTextureFormat::Depth24plus:
    case dom::GPUTextureFormat::Depth24plus_stencil8:
    case dom::GPUTextureFormat::EndGuard_:
      return Nothing();
  }
  return Nothing();
}

Texture::Texture(Device* const aParent, RawId aId,
                 const dom::GPUTextureDescriptor& aDesc)
    : ChildOf(aParent),
      mId(aId),
      mFormat(aDesc.mFormat),
      mBytesPerBlock(GetBytesPerBlock(aDesc.mFormat)) {}

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
  auto bridge = mParent->GetBridge();
  RawId id = 0;
  if (bridge->IsOpen()) {
    id = bridge->TextureCreateView(mId, mParent->mId, aDesc);
  }

  RefPtr<TextureView> view = new TextureView(this, id);
  return view.forget();
}

void Texture::Destroy() {
  // TODO: we don't have to implement it right now, but it's used by the
  // examples
}

}  // namespace mozilla::webgpu

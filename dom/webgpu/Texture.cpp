/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Texture.h"

#include "ipc/WebGPUChild.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "TextureView.h"
#include "Utility.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(Texture, mParent)
GPU_IMPL_JS_WRAP(Texture)

static Maybe<uint8_t> GetBytesPerBlockSingleAspect(
    dom::GPUTextureFormat aFormat) {
  auto format = WebGPUChild::ConvertTextureFormat(aFormat);
  uint32_t bytes = ffi::wgpu_texture_format_block_size_single_aspect(format);
  if (bytes == 0) {
    // The above function returns zero if the texture has multiple aspects like
    // depth and stencil.
    return Nothing();
  }

  return Some((uint8_t)bytes);
}

Texture::Texture(Device* const aParent, RawId aId,
                 const dom::GPUTextureDescriptor& aDesc)
    : ChildOf(aParent),
      mId(aId),
      mFormat(aDesc.mFormat),
      mBytesPerBlock(GetBytesPerBlockSingleAspect(aDesc.mFormat)),
      mSize(ConvertExtent(aDesc.mSize)),
      mMipLevelCount(aDesc.mMipLevelCount),
      mSampleCount(aDesc.mSampleCount),
      mDimension(aDesc.mDimension),
      mUsage(aDesc.mUsage) {
  MOZ_RELEASE_ASSERT(aId);
}

void Texture::Cleanup() {
  if (!mParent) {
    return;
  }

  auto bridge = mParent->GetBridge();
  if (bridge && bridge->IsOpen()) {
    bridge->SendTextureDrop(mId);
  }

  // After cleanup is called, no other method should ever be called on the
  // object so we don't have to null-check mParent in other places.
  // This serves the purpose of preventing SendTextureDrop from happening
  // twice. TODO: Does it matter for breaking cycles too? Cleanup is called
  // by the macros that deal with cycle colleciton.
  mParent = nullptr;
}

Texture::~Texture() { Cleanup(); }

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
  auto bridge = mParent->GetBridge();
  if (bridge && bridge->IsOpen()) {
    bridge->SendTextureDestroy(mId, mParent->GetId());
  }
}

}  // namespace mozilla::webgpu

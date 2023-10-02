/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DcompSurfaceImage.h"

#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "mozilla/webrender/RenderDcompSurfaceTextureHost.h"
#include "mozilla/webrender/WebRenderAPI.h"

extern mozilla::LazyLogModule gDcompSurface;
#define LOG(...) MOZ_LOG(gDcompSurface, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla::layers {

already_AddRefed<TextureHost> CreateTextureHostDcompSurface(
    const SurfaceDescriptor& aDesc, ISurfaceAllocator* aDeallocator,
    LayersBackend aBackend, TextureFlags aFlags) {
  MOZ_ASSERT(aDesc.type() == SurfaceDescriptor::TSurfaceDescriptorDcompSurface);
  RefPtr<TextureHost> result = new DcompSurfaceHandleHost(
      aFlags, aDesc.get_SurfaceDescriptorDcompSurface());
  return result.forget();
}

/* static */
already_AddRefed<TextureClient> DcompSurfaceTexture::CreateTextureClient(
    HANDLE aHandle, gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
    KnowsCompositor* aKnowsCompositor) {
  RefPtr<TextureClient> textureClient = MakeAndAddRef<TextureClient>(
      new DcompSurfaceTexture(aHandle, aSize, aFormat), TextureFlags::NO_FLAGS,
      aKnowsCompositor->GetTextureForwarder());
  return textureClient.forget();
}

DcompSurfaceTexture::~DcompSurfaceTexture() {
  LOG("Destroy DcompSurfaceTexture %p, close handle=%p", this, mHandle);
  CloseHandle(mHandle);
}

bool DcompSurfaceTexture::Serialize(SurfaceDescriptor& aOutDescriptor) {
  aOutDescriptor = SurfaceDescriptorDcompSurface(ipc::FileDescriptor(mHandle),
                                                 mSize, mFormat);
  return true;
}

void DcompSurfaceTexture::GetSubDescriptor(
    RemoteDecoderVideoSubDescriptor* aOutDesc) {
  *aOutDesc = SurfaceDescriptorDcompSurface(ipc::FileDescriptor(mHandle), mSize,
                                            mFormat);
}

DcompSurfaceImage::DcompSurfaceImage(HANDLE aHandle, gfx::IntSize aSize,
                                     gfx::SurfaceFormat aFormat,
                                     KnowsCompositor* aKnowsCompositor)
    : Image(nullptr, ImageFormat::DCOMP_SURFACE),
      mTextureClient(DcompSurfaceTexture::CreateTextureClient(
          aHandle, aSize, aFormat, aKnowsCompositor)) {
  // Dcomp surface supports DXGI_FORMAT_B8G8R8A8_UNORM,
  // DXGI_FORMAT_R8G8B8A8_UNORM and DXGI_FORMAT_R16G16B16A16_FLOAT
  MOZ_ASSERT(aFormat == gfx::SurfaceFormat::B8G8R8A8 ||
             aFormat == gfx::SurfaceFormat::R8G8B8A8);
}

TextureClient* DcompSurfaceImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  return mTextureClient;
}

DcompSurfaceHandleHost::DcompSurfaceHandleHost(
    TextureFlags aFlags, const SurfaceDescriptorDcompSurface& aDescriptor)
    : TextureHost(TextureHostType::DcompSurface, aFlags),
      mHandle(const_cast<ipc::FileDescriptor&>(aDescriptor.handle())
                  .TakePlatformHandle()),
      mSize(aDescriptor.size()),
      mFormat(aDescriptor.format()) {
  LOG("Create DcompSurfaceHandleHost %p", this);
}

DcompSurfaceHandleHost::~DcompSurfaceHandleHost() {
  LOG("Destroy DcompSurfaceHandleHost %p", this);
}

void DcompSurfaceHandleHost::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(mExternalImageId.isSome());
  LOG("DcompSurfaceHandleHost %p CreateRenderTexture, ext-id=%" PRIu64, this,
      wr::AsUint64(aExternalImageId));
  RefPtr<wr::RenderTextureHost> texture =
      new wr::RenderDcompSurfaceTextureHost(mHandle.get(), mSize, mFormat);
  wr::RenderThread::Get()->RegisterExternalImage(aExternalImageId,
                                                 texture.forget());
}

void DcompSurfaceHandleHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys,
    const wr::ExternalImageId& aExternalImageId) {
  if (!gfx::gfxVars::UseWebRenderANGLE()) {
    MOZ_ASSERT_UNREACHABLE("Unexpected to be called without ANGLE");
    return;
  }
  MOZ_ASSERT(mHandle);
  MOZ_ASSERT(aImageKeys.length() == 1);
  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;
  wr::ImageDescriptor descriptor(mSize, GetFormat());
  // Prefer TextureExternal unless the backend requires TextureRect.
  TextureHost::NativeTexturePolicy policy =
      TextureHost::BackendNativeTexturePolicy(aResources.GetBackendType(),
                                              mSize);
  auto imageType = policy == TextureHost::NativeTexturePolicy::REQUIRE
                       ? wr::ExternalImageType::TextureHandle(
                             wr::ImageBufferKind::TextureRect)
                       : wr::ExternalImageType::TextureHandle(
                             wr::ImageBufferKind::TextureExternal);
  LOG("DcompSurfaceHandleHost %p PushResourceUpdate, exi-id=%" PRIu64
      ", type=%s",
      this, wr::AsUint64(aExternalImageId),
      policy == TextureHost::NativeTexturePolicy::REQUIRE ? "rect" : "ext");
  (aResources.*method)(aImageKeys[0], descriptor, aExternalImageId, imageType,
                       0);
}

void DcompSurfaceHandleHost::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  if (!gfx::gfxVars::UseWebRenderANGLE()) {
    MOZ_ASSERT_UNREACHABLE("Unexpected to be called without ANGLE");
    return;
  }
  LOG("DcompSurfaceHandleHost %p PushDisplayItems", this);
  MOZ_ASSERT(aImageKeys.length() == 1);
  aBuilder.PushImage(
      aBounds, aClip, true, false, aFilter, aImageKeys[0],
      !(mFlags & TextureFlags::NON_PREMULTIPLIED),
      wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f},
      aFlags.contains(PushDisplayItemFlag::PREFER_COMPOSITOR_SURFACE),
      SupportsExternalCompositing(aBuilder.GetBackendType()));
}

}  // namespace mozilla::layers

#undef LOG

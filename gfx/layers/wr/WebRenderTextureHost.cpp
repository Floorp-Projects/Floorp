/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextureHost.h"

#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/TextureSourceProvider.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/layers/TextureHostOGL.h"
#endif

namespace mozilla::layers {

class ScheduleHandleRenderTextureOps : public wr::NotificationHandler {
 public:
  explicit ScheduleHandleRenderTextureOps() {}

  virtual void Notify(wr::Checkpoint aCheckpoint) override {
    if (aCheckpoint == wr::Checkpoint::FrameTexturesUpdated) {
      MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
      wr::RenderThread::Get()->HandleRenderTextureOps();
    } else {
      MOZ_ASSERT(aCheckpoint == wr::Checkpoint::TransactionDropped);
    }
  }

 protected:
};

WebRenderTextureHost::WebRenderTextureHost(
    const SurfaceDescriptor& aDesc, TextureFlags aFlags, TextureHost* aTexture,
    const wr::ExternalImageId& aExternalImageId)
    : TextureHost(aFlags), mWrappedTextureHost(aTexture) {
  // The wrapped textureHost will be used in WebRender, and the WebRender could
  // run at another thread. It's hard to control the life-time when gecko
  // receives PTextureParent destroy message. It's possible that textureHost is
  // still used by WebRender. So, we only accept the textureHost without
  // DEALLOCATE_CLIENT flag here. If the buffer deallocation is controlled by
  // parent, we could do something to make sure the wrapped textureHost is not
  // used by WebRender and then release it.
  MOZ_ASSERT(!(aFlags & TextureFlags::DEALLOCATE_CLIENT));
  MOZ_ASSERT(mWrappedTextureHost);

  MOZ_COUNT_CTOR(WebRenderTextureHost);

  mExternalImageId = Some(aExternalImageId);
}

WebRenderTextureHost::~WebRenderTextureHost() {
  MOZ_COUNT_DTOR(WebRenderTextureHost);
}

wr::ExternalImageId WebRenderTextureHost::GetExternalImageKey() {
  if (IsValid()) {
    mWrappedTextureHost->EnsureRenderTexture(mExternalImageId);
  }
  MOZ_ASSERT(mWrappedTextureHost->mExternalImageId.isSome());
  return mWrappedTextureHost->mExternalImageId.ref();
}

bool WebRenderTextureHost::IsValid() { return mWrappedTextureHost->IsValid(); }

void WebRenderTextureHost::UnbindTextureSource() {
  if (mWrappedTextureHost->IsWrappingBufferTextureHost()) {
    mWrappedTextureHost->UnbindTextureSource();
  }
  // Handle read unlock
  TextureHost::UnbindTextureSource();
}

already_AddRefed<gfx::DataSourceSurface> WebRenderTextureHost::GetAsSurface() {
  return mWrappedTextureHost->GetAsSurface();
}

gfx::ColorDepth WebRenderTextureHost::GetColorDepth() const {
  return mWrappedTextureHost->GetColorDepth();
}

gfx::YUVColorSpace WebRenderTextureHost::GetYUVColorSpace() const {
  return mWrappedTextureHost->GetYUVColorSpace();
}

gfx::ColorRange WebRenderTextureHost::GetColorRange() const {
  return mWrappedTextureHost->GetColorRange();
}

gfx::IntSize WebRenderTextureHost::GetSize() const {
  return mWrappedTextureHost->GetSize();
}

gfx::SurfaceFormat WebRenderTextureHost::GetFormat() const {
  return mWrappedTextureHost->GetFormat();
}

void WebRenderTextureHost::NotifyNotUsed() {
#ifdef MOZ_WIDGET_ANDROID
  if (mWrappedTextureHost->AsSurfaceTextureHost()) {
    wr::RenderThread::Get()->NotifyNotUsed(GetExternalImageKey());
  }
#endif
  TextureHost::NotifyNotUsed();
}

void WebRenderTextureHost::MaybeNotifyForUse(wr::TransactionBuilder& aTxn) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mWrappedTextureHost->AsSurfaceTextureHost()) {
    wr::RenderThread::Get()->NotifyForUse(GetExternalImageKey());
    aTxn.Notify(wr::Checkpoint::FrameTexturesUpdated,
                MakeUnique<ScheduleHandleRenderTextureOps>());
  }
#endif
}

bool WebRenderTextureHost::IsWrappingBufferTextureHost() {
  return mWrappedTextureHost->IsWrappingBufferTextureHost();
}

void WebRenderTextureHost::PrepareForUse() {
  if (mWrappedTextureHost->AsSurfaceTextureHost() ||
      mWrappedTextureHost->IsWrappingBufferTextureHost()) {
    // Call PrepareForUse on render thread.
    // See RenderAndroidSurfaceTextureHostOGL::PrepareForUse.
    wr::RenderThread::Get()->PrepareForUse(GetExternalImageKey());
  }
}

gfx::SurfaceFormat WebRenderTextureHost::GetReadFormat() const {
  return mWrappedTextureHost->GetReadFormat();
}

int32_t WebRenderTextureHost::GetRGBStride() {
  gfx::SurfaceFormat format = GetFormat();
  if (GetFormat() == gfx::SurfaceFormat::YUV) {
    // XXX this stride is used until yuv image rendering by webrender is used.
    // Software converted RGB buffers strides are aliened to 16
    return gfx::GetAlignedStride<16>(
        GetSize().width, BytesPerPixel(gfx::SurfaceFormat::B8G8R8A8));
  }
  return ImageDataSerializer::ComputeRGBStride(format, GetSize().width);
}

bool WebRenderTextureHost::NeedsDeferredDeletion() const {
  return mWrappedTextureHost->NeedsDeferredDeletion();
}

uint32_t WebRenderTextureHost::NumSubTextures() {
  return mWrappedTextureHost->NumSubTextures();
}

void WebRenderTextureHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(GetExternalImageKey() == aExtID);

  mWrappedTextureHost->PushResourceUpdates(aResources, aOp, aImageKeys, aExtID);
}

void WebRenderTextureHost::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  MOZ_ASSERT(aImageKeys.length() > 0);

  mWrappedTextureHost->PushDisplayItems(aBuilder, aBounds, aClip, aFilter,
                                        aImageKeys, aFlags);
}

bool WebRenderTextureHost::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  return mWrappedTextureHost->SupportsExternalCompositing(aBackend);
}

void WebRenderTextureHost::SetAcquireFence(
    mozilla::ipc::FileDescriptor&& aFenceFd) {
  mWrappedTextureHost->SetAcquireFence(std::move(aFenceFd));
}

void WebRenderTextureHost::SetReleaseFence(
    mozilla::ipc::FileDescriptor&& aFenceFd) {
  mWrappedTextureHost->SetReleaseFence(std::move(aFenceFd));
}

mozilla::ipc::FileDescriptor WebRenderTextureHost::GetAndResetReleaseFence() {
  return mWrappedTextureHost->GetAndResetReleaseFence();
}

AndroidHardwareBuffer* WebRenderTextureHost::GetAndroidHardwareBuffer() const {
  return mWrappedTextureHost->GetAndroidHardwareBuffer();
}

}  // namespace mozilla::layers

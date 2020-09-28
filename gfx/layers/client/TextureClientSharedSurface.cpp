/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientSharedSurface.h"

#include "GLContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"  // for gfxDebug
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"
#include "SharedSurface.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

/* static */
already_AddRefed<TextureClient> SharedSurfaceTextureData::CreateTextureClient(
    const layers::SurfaceDescriptor& aDesc, const gfx::SurfaceFormat aFormat,
    gfx::IntSize aSize, TextureFlags aFlags, LayersIPCChannel* aAllocator) {
  auto data = MakeUnique<SharedSurfaceTextureData>(aDesc, aFormat, aSize);
  return TextureClient::CreateWithData(data.release(), aFlags, aAllocator);
}

SharedSurfaceTextureData::SharedSurfaceTextureData(
    const SurfaceDescriptor& desc, const gfx::SurfaceFormat format,
    const gfx::IntSize size)
    : mDesc(desc), mFormat(format), mSize(size) {}

SharedSurfaceTextureData::~SharedSurfaceTextureData() = default;

void SharedSurfaceTextureData::Deallocate(LayersIPCChannel*) {}

void SharedSurfaceTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = false;
  aInfo.canExposeMappedData = false;
}

bool SharedSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  if (mDesc.type() !=
      SurfaceDescriptor::TSurfaceDescriptorAndroidHardwareBuffer) {
    aOutDescriptor = mDesc;
    return true;
  }

#ifdef MOZ_WIDGET_ANDROID
  // File descriptor is created heare for reducing its allocation.
  const SurfaceDescriptorAndroidHardwareBuffer& desc =
      mDesc.get_SurfaceDescriptorAndroidHardwareBuffer();
  RefPtr<AndroidHardwareBuffer> buffer =
      AndroidHardwareBufferManager::Get()->GetBuffer(desc.bufferId());
  if (!buffer) {
    return false;
  }

  int fd[2] = {};
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fd) != 0) {
    return false;
  }

  UniqueFileHandle readerFd(fd[0]);
  UniqueFileHandle writerFd(fd[1]);

  // Send the AHardwareBuffer to an AF_UNIX socket. It does not acquire or
  // retain a reference to the buffer object. The caller is therefore
  // responsible for ensuring that the buffer remains alive through the lifetime
  // of this file descriptor.
  int ret = buffer->SendHandleToUnixSocket(writerFd.get());
  if (ret < 0) {
    return false;
  }

  aOutDescriptor = layers::SurfaceDescriptorAndroidHardwareBuffer(
      ipc::FileDescriptor(readerFd.release()), buffer->mId, buffer->mSize,
      buffer->mFormat);
  return true;
#else
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return false;
#endif
}

TextureFlags SharedSurfaceTextureData::GetTextureFlags() const {
  TextureFlags flags = TextureFlags::NO_FLAGS;

#ifdef MOZ_WIDGET_ANDROID
  if (mDesc.type() ==
      SurfaceDescriptor::TSurfaceDescriptorAndroidHardwareBuffer) {
    flags |= TextureFlags::WAIT_HOST_USAGE_END;
  }
#endif
  return flags;
}

Maybe<uint64_t> SharedSurfaceTextureData::GetBufferId() const {
#ifdef MOZ_WIDGET_ANDROID
  if (mDesc.type() ==
      SurfaceDescriptor::TSurfaceDescriptorAndroidHardwareBuffer) {
    const SurfaceDescriptorAndroidHardwareBuffer& desc =
        mDesc.get_SurfaceDescriptorAndroidHardwareBuffer();
    return Some(desc.bufferId());
  }
#endif
  return Nothing();
}

mozilla::ipc::FileDescriptor SharedSurfaceTextureData::GetAcquireFence() {
#ifdef MOZ_WIDGET_ANDROID
  if (mDesc.type() ==
      SurfaceDescriptor::TSurfaceDescriptorAndroidHardwareBuffer) {
    const SurfaceDescriptorAndroidHardwareBuffer& desc =
        mDesc.get_SurfaceDescriptorAndroidHardwareBuffer();
    RefPtr<AndroidHardwareBuffer> buffer =
        AndroidHardwareBufferManager::Get()->GetBuffer(desc.bufferId());
    if (!buffer) {
      return ipc::FileDescriptor();
    }

    return buffer->GetAcquireFence();
  }
#endif
  return ipc::FileDescriptor();
}

/*
static TextureFlags FlagsFrom(const SharedSurfaceDescriptor& desc) {
  auto flags = TextureFlags::ORIGIN_BOTTOM_LEFT;
  if (!desc.isPremultAlpha) {
    flags |= TextureFlags::NON_PREMULTIPLIED;
  }
  return flags;
}

SharedSurfaceTextureClient::SharedSurfaceTextureClient(
    const SharedSurfaceDescriptor& aDesc, LayersIPCChannel* aAllocator)
    : TextureClient(new SharedSurfaceTextureData(desc), FlagsFrom(desc),
aAllocator) { mWorkaroundAnnoyingSharedSurfaceLifetimeIssues = true;
}

SharedSurfaceTextureClient::~SharedSurfaceTextureClient() {
  // XXX - Things break when using the proper destruction handshake with
  // SharedSurfaceTextureData because the TextureData outlives its gl
  // context. Having a strong reference to the gl context creates a cycle.
  // This needs to be fixed in a better way, though, because deleting
  // the TextureData here can race with the compositor and cause flashing.
  TextureData* data = mData;
  mData = nullptr;

  Destroy();

  if (data) {
    // Destroy mData right away without doing the proper deallocation handshake,
    // because SharedSurface depends on things that may not outlive the
    // texture's destructor so we can't wait until we know the compositor isn't
    // using the texture anymore. It goes without saying that this is really bad
    // and we should fix the bugs that block doing the right thing such as bug
    // 1224199 sooner rather than later.
    delete data;
  }
}
*/

}  // namespace layers
}  // namespace mozilla

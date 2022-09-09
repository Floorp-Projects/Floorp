/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"           // for GLContext, etc
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/gfx/2D.h"     // for Factory
#include "mozilla/gfx/Point.h"  // for IntSize
#include "GLLibraryEGL.h"

#ifdef MOZ_WIDGET_ANDROID
#  include <jni.h>
#  include <android/native_window.h>
#  include <android/native_window_jni.h>
#  include <sys/socket.h>
#  include "mozilla/ipc/FileDescriptor.h"
#  include "mozilla/java/GeckoSurfaceWrappers.h"
#  include "mozilla/java/SurfaceAllocatorWrappers.h"
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#  include "mozilla/UniquePtrExtensions.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class CompositableForwarder;

////////////////////////////////////////////////////////////////////////
// AndroidSurface

#ifdef MOZ_WIDGET_ANDROID

already_AddRefed<TextureClient> AndroidSurfaceTextureData::CreateTextureClient(
    AndroidSurfaceTextureHandle aHandle, gfx::IntSize aSize, bool aContinuous,
    gl::OriginPos aOriginPos, bool aHasAlpha,
    Maybe<gfx::Matrix4x4> aTransformOverride, LayersIPCChannel* aAllocator,
    TextureFlags aFlags) {
  if (aOriginPos == gl::OriginPos::BottomLeft) {
    aFlags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
  }

  return TextureClient::CreateWithData(
      new AndroidSurfaceTextureData(aHandle, aSize, aContinuous, aHasAlpha,
                                    aTransformOverride),
      aFlags, aAllocator);
}

AndroidSurfaceTextureData::AndroidSurfaceTextureData(
    AndroidSurfaceTextureHandle aHandle, gfx::IntSize aSize, bool aContinuous,
    bool aHasAlpha, Maybe<gfx::Matrix4x4> aTransformOverride)
    : mHandle(aHandle),
      mSize(aSize),
      mContinuous(aContinuous),
      mHasAlpha(aHasAlpha),
      mTransformOverride(aTransformOverride) {
  MOZ_ASSERT(mHandle);
}

AndroidSurfaceTextureData::~AndroidSurfaceTextureData() {}

void AndroidSurfaceTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = gfx::SurfaceFormat::UNKNOWN;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = false;
  aInfo.canExposeMappedData = false;
}

bool AndroidSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  aOutDescriptor = SurfaceTextureDescriptor(
      mHandle, mSize,
      mHasAlpha ? gfx::SurfaceFormat::R8G8B8A8 : gfx::SurfaceFormat::R8G8B8X8,
      mContinuous, mTransformOverride);
  return true;
}

#endif  // MOZ_WIDGET_ANDROID

////////////////////////////////////////////////////////////////////////
// AndroidNativeWindow

#ifdef MOZ_WIDGET_ANDROID

AndroidNativeWindowTextureData* AndroidNativeWindowTextureData::Create(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  if (aFormat != gfx::SurfaceFormat::R8G8B8A8 &&
      aFormat != gfx::SurfaceFormat::R8G8B8X8 &&
      aFormat != gfx::SurfaceFormat::B8G8R8A8 &&
      aFormat != gfx::SurfaceFormat::B8G8R8X8 &&
      aFormat != gfx::SurfaceFormat::R5G6B5_UINT16) {
    return nullptr;
  }

  auto surface =
      java::GeckoSurface::LocalRef(java::SurfaceAllocator::AcquireSurface(
          aSize.width, aSize.height, true /* single-buffer mode */));
  if (surface) {
    return new AndroidNativeWindowTextureData(surface, aSize, aFormat);
  }

  return nullptr;
}

AndroidNativeWindowTextureData::AndroidNativeWindowTextureData(
    java::GeckoSurface::Param aSurface, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat)
    : mSurface(aSurface), mIsLocked(false), mSize(aSize), mFormat(aFormat) {
  mNativeWindow =
      ANativeWindow_fromSurface(jni::GetEnvForThread(), mSurface.Get());
  MOZ_ASSERT(mNativeWindow, "Failed to create NativeWindow.");

  // SurfaceTextures don't technically support BGR, but we can just pretend to
  // be RGB.
  int32_t format = WINDOW_FORMAT_RGBA_8888;
  switch (aFormat) {
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
      format = WINDOW_FORMAT_RGBA_8888;
      break;

    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::B8G8R8X8:
      format = WINDOW_FORMAT_RGBX_8888;
      break;

    case gfx::SurfaceFormat::R5G6B5_UINT16:
      format = WINDOW_FORMAT_RGB_565;
      break;

    default:
      MOZ_ASSERT(false, "Unsupported AndroidNativeWindowTextureData format.");
  }

  DebugOnly<int32_t> r = ANativeWindow_setBuffersGeometry(
      mNativeWindow, mSize.width, mSize.height, format);
  MOZ_ASSERT(r == 0, "ANativeWindow_setBuffersGeometry failed.");

  // Ideally here we'd call ANativeWindow_setBuffersTransform() with the
  // identity transform, but that is only available on api level >= 26.
  // Instead use SurfaceDescriptor's transformOverride flag when serializing.
}

void AndroidNativeWindowTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = false;
  aInfo.canConcurrentlyReadLock = false;
}

bool AndroidNativeWindowTextureData::Serialize(
    SurfaceDescriptor& aOutDescriptor) {
  aOutDescriptor = SurfaceTextureDescriptor(
      mSurface->GetHandle(), mSize, mFormat, false /* not continuous */,
      Some(gfx::Matrix4x4()) /* always use identity transform */);
  return true;
}

bool AndroidNativeWindowTextureData::Lock(OpenMode) {
  // ANativeWindows can only be locked and unlocked a single time, after which
  // we must wait until they receive ownership back from the host.
  // Therefore we must only actually call ANativeWindow_lock() once per cycle.
  if (!mIsLocked) {
    int32_t r = ANativeWindow_lock(mNativeWindow, &mBuffer, nullptr);
    if (r == -ENOMEM) {
      return false;
    } else if (r < 0) {
      MOZ_CRASH("ANativeWindow_lock failed.");
    }
    mIsLocked = true;
  }
  return true;
}

void AndroidNativeWindowTextureData::Unlock() {
  // The TextureClient may want to call Lock again before handing ownership
  // to the host, so we cannot call ANativeWindow_unlockAndPost yet.
}

void AndroidNativeWindowTextureData::Forget(LayersIPCChannel*) {
  MOZ_ASSERT(!mIsLocked,
             "ANativeWindow should not be released while locked.\n");
  ANativeWindow_release(mNativeWindow);
  mNativeWindow = nullptr;
  java::SurfaceAllocator::DisposeSurface(mSurface);
  mSurface = nullptr;
}

already_AddRefed<gfx::DrawTarget>
AndroidNativeWindowTextureData::BorrowDrawTarget() {
  const int bpp = (mFormat == gfx::SurfaceFormat::R5G6B5_UINT16) ? 2 : 4;

  return gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, static_cast<unsigned char*>(mBuffer.bits),
      gfx::IntSize(mBuffer.width, mBuffer.height), mBuffer.stride * bpp,
      mFormat, true);
}

void AndroidNativeWindowTextureData::OnForwardedToHost() {
  if (mIsLocked) {
    int32_t r = ANativeWindow_unlockAndPost(mNativeWindow);
    if (r < 0) {
      MOZ_CRASH("ANativeWindow_unlockAndPost failed\n.");
    }
    mIsLocked = false;
  }
}

AndroidHardwareBufferTextureData* AndroidHardwareBufferTextureData::Create(
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  RefPtr<AndroidHardwareBuffer> buffer =
      AndroidHardwareBuffer::Create(aSize, aFormat);
  if (!buffer) {
    return nullptr;
  }
  return new AndroidHardwareBufferTextureData(buffer, aSize, aFormat);
}

AndroidHardwareBufferTextureData::AndroidHardwareBufferTextureData(
    AndroidHardwareBuffer* aAndroidHardwareBuffer, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat)
    : mAndroidHardwareBuffer(aAndroidHardwareBuffer),
      mSize(aSize),
      mFormat(aFormat),
      mAddress(nullptr),
      mIsLocked(false) {}

AndroidHardwareBufferTextureData::~AndroidHardwareBufferTextureData() {}

void AndroidHardwareBufferTextureData::FillInfo(
    TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.hasSynchronization = true;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = false;
  aInfo.canConcurrentlyReadLock = true;
}

bool AndroidHardwareBufferTextureData::Serialize(
    SurfaceDescriptor& aOutDescriptor) {
  int fd[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fd) != 0) {
    aOutDescriptor = SurfaceDescriptorAndroidHardwareBuffer(
        ipc::FileDescriptor(), mAndroidHardwareBuffer->mId, mSize, mFormat);
    return false;
  }

  UniqueFileHandle readerFd(fd[0]);
  UniqueFileHandle writerFd(fd[1]);

  // Send the AHardwareBuffer to an AF_UNIX socket. It does not acquire or
  // retain a reference to the buffer object. The caller is therefore
  // responsible for ensuring that the buffer remains alive through the lifetime
  // of this file descriptor.
  int ret = mAndroidHardwareBuffer->SendHandleToUnixSocket(writerFd.get());
  if (ret < 0) {
    aOutDescriptor = SurfaceDescriptorAndroidHardwareBuffer(
        ipc::FileDescriptor(), mAndroidHardwareBuffer->mId, mSize, mFormat);
    return false;
  }

  aOutDescriptor = SurfaceDescriptorAndroidHardwareBuffer(
      ipc::FileDescriptor(std::move(readerFd)), mAndroidHardwareBuffer->mId,
      mSize, mFormat);
  return true;
}

bool AndroidHardwareBufferTextureData::Lock(OpenMode aMode) {
  if (!mIsLocked) {
    MOZ_ASSERT(!mAddress);

    mAndroidHardwareBuffer->WaitForBufferOwnership();

    uint64_t usage = 0;
    if (aMode & OpenMode::OPEN_READ) {
      usage |= AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
    }
    if (aMode & OpenMode::OPEN_WRITE) {
      usage |= AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    }

    int ret = mAndroidHardwareBuffer->Lock(usage, 0, &mAddress);
    if (ret) {
      mAddress = nullptr;
      return false;
    }
    mIsLocked = true;
  }
  return true;
}

void AndroidHardwareBufferTextureData::Unlock() {
  // The TextureClient may want to call Lock again before handing ownership
  // to the host, so we cannot call AHardwareBuffer_unlock yet.
}

void AndroidHardwareBufferTextureData::Forget(LayersIPCChannel*) {
  MOZ_ASSERT(!mIsLocked);
  mAndroidHardwareBuffer = nullptr;
  mAddress = nullptr;
}

already_AddRefed<gfx::DrawTarget>
AndroidHardwareBufferTextureData::BorrowDrawTarget() {
  MOZ_ASSERT(mIsLocked);

  const int bpp = (mFormat == gfx::SurfaceFormat::R5G6B5_UINT16) ? 2 : 4;

  return gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, static_cast<unsigned char*>(mAddress),
      gfx::IntSize(mAndroidHardwareBuffer->mSize.width,
                   mAndroidHardwareBuffer->mSize.height),
      mAndroidHardwareBuffer->mStride * bpp, mFormat, true);
}

void AndroidHardwareBufferTextureData::OnForwardedToHost() {
  if (mIsLocked) {
    mAndroidHardwareBuffer->Unlock();
    mAddress = nullptr;
    mIsLocked = false;
  }
}

TextureFlags AndroidHardwareBufferTextureData::GetTextureFlags() const {
  return TextureFlags::WAIT_HOST_USAGE_END;
}

Maybe<uint64_t> AndroidHardwareBufferTextureData::GetBufferId() const {
  return Some(mAndroidHardwareBuffer->mId);
}

mozilla::ipc::FileDescriptor
AndroidHardwareBufferTextureData::GetAcquireFence() {
  if (!mAndroidHardwareBuffer) {
    return ipc::FileDescriptor();
  }

  return mAndroidHardwareBuffer->GetAcquireFence();
}

#endif  // MOZ_WIDGET_ANDROID

}  // namespace layers
}  // namespace mozilla

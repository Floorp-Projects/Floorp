/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_ANDROID_HARDWARE_BUFFER
#define MOZILLA_LAYERS_ANDROID_HARDWARE_BUFFER

#include <android/hardware_buffer.h>

#include "mozilla/layers/TextureClient.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace layers {

/**
 * AndroidHardwareBufferApi provides apis for managing AHardwareBuffer.
 * The apis are supported since Android O(APIVersion 26).
 */
class AndroidHardwareBufferApi final {
 public:
  static void Init();
  static void Shutdown();

  static AndroidHardwareBufferApi* Get() { return sInstance; }

  void Allocate(const AHardwareBuffer_Desc* aDesc,
                AHardwareBuffer** aOutBuffer);
  void Acquire(AHardwareBuffer* aBuffer);
  void Release(AHardwareBuffer* aBuffer);
  void Describe(const AHardwareBuffer* aBuffer, AHardwareBuffer_Desc* aOutDesc);
  int Lock(AHardwareBuffer* aBuffer, uint64_t aUsage, int32_t aFence,
           const ARect* aRect, void** aOutVirtualAddress);
  int Unlock(AHardwareBuffer* aBuffer, int32_t* aFence);
  int SendHandleToUnixSocket(const AHardwareBuffer* aBuffer, int aSocketFd);
  int RecvHandleFromUnixSocket(int aSocketFd, AHardwareBuffer** aOutBuffer);

 private:
  AndroidHardwareBufferApi();
  bool Load();

  using _AHardwareBuffer_allocate = int (*)(const AHardwareBuffer_Desc* desc,
                                            AHardwareBuffer** outBuffer);
  using _AHardwareBuffer_acquire = void (*)(AHardwareBuffer* buffer);
  using _AHardwareBuffer_release = void (*)(AHardwareBuffer* buffer);
  using _AHardwareBuffer_describe = void (*)(const AHardwareBuffer* buffer,
                                             AHardwareBuffer_Desc* outDesc);
  using _AHardwareBuffer_lock = int (*)(AHardwareBuffer* buffer, uint64_t usage,
                                        int32_t fence, const ARect* rect,
                                        void** outVirtualAddress);
  using _AHardwareBuffer_unlock = int (*)(AHardwareBuffer* buffer,
                                          int32_t* fence);
  using _AHardwareBuffer_sendHandleToUnixSocket =
      int (*)(const AHardwareBuffer* buffer, int socketFd);
  using _AHardwareBuffer_recvHandleFromUnixSocket =
      int (*)(int socketFd, AHardwareBuffer** outBuffer);

  _AHardwareBuffer_allocate mAHardwareBuffer_allocate = nullptr;
  _AHardwareBuffer_acquire mAHardwareBuffer_acquire = nullptr;
  _AHardwareBuffer_release mAHardwareBuffer_release = nullptr;
  _AHardwareBuffer_describe mAHardwareBuffer_describe = nullptr;
  _AHardwareBuffer_lock mAHardwareBuffer_lock = nullptr;
  _AHardwareBuffer_unlock mAHardwareBuffer_unlock = nullptr;
  _AHardwareBuffer_sendHandleToUnixSocket
      mAHardwareBuffer_sendHandleToUnixSocket = nullptr;
  _AHardwareBuffer_recvHandleFromUnixSocket
      mAHardwareBuffer_recvHandleFromUnixSocket = nullptr;

  static StaticAutoPtr<AndroidHardwareBufferApi> sInstance;
};

/**
 * AndroidHardwareBuffer is a wrapper of AHardwareBuffer. AHardwareBuffer wraps
 * android GraphicBuffer. It is supported since Android O(APIVersion 26).
 */
class AndroidHardwareBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AndroidHardwareBuffer)

  static already_AddRefed<AndroidHardwareBuffer> Create(
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  static already_AddRefed<AndroidHardwareBuffer> FromFileDescriptor(
      ipc::FileDescriptor& aFileDescriptor, gfx::IntSize aSize,
      gfx::SurfaceFormat aFormat);

  int Lock(uint64_t aUsage, int32_t aFence, const ARect* aRect,
           void** aOutVirtualAddress);
  int Unlock(int32_t* aFence);

  int SendHandleToUnixSocket(int aSocketFd);

  gfx::IntSize GetSize() const;
  gfx::SurfaceFormat GetSurfaceFormat() const;

  AHardwareBuffer* GetNativeBuffer() const { return mNativeBuffer; }

  const gfx::IntSize mSize;
  const uint32_t mStride;
  const gfx::SurfaceFormat mFormat;

 protected:
  AndroidHardwareBuffer(AHardwareBuffer* aNativeBuffer, gfx::IntSize aSize,
                        uint32_t aStride, gfx::SurfaceFormat aFormat);
  ~AndroidHardwareBuffer();

  AHardwareBuffer* mNativeBuffer;
};

}  // namespace layers
}  // namespace mozilla

#endif

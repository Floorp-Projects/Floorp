/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_ANDROID_HARDWARE_BUFFER
#define MOZILLA_LAYERS_ANDROID_HARDWARE_BUFFER

#include <android/hardware_buffer.h>
#include <atomic>
#include <unordered_map>

#include "mozilla/layers/TextureClient.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"

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
 * The manager is mainly used for release fences delivery from
 * host side to client side.
 */
class AndroidHardwareBuffer
    : public SupportsThreadSafeWeakPtr<AndroidHardwareBuffer> {
 public:
  MOZ_DECLARE_THREADSAFEWEAKREFERENCE_TYPENAME(AndroidHardwareBuffer)
  MOZ_DECLARE_REFCOUNTED_TYPENAME(AndroidHardwareBuffer)

  static already_AddRefed<AndroidHardwareBuffer> Create(
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  // This function creates AndroidHardwareBuffer from FileDescriptor.
  // The fuction is expected to be called on host side. Client side creates
  // the FileDescriptor and it is delivered to host side via ipc.
  static already_AddRefed<AndroidHardwareBuffer> FromFileDescriptor(
      ipc::FileDescriptor& aFileDescriptor, uint64_t aBufferId,
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  virtual ~AndroidHardwareBuffer();

  int Lock(uint64_t aUsage, const ARect* aRect, void** aOutVirtualAddress);
  int Unlock();

  int SendHandleToUnixSocket(int aSocketFd);

  AHardwareBuffer* GetNativeBuffer() const { return mNativeBuffer; }

  // Waits until the buffer is no longer used by host side.
  // Returns false when wait is aborted by timeout.
  bool WaitForBufferOwnership();

  // Returns true when the buffer is still in use by host side.
  bool IsWaitingForBufferOwnership();

  void SetAcquireFence(ipc::FileDescriptor&& aFenceFd);

  void SetReleaseFence(ipc::FileDescriptor&& aFenceFd);

  ipc::FileDescriptor GetAndResetReleaseFence();

  ipc::FileDescriptor GetAndResetAcquireFence();

  ipc::FileDescriptor GetAcquireFence();

  RefPtr<TextureClient> GetTextureClientOfSharedSurfaceTextureData(
      const layers::SurfaceDescriptor& aDesc, const gfx::SurfaceFormat aFormat,
      const gfx::IntSize& aSize, const TextureFlags aFlags,
      LayersIPCChannel* aAllocator);

  const gfx::IntSize mSize;
  const uint32_t mStride;
  const gfx::SurfaceFormat mFormat;
  const uint64_t mId;

 protected:
  AndroidHardwareBuffer(AHardwareBuffer* aNativeBuffer, gfx::IntSize aSize,
                        uint32_t aStride, gfx::SurfaceFormat aFormat,
                        uint64_t aId);

  void SetLastFwdTransactionId(uint64_t aFwdTransactionId,
                               bool aUsesImageBridge,
                               const MonitorAutoLock& aAutoLock);

  uint64_t GetLastFwdTransactionId(const MonitorAutoLock& aAutoLock);

  void SetReleaseFence(ipc::FileDescriptor&& aFenceFd,
                       const MonitorAutoLock& aAutoLock);

  struct FwdTransactionId {
    FwdTransactionId(uint64_t aFwdTransactionId, bool aUsesImageBridge)
        : mId(aFwdTransactionId), mUsesImageBridge(aUsesImageBridge) {}
    uint64_t mId;
    bool mUsesImageBridge;
  };

  AHardwareBuffer* mNativeBuffer;

  // When true, AndroidHardwareBuffer is registered to
  // AndroidHardwareBufferManager.
  bool mIsRegistered;

  // protected by AndroidHardwareBufferManager::mMonitor

  Maybe<FwdTransactionId> mTransactionId;

  // FileDescriptor of release fence.
  // Release fence is a fence that is used for waiting until usage/composite of
  // AHardwareBuffer is ended. The fence is delivered via ImageBridge.
  ipc::FileDescriptor mReleaseFenceFd;

  // FileDescriptor of acquire fence.
  // Acquire fence is a fence that is used for waiting until rendering to
  // its AHardwareBuffer is completed.
  ipc::FileDescriptor mAcquireFenceFd;

  // Only TextureClient of SharedSurfaceTextureData could be here.
  // SharedSurfaceTextureData does not own AndroidHardwareBuffer,
  // then it does not affect to a lifetime of AndroidHardwareBuffer.
  // It is used for reducing SharedSurfaceTextureData re-creation to
  // avoid re-allocating file descriptor by
  // SharedSurfaceTextureData::Serialize().
  RefPtr<TextureClient> mTextureClientOfSharedSurfaceTextureData;

  static uint64_t GetNextId();

  friend class AndroidHardwareBufferManager;
};

/**
 * AndroidHardwareBufferManager manages AndroidHardwareBuffers that is
 * allocated by client side.
 * Host side only uses mMonitor for thread safety of AndroidHardwareBuffer.
 */
class AndroidHardwareBufferManager {
 public:
  static void Init();
  static void Shutdown();

  AndroidHardwareBufferManager();

  static AndroidHardwareBufferManager* Get() { return sInstance; }

  void Register(RefPtr<AndroidHardwareBuffer> aBuffer);

  void Unregister(AndroidHardwareBuffer* aBuffer);

  already_AddRefed<AndroidHardwareBuffer> GetBuffer(uint64_t aBufferId);

  bool WaitForBufferOwnership(AndroidHardwareBuffer* aBuffer);

  bool IsWaitingForBufferOwnership(AndroidHardwareBuffer* aBuffer);

  void HoldUntilNotifyNotUsed(uint64_t aBufferId, uint64_t aFwdTransactionId,
                              bool aUsesImageBridge);

  void NotifyNotUsed(ipc::FileDescriptor&& aFenceFd, uint64_t aBufferId,
                     uint64_t aTransactionId, bool aUsesImageBridge);

  Monitor& GetMonitor() { return mMonitor; }

 private:
  Monitor mMonitor;
  std::unordered_map<uint64_t, ThreadSafeWeakPtr<AndroidHardwareBuffer>>
      mBuffers;

  /**
   * Hold AndroidHardwareBuffers that are used by host side via
   * CompositorBridgeChild and ImageBridgeChild until end of their usages
   * on host side.
   */
  std::unordered_map<uint64_t, RefPtr<AndroidHardwareBuffer>>
      mWaitingNotifyNotUsed;

  static StaticAutoPtr<AndroidHardwareBufferManager> sInstance;
};

}  // namespace layers
}  // namespace mozilla

#endif

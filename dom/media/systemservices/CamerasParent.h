/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasParent_h
#define mozilla_CamerasParent_h

#include "CamerasChild.h"
#include "VideoEngine.h"
#include "mozilla/Atomics.h"
#include "mozilla/camera/PCamerasParent.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ShmemPool.h"
#include "api/video/video_sink_interface.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_defines.h"
#include "video/render/incoming_video_stream.h"

class WebrtcLogSinkHandle;
class nsIThread;

namespace mozilla {
class VideoCaptureFactory;
}

namespace mozilla::camera {

class CamerasParent;

class CallbackHelper : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  CallbackHelper(CaptureEngine aCapEng, uint32_t aStreamId,
                 CamerasParent* aParent)
      : mCapEngine(aCapEng),
        mStreamId(aStreamId),
        mTrackingId(CaptureEngineToTrackingSourceStr(aCapEng), aStreamId),
        mParent(aParent){};

  // These callbacks end up running on the VideoCapture thread.
  // From  VideoCaptureCallback
  void OnFrame(const webrtc::VideoFrame& aVideoFrame) override;

  friend CamerasParent;

 private:
  const CaptureEngine mCapEngine;
  const uint32_t mStreamId;
  const TrackingId mTrackingId;
  CamerasParent* const mParent;
};

class DeliverFrameRunnable;

class CamerasParent final : public PCamerasParent,
                            private webrtc::VideoInputFeedBack {
 public:
  using ShutdownMozPromise = media::ShutdownBlockingTicket::ShutdownMozPromise;

  using CameraAccessRequestPromise = MozPromise<CamerasAccessStatus, void_t,
                                                /* IsExclusive = */ false>;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_EVENT_TARGET(
      CamerasParent, mPBackgroundEventTarget)

  class VideoEngineArray;
  friend DeliverFrameRunnable;

  static already_AddRefed<CamerasParent> Create();

  /**
   * Request camera access
   *   Currently only used on desktop. If @value
   *   aAllowPermissionRequest is true, a request for full camera access may be
   *   made and the returned promise may be blocked on user input on a modal
   *   dialog. If @value aAllowPermissionRequest is false, only a request to
   *   check camera device presence will be made. If any camera device is
   *   present, we will enumerate a single placeholder device until a successful
   *   RequestCameraAccess with a true aAllowPermissionRequest.
   *   The returned promise will never be rejected.
   */
  static RefPtr<CameraAccessRequestPromise> RequestCameraAccess(
      bool aAllowPermissionRequest);

  // Messages received from the child. These run on the IPC/PBackground thread.
  mozilla::ipc::IPCResult RecvPCamerasConstructor();
  mozilla::ipc::IPCResult RecvAllocateCapture(
      const CaptureEngine& aCapEngine, const nsACString& aUniqueIdUTF8,
      const uint64_t& aWindowID) override;
  mozilla::ipc::IPCResult RecvReleaseCapture(const CaptureEngine& aCapEngine,
                                             const int& aCaptureId) override;
  mozilla::ipc::IPCResult RecvNumberOfCaptureDevices(
      const CaptureEngine& aCapEngine) override;
  mozilla::ipc::IPCResult RecvNumberOfCapabilities(
      const CaptureEngine& aCapEngine, const nsACString& aUniqueId) override;
  mozilla::ipc::IPCResult RecvGetCaptureCapability(
      const CaptureEngine& aCapEngine, const nsACString& aUniqueId,
      const int& aIndex) override;
  mozilla::ipc::IPCResult RecvGetCaptureDevice(
      const CaptureEngine& aCapEngine, const int& aDeviceIndex) override;
  mozilla::ipc::IPCResult RecvStartCapture(
      const CaptureEngine& aCapEngine, const int& aCaptureId,
      const VideoCaptureCapability& aIpcCaps) override;
  mozilla::ipc::IPCResult RecvFocusOnSelectedSource(
      const CaptureEngine& aCapEngine, const int& aCaptureId) override;
  mozilla::ipc::IPCResult RecvStopCapture(const CaptureEngine& aCapEngine,
                                          const int& aCaptureId) override;
  mozilla::ipc::IPCResult RecvReleaseFrame(
      mozilla::ipc::Shmem&& aShmem) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvEnsureInitialized(
      const CaptureEngine& aCapEngine) override;

  nsIEventTarget* GetBackgroundEventTarget() {
    return mPBackgroundEventTarget;
  };
  bool IsShuttingDown() {
    // the first 2 are pBackground only, the last is atomic
    MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
    return mDestroyed;
  };
  ShmemBuffer GetBuffer(size_t aSize);

  // helper to forward to the PBackground thread
  int DeliverFrameOverIPC(CaptureEngine aCapEngine, uint32_t aStreamId,
                          const TrackingId& aTrackingId, ShmemBuffer aBuffer,
                          unsigned char* aAltBuffer,
                          const VideoFrameProperties& aProps);

  CamerasParent();

 private:
  virtual ~CamerasParent();

  // We use these helpers for shutdown and for the respective IPC commands.
  void StopCapture(const CaptureEngine& aCapEngine, int aCaptureId);
  int ReleaseCapture(const CaptureEngine& aCapEngine, int aCaptureId);

  // VideoInputFeedBack
  void OnDeviceChange() override;

  VideoEngine* EnsureInitialized(int aEngine);

  // Stops any ongoing capturing and releases resources. Called on
  // mVideoCaptureThread. Idempotent.
  void CloseEngines();

  void OnShutdown();

  nsTArray<CallbackHelper*> mCallbacks;
  // If existent, blocks xpcom shutdown while alive.
  // Note that this makes a reference cycle that gets broken in ActorDestroy().
  const UniquePtr<media::ShutdownBlockingTicket> mShutdownBlocker;
  // Tracks the mShutdownBlocker shutdown handler. mPBackgroundEventTarget only.
  MozPromiseRequestHolder<ShutdownMozPromise> mShutdownRequest;

  // Local copy of sVideoCaptureThread. Guaranteed alive if non-null.
  const nsCOMPtr<nsISerialEventTarget> mVideoCaptureThread;

  // Reference to same VideoEngineArray as sEngines. Video capture thread only.
  const RefPtr<VideoEngineArray> mEngines;

  // Reference to same VideoCaptureFactory as sVideoCaptureFactory. Video
  // capture thread only.
  const RefPtr<VideoCaptureFactory> mVideoCaptureFactory;

  // image buffers
  ShmemPool mShmemPool;

  // PBackgroundParent thread
  const nsCOMPtr<nsISerialEventTarget> mPBackgroundEventTarget;

  // Set to true in ActorDestroy. PBackground only.
  bool mDestroyed;

  std::map<nsCString, std::map<uint32_t, webrtc::VideoCaptureCapability>>
      mAllCandidateCapabilities;

  // While alive, ensure webrtc logging is hooked up to MOZ_LOG. Main thread
  // only.
  nsMainThreadPtrHandle<WebrtcLogSinkHandle> mLogHandle;
};

}  // namespace mozilla::camera

#endif  // mozilla_CameraParent_h

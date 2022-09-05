/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasParent_h
#define mozilla_CamerasParent_h

#include "VideoEngine.h"
#include "mozilla/camera/PCamerasParent.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ShmemPool.h"
#include "mozilla/Atomics.h"
#include "api/video/video_sink_interface.h"
#include "common_video/include/incoming_video_stream.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_defines.h"

#include "CamerasChild.h"

#include "base/thread.h"

namespace mozilla::camera {

class CamerasParent;

class CallbackHelper : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  CallbackHelper(CaptureEngine aCapEng, uint32_t aStreamId,
                 CamerasParent* aParent)
      : mCapEngine(aCapEng), mStreamId(aStreamId), mParent(aParent){};

  // These callbacks end up running on the VideoCapture thread.
  // From  VideoCaptureCallback
  void OnFrame(const webrtc::VideoFrame& videoFrame) override;

  friend CamerasParent;

 private:
  CaptureEngine mCapEngine;
  uint32_t mStreamId;
  CamerasParent* mParent;
};

class InputObserver : public webrtc::VideoInputFeedBack {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InputObserver)

  explicit InputObserver(CamerasParent* aParent) : mParent(aParent){};

  virtual void OnDeviceChange() override;

  friend CamerasParent;

 private:
  ~InputObserver() = default;

  const RefPtr<CamerasParent> mParent;
};

class DeliverFrameRunnable;

class CamerasParent final : public PCamerasParent,
                            public nsIAsyncShutdownBlocker {
  NS_DECL_THREADSAFE_ISUPPORTS

 public:
  friend DeliverFrameRunnable;

  static already_AddRefed<CamerasParent> Create();

  // Messages received from the child. These run on the IPC/PBackground thread.
  mozilla::ipc::IPCResult RecvPCamerasConstructor();
  mozilla::ipc::IPCResult RecvAllocateCapture(
      const CaptureEngine& aEngine, const nsACString& aUnique_idUTF8,
      const uint64_t& aWindowID) override;
  mozilla::ipc::IPCResult RecvReleaseCapture(const CaptureEngine&,
                                             const int&) override;
  mozilla::ipc::IPCResult RecvNumberOfCaptureDevices(
      const CaptureEngine&) override;
  mozilla::ipc::IPCResult RecvNumberOfCapabilities(const CaptureEngine&,
                                                   const nsACString&) override;
  mozilla::ipc::IPCResult RecvGetCaptureCapability(const CaptureEngine&,
                                                   const nsACString&,
                                                   const int&) override;
  mozilla::ipc::IPCResult RecvGetCaptureDevice(const CaptureEngine&,
                                               const int&) override;
  mozilla::ipc::IPCResult RecvStartCapture(
      const CaptureEngine&, const int&, const VideoCaptureCapability&) override;
  mozilla::ipc::IPCResult RecvFocusOnSelectedSource(const CaptureEngine&,
                                                    const int&) override;
  mozilla::ipc::IPCResult RecvStopCapture(const CaptureEngine&,
                                          const int&) override;
  mozilla::ipc::IPCResult RecvReleaseFrame(mozilla::ipc::Shmem&&) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvEnsureInitialized(const CaptureEngine&) override;

  nsIEventTarget* GetBackgroundEventTarget() {
    return mPBackgroundEventTarget;
  };
  bool IsShuttingDown() {
    // the first 2 are pBackground only, the last is atomic
    MOZ_ASSERT(GetCurrentSerialEventTarget() == mPBackgroundEventTarget);
    return !mChildIsAlive || mDestroyed || !mWebRTCAlive;
  };
  ShmemBuffer GetBuffer(size_t aSize);

  // helper to forward to the PBackground thread
  int DeliverFrameOverIPC(CaptureEngine capEng, uint32_t aStreamId,
                          ShmemBuffer buffer, unsigned char* altbuffer,
                          VideoFrameProperties& aProps);

  CamerasParent();

 protected:
  virtual ~CamerasParent();

  // We use these helpers for shutdown and for the respective IPC commands.
  void StopCapture(const CaptureEngine& aCapEngine, int aCaptureId);
  int ReleaseCapture(const CaptureEngine& aCapEngine, int aCaptureId);

  bool SetupEngine(CaptureEngine aCapEngine);
  VideoEngine* EnsureInitialized(int aEngine);
  void CloseEngines();
  void StopIPC();
  void StopVideoCapture();
  nsresult DispatchToVideoCaptureThread(RefPtr<Runnable> event);
  NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override;
  NS_IMETHOD GetName(nsAString& aName) override {
    aName = mName;
    return NS_OK;
  }
  NS_IMETHOD GetState(nsIPropertyBag**) override { return NS_OK; }
  static nsString GetNewName();

  // sEngines will be accessed by VideoCapture thread only
  // sNumOfCamerasParents, sNumOfOpenCamerasParentEngines, and
  // sVideoCaptureThread will be accessed by main thread / PBackground thread /
  // VideoCapture thread
  // sNumOfCamerasParents and sThreadMonitor create & delete are protected by
  // sMutex
  // sNumOfOpenCamerasParentEngines and sVideoCaptureThread are protected by
  // sThreadMonitor
  static StaticRefPtr<VideoEngine> sEngines[CaptureEngine::MaxEngine];
  // Number of CamerasParents for which mWebRTCAlive is true.
  static int32_t sNumOfOpenCamerasParentEngines;
  static int32_t sNumOfCamerasParents;
  static StaticMutex sMutex;
  static Monitor* sThreadMonitor;
  // video processing thread - where webrtc.org capturer code runs
  static base::Thread* sVideoCaptureThread;

  nsTArray<CallbackHelper*> mCallbacks;
  nsString mName;

  // image buffers
  ShmemPool mShmemPool;

  // PBackgroundParent thread
  const nsCOMPtr<nsISerialEventTarget> mPBackgroundEventTarget;

  // Shutdown handling
  bool mChildIsAlive;
  bool mDestroyed;
  // Above 2 are PBackground only, but this is potentially
  // read cross-thread.
  Atomic<bool> mWebRTCAlive;
  RefPtr<InputObserver> mCameraObserver;
  std::map<nsCString, std::map<uint32_t, webrtc::VideoCaptureCapability>>
      mAllCandidateCapabilities;
};

PCamerasParent* CreateCamerasParent();

}  // namespace mozilla::camera

#endif  // mozilla_CameraParent_h

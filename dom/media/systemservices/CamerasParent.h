/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasParent_h
#define mozilla_CamerasParent_h

#include "nsIObserver.h"
#include "VideoEngine.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/camera/PCamerasParent.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ShmemPool.h"
#include "mozilla/Atomics.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_capture/video_capture_defines.h"
#include "webrtc/common_video/include/incoming_video_stream.h"
#include "webrtc/media/base/videosinkinterface.h"

// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/common_types.h"

#include "CamerasChild.h"

#include "base/thread.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}

namespace camera {

class CamerasParent;

class CallbackHelper :
  public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
  CallbackHelper(CaptureEngine aCapEng, uint32_t aStreamId, CamerasParent *aParent)
    : mCapEngine(aCapEng), mStreamId(aStreamId), mParent(aParent) {};

  // These callbacks end up running on the VideoCapture thread.
  // From  VideoCaptureCallback
  void OnFrame(const webrtc::VideoFrame& videoFrame) override;

  friend CamerasParent;

private:
  CaptureEngine mCapEngine;
  uint32_t mStreamId;
  CamerasParent *mParent;
};

class InputObserver :  public webrtc::VideoInputFeedBack
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InputObserver)

  explicit InputObserver(CamerasParent* aParent)
    : mParent(aParent) {};

  virtual void OnDeviceChange() override;

  friend CamerasParent;

private:
  ~InputObserver() {}

  RefPtr<CamerasParent> mParent;
};

class CamerasParent final
  : public PCamerasParent
  , public nsIObserver
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

public:
  static already_AddRefed<CamerasParent> Create();

  // Messages received form the child. These run on the IPC/PBackground thread.
  mozilla::ipc::IPCResult
  RecvAllocateCaptureDevice(const CaptureEngine& aEngine,
                            const nsCString& aUnique_idUTF8,
                            const ipc::PrincipalInfo& aPrincipalInfo) override;
  mozilla::ipc::IPCResult RecvReleaseCaptureDevice(const CaptureEngine&,
                                                   const int&) override;
  mozilla::ipc::IPCResult RecvNumberOfCaptureDevices(const CaptureEngine&) override;
  mozilla::ipc::IPCResult RecvNumberOfCapabilities(const CaptureEngine&,
                                                   const nsCString&) override;
  mozilla::ipc::IPCResult RecvGetCaptureCapability(const CaptureEngine&, const nsCString&,
                                                   const int&) override;
  mozilla::ipc::IPCResult RecvGetCaptureDevice(const CaptureEngine&, const int&) override;
  mozilla::ipc::IPCResult RecvStartCapture(const CaptureEngine&, const int&,
                                           const VideoCaptureCapability&) override;
  mozilla::ipc::IPCResult RecvFocusOnSelectedSource(const CaptureEngine&,
                                                    const int&) override;
  mozilla::ipc::IPCResult RecvStopCapture(const CaptureEngine&, const int&) override;
  mozilla::ipc::IPCResult RecvReleaseFrame(mozilla::ipc::Shmem&&) override;
  mozilla::ipc::IPCResult RecvAllDone() override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvEnsureInitialized(const CaptureEngine&) override;

  nsIEventTarget* GetBackgroundEventTarget() { return mPBackgroundEventTarget; };
  bool IsShuttingDown()
  {
    return !mChildIsAlive || mDestroyed || !mWebRTCAlive;
  };
  ShmemBuffer GetBuffer(size_t aSize);

  // helper to forward to the PBackground thread
  int DeliverFrameOverIPC(CaptureEngine capEng,
                          uint32_t aStreamId,
                          ShmemBuffer buffer,
                          unsigned char* altbuffer,
                          VideoFrameProperties& aProps);


  CamerasParent();

protected:
  virtual ~CamerasParent();

  // We use these helpers for shutdown and for the respective IPC commands.
  void StopCapture(const CaptureEngine& aCapEngine, const int& capnum);
  int ReleaseCaptureDevice(const CaptureEngine& aCapEngine, const int& capnum);

  bool SetupEngine(CaptureEngine aCapEngine);
  VideoEngine* EnsureInitialized(int aEngine);
  void CloseEngines();
  void StopIPC();
  void StopVideoCapture();
  // Can't take already_AddRefed because it can fail in stupid ways.
  nsresult DispatchToVideoCaptureThread(Runnable* event);

  // sEngines will be accessed by VideoCapture thread only
  // sNumOfCamerasParent, sNumOfOpenCamerasParentEngines, and sVideoCaptureThread will
  // be accessed by main thread / PBackground thread / VideoCapture thread
  // sNumOfCamerasParent and sThreadMonitor create & delete are protected by sMutex
  // sNumOfOpenCamerasParentEngines and sVideoCaptureThread are protected by sThreadMonitor
  static StaticRefPtr<VideoEngine> sEngines[CaptureEngine::MaxEngine];
  static int32_t sNumOfOpenCamerasParentEngines;
  static int32_t sNumOfCamerasParents;
  nsTArray<CallbackHelper*> mCallbacks;

  // image buffers
  ShmemPool mShmemPool;

  // PBackground parent thread
  nsCOMPtr<nsISerialEventTarget> mPBackgroundEventTarget;

  static StaticMutex sMutex;
  static Monitor* sThreadMonitor;

  // video processing thread - where webrtc.org capturer code runs
  static base::Thread* sVideoCaptureThread;

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

} // namespace camera
} // namespace mozilla

#endif  // mozilla_CameraParent_h

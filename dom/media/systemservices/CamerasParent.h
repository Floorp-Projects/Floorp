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
  virtual void OnFrame(const webrtc::VideoFrame& videoFrame) override;

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
  virtual void OnDeviceChange();

  friend CamerasParent;

private:
  ~InputObserver() {}

  RefPtr<CamerasParent> mParent;
};

class CamerasParent :  public PCamerasParent,
                       public nsIObserver
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

public:
  static already_AddRefed<CamerasParent> Create();

  // Messages received form the child. These run on the IPC/PBackground thread.
  virtual mozilla::ipc::IPCResult
  RecvAllocateCaptureDevice(const CaptureEngine& aEngine,
                            const nsCString& aUnique_idUTF8,
                            const ipc::PrincipalInfo& aPrincipalInfo) override;
  virtual mozilla::ipc::IPCResult RecvReleaseCaptureDevice(const CaptureEngine&,
                                                           const int&) override;
  virtual mozilla::ipc::IPCResult RecvNumberOfCaptureDevices(const CaptureEngine&) override;
  virtual mozilla::ipc::IPCResult RecvNumberOfCapabilities(const CaptureEngine&,
                                                           const nsCString&) override;
  virtual mozilla::ipc::IPCResult RecvGetCaptureCapability(const CaptureEngine&, const nsCString&,
                                                           const int&) override;
  virtual mozilla::ipc::IPCResult RecvGetCaptureDevice(const CaptureEngine&, const int&) override;
  virtual mozilla::ipc::IPCResult RecvStartCapture(const CaptureEngine&, const int&,
                                                   const VideoCaptureCapability&) override;
  virtual mozilla::ipc::IPCResult RecvStopCapture(const CaptureEngine&, const int&) override;
  virtual mozilla::ipc::IPCResult RecvReleaseFrame(mozilla::ipc::Shmem&&) override;
  virtual mozilla::ipc::IPCResult RecvAllDone() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult RecvEnsureInitialized(const CaptureEngine&) override;

  nsIEventTarget* GetBackgroundEventTarget() { return mPBackgroundEventTarget; };
  bool IsShuttingDown() { return !mChildIsAlive
                              ||  mDestroyed
                              || !mWebRTCAlive; };
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

  RefPtr<VideoEngine> mEngines[CaptureEngine::MaxEngine];
  nsTArray<CallbackHelper*> mCallbacks;

  // image buffers
  mozilla::ShmemPool mShmemPool;

  // PBackground parent thread
  nsCOMPtr<nsISerialEventTarget> mPBackgroundEventTarget;

  // Monitors creation of the thread below
  Monitor mThreadMonitor;

  // video processing thread - where webrtc.org capturer code runs
  base::Thread* mVideoCaptureThread;

  // Shutdown handling
  bool mChildIsAlive;
  bool mDestroyed;
  // Above 2 are PBackground only, but this is potentially
  // read cross-thread.
  mozilla::Atomic<bool> mWebRTCAlive;
  nsTArray<RefPtr<InputObserver>> mObservers;
};

PCamerasParent* CreateCamerasParent();

} // namespace camera
} // namespace mozilla

#endif  // mozilla_CameraParent_h

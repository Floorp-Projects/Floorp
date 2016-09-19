/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasParent_h
#define mozilla_CamerasParent_h

#include "nsIObserver.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/camera/PCamerasParent.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ShmemPool.h"
#include "mozilla/Atomics.h"

// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/common.h"
// Video Engine
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "CamerasChild.h"

#include "base/thread.h"

namespace mozilla {
namespace camera {

class CamerasParent;

class CallbackHelper : public webrtc::ExternalRenderer
{
public:
  CallbackHelper(CaptureEngine aCapEng, int aCapId, CamerasParent *aParent)
    : mCapEngine(aCapEng), mCapturerId(aCapId), mParent(aParent) {};

  // ViEExternalRenderer implementation. These callbacks end up
  // running on the VideoCapture thread.
  virtual int FrameSizeChange(unsigned int w, unsigned int h,
                              unsigned int streams) override;
  virtual int DeliverFrame(unsigned char* buffer,
                           size_t size,
                           uint32_t time_stamp,
                           int64_t ntp_time,
                           int64_t render_time,
                           void *handle) override;
  virtual int DeliverI420Frame(const webrtc::I420VideoFrame& webrtc_frame) override;
  virtual bool IsTextureSupported() override { return false; };

  friend CamerasParent;

private:
  CaptureEngine mCapEngine;
  int mCapturerId;
  CamerasParent *mParent;
};

class EngineHelper
{
public:
  EngineHelper() :
    mEngine(nullptr), mPtrViEBase(nullptr), mPtrViECapture(nullptr),
    mPtrViERender(nullptr), mEngineIsRunning(false) {};

  webrtc::VideoEngine *mEngine;
  webrtc::ViEBase *mPtrViEBase;
  webrtc::ViECapture *mPtrViECapture;
  webrtc::ViERender *mPtrViERender;

  // The webrtc code keeps a reference to this one.
  webrtc::Config mConfig;

  // Engine alive
  bool mEngineIsRunning;
};

class InputObserver :  public webrtc::ViEInputObserver
{
public:
  explicit InputObserver(CamerasParent* aParent)
    : mParent(aParent) {};
  virtual void DeviceChange();

  friend CamerasParent;

private:
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
  virtual bool RecvAllocateCaptureDevice(const CaptureEngine&, const nsCString&,
                                         const nsCString&) override;
  virtual bool RecvReleaseCaptureDevice(const CaptureEngine&,
                                        const int&) override;
  virtual bool RecvNumberOfCaptureDevices(const CaptureEngine&) override;
  virtual bool RecvNumberOfCapabilities(const CaptureEngine&,
                                        const nsCString&) override;
  virtual bool RecvGetCaptureCapability(const CaptureEngine&, const nsCString&,
                                        const int&) override;
  virtual bool RecvGetCaptureDevice(const CaptureEngine&, const int&) override;
  virtual bool RecvStartCapture(const CaptureEngine&, const int&,
                                const CaptureCapability&) override;
  virtual bool RecvStopCapture(const CaptureEngine&, const int&) override;
  virtual bool RecvReleaseFrame(mozilla::ipc::Shmem&&) override;
  virtual bool RecvAllDone() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  nsIThread* GetBackgroundThread() { return mPBackgroundThread; };
  bool IsShuttingDown() { return !mChildIsAlive
                              ||  mDestroyed
                              || !mWebRTCAlive; };
  ShmemBuffer GetBuffer(size_t aSize);

  // helper to forward to the PBackground thread
  int DeliverFrameOverIPC(CaptureEngine capEng,
                          int cap_id,
                          ShmemBuffer buffer,
                          unsigned char* altbuffer,
                          size_t size,
                          uint32_t time_stamp,
                          int64_t ntp_time,
                          int64_t render_time);


  CamerasParent();

protected:
  virtual ~CamerasParent();

  // We use these helpers for shutdown and for the respective IPC commands.
  void StopCapture(const CaptureEngine& aCapEngine, const int& capnum);
  int ReleaseCaptureDevice(const CaptureEngine& aCapEngine, const int& capnum);

  bool SetupEngine(CaptureEngine aCapEngine);
  bool EnsureInitialized(int aEngine);
  void CloseEngines();
  void StopIPC();
  void StopVideoCapture();
  // Can't take already_AddRefed because it can fail in stupid ways.
  nsresult DispatchToVideoCaptureThread(Runnable* event);

  EngineHelper mEngines[CaptureEngine::MaxEngine];
  nsTArray<CallbackHelper*> mCallbacks;

  // image buffers
  mozilla::ShmemPool mShmemPool;

  // PBackground parent thread
  nsCOMPtr<nsIThread> mPBackgroundThread;

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
  nsTArray<InputObserver*> mObservers;
};

PCamerasParent* CreateCamerasParent();

} // namespace camera
} // namespace mozilla

#endif  // mozilla_CameraParent_h

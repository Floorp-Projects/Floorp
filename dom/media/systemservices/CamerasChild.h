/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasChild_h
#define mozilla_CamerasChild_h

#include "mozilla/Pair.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/camera/PCamerasChild.h"
#include "mozilla/camera/PCamerasParent.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"

// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/common.h"
// Video Engine
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_render.h"

namespace mozilla {

namespace ipc {
class BackgroundChildImpl;
}

namespace camera {

enum CaptureEngine : int {
  InvalidEngine = 0,
  ScreenEngine,
  BrowserEngine,
  WinEngine,
  AppEngine,
  CameraEngine,
  MaxEngine
};

struct CapturerElement {
  CaptureEngine engine;
  int id;
  webrtc::ExternalRenderer* callback;
};

// statically mirror webrtc.org ViECapture API
// these are called via MediaManager->MediaEngineRemoteVideoSource
// on the MediaManager thread
int NumberOfCapabilities(CaptureEngine aCapEngine,
                         const char* deviceUniqueIdUTF8);
int GetCaptureCapability(CaptureEngine aCapEngine,
                         const char* unique_idUTF8,
                         const unsigned int capability_number,
                         webrtc::CaptureCapability& capability);
int NumberOfCaptureDevices(CaptureEngine aCapEngine);
int GetCaptureDevice(CaptureEngine aCapEngine,
                     unsigned int list_number, char* device_nameUTF8,
                     const unsigned int device_nameUTF8Length,
                     char* unique_idUTF8,
                     const unsigned int unique_idUTF8Length);
int AllocateCaptureDevice(CaptureEngine aCapEngine,
                          const char* unique_idUTF8,
                          const unsigned int unique_idUTF8Length,
                          int& capture_id);
int ReleaseCaptureDevice(CaptureEngine aCapEngine,
                         const int capture_id);
int StartCapture(CaptureEngine aCapEngine,
                 const int capture_id, webrtc::CaptureCapability& capability,
                 webrtc::ExternalRenderer* func);
int StopCapture(CaptureEngine aCapEngine, const int capture_id);
void Shutdown(void);

class CamerasChild final : public PCamerasChild
{
  friend class mozilla::ipc::BackgroundChildImpl;

public:
  // We are owned by the PBackground thread only. CamerasSingleton
  // takes a non-owning reference.
  NS_INLINE_DECL_REFCOUNTING(CamerasChild)

  // IPC messages recevied, received on the PBackground thread
  // these are the actual callbacks with data
  virtual bool RecvDeliverFrame(const int&, const int&, mozilla::ipc::Shmem&&,
                                const int&, const uint32_t&, const int64_t&,
                                const int64_t&) override;
  virtual bool RecvFrameSizeChange(const int&, const int&,
                                   const int& w, const int& h) override;

  // these are response messages to our outgoing requests
  virtual bool RecvReplyNumberOfCaptureDevices(const int&) override;
  virtual bool RecvReplyNumberOfCapabilities(const int&) override;
  virtual bool RecvReplyAllocateCaptureDevice(const int&) override;
  virtual bool RecvReplyGetCaptureCapability(const CaptureCapability& capability) override;
  virtual bool RecvReplyGetCaptureDevice(const nsCString& device_name,
                                         const nsCString& device_id) override;
  virtual bool RecvReplyFailure(void) override;
  virtual bool RecvReplySuccess(void) override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // the webrtc.org ViECapture calls are mirrored here, but with access
  // to a specific PCameras instance to communicate over. These also
  // run on the MediaManager thread
  int NumberOfCaptureDevices(CaptureEngine aCapEngine);
  int NumberOfCapabilities(CaptureEngine aCapEngine,
                           const char* deviceUniqueIdUTF8);
  int ReleaseCaptureDevice(CaptureEngine aCapEngine,
                           const int capture_id);
  int StartCapture(CaptureEngine aCapEngine,
                   const int capture_id, webrtc::CaptureCapability& capability,
                   webrtc::ExternalRenderer* func);
  int StopCapture(CaptureEngine aCapEngine, const int capture_id);
  int AllocateCaptureDevice(CaptureEngine aCapEngine,
                            const char* unique_idUTF8,
                            const unsigned int unique_idUTF8Length,
                            int& capture_id);
  int GetCaptureCapability(CaptureEngine aCapEngine,
                           const char* unique_idUTF8,
                           const unsigned int capability_number,
                           webrtc::CaptureCapability& capability);
  int GetCaptureDevice(CaptureEngine aCapEngine,
                       unsigned int list_number, char* device_nameUTF8,
                       const unsigned int device_nameUTF8Length,
                       char* unique_idUTF8,
                       const unsigned int unique_idUTF8Length);
  void Shutdown();

  webrtc::ExternalRenderer* Callback(CaptureEngine aCapEngine, int capture_id);
  void AddCallback(const CaptureEngine aCapEngine, const int capture_id,
                   webrtc::ExternalRenderer* render);
  void RemoveCallback(const CaptureEngine aCapEngine, const int capture_id);


private:
  CamerasChild();
  ~CamerasChild();
  // Dispatch a Runnable to the PCamerasParent, by executing it on the
  // decidecated Cameras IPC/PBackground thread.
  bool DispatchToParent(nsIRunnable* aRunnable,
                        MonitorAutoLock& aMonitor);

  nsTArray<CapturerElement> mCallbacks;
  // Protects the callback arrays
  Mutex mCallbackMutex;

  bool mIPCIsAlive;

  // Hold to prevent multiple outstanding requests. We don't use
  // request IDs so we only support one at a time. Don't want try
  // to use the webrtc.org API from multiple threads simultanously.
  // The monitor below isn't sufficient for this, as it will drop
  // the lock when Wait-ing for a response, allowing us to send a new
  // request. The Notify on receiving the response will then unblock
  // both waiters and one will be guaranteed to get the wrong result.
  // Take this one before taking mReplyMonitor.
  Mutex mRequestMutex;
  // Hold to wait for an async response to our calls
  Monitor mReplyMonitor;
  // Async resposne valid?
  bool mReceivedReply;
  // Aynsc reponses data contents;
  bool mReplySuccess;
  int mReplyInteger;
  webrtc::CaptureCapability mReplyCapability;
  nsCString mReplyDeviceName;
  nsCString mReplyDeviceID;
};

} // namespace camera
} // namespace mozilla

#endif  // mozilla_CamerasChild_h

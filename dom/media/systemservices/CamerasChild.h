/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasChild_h
#define mozilla_CamerasChild_h

#include <utility>

#include "MediaEventSource.h"
#include "mozilla/Mutex.h"
#include "mozilla/camera/PCamerasChild.h"
#include "mozilla/camera/PCamerasParent.h"
#include "nsCOMPtr.h"

// conflicts with #include of scoped_ptr.h
#undef FF
#include "modules/video_capture/video_capture_defines.h"

namespace mozilla {

namespace ipc {
class BackgroundChildImpl;
}  // namespace ipc

namespace camera {

class FrameRelay {
 public:
  virtual int DeliverFrame(
      uint8_t* buffer, const mozilla::camera::VideoFrameProperties& props) = 0;
};

struct CapturerElement {
  CaptureEngine engine;
  int id;
  FrameRelay* callback;
};

// Forward declaration so we can work with pointers to it.
class CamerasChild;
// Helper class in impl that we friend.
template <class T>
class LockAndDispatch;

// We emulate the sync webrtc.org API with the help of singleton
// CamerasSingleton, which manages a pointer to an IPC object, a thread
// where IPC operations should run on, and a mutex.
// The static function Cameras() will use that Singleton to set up,
// if needed, both the thread and the associated IPC objects and return
// a pointer to the IPC object. Users can then do IPC calls on that object
// after dispatching them to aforementioned thread.

// 2 Threads are involved in this code:
// - the MediaManager thread, which will call the (static, sync API) functions
//   through MediaEngineRemoteVideoSource
// - the Cameras IPC thread, which will be doing our IPC to the parent process
//   via PBackground

// Our main complication is that we emulate a sync API while (having to do)
// async messaging. We dispatch the messages to another thread to send them
// async and hold a Monitor to wait for the result to be asynchronously received
// again. The requirement for async messaging originates on the parent side:
// it's not reasonable to block all PBackground IPC there while waiting for
// something like device enumeration to complete.

class CamerasSingleton {
 public:
  static OffTheBooksMutex& Mutex() { return singleton().mCamerasMutex; }

  static CamerasChild*& Child() {
    Mutex().AssertCurrentThreadOwns();
    return singleton().mCameras;
  }

  static nsCOMPtr<nsIThread>& Thread() {
    Mutex().AssertCurrentThreadOwns();
    return singleton().mCamerasChildThread;
  }
  // The mutex is not held because mCameras is known not to be modified
  // concurrently when this is asserted.
  static void AssertNoChild() { MOZ_ASSERT(!singleton().mCameras); }

 private:
  CamerasSingleton();
  ~CamerasSingleton();

  static CamerasSingleton& singleton() {
    static CamerasSingleton camera;
    return camera;
  }

  // Reinitializing CamerasChild will change the pointers below.
  // We don't want this to happen in the middle of preparing IPC.
  // We will be alive on destruction, so this needs to be off the books.
  mozilla::OffTheBooksMutex mCamerasMutex;

  // This is owned by the IPC code, and the same code controls the lifetime.
  // It will set and clear this pointer as appropriate in setup/teardown.
  // We'd normally make this a WeakPtr but unfortunately the IPC code already
  // uses the WeakPtr mixin in a protected base class of CamerasChild, and in
  // any case the object becomes unusable as soon as IPC is tearing down, which
  // will be before actual destruction.
  CamerasChild* mCameras;
  nsCOMPtr<nsIThread> mCamerasChildThread;
};

// Get a pointer to a CamerasChild object we can use to do IPC with.
// This does everything needed to set up, including starting the IPC
// channel with PBackground, blocking until thats done, and starting the
// thread to do IPC on. This will fail if we're in shutdown. On success
// it will set up the CamerasSingleton.
CamerasChild* GetCamerasChild();

CamerasChild* GetCamerasChildIfExists();

// Shut down the IPC channel and everything associated, like WebRTC.
// This is a static call because the CamerasChild object may not even
// be alive when we're called.
void Shutdown(void);

// Obtain the CamerasChild object (if possible, i.e. not shutting down),
// and maintain a grip on the object for the duration of the call.
template <class MEM_FUN, class... ARGS>
int GetChildAndCall(MEM_FUN&& f, ARGS&&... args) {
  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
  CamerasChild* child = GetCamerasChild();
  if (child) {
    return (child->*f)(std::forward<ARGS>(args)...);
  } else {
    return -1;
  }
}

class CamerasChild final : public PCamerasChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  template <class T>
  friend class mozilla::camera::LockAndDispatch;

 public:
  // We are owned by the PBackground thread only. CamerasSingleton
  // takes a non-owning reference.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CamerasChild)

  // IPC messages recevied, received on the PBackground thread
  // these are the actual callbacks with data
  mozilla::ipc::IPCResult RecvDeliverFrame(
      const CaptureEngine&, const int&, mozilla::ipc::Shmem&&,
      const VideoFrameProperties& prop) override;

  mozilla::ipc::IPCResult RecvDeviceChange() override;

  // these are response messages to our outgoing requests
  mozilla::ipc::IPCResult RecvReplyNumberOfCaptureDevices(const int&) override;
  mozilla::ipc::IPCResult RecvReplyNumberOfCapabilities(const int&) override;
  mozilla::ipc::IPCResult RecvReplyAllocateCapture(const int&) override;
  mozilla::ipc::IPCResult RecvReplyGetCaptureCapability(
      const VideoCaptureCapability& capability) override;
  mozilla::ipc::IPCResult RecvReplyGetCaptureDevice(
      const nsACString& device_name, const nsACString& device_id,
      const bool& scary, const bool& device_is_placeholder) override;
  mozilla::ipc::IPCResult RecvReplyFailure(void) override;
  mozilla::ipc::IPCResult RecvReplySuccess(void) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;

  // the webrtc.org ViECapture calls are mirrored here, but with access
  // to a specific PCameras instance to communicate over. These also
  // run on the MediaManager thread
  int NumberOfCaptureDevices(CaptureEngine aCapEngine);
  int NumberOfCapabilities(CaptureEngine aCapEngine,
                           const char* deviceUniqueIdUTF8);
  int ReleaseCapture(CaptureEngine aCapEngine, const int capture_id);
  int StartCapture(CaptureEngine aCapEngine, const int capture_id,
                   const webrtc::VideoCaptureCapability& capability,
                   FrameRelay* func);
  int FocusOnSelectedSource(CaptureEngine aCapEngine, const int capture_id);
  int StopCapture(CaptureEngine aCapEngine, const int capture_id);
  // Returns a non-negative capture identifier or -1 on failure.
  int AllocateCapture(CaptureEngine aCapEngine, const char* unique_idUTF8,
                      uint64_t aWindowID);
  int GetCaptureCapability(CaptureEngine aCapEngine, const char* unique_idUTF8,
                           const unsigned int capability_number,
                           webrtc::VideoCaptureCapability* capability);
  int GetCaptureDevice(CaptureEngine aCapEngine, unsigned int list_number,
                       char* device_nameUTF8,
                       const unsigned int device_nameUTF8Length,
                       char* unique_idUTF8,
                       const unsigned int unique_idUTF8Length, bool* scary,
                       bool* device_is_placeholder);
  int EnsureInitialized(CaptureEngine aCapEngine);

  template <typename This>
  int ConnectDeviceListChangeListener(MediaEventListener* aListener,
                                      AbstractThread* aTarget, This* aThis,
                                      void (This::*aMethod)()) {
    // According to the spec, if the script sets
    // navigator.mediaDevices.ondevicechange and the permission state is
    // "always granted", the User Agent MUST fires a devicechange event when
    // a new media input device is made available, even the script never
    // call getusermedia or enumerateDevices.

    // In order to detect the event, we need to init the camera engine.
    // Currently EnsureInitialized(aCapEngine) is only called when one of
    // CamerasParent api, e.g., RecvNumberOfCaptureDevices(), is called.

    // So here we setup camera engine via EnsureInitialized(aCapEngine)

    EnsureInitialized(CameraEngine);
    *aListener = mDeviceListChangeEvent.Connect(aTarget, aThis, aMethod);
    return IPC_OK();
  }

  FrameRelay* Callback(CaptureEngine aCapEngine, int capture_id);

 private:
  CamerasChild();
  ~CamerasChild();
  // Dispatch a Runnable to the PCamerasParent, by executing it on the
  // decidecated Cameras IPC/PBackground thread.
  bool DispatchToParent(nsIRunnable* aRunnable, MonitorAutoLock& aMonitor);
  void AddCallback(const CaptureEngine aCapEngine, const int capture_id,
                   FrameRelay* render);
  void RemoveCallback(const CaptureEngine aCapEngine, const int capture_id);

  nsTArray<CapturerElement> mCallbacks;
  // Protects the callback arrays
  Mutex mCallbackMutex MOZ_UNANNOTATED;

  bool mIPCIsAlive;

  // Hold to prevent multiple outstanding requests. We don't use
  // request IDs so we only support one at a time. Don't want try
  // to use the webrtc.org API from multiple threads simultanously.
  // The monitor below isn't sufficient for this, as it will drop
  // the lock when Wait-ing for a response, allowing us to send a new
  // request. The Notify on receiving the response will then unblock
  // both waiters and one will be guaranteed to get the wrong result.
  // Take this one before taking mReplyMonitor.
  Mutex mRequestMutex MOZ_UNANNOTATED;
  // Hold to wait for an async response to our calls *and* until the
  // user of LockAndDispatch<> has read the data out. This is done by
  // keeping the LockAndDispatch object alive.
  Monitor mReplyMonitor MOZ_UNANNOTATED;
  // Async response valid?
  bool mReceivedReply;
  // Async responses data contents;
  bool mReplySuccess;
  const int mZero;
  int mReplyInteger;
  webrtc::VideoCaptureCapability* mReplyCapability = nullptr;
  nsCString mReplyDeviceName;
  nsCString mReplyDeviceID;
  bool mReplyScary;
  bool mReplyDeviceIsPlaceholder;
  MediaEventProducer<void> mDeviceListChangeEvent;
};

}  // namespace camera
}  // namespace mozilla

#endif  // mozilla_CamerasChild_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasChild.h"

#undef FF

#include "mozilla/Assertions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/Unused.h"
#include "MediaUtils.h"
#include "nsThreadUtils.h"

#undef LOG
#undef LOG_ENABLED
mozilla::LazyLogModule gCamerasChildLog("CamerasChild");
#define LOG(args) MOZ_LOG(gCamerasChildLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasChildLog, mozilla::LogLevel::Debug)

namespace mozilla::camera {

CamerasSingleton::CamerasSingleton()
    : mCamerasMutex("CamerasSingleton::mCamerasMutex"),
      mCameras(nullptr),
      mCamerasChildThread(nullptr) {
  LOG(("CamerasSingleton: %p", this));
}

CamerasSingleton::~CamerasSingleton() { LOG(("~CamerasSingleton: %p", this)); }

class InitializeIPCThread : public Runnable {
 public:
  InitializeIPCThread()
      : Runnable("camera::InitializeIPCThread"), mCamerasChild(nullptr) {}

  NS_IMETHOD Run() override {
    // Try to get the PBackground handle
    ipc::PBackgroundChild* existingBackgroundChild =
        ipc::BackgroundChild::GetForCurrentThread();
    // If it's not spun up yet, block until it is, and retry
    if (!existingBackgroundChild) {
      LOG(("No existingBackgroundChild"));
      existingBackgroundChild =
          ipc::BackgroundChild::GetOrCreateForCurrentThread();
      LOG(("BackgroundChild: %p", existingBackgroundChild));
      if (!existingBackgroundChild) {
        return NS_ERROR_FAILURE;
      }
    }

    // Create CamerasChild
    // We will be returning the resulting pointer (synchronously) to our caller.
    mCamerasChild = static_cast<mozilla::camera::CamerasChild*>(
        existingBackgroundChild->SendPCamerasConstructor());

    return NS_OK;
  }

  CamerasChild* GetCamerasChild() { return mCamerasChild; }

 private:
  CamerasChild* mCamerasChild;
};

CamerasChild* GetCamerasChild() {
  CamerasSingleton::Mutex().AssertCurrentThreadOwns();
  if (!CamerasSingleton::Child()) {
    MOZ_ASSERT(!NS_IsMainThread(), "Should not be on the main Thread");
    MOZ_ASSERT(!CamerasSingleton::Thread());
    LOG(("No sCameras, setting up IPC Thread"));
    nsresult rv = NS_NewNamedThread("Cameras IPC",
                                    getter_AddRefs(CamerasSingleton::Thread()));
    if (NS_FAILED(rv)) {
      LOG(("Error launching IPC Thread"));
      return nullptr;
    }

    // At this point we are in the MediaManager thread, and the thread we are
    // dispatching to is the specific Cameras IPC thread that was just made
    // above, so now we will fire off a runnable to run
    // BackgroundChild::GetOrCreateForCurrentThread there, while we
    // block in this thread.
    // We block until the following happens in the Cameras IPC thread:
    // 1) Creation of PBackground finishes
    // 2) Creation of PCameras finishes by sending a message to the parent
    RefPtr<InitializeIPCThread> runnable = new InitializeIPCThread();
    RefPtr<SyncRunnable> sr = new SyncRunnable(runnable);
    sr->DispatchToThread(CamerasSingleton::Thread());
    CamerasSingleton::Child() = runnable->GetCamerasChild();
  }
  if (!CamerasSingleton::Child()) {
    LOG(("Failed to set up CamerasChild, are we in shutdown?"));
  }
  return CamerasSingleton::Child();
}

CamerasChild* GetCamerasChildIfExists() {
  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
  return CamerasSingleton::Child();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplyFailure(void) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = false;
  monitor.Notify();
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplySuccess(void) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  monitor.Notify();
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplyNumberOfCapabilities(
    const int& capabilityCount) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = capabilityCount;
  monitor.Notify();
  return IPC_OK();
}

// Helper function to dispatch calls to the IPC Thread and
// CamerasChild object. Takes the needed locks and dispatches.
// Takes a "failed" value and a reference to the output variable
// as parameters, will return the right one depending on whether
// dispatching succeeded.
//
// The LockAndDispatch object in the caller must stay alive until after any
// reply data has been retreived (mReplyInteger, etc) so that the data is
// protected by the ReplyMonitor/RequestMutex
template <class T = int>
class LockAndDispatch {
 public:
  LockAndDispatch(CamerasChild* aCamerasChild, const char* aRequestingFunc,
                  nsIRunnable* aRunnable, T aFailureValue,
                  const T& aSuccessValue)
      : mCamerasChild(aCamerasChild),
        mRequestingFunc(aRequestingFunc),
        mRunnable(aRunnable),
        mReplyLock(aCamerasChild->mReplyMonitor),
        mRequestLock(aCamerasChild->mRequestMutex),
        mSuccess(true),
        mFailureValue(aFailureValue),
        mSuccessValue(aSuccessValue) {
    Dispatch();
  }

  T ReturnValue() const {
    if (mSuccess) {
      return mSuccessValue;
    } else {
      return mFailureValue;
    }
  }

  const bool& Success() const { return mSuccess; }

 private:
  void Dispatch() {
    if (!mCamerasChild->DispatchToParent(mRunnable, mReplyLock)) {
      LOG(("Cameras dispatch for IPC failed in %s", mRequestingFunc));
      mSuccess = false;
    }
  }

  CamerasChild* mCamerasChild;
  const char* mRequestingFunc;
  nsIRunnable* mRunnable;
  // Prevent concurrent use of the reply variables by holding
  // the mReplyMonitor. Note that this is unlocked while waiting for
  // the reply to be filled in, necessitating the additional mRequestLock/Mutex;
  MonitorAutoLock mReplyLock;
  MutexAutoLock mRequestLock;
  bool mSuccess;
  const T mFailureValue;
  const T& mSuccessValue;
};

bool CamerasChild::DispatchToParent(nsIRunnable* aRunnable,
                                    MonitorAutoLock& aMonitor) {
  CamerasSingleton::Mutex().AssertCurrentThreadOwns();
  mReplyMonitor.AssertCurrentThreadOwns();
  CamerasSingleton::Thread()->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  // Guard against spurious wakeups.
  mReceivedReply = false;
  // Wait for a reply
  do {
    // If the parent has been shut down, then we won't receive a reply.
    if (!mIPCIsAlive) {
      return false;
    }
    aMonitor.Wait();
  } while (!mReceivedReply);
  return mReplySuccess;
}

int CamerasChild::NumberOfCapabilities(CaptureEngine aCapEngine,
                                       const char* deviceUniqueIdUTF8) {
  LOG(("%s", __PRETTY_FUNCTION__));
  LOG(("NumberOfCapabilities for %s", deviceUniqueIdUTF8));
  nsCString unique_id(deviceUniqueIdUTF8);
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, nsCString>(
          "camera::PCamerasChild::SendNumberOfCapabilities", this,
          &CamerasChild::SendNumberOfCapabilities, aCapEngine, unique_id);
  LockAndDispatch<> dispatcher(this, __func__, runnable, 0, mReplyInteger);
  LOG(("Capture capability count: %d", dispatcher.ReturnValue()));
  return dispatcher.ReturnValue();
}

int CamerasChild::NumberOfCaptureDevices(CaptureEngine aCapEngine) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable = mozilla::NewRunnableMethod<CaptureEngine>(
      "camera::PCamerasChild::SendNumberOfCaptureDevices", this,
      &CamerasChild::SendNumberOfCaptureDevices, aCapEngine);
  LockAndDispatch<> dispatcher(this, __func__, runnable, 0, mReplyInteger);
  LOG(("Capture Devices: %d", dispatcher.ReturnValue()));
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplyNumberOfCaptureDevices(
    const int& aDeviceCount) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = aDeviceCount;
  monitor.Notify();
  return IPC_OK();
}

int CamerasChild::EnsureInitialized(CaptureEngine aCapEngine) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable = mozilla::NewRunnableMethod<CaptureEngine>(
      "camera::PCamerasChild::SendEnsureInitialized", this,
      &CamerasChild::SendEnsureInitialized, aCapEngine);
  LockAndDispatch<> dispatcher(this, __func__, runnable, 0, mReplyInteger);
  LOG(("Capture Devices: %d", dispatcher.ReturnValue()));
  return dispatcher.ReturnValue();
}

int CamerasChild::GetCaptureCapability(
    CaptureEngine aCapEngine, const char* unique_idUTF8,
    const unsigned int capability_number,
    webrtc::VideoCaptureCapability* capability) {
  LOG(("GetCaptureCapability: %s %d", unique_idUTF8, capability_number));
  MOZ_ASSERT(capability);
  nsCString unique_id(unique_idUTF8);
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, nsCString, unsigned int>(
          "camera::PCamerasChild::SendGetCaptureCapability", this,
          &CamerasChild::SendGetCaptureCapability, aCapEngine, unique_id,
          capability_number);
  mReplyCapability = capability;
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mZero);
  mReplyCapability = nullptr;
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplyGetCaptureCapability(
    const VideoCaptureCapability& ipcCapability) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyCapability->width = ipcCapability.width();
  mReplyCapability->height = ipcCapability.height();
  mReplyCapability->maxFPS = ipcCapability.maxFPS();
  mReplyCapability->videoType =
      static_cast<webrtc::VideoType>(ipcCapability.videoType());
  mReplyCapability->interlaced = ipcCapability.interlaced();
  monitor.Notify();
  return IPC_OK();
}

int CamerasChild::GetCaptureDevice(
    CaptureEngine aCapEngine, unsigned int list_number, char* device_nameUTF8,
    const unsigned int device_nameUTF8Length, char* unique_idUTF8,
    const unsigned int unique_idUTF8Length, bool* scary) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, unsigned int>(
          "camera::PCamerasChild::SendGetCaptureDevice", this,
          &CamerasChild::SendGetCaptureDevice, aCapEngine, list_number);
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mZero);
  if (dispatcher.Success()) {
    base::strlcpy(device_nameUTF8, mReplyDeviceName.get(),
                  device_nameUTF8Length);
    base::strlcpy(unique_idUTF8, mReplyDeviceID.get(), unique_idUTF8Length);
    if (scary) {
      *scary = mReplyScary;
    }
    LOG(("Got %s name %s id", device_nameUTF8, unique_idUTF8));
  }
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplyGetCaptureDevice(
    const nsACString& device_name, const nsACString& device_id,
    const bool& scary) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyDeviceName = device_name;
  mReplyDeviceID = device_id;
  mReplyScary = scary;
  monitor.Notify();
  return IPC_OK();
}

int CamerasChild::AllocateCapture(CaptureEngine aCapEngine,
                                  const char* unique_idUTF8,
                                  uint64_t aWindowID) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCString unique_id(unique_idUTF8);
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, nsCString, const uint64_t&>(
          "camera::PCamerasChild::SendAllocateCapture", this,
          &CamerasChild::SendAllocateCapture, aCapEngine, unique_id, aWindowID);
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mReplyInteger);
  if (dispatcher.Success()) {
    LOG(("Capture Device allocated: %d", mReplyInteger));
  }
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult CamerasChild::RecvReplyAllocateCapture(
    const int& aCaptureId) {
  LOG(("%s", __PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = aCaptureId;
  monitor.Notify();
  return IPC_OK();
}

int CamerasChild::ReleaseCapture(CaptureEngine aCapEngine,
                                 const int capture_id) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, int>(
          "camera::PCamerasChild::SendReleaseCapture", this,
          &CamerasChild::SendReleaseCapture, aCapEngine, capture_id);
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mZero);
  return dispatcher.ReturnValue();
}

void CamerasChild::AddCallback(const CaptureEngine aCapEngine,
                               const int capture_id, FrameRelay* render) {
  MutexAutoLock lock(mCallbackMutex);
  CapturerElement ce;
  ce.engine = aCapEngine;
  ce.id = capture_id;
  ce.callback = render;
  mCallbacks.AppendElement(ce);
}

void CamerasChild::RemoveCallback(const CaptureEngine aCapEngine,
                                  const int capture_id) {
  MutexAutoLock lock(mCallbackMutex);
  for (unsigned int i = 0; i < mCallbacks.Length(); i++) {
    CapturerElement ce = mCallbacks[i];
    if (ce.engine == aCapEngine && ce.id == capture_id) {
      mCallbacks.RemoveElementAt(i);
      break;
    }
  }
}

int CamerasChild::StartCapture(CaptureEngine aCapEngine, const int capture_id,
                               const webrtc::VideoCaptureCapability& webrtcCaps,
                               FrameRelay* cb) {
  LOG(("%s", __PRETTY_FUNCTION__));
  AddCallback(aCapEngine, capture_id, cb);
  VideoCaptureCapability capCap(
      webrtcCaps.width, webrtcCaps.height, webrtcCaps.maxFPS,
      static_cast<int>(webrtcCaps.videoType), webrtcCaps.interlaced);
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, int, VideoCaptureCapability>(
          "camera::PCamerasChild::SendStartCapture", this,
          &CamerasChild::SendStartCapture, aCapEngine, capture_id, capCap);
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mZero);
  return dispatcher.ReturnValue();
}

int CamerasChild::FocusOnSelectedSource(CaptureEngine aCapEngine,
                                        const int aCaptureId) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, int>(
          "camera::PCamerasChild::SendFocusOnSelectedSource", this,
          &CamerasChild::SendFocusOnSelectedSource, aCapEngine, aCaptureId);
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mZero);
  return dispatcher.ReturnValue();
}

int CamerasChild::StopCapture(CaptureEngine aCapEngine, const int capture_id) {
  LOG(("%s", __PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
      mozilla::NewRunnableMethod<CaptureEngine, int>(
          "camera::PCamerasChild::SendStopCapture", this,
          &CamerasChild::SendStopCapture, aCapEngine, capture_id);
  LockAndDispatch<> dispatcher(this, __func__, runnable, -1, mZero);
  if (dispatcher.Success()) {
    RemoveCallback(aCapEngine, capture_id);
  }
  return dispatcher.ReturnValue();
}

class ShutdownRunnable : public Runnable {
 public:
  explicit ShutdownRunnable(already_AddRefed<Runnable>&& aReplyEvent)
      : Runnable("camera::ShutdownRunnable"), mReplyEvent(aReplyEvent){};

  NS_IMETHOD Run() override {
    LOG(("Closing BackgroundChild"));
    // This will also destroy the CamerasChild.
    ipc::BackgroundChild::CloseForCurrentThread();

    NS_DispatchToMainThread(mReplyEvent.forget());

    return NS_OK;
  }

 private:
  RefPtr<Runnable> mReplyEvent;
};

void Shutdown(void) {
  // Called from both MediaEngineWebRTC::Shutdown() on the MediaManager thread
  // and DeallocPCamerasChild() on the dedicated IPC thread.
  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());

  CamerasChild* child = CamerasSingleton::Child();
  if (!child) {
    // We don't want to cause everything to get fired up if we're
    // really already shut down.
    LOG(("Shutdown when already shut down"));
    return;
  }
  if (CamerasSingleton::Thread()) {
    LOG(("PBackground thread exists, dispatching close"));
    // The IPC thread is shut down on the main thread after the
    // BackgroundChild is closed.
    RefPtr<ShutdownRunnable> runnable = new ShutdownRunnable(
        NewRunnableMethod("nsIThread::Shutdown", CamerasSingleton::Thread(),
                          &nsIThread::Shutdown));
    CamerasSingleton::Thread()->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  } else {
    LOG(("Shutdown called without PBackground thread"));
  }
  LOG(("Erasing sCameras & thread refs (original thread)"));
  CamerasSingleton::Child() = nullptr;
  CamerasSingleton::Thread() = nullptr;
}

mozilla::ipc::IPCResult CamerasChild::RecvDeliverFrame(
    const CaptureEngine& capEngine, const int& capId,
    mozilla::ipc::Shmem&& shmem, const VideoFrameProperties& prop) {
  MutexAutoLock lock(mCallbackMutex);
  if (Callback(capEngine, capId)) {
    unsigned char* image = shmem.get<unsigned char>();
    Callback(capEngine, capId)->DeliverFrame(image, prop);
  } else {
    LOG(("DeliverFrame called with dead callback"));
  }
  SendReleaseFrame(std::move(shmem));
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasChild::RecvDeviceChange() {
  mDeviceListChangeEvent.Notify();
  return IPC_OK();
}

void CamerasChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("ActorDestroy"));
  MonitorAutoLock monitor(mReplyMonitor);
  mIPCIsAlive = false;
  // Hopefully prevent us from getting stuck
  // on replies that'll never come.
  monitor.NotifyAll();
}

CamerasChild::CamerasChild()
    : mCallbackMutex("mozilla::cameras::CamerasChild::mCallbackMutex"),
      mIPCIsAlive(true),
      mRequestMutex("mozilla::cameras::CamerasChild::mRequestMutex"),
      mReplyMonitor("mozilla::cameras::CamerasChild::mReplyMonitor"),
      mReceivedReply(false),
      mReplySuccess(false),
      mZero(0),
      mReplyInteger(0),
      mReplyScary(false) {
  LOG(("CamerasChild: %p", this));

  MOZ_COUNT_CTOR(CamerasChild);
}

CamerasChild::~CamerasChild() {
  LOG(("~CamerasChild: %p", this));
  CamerasSingleton::AssertNoChild();
  MOZ_COUNT_DTOR(CamerasChild);
}

FrameRelay* CamerasChild::Callback(CaptureEngine aCapEngine, int capture_id) {
  for (unsigned int i = 0; i < mCallbacks.Length(); i++) {
    CapturerElement ce = mCallbacks[i];
    if (ce.engine == aCapEngine && ce.id == capture_id) {
      return ce.callback;
    }
  }

  return nullptr;
}

}  // namespace mozilla::camera

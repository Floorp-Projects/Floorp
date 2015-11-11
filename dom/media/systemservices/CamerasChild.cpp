/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasChild.h"
#include "CamerasUtils.h"

#include "webrtc/video_engine/include/vie_capture.h"
#undef FF

#include "mozilla/Assertions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/unused.h"
#include "MediaUtils.h"
#include "nsThreadUtils.h"

#undef LOG
#undef LOG_ENABLED
PRLogModuleInfo *gCamerasChildLog;
#define LOG(args) MOZ_LOG(gCamerasChildLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasChildLog, mozilla::LogLevel::Debug)

namespace mozilla {
namespace camera {

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
  CamerasSingleton()
    : mCamerasMutex("CamerasSingleton::mCamerasMutex"),
      mCameras(nullptr),
      mCamerasChildThread(nullptr) {
    if (!gCamerasChildLog) {
      gCamerasChildLog = PR_NewLogModule("CamerasChild");
    }
    LOG(("CamerasSingleton: %p", this));
  }

  ~CamerasSingleton() {
    LOG(("~CamerasSingleton: %p", this));
  }

  static CamerasSingleton& GetInstance() {
    static CamerasSingleton instance;
    return instance;
  }

  static OffTheBooksMutex& Mutex() {
    return GetInstance().mCamerasMutex;
  }

  static CamerasChild*& Child() {
    GetInstance().Mutex().AssertCurrentThreadOwns();
    return GetInstance().mCameras;
  }

  static nsCOMPtr<nsIThread>& Thread() {
    GetInstance().Mutex().AssertCurrentThreadOwns();
    return GetInstance().mCamerasChildThread;
  }

private:
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

class InitializeIPCThread : public nsRunnable
{
public:
  InitializeIPCThread()
    : mCamerasChild(nullptr) {}

  NS_IMETHOD Run() override {
    // Try to get the PBackground handle
    ipc::PBackgroundChild* existingBackgroundChild =
      ipc::BackgroundChild::GetForCurrentThread();
    // If it's not spun up yet, block until it is, and retry
    if (!existingBackgroundChild) {
      LOG(("No existingBackgroundChild"));
      SynchronouslyCreatePBackground();
      existingBackgroundChild =
        ipc::BackgroundChild::GetForCurrentThread();
      LOG(("BackgroundChild: %p", existingBackgroundChild));
    }
    // By now PBackground is guaranteed to be up
    MOZ_RELEASE_ASSERT(existingBackgroundChild);

    // Create CamerasChild
    // We will be returning the resulting pointer (synchronously) to our caller.
    mCamerasChild =
      static_cast<mozilla::camera::CamerasChild*>(existingBackgroundChild->SendPCamerasConstructor());

    return NS_OK;
  }

  CamerasChild* GetCamerasChild() {
    MOZ_ASSERT(mCamerasChild);
    return mCamerasChild;
  }

private:
  CamerasChild* mCamerasChild;
};

static CamerasChild*
Cameras() {
  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
  if (!CamerasSingleton::Child()) {
    MOZ_ASSERT(!NS_IsMainThread(), "Should not be on the main Thread");
    if (!gCamerasChildLog) {
      gCamerasChildLog = PR_NewLogModule("CamerasChild");
    }

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
    // SynchronouslyCreatePBackground there, while we block in this thread.
    // We block until the following happens in the Cameras IPC thread:
    // 1) Creation of PBackground finishes
    // 2) Creation of PCameras finishes by sending a message to the parent
    RefPtr<InitializeIPCThread> runnable = new InitializeIPCThread();
    RefPtr<SyncRunnable> sr = new SyncRunnable(runnable);
    sr->DispatchToThread(CamerasSingleton::Thread());
    CamerasSingleton::Child() = runnable->GetCamerasChild();
  }
  MOZ_ASSERT(CamerasSingleton::Child());
  return CamerasSingleton::Child();
}

bool
CamerasChild::RecvReplyFailure(void)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = false;
  monitor.Notify();
  return true;
}

bool
CamerasChild::RecvReplySuccess(void)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  monitor.Notify();
  return true;
}

int NumberOfCapabilities(CaptureEngine aCapEngine, const char* deviceUniqueIdUTF8)
{
  return Cameras()->NumberOfCapabilities(aCapEngine, deviceUniqueIdUTF8);
}

bool
CamerasChild::RecvReplyNumberOfCapabilities(const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = numdev;
  monitor.Notify();
  return true;
}

bool
CamerasChild::DispatchToParent(nsIRunnable* aRunnable,
                               MonitorAutoLock& aMonitor)
{
  {
    OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
    CamerasSingleton::Thread()->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  }
  // We can't see if the send worked, so we need to be able to bail
  // out on shutdown (when it failed and we won't get a reply).
  if (!mIPCIsAlive) {
    return false;
  }
  // Guard against spurious wakeups.
  mReceivedReply = false;
  // Wait for a reply
  do {
    aMonitor.Wait();
  } while (!mReceivedReply && mIPCIsAlive);
  if (!mReplySuccess) {
    return false;
  }
  return true;
}

int
CamerasChild::NumberOfCapabilities(CaptureEngine aCapEngine,
                                   const char* deviceUniqueIdUTF8)
{
  // Prevents multiple outstanding requests from happening.
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  LOG(("NumberOfCapabilities for %s", deviceUniqueIdUTF8));
  nsCString unique_id(deviceUniqueIdUTF8);
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, unique_id]() -> nsresult {
      if (this->SendNumberOfCapabilities(aCapEngine, unique_id)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  // Prevent concurrent use of the reply variables. Note
  // that this is unlocked while waiting for the reply to be
  // filled in, necessitating the first Mutex above.
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    LOG(("Get capture capability count failed"));
    return 0;
  }
  LOG(("Capture capability count: %d", mReplyInteger));
  return mReplyInteger;
}

int NumberOfCaptureDevices(CaptureEngine aCapEngine)
{
  return Cameras()->NumberOfCaptureDevices(aCapEngine);
}

int
CamerasChild::NumberOfCaptureDevices(CaptureEngine aCapEngine)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine]() -> nsresult {
      if (this->SendNumberOfCaptureDevices(aCapEngine)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    LOG(("Get NumberOfCaptureDevices failed"));
    return 0;
  }
  LOG(("Capture Devices: %d", mReplyInteger));
  return mReplyInteger;
}

bool
CamerasChild::RecvReplyNumberOfCaptureDevices(const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = numdev;
  monitor.Notify();
  return true;
}

int GetCaptureCapability(CaptureEngine aCapEngine, const char* unique_idUTF8,
                         const unsigned int capability_number,
                         webrtc::CaptureCapability& capability)
{
  return Cameras()->GetCaptureCapability(aCapEngine,
                                         unique_idUTF8,
                                         capability_number,
                                         capability);
}

int
CamerasChild::GetCaptureCapability(CaptureEngine aCapEngine,
                                   const char* unique_idUTF8,
                                   const unsigned int capability_number,
                                   webrtc::CaptureCapability& capability)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG(("GetCaptureCapability: %s %d", unique_idUTF8, capability_number));
  nsCString unique_id(unique_idUTF8);
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, unique_id, capability_number]() -> nsresult {
      if (this->SendGetCaptureCapability(aCapEngine, unique_id, capability_number)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    return -1;
  }
  capability = mReplyCapability;
  return 0;
}

bool
CamerasChild::RecvReplyGetCaptureCapability(const CaptureCapability& ipcCapability)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyCapability.width = ipcCapability.width();
  mReplyCapability.height = ipcCapability.height();
  mReplyCapability.maxFPS = ipcCapability.maxFPS();
  mReplyCapability.expectedCaptureDelay = ipcCapability.expectedCaptureDelay();
  mReplyCapability.rawType = static_cast<webrtc::RawVideoType>(ipcCapability.rawType());
  mReplyCapability.codecType = static_cast<webrtc::VideoCodecType>(ipcCapability.codecType());
  mReplyCapability.interlaced = ipcCapability.interlaced();
  monitor.Notify();
  return true;
}


int GetCaptureDevice(CaptureEngine aCapEngine,
                     unsigned int list_number, char* device_nameUTF8,
                     const unsigned int device_nameUTF8Length,
                     char* unique_idUTF8,
                     const unsigned int unique_idUTF8Length)
{
  return Cameras()->GetCaptureDevice(aCapEngine,
                                     list_number,
                                     device_nameUTF8,
                                     device_nameUTF8Length,
                                     unique_idUTF8,
                                     unique_idUTF8Length);
}

int
CamerasChild::GetCaptureDevice(CaptureEngine aCapEngine,
                               unsigned int list_number, char* device_nameUTF8,
                               const unsigned int device_nameUTF8Length,
                               char* unique_idUTF8,
                               const unsigned int unique_idUTF8Length)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, list_number]() -> nsresult {
      if (this->SendGetCaptureDevice(aCapEngine, list_number)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    LOG(("GetCaptureDevice failed"));
    return -1;
  }
  base::strlcpy(device_nameUTF8, mReplyDeviceName.get(), device_nameUTF8Length);
  base::strlcpy(unique_idUTF8, mReplyDeviceID.get(), unique_idUTF8Length);
  LOG(("Got %s name %s id", device_nameUTF8, unique_idUTF8));
  return 0;
}

bool
CamerasChild::RecvReplyGetCaptureDevice(const nsCString& device_name,
                                        const nsCString& device_id)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyDeviceName = device_name;
  mReplyDeviceID = device_id;
  monitor.Notify();
  return true;
}

int AllocateCaptureDevice(CaptureEngine aCapEngine,
                          const char* unique_idUTF8,
                          const unsigned int unique_idUTF8Length,
                          int& capture_id)
{
  return Cameras()->AllocateCaptureDevice(aCapEngine,
                                          unique_idUTF8,
                                          unique_idUTF8Length,
                                          capture_id);
}

int
CamerasChild::AllocateCaptureDevice(CaptureEngine aCapEngine,
                                    const char* unique_idUTF8,
                                    const unsigned int unique_idUTF8Length,
                                    int& capture_id)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  nsCString unique_id(unique_idUTF8);
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, unique_id]() -> nsresult {
      if (this->SendAllocateCaptureDevice(aCapEngine, unique_id)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    LOG(("AllocateCaptureDevice failed"));
    return -1;
  }
  LOG(("Capture Device allocated: %d", mReplyInteger));
  capture_id = mReplyInteger;
  return 0;
}


bool
CamerasChild::RecvReplyAllocateCaptureDevice(const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = numdev;
  monitor.Notify();
  return true;
}

int ReleaseCaptureDevice(CaptureEngine aCapEngine, const int capture_id)
{
  return Cameras()->ReleaseCaptureDevice(aCapEngine, capture_id);
}

int
CamerasChild::ReleaseCaptureDevice(CaptureEngine aCapEngine,
                                   const int capture_id)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, capture_id]() -> nsresult {
      if (this->SendReleaseCaptureDevice(aCapEngine, capture_id)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    return -1;
  }
  return 0;
}

void
CamerasChild::AddCallback(const CaptureEngine aCapEngine, const int capture_id,
                          webrtc::ExternalRenderer* render)
{
  MutexAutoLock lock(mCallbackMutex);
  CapturerElement ce;
  ce.engine = aCapEngine;
  ce.id = capture_id;
  ce.callback = render;
  mCallbacks.AppendElement(ce);
}

void
CamerasChild::RemoveCallback(const CaptureEngine aCapEngine, const int capture_id)
{
  MutexAutoLock lock(mCallbackMutex);
  for (unsigned int i = 0; i < mCallbacks.Length(); i++) {
    CapturerElement ce = mCallbacks[i];
    if (ce.engine == aCapEngine && ce.id == capture_id) {
      mCallbacks.RemoveElementAt(i);
      break;
    }
  }
}

int StartCapture(CaptureEngine aCapEngine,
                 const int capture_id,
                 webrtc::CaptureCapability& webrtcCaps,
                 webrtc::ExternalRenderer* cb)
{
  return Cameras()->StartCapture(aCapEngine,
                                 capture_id,
                                 webrtcCaps,
                                 cb);
}

int
CamerasChild::StartCapture(CaptureEngine aCapEngine,
                           const int capture_id,
                           webrtc::CaptureCapability& webrtcCaps,
                           webrtc::ExternalRenderer* cb)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  AddCallback(aCapEngine, capture_id, cb);
  CaptureCapability capCap(webrtcCaps.width,
                           webrtcCaps.height,
                           webrtcCaps.maxFPS,
                           webrtcCaps.expectedCaptureDelay,
                           webrtcCaps.rawType,
                           webrtcCaps.codecType,
                           webrtcCaps.interlaced);
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, capture_id, capCap]() -> nsresult {
      if (this->SendStartCapture(aCapEngine, capture_id, capCap)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    return -1;
  }
  return 0;
}

int StopCapture(CaptureEngine aCapEngine, const int capture_id)
{
  return Cameras()->StopCapture(aCapEngine, capture_id);
}

int
CamerasChild::StopCapture(CaptureEngine aCapEngine, const int capture_id)
{
  MutexAutoLock requestLock(mRequestMutex);
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    media::NewRunnableFrom([this, aCapEngine, capture_id]() -> nsresult {
      if (this->SendStopCapture(aCapEngine, capture_id)) {
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    });
  MonitorAutoLock monitor(mReplyMonitor);
  if (!DispatchToParent(runnable, monitor)) {
    return -1;
  }
  RemoveCallback(aCapEngine, capture_id);
  return 0;
}

void
Shutdown(void)
{
  {
    OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
    if (!CamerasSingleton::Child()) {
      // We don't want to cause everything to get fired up if we're
      // really already shut down.
      LOG(("Shutdown when already shut down"));
      return;
    }
  }
  Cameras()->Shutdown();
}

class ShutdownRunnable : public nsRunnable {
public:
  ShutdownRunnable(RefPtr<nsRunnable> aReplyEvent,
                   nsIThread* aReplyThread)
    : mReplyEvent(aReplyEvent), mReplyThread(aReplyThread) {};

  NS_IMETHOD Run() override {
    LOG(("Closing BackgroundChild"));
    ipc::BackgroundChild::CloseForCurrentThread();

    LOG(("PBackground thread exists, shutting down thread"));
    mReplyThread->Dispatch(mReplyEvent, NS_DISPATCH_NORMAL);

    return NS_OK;
  }

private:
  RefPtr<nsRunnable> mReplyEvent;
  nsIThread* mReplyThread;
};

void
CamerasChild::Shutdown()
{
  {
    MonitorAutoLock monitor(mReplyMonitor);
    mIPCIsAlive = false;
    monitor.NotifyAll();
  }

  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
  if (CamerasSingleton::Thread()) {
    LOG(("Dispatching actor deletion"));
    // Delete the parent actor.
    RefPtr<nsRunnable> deleteRunnable =
      // CamerasChild (this) will remain alive and is only deleted by the
      // IPC layer when SendAllDone returns.
      media::NewRunnableFrom([this]() -> nsresult {
        Unused << this->SendAllDone();
        return NS_OK;
      });
    CamerasSingleton::Thread()->Dispatch(deleteRunnable, NS_DISPATCH_NORMAL);
    LOG(("PBackground thread exists, dispatching close"));
    // Dispatch closing the IPC thread back to us when the
    // BackgroundChild is closed.
    RefPtr<nsRunnable> event =
      new ThreadDestructor(CamerasSingleton::Thread());
    RefPtr<ShutdownRunnable> runnable =
      new ShutdownRunnable(event, NS_GetCurrentThread());
    CamerasSingleton::Thread()->Dispatch(runnable, NS_DISPATCH_NORMAL);
  } else {
    LOG(("Shutdown called without PBackground thread"));
  }
  LOG(("Erasing sCameras & thread refs (original thread)"));
  CamerasSingleton::Child() = nullptr;
  CamerasSingleton::Thread() = nullptr;
}

bool
CamerasChild::RecvDeliverFrame(const int& capEngine,
                               const int& capId,
                               mozilla::ipc::Shmem&& shmem,
                               const int& size,
                               const uint32_t& time_stamp,
                               const int64_t& ntp_time,
                               const int64_t& render_time)
{
  MutexAutoLock lock(mCallbackMutex);
  CaptureEngine capEng = static_cast<CaptureEngine>(capEngine);
  if (Callback(capEng, capId)) {
    unsigned char* image = shmem.get<unsigned char>();
    Callback(capEng, capId)->DeliverFrame(image, size,
                                          time_stamp,
                                          ntp_time, render_time,
                                          nullptr);
  } else {
    LOG(("DeliverFrame called with dead callback"));
  }
  SendReleaseFrame(shmem);
  return true;
}

bool
CamerasChild::RecvFrameSizeChange(const int& capEngine,
                                  const int& capId,
                                  const int& w, const int& h)
{
  LOG((__PRETTY_FUNCTION__));
  MutexAutoLock lock(mCallbackMutex);
  CaptureEngine capEng = static_cast<CaptureEngine>(capEngine);
  if (Callback(capEng, capId)) {
    Callback(capEng, capId)->FrameSizeChange(w, h, 0);
  } else {
    LOG(("Frame size change with dead callback"));
  }
  return true;
}

void
CamerasChild::ActorDestroy(ActorDestroyReason aWhy)
{
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
    mReplyMonitor("mozilla::cameras::CamerasChild::mReplyMonitor")
{
  if (!gCamerasChildLog) {
    gCamerasChildLog = PR_NewLogModule("CamerasChild");
  }

  LOG(("CamerasChild: %p", this));

  MOZ_COUNT_CTOR(CamerasChild);
}

CamerasChild::~CamerasChild()
{
  LOG(("~CamerasChild: %p", this));

  Shutdown();

  MOZ_COUNT_DTOR(CamerasChild);
}

webrtc::ExternalRenderer* CamerasChild::Callback(CaptureEngine aCapEngine,
                                                 int capture_id)
{
  for (unsigned int i = 0; i < mCallbacks.Length(); i++) {
    CapturerElement ce = mCallbacks[i];
    if (ce.engine == aCapEngine && ce.id == capture_id) {
      return ce.callback;
    }
  }

  return nullptr;
}

}
}

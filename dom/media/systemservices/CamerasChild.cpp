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

#define FAKE_ONDEVICECHANGE_EVENT_PERIOD_IN_MS 5000
#define FAKE_ONDEVICECHANGE_EVENT_REPEAT_COUNT 30

namespace mozilla {
namespace camera {

CamerasSingleton::CamerasSingleton()
  : mCamerasMutex("CamerasSingleton::mCamerasMutex"),
    mCameras(nullptr),
    mCamerasChildThread(nullptr),
    mFakeDeviceChangeEventThread(nullptr) {
  LOG(("CamerasSingleton: %p", this));
}

CamerasSingleton::~CamerasSingleton() {
  LOG(("~CamerasSingleton: %p", this));
}

class FakeOnDeviceChangeEventRunnable : public Runnable
{
public:
  explicit FakeOnDeviceChangeEventRunnable(uint8_t counter)
    : Runnable("camera::FakeOnDeviceChangeEventRunnable")
    , mCounter(counter)
  {
  }

  NS_IMETHOD Run() override
  {
    OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());

    CamerasChild* child = CamerasSingleton::Child();
    if (child) {
      child->OnDeviceChange();

      if (mCounter++ < FAKE_ONDEVICECHANGE_EVENT_REPEAT_COUNT) {
        RefPtr<FakeOnDeviceChangeEventRunnable> evt = new FakeOnDeviceChangeEventRunnable(mCounter);
        CamerasSingleton::FakeDeviceChangeEventThread()->DelayedDispatch(evt.forget(),
          FAKE_ONDEVICECHANGE_EVENT_PERIOD_IN_MS);
      }
    }

    return NS_OK;
  }

private:
  uint8_t mCounter;
};

class InitializeIPCThread : public Runnable
{
public:
  InitializeIPCThread()
    : Runnable("camera::InitializeIPCThread")
    , mCamerasChild(nullptr)
  {
  }

  NS_IMETHOD Run() override {
    // Try to get the PBackground handle
    ipc::PBackgroundChild* existingBackgroundChild =
      ipc::BackgroundChild::GetForCurrentThread();
    // If it's not spun up yet, block until it is, and retry
    if (!existingBackgroundChild) {
      LOG(("No existingBackgroundChild"));
      existingBackgroundChild =
        ipc::BackgroundChild::SynchronouslyCreateForCurrentThread();
      LOG(("BackgroundChild: %p", existingBackgroundChild));
      if (!existingBackgroundChild) {
        return NS_ERROR_FAILURE;
      }
    }

    // Create CamerasChild
    // We will be returning the resulting pointer (synchronously) to our caller.
    mCamerasChild =
      static_cast<mozilla::camera::CamerasChild*>(existingBackgroundChild->SendPCamerasConstructor());

    return NS_OK;
  }

  CamerasChild* GetCamerasChild() {
    return mCamerasChild;
  }

private:
  CamerasChild* mCamerasChild;
};

CamerasChild*
GetCamerasChild() {
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
    // BackgroundChild::SynchronouslyCreateForCurrentThread there, while we
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

CamerasChild*
GetCamerasChildIfExists() {
  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
  return CamerasSingleton::Child();
}

int CamerasChild::AddDeviceChangeCallback(DeviceChangeCallback* aCallback)
{
  // According to the spec, if the script sets
  // navigator.mediaDevices.ondevicechange and the permission state is
  // "always granted", the User Agent MUST fires a devicechange event when
  // a new media input device is made available, even the script never
  // call getusermedia or enumerateDevices.

  // In order to detect the event, we need to init the camera engine.
  // Currently EnsureInitialized(aCapEngine) is only called when one of
  // CamerasaParent api, e.g., RecvNumberOfCaptureDevices(), is called.

  // So here we setup camera engine via EnsureInitialized(aCapEngine)

  EnsureInitialized(CameraEngine);
  return DeviceChangeCallback::AddDeviceChangeCallback(aCallback);
}

mozilla::ipc::IPCResult
CamerasChild::RecvReplyFailure(void)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = false;
  monitor.Notify();
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasChild::RecvReplySuccess(void)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  monitor.Notify();
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasChild::RecvReplyNumberOfCapabilities(const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = numdev;
  monitor.Notify();
  return IPC_OK();
}

// Helper function to dispatch calls to the IPC Thread and
// CamerasChild object. Takes the needed locks and dispatches.
// Takes a "failed" value and a reference to the output variable
// as parameters, will return the right one depending on whether
// dispatching succeeded.
template <class T = int>
class LockAndDispatch
{
public:
  LockAndDispatch(CamerasChild* aCamerasChild,
                  const char* aRequestingFunc,
                  nsIRunnable *aRunnable,
                  const T& aFailureValue = T(-1), const T& aSuccessValue = T(0))
    : mCamerasChild(aCamerasChild), mRequestingFunc(aRequestingFunc),
      mRunnable(aRunnable),
      mReplyLock(aCamerasChild->mReplyMonitor),
      mRequestLock(aCamerasChild->mRequestMutex),
      mSuccess(true),
      mFailureValue(aFailureValue), mSuccessValue(aSuccessValue)
  {
    Dispatch();
  }

  T ReturnValue() const {
    if (mSuccess) {
      return mSuccessValue;
    } else {
      return mFailureValue;
    }
  }

  const bool& Success() const {
    return mSuccess;
  }

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
  const T& mFailureValue;
  const T& mSuccessValue;
};

bool
CamerasChild::DispatchToParent(nsIRunnable* aRunnable,
                               MonitorAutoLock& aMonitor)
{
  CamerasSingleton::Mutex().AssertCurrentThreadOwns();
  CamerasSingleton::Thread()->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
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
  LOG((__PRETTY_FUNCTION__));
  LOG(("NumberOfCapabilities for %s", deviceUniqueIdUTF8));
  nsCString unique_id(deviceUniqueIdUTF8);
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine, nsCString>(
      "camera::PCamerasChild::SendNumberOfCapabilities",
      this,
      &CamerasChild::SendNumberOfCapabilities,
      aCapEngine,
      unique_id);
  LockAndDispatch<> dispatcher(this, __func__, runnable, 0, mReplyInteger);
  LOG(("Capture capability count: %d", dispatcher.ReturnValue()));
  return dispatcher.ReturnValue();
}

int
CamerasChild::NumberOfCaptureDevices(CaptureEngine aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine>(
      "camera::PCamerasChild::SendNumberOfCaptureDevices",
      this,
      &CamerasChild::SendNumberOfCaptureDevices,
      aCapEngine);
  LockAndDispatch<> dispatcher(this, __func__, runnable, 0, mReplyInteger);
  LOG(("Capture Devices: %d", dispatcher.ReturnValue()));
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult
CamerasChild::RecvReplyNumberOfCaptureDevices(const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = numdev;
  monitor.Notify();
  return IPC_OK();
}

int
CamerasChild::EnsureInitialized(CaptureEngine aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine>(
      "camera::PCamerasChild::SendEnsureInitialized",
      this,
      &CamerasChild::SendEnsureInitialized,
      aCapEngine);
  LockAndDispatch<> dispatcher(this, __func__, runnable, 0, mReplyInteger);
  LOG(("Capture Devices: %d", dispatcher.ReturnValue()));
  return dispatcher.ReturnValue();
}

int
CamerasChild::GetCaptureCapability(CaptureEngine aCapEngine,
                                   const char* unique_idUTF8,
                                   const unsigned int capability_number,
                                   webrtc::VideoCaptureCapability& capability)
{
  LOG(("GetCaptureCapability: %s %d", unique_idUTF8, capability_number));
  nsCString unique_id(unique_idUTF8);
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine, nsCString, unsigned int>(
      "camera::PCamerasChild::SendGetCaptureCapability",
      this,
      &CamerasChild::SendGetCaptureCapability,
      aCapEngine,
      unique_id,
      capability_number);
  LockAndDispatch<> dispatcher(this, __func__, runnable);
  if (dispatcher.Success()) {
    capability = mReplyCapability;
  }
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult
CamerasChild::RecvReplyGetCaptureCapability(const VideoCaptureCapability& ipcCapability)
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
  return IPC_OK();
}

int
CamerasChild::GetCaptureDevice(CaptureEngine aCapEngine,
                               unsigned int list_number, char* device_nameUTF8,
                               const unsigned int device_nameUTF8Length,
                               char* unique_idUTF8,
                               const unsigned int unique_idUTF8Length,
                               bool* scary)
{
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine, unsigned int>(
      "camera::PCamerasChild::SendGetCaptureDevice",
      this,
      &CamerasChild::SendGetCaptureDevice,
      aCapEngine,
      list_number);
  LockAndDispatch<> dispatcher(this, __func__, runnable);
  if (dispatcher.Success()) {
    base::strlcpy(device_nameUTF8, mReplyDeviceName.get(), device_nameUTF8Length);
    base::strlcpy(unique_idUTF8, mReplyDeviceID.get(), unique_idUTF8Length);
    if (scary) {
      *scary = mReplyScary;
    }
    LOG(("Got %s name %s id", device_nameUTF8, unique_idUTF8));
  }
  return dispatcher.ReturnValue();
}

mozilla::ipc::IPCResult
CamerasChild::RecvReplyGetCaptureDevice(const nsCString& device_name,
                                        const nsCString& device_id,
                                        const bool& scary)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyDeviceName = device_name;
  mReplyDeviceID = device_id;
  mReplyScary = scary;
  monitor.Notify();
  return IPC_OK();
}

int
CamerasChild::AllocateCaptureDevice(CaptureEngine aCapEngine,
                                    const char* unique_idUTF8,
                                    const unsigned int unique_idUTF8Length,
                                    int& aStreamId,
                                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo)
{
  LOG((__PRETTY_FUNCTION__));
  nsCString unique_id(unique_idUTF8);
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine,
                                        nsCString,
                                        const mozilla::ipc::PrincipalInfo&>(
      "camera::PCamerasChild::SendAllocateCaptureDevice",
      this,
      &CamerasChild::SendAllocateCaptureDevice,
      aCapEngine,
      unique_id,
      aPrincipalInfo);
  LockAndDispatch<> dispatcher(this, __func__, runnable);
  if (dispatcher.Success()) {
    LOG(("Capture Device allocated: %d", mReplyInteger));
    aStreamId = mReplyInteger;
  }
  return dispatcher.ReturnValue();
}


mozilla::ipc::IPCResult
CamerasChild::RecvReplyAllocateCaptureDevice(const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  MonitorAutoLock monitor(mReplyMonitor);
  mReceivedReply = true;
  mReplySuccess = true;
  mReplyInteger = numdev;
  monitor.Notify();
  return IPC_OK();
}

int
CamerasChild::ReleaseCaptureDevice(CaptureEngine aCapEngine,
                                   const int capture_id)
{
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine, int>(
      "camera::PCamerasChild::SendReleaseCaptureDevice",
      this,
      &CamerasChild::SendReleaseCaptureDevice,
      aCapEngine,
      capture_id);
  LockAndDispatch<> dispatcher(this, __func__, runnable);
  return dispatcher.ReturnValue();
}

void
CamerasChild::AddCallback(const CaptureEngine aCapEngine, const int capture_id,
                          FrameRelay* render)
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

int
CamerasChild::StartCapture(CaptureEngine aCapEngine,
                           const int capture_id,
                           webrtc::VideoCaptureCapability& webrtcCaps,
                           FrameRelay* cb)
{
  LOG((__PRETTY_FUNCTION__));
  AddCallback(aCapEngine, capture_id, cb);
  VideoCaptureCapability capCap(webrtcCaps.width,
                           webrtcCaps.height,
                           webrtcCaps.maxFPS,
                           webrtcCaps.expectedCaptureDelay,
                           webrtcCaps.rawType,
                           webrtcCaps.codecType,
                           webrtcCaps.interlaced);
  nsCOMPtr<nsIRunnable> runnable = mozilla::
    NewNonOwningRunnableMethod<CaptureEngine, int, VideoCaptureCapability>(
      "camera::PCamerasChild::SendStartCapture",
      this,
      &CamerasChild::SendStartCapture,
      aCapEngine,
      capture_id,
      capCap);
  LockAndDispatch<> dispatcher(this, __func__, runnable);
  return dispatcher.ReturnValue();
}

int
CamerasChild::StopCapture(CaptureEngine aCapEngine, const int capture_id)
{
  LOG((__PRETTY_FUNCTION__));
  nsCOMPtr<nsIRunnable> runnable =
    mozilla::NewNonOwningRunnableMethod<CaptureEngine, int>(
      "camera::PCamerasChild::SendStopCapture",
      this,
      &CamerasChild::SendStopCapture,
      aCapEngine,
      capture_id);
  LockAndDispatch<> dispatcher(this, __func__, runnable);
  if (dispatcher.Success()) {
    RemoveCallback(aCapEngine, capture_id);
  }
  return dispatcher.ReturnValue();
}

void
Shutdown(void)
{
  OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
  CamerasChild* child = CamerasSingleton::Child();
  if (!child) {
    // We don't want to cause everything to get fired up if we're
    // really already shut down.
    LOG(("Shutdown when already shut down"));
    return;
  }
  child->ShutdownAll();
}

class ShutdownRunnable : public Runnable {
public:
  explicit ShutdownRunnable(already_AddRefed<Runnable>&& aReplyEvent)
    : Runnable("camera::ShutdownRunnable")
    , mReplyEvent(aReplyEvent){};

  NS_IMETHOD Run() override {
    LOG(("Closing BackgroundChild"));
    ipc::BackgroundChild::CloseForCurrentThread();

    NS_DispatchToMainThread(mReplyEvent.forget());

    return NS_OK;
  }

private:
  RefPtr<Runnable> mReplyEvent;
};

void
CamerasChild::ShutdownAll()
{
  // Called with CamerasSingleton::Mutex() held
  ShutdownParent();
  ShutdownChild();
}

void
CamerasChild::ShutdownParent()
{
  // Called with CamerasSingleton::Mutex() held
  {
    MonitorAutoLock monitor(mReplyMonitor);
    mIPCIsAlive = false;
    monitor.NotifyAll();
  }
  if (CamerasSingleton::Thread()) {
    LOG(("Dispatching actor deletion"));
    // Delete the parent actor.
    // CamerasChild (this) will remain alive and is only deleted by the
    // IPC layer when SendAllDone returns.
    nsCOMPtr<nsIRunnable> deleteRunnable = mozilla::NewNonOwningRunnableMethod(
      "camera::PCamerasChild::SendAllDone", this, &CamerasChild::SendAllDone);
    CamerasSingleton::Thread()->Dispatch(deleteRunnable, NS_DISPATCH_NORMAL);
  } else {
    LOG(("ShutdownParent called without PBackground thread"));
  }
}

void
CamerasChild::ShutdownChild()
{
  // Called with CamerasSingleton::Mutex() held
  if (CamerasSingleton::Thread()) {
    LOG(("PBackground thread exists, dispatching close"));
    // Dispatch closing the IPC thread back to us when the
    // BackgroundChild is closed.
    RefPtr<ShutdownRunnable> runnable = new ShutdownRunnable(NewRunnableMethod(
      "nsIThread::Shutdown", CamerasSingleton::Thread(), &nsIThread::Shutdown));
    CamerasSingleton::Thread()->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  } else {
    LOG(("Shutdown called without PBackground thread"));
  }
  LOG(("Erasing sCameras & thread refs (original thread)"));
  CamerasSingleton::Child() = nullptr;
  CamerasSingleton::Thread() = nullptr;

  if (CamerasSingleton::FakeDeviceChangeEventThread()) {
    RefPtr<ShutdownRunnable> runnable = new ShutdownRunnable(
      NewRunnableMethod("nsIThread::Shutdown",
                        CamerasSingleton::FakeDeviceChangeEventThread(),
                        &nsIThread::Shutdown));
    CamerasSingleton::FakeDeviceChangeEventThread()->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  }
  CamerasSingleton::FakeDeviceChangeEventThread() = nullptr;
}

mozilla::ipc::IPCResult
CamerasChild::RecvDeliverFrame(const CaptureEngine& capEngine,
                               const int& capId,
                               mozilla::ipc::Shmem&& shmem,
                               const VideoFrameProperties & prop)
{
  MutexAutoLock lock(mCallbackMutex);
  if (Callback(capEngine, capId)) {
    unsigned char* image = shmem.get<unsigned char>();
    Callback(capEngine, capId)->DeliverFrame(image, prop);
  } else {
    LOG(("DeliverFrame called with dead callback"));
  }
  SendReleaseFrame(shmem);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasChild::RecvDeviceChange()
{
  this->OnDeviceChange();
  return IPC_OK();
}

int
CamerasChild::SetFakeDeviceChangeEvents()
{
  CamerasSingleton::Mutex().AssertCurrentThreadOwns();

  if(!CamerasSingleton::FakeDeviceChangeEventThread()) {
    nsresult rv = NS_NewNamedThread("Fake DC Event",
                                    getter_AddRefs(CamerasSingleton::FakeDeviceChangeEventThread()));
    if (NS_FAILED(rv)) {
      LOG(("Error launching Fake OnDeviceChange Event Thread"));
      return -1;
    }
  }

  // To simulate the devicechange event in mochitest,
  // we fire a fake devicechange event in Camera IPC thread periodically
  RefPtr<FakeOnDeviceChangeEventRunnable> evt = new FakeOnDeviceChangeEventRunnable(0);
  CamerasSingleton::FakeDeviceChangeEventThread()->Dispatch(evt.forget(), NS_DISPATCH_NORMAL);

  return 0;
}

mozilla::ipc::IPCResult
CamerasChild::RecvFrameSizeChange(const CaptureEngine& capEngine,
                                  const int& capId,
                                  const int& w, const int& h)
{
  LOG((__PRETTY_FUNCTION__));
  MutexAutoLock lock(mCallbackMutex);
  if (Callback(capEngine, capId)) {
    Callback(capEngine, capId)->FrameSizeChange(w, h);
  } else {
    LOG(("Frame size change with dead callback"));
  }
  return IPC_OK();
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
  LOG(("CamerasChild: %p", this));

  MOZ_COUNT_CTOR(CamerasChild);
}

CamerasChild::~CamerasChild()
{
  LOG(("~CamerasChild: %p", this));

  {
    OffTheBooksMutexAutoLock lock(CamerasSingleton::Mutex());
    // In normal circumstances we've already shut down and the
    // following does nothing. But on fatal IPC errors we will
    // get destructed immediately, and should not try to reach
    // the parent.
    ShutdownChild();
  }

  MOZ_COUNT_DTOR(CamerasChild);
}

FrameRelay* CamerasChild::Callback(CaptureEngine aCapEngine,
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

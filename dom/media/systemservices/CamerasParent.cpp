/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasParent.h"
#include "MediaEngine.h"
#include "MediaUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "mozilla/Services.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Preferences.h"
#include "nsIPermissionManager.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsNetUtil.h"

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

#undef LOG
#undef LOG_ENABLED
mozilla::LazyLogModule gCamerasParentLog("CamerasParent");
#define LOG(args) MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasParentLog, mozilla::LogLevel::Debug)

namespace mozilla {
namespace camera {

// 3 threads are involved in this code:
// - the main thread for some setups, and occassionally for video capture setup
//   calls that don't work correctly elsewhere.
// - the IPC thread on which PBackground is running and which receives and
//   sends messages
// - a thread which will execute the actual (possibly slow) camera access
//   called "VideoCapture". On Windows this is a thread with an event loop
//   suitable for UI access.

void InputObserver::DeviceChange() {
  LOG((__PRETTY_FUNCTION__));
  MOZ_ASSERT(mParent);

  RefPtr<nsIRunnable> ipc_runnable =
    media::NewRunnableFrom([this]() -> nsresult {
      if (mParent->IsShuttingDown()) {
        return NS_ERROR_FAILURE;
      }
      Unused << mParent->SendDeviceChange();
      return NS_OK;
    });

  nsIThread* thread = mParent->GetBackgroundThread();
  MOZ_ASSERT(thread != nullptr);
  thread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
};

class FrameSizeChangeRunnable : public Runnable {
public:
  FrameSizeChangeRunnable(CamerasParent *aParent, CaptureEngine capEngine,
                          int cap_id, unsigned int aWidth, unsigned int aHeight)
    : mParent(aParent), mCapEngine(capEngine), mCapId(cap_id),
      mWidth(aWidth), mHeight(aHeight) {}

  NS_IMETHOD Run() override {
    if (mParent->IsShuttingDown()) {
      // Communication channel is being torn down
      LOG(("FrameSizeChangeRunnable is active without active Child"));
      mResult = 0;
      return NS_OK;
    }
    if (!mParent->SendFrameSizeChange(mCapEngine, mCapId, mWidth, mHeight)) {
      mResult = -1;
    } else {
      mResult = 0;
    }
    return NS_OK;
  }

  int GetResult() {
    return mResult;
  }

private:
  RefPtr<CamerasParent> mParent;
  CaptureEngine mCapEngine;
  int mCapId;
  unsigned int mWidth;
  unsigned int mHeight;
  int mResult;
};

int
CallbackHelper::FrameSizeChange(unsigned int w, unsigned int h,
                                unsigned int streams)
{
  LOG(("CallbackHelper Video FrameSizeChange: %ux%u", w, h));
  RefPtr<FrameSizeChangeRunnable> runnable =
    new FrameSizeChangeRunnable(mParent, mCapEngine, mCapturerId, w, h);
  MOZ_ASSERT(mParent);
  nsIThread * thread = mParent->GetBackgroundThread();
  MOZ_ASSERT(thread != nullptr);
  thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  return 0;
}

class DeliverFrameRunnable : public Runnable {
public:
  DeliverFrameRunnable(CamerasParent *aParent,
                       CaptureEngine engine,
                       int cap_id,
                       ShmemBuffer buffer,
                       unsigned char* altbuffer,
                       size_t size,
                       uint32_t time_stamp,
                       int64_t ntp_time,
                       int64_t render_time)
    : mParent(aParent), mCapEngine(engine), mCapId(cap_id), mBuffer(Move(buffer)),
      mSize(size), mTimeStamp(time_stamp), mNtpTime(ntp_time),
      mRenderTime(render_time) {
    // No ShmemBuffer (of the right size) was available, so make an
    // extra buffer here.  We have no idea when we are going to run and
    // it will be potentially long after the webrtc frame callback has
    // returned, so the copy needs to be no later than here.
    // We will need to copy this back into a Shmem later on so we prefer
    // using ShmemBuffers to avoid the extra copy.
    if (altbuffer != nullptr) {
      mAlternateBuffer.reset(new unsigned char[size]);
      memcpy(mAlternateBuffer.get(), altbuffer, size);
    }
  };

  NS_IMETHOD Run() override {
    if (mParent->IsShuttingDown()) {
      // Communication channel is being torn down
      mResult = 0;
      return NS_OK;
    }
    if (!mParent->DeliverFrameOverIPC(mCapEngine, mCapId,
                                      Move(mBuffer), mAlternateBuffer.get(),
                                      mSize, mTimeStamp,
                                      mNtpTime, mRenderTime)) {
      mResult = -1;
    } else {
      mResult = 0;
    }
    return NS_OK;
  }

  int GetResult() {
    return mResult;
  }

private:
  RefPtr<CamerasParent> mParent;
  CaptureEngine mCapEngine;
  int mCapId;
  ShmemBuffer mBuffer;
  mozilla::UniquePtr<unsigned char[]> mAlternateBuffer;
  size_t mSize;
  uint32_t mTimeStamp;
  int64_t mNtpTime;
  int64_t mRenderTime;
  int mResult;
};

NS_IMPL_ISUPPORTS(CamerasParent, nsIObserver)

NS_IMETHODIMP
CamerasParent::Observe(nsISupports *aSubject,
                       const char *aTopic,
                       const char16_t *aData)
{
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID));
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  MOZ_ASSERT(obs);
  obs->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
  StopVideoCapture();
  return NS_OK;
}

nsresult
CamerasParent::DispatchToVideoCaptureThread(Runnable* event)
{
  // Don't try to dispatch if we're already on the right thread.
  // There's a potential deadlock because the mThreadMonitor is likely
  // to be taken already.
  MOZ_ASSERT(!mVideoCaptureThread ||
             mVideoCaptureThread->thread_id() != PlatformThread::CurrentId());

  MonitorAutoLock lock(mThreadMonitor);

  while(mChildIsAlive && mWebRTCAlive &&
        (!mVideoCaptureThread || !mVideoCaptureThread->IsRunning())) {
    mThreadMonitor.Wait();
  }
  if (!mVideoCaptureThread || !mVideoCaptureThread->IsRunning()) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Runnable> addrefedEvent = event;
  mVideoCaptureThread->message_loop()->PostTask(addrefedEvent.forget());
  return NS_OK;
}

void
CamerasParent::StopVideoCapture()
{
  LOG((__PRETTY_FUNCTION__));
  // We are called from the main thread (xpcom-shutdown) or
  // from PBackground (when the Actor shuts down).
  // Shut down the WebRTC stack (on the capture thread)
  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self]() -> nsresult {
      MonitorAutoLock lock(self->mThreadMonitor);
      self->CloseEngines();
      self->mThreadMonitor.NotifyAll();
      return NS_OK;
    });
  DebugOnly<nsresult> rv = DispatchToVideoCaptureThread(webrtc_runnable);
#ifdef DEBUG
  // It's ok for the dispatch to fail if the cleanup it has to do
  // has been done already.
  MOZ_ASSERT(NS_SUCCEEDED(rv) || !mWebRTCAlive);
#endif
  // Hold here until the WebRTC thread is gone. We need to dispatch
  // the thread deletion *now*, or there will be no more possibility
  // to get to the main thread.
  MonitorAutoLock lock(mThreadMonitor);
  while (mWebRTCAlive) {
    mThreadMonitor.Wait();
  }
  // After closing the WebRTC stack, clean up the
  // VideoCapture thread.
  if (self->mVideoCaptureThread) {
    base::Thread *thread = self->mVideoCaptureThread;
    self->mVideoCaptureThread = nullptr;
    RefPtr<Runnable> threadShutdown =
      media::NewRunnableFrom([thread]() -> nsresult {
        if (thread->IsRunning()) {
          thread->Stop();
        }
        delete thread;
        return NS_OK;
      });
    if (NS_FAILED(NS_DispatchToMainThread(threadShutdown))) {
      LOG(("Could not dispatch VideoCaptureThread destruction"));
    }
  }
}

int
CamerasParent::DeliverFrameOverIPC(CaptureEngine cap_engine,
                                   int cap_id,
                                   ShmemBuffer buffer,
                                   unsigned char* altbuffer,
                                   size_t size,
                                   uint32_t time_stamp,
                                   int64_t ntp_time,
                                   int64_t render_time)
{
  // No ShmemBuffers were available, so construct one now of the right size
  // and copy into it. That is an extra copy, but we expect this to be
  // the exceptional case, because we just assured the next call *will* have a
  // buffer of the right size.
  if (altbuffer != nullptr) {
    // Get a shared memory buffer from the pool, at least size big
    ShmemBuffer shMemBuff = mShmemPool.Get(this, size);

    if (!shMemBuff.Valid()) {
      LOG(("No usable Video shmem in DeliverFrame (out of buffers?)"));
      // We can skip this frame if we run out of buffers, it's not a real error.
      return 0;
    }

    // get() and Size() check for proper alignment of the segment
    memcpy(shMemBuff.GetBytes(), altbuffer, size);

    if (!SendDeliverFrame(cap_engine, cap_id,
                          shMemBuff.Get(), size,
                          time_stamp, ntp_time, render_time)) {
      return -1;
    }
  } else {
    MOZ_ASSERT(buffer.Valid());
    // ShmemBuffer was available, we're all good. A single copy happened
    // in the original webrtc callback.
    if (!SendDeliverFrame(cap_engine, cap_id,
                          buffer.Get(), size,
                          time_stamp, ntp_time, render_time)) {
      return -1;
    }
  }

  return 0;
}

ShmemBuffer
CamerasParent::GetBuffer(size_t aSize)
{
  return mShmemPool.GetIfAvailable(aSize);
}

int
CallbackHelper::DeliverFrame(unsigned char* buffer,
                             size_t size,
                             uint32_t time_stamp,
                             int64_t ntp_time,
                             int64_t render_time,
                             void *handle)
{
  // Get a shared memory buffer to copy the frame data into
  ShmemBuffer shMemBuffer = mParent->GetBuffer(size);
  if (!shMemBuffer.Valid()) {
    // Either we ran out of buffers or they're not the right size yet
    LOG(("Correctly sized Video shmem not available in DeliverFrame"));
    // We will do the copy into a(n extra) temporary buffer inside
    // the DeliverFrameRunnable constructor.
  } else {
    // Shared memory buffers of the right size are available, do the copy here.
    memcpy(shMemBuffer.GetBytes(), buffer, size);
    // Mark the original buffer as cleared.
    buffer = nullptr;
  }
  RefPtr<DeliverFrameRunnable> runnable =
    new DeliverFrameRunnable(mParent, mCapEngine, mCapturerId,
                             Move(shMemBuffer), buffer, size, time_stamp,
                             ntp_time, render_time);
  MOZ_ASSERT(mParent);
  nsIThread* thread = mParent->GetBackgroundThread();
  MOZ_ASSERT(thread != nullptr);
  thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  return 0;
}
// XXX!!! FIX THIS -- we should move to pure DeliverI420Frame
int
CallbackHelper::DeliverI420Frame(const webrtc::I420VideoFrame& webrtc_frame)
{
  return DeliverFrame(const_cast<uint8_t*>(webrtc_frame.buffer(webrtc::kYPlane)),
                      CalcBufferSize(webrtc::kI420, webrtc_frame.width(), webrtc_frame.height()),
                      webrtc_frame.timestamp(),
                      webrtc_frame.ntp_time_ms(),
                      webrtc_frame.render_time_ms(),
                      (void*) webrtc_frame.native_handle());
}

bool
CamerasParent::RecvReleaseFrame(mozilla::ipc::Shmem&& s) {
  mShmemPool.Put(ShmemBuffer(s));
  return true;
}

bool
CamerasParent::SetupEngine(CaptureEngine aCapEngine)
{
  MOZ_ASSERT(mVideoCaptureThread->thread_id() == PlatformThread::CurrentId());
  EngineHelper *helper = &mEngines[aCapEngine];

  // Already initialized
  if (helper->mEngine) {
    return true;
  }

  webrtc::CaptureDeviceInfo *captureDeviceInfo = nullptr;

  switch (aCapEngine) {
  case ScreenEngine:
    captureDeviceInfo =
      new webrtc::CaptureDeviceInfo(webrtc::CaptureDeviceType::Screen);
    break;
  case BrowserEngine:
    captureDeviceInfo =
      new webrtc::CaptureDeviceInfo(webrtc::CaptureDeviceType::Browser);
    break;
  case WinEngine:
    captureDeviceInfo =
      new webrtc::CaptureDeviceInfo(webrtc::CaptureDeviceType::Window);
    break;
  case AppEngine:
    captureDeviceInfo =
      new webrtc::CaptureDeviceInfo(webrtc::CaptureDeviceType::Application);
    break;
  case CameraEngine:
    captureDeviceInfo =
      new webrtc::CaptureDeviceInfo(webrtc::CaptureDeviceType::Camera);
    break;
  default:
    LOG(("Invalid webrtc Video engine"));
    MOZ_CRASH();
    break;
  }

  helper->mConfig.Set<webrtc::CaptureDeviceInfo>(captureDeviceInfo);
  helper->mEngine = webrtc::VideoEngine::Create(helper->mConfig);

  if (!helper->mEngine) {
    LOG(("VideoEngine::Create failed"));
    return false;
  }

  helper->mPtrViEBase = webrtc::ViEBase::GetInterface(helper->mEngine);
  if (!helper->mPtrViEBase) {
    LOG(("ViEBase::GetInterface failed"));
    return false;
  }

  if (helper->mPtrViEBase->Init() < 0) {
    LOG(("ViEBase::Init failed"));
    return false;
  }

  helper->mPtrViECapture = webrtc::ViECapture::GetInterface(helper->mEngine);
  if (!helper->mPtrViECapture) {
    LOG(("ViECapture::GetInterface failed"));
    return false;
  }

  InputObserver** observer = mObservers.AppendElement(
          new InputObserver(this));

#ifdef DEBUG
  MOZ_ASSERT(0 == helper->mPtrViECapture->RegisterInputObserver(*observer));
#else
  helper->mPtrViECapture->RegisterInputObserver(*observer);
#endif

  helper->mPtrViERender = webrtc::ViERender::GetInterface(helper->mEngine);
  if (!helper->mPtrViERender) {
    LOG(("ViERender::GetInterface failed"));
    return false;
  }

  return true;
}

void
CamerasParent::CloseEngines()
{
  LOG((__PRETTY_FUNCTION__));
  if (!mWebRTCAlive) {
    return;
  }
  MOZ_ASSERT(mVideoCaptureThread->thread_id() == PlatformThread::CurrentId());

  // Stop the callers
  while (mCallbacks.Length()) {
    auto capEngine = mCallbacks[0]->mCapEngine;
    auto capNum = mCallbacks[0]->mCapturerId;
    LOG(("Forcing shutdown of engine %d, capturer %d", capEngine, capNum));
    StopCapture(capEngine, capNum);
    Unused << ReleaseCaptureDevice(capEngine, capNum);
  }

  for (int i = 0; i < CaptureEngine::MaxEngine; i++) {
    if (mEngines[i].mEngineIsRunning) {
      LOG(("Being closed down while engine %d is running!", i));
    }
    if (mEngines[i].mPtrViERender) {
      mEngines[i].mPtrViERender->Release();
      mEngines[i].mPtrViERender = nullptr;
    }
    if (mEngines[i].mPtrViECapture) {
#ifdef DEBUG
      MOZ_ASSERT(0 == mEngines[i].mPtrViECapture->DeregisterInputObserver());
#else
      mEngines[i].mPtrViECapture->DeregisterInputObserver();
#endif

      mEngines[i].mPtrViECapture->Release();
        mEngines[i].mPtrViECapture = nullptr;
    }
    if(mEngines[i].mPtrViEBase) {
      mEngines[i].mPtrViEBase->Release();
      mEngines[i].mPtrViEBase = nullptr;
    }
    if (mEngines[i].mEngine) {
      mEngines[i].mEngine->SetTraceCallback(nullptr);
      webrtc::VideoEngine::Delete(mEngines[i].mEngine);
      mEngines[i].mEngine = nullptr;
    }
  }

  for (InputObserver* observer : mObservers) {
    delete observer;
  }
  mObservers.Clear();

  mWebRTCAlive = false;
}

bool
CamerasParent::EnsureInitialized(int aEngine)
{
  LOG((__PRETTY_FUNCTION__));
  // We're shutting down, don't try to do new WebRTC ops.
  if (!mWebRTCAlive) {
    return false;
  }
  CaptureEngine capEngine = static_cast<CaptureEngine>(aEngine);
  if (!SetupEngine(capEngine)) {
    LOG(("CamerasParent failed to initialize engine"));
    return false;
  }

  return true;
}

// Dispatch the runnable to do the camera operation on the
// specific Cameras thread, preventing us from blocking, and
// chain a runnable to send back the result on the IPC thread.
// It would be nice to get rid of the code duplication here,
// perhaps via Promises.
bool
CamerasParent::RecvNumberOfCaptureDevices(const CaptureEngine& aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine]() -> nsresult {
      int num = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        num = self->mEngines[aCapEngine].mPtrViECapture->NumberOfCaptureDevices();
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, num]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (num < 0) {
            LOG(("RecvNumberOfCaptureDevices couldn't find devices"));
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            LOG(("RecvNumberOfCaptureDevices: %d", num));
            Unused << self->SendReplyNumberOfCaptureDevices(num);
            return NS_OK;
          }
        });
        self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvNumberOfCapabilities(const CaptureEngine& aCapEngine,
                                        const nsCString& unique_id)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("Getting caps for %s", unique_id.get()));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, unique_id, aCapEngine]() -> nsresult {
      int num = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        num =
          self->mEngines[aCapEngine].mPtrViECapture->NumberOfCapabilities(
            unique_id.get(),
            MediaEngineSource::kMaxUniqueIdLength);
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, num]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (num < 0) {
            LOG(("RecvNumberOfCapabilities couldn't find capabilities"));
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            LOG(("RecvNumberOfCapabilities: %d", num));
          }
          Unused << self->SendReplyNumberOfCapabilities(num);
          return NS_OK;
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvGetCaptureCapability(const CaptureEngine& aCapEngine,
                                        const nsCString& unique_id,
                                        const int& num)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("RecvGetCaptureCapability: %s %d", unique_id.get(), num));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, unique_id, aCapEngine, num]() -> nsresult {
      webrtc::CaptureCapability webrtcCaps;
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        error = self->mEngines[aCapEngine].mPtrViECapture->GetCaptureCapability(
          unique_id.get(), MediaEngineSource::kMaxUniqueIdLength, num, webrtcCaps);
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, webrtcCaps, error]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          CaptureCapability capCap(webrtcCaps.width,
                                   webrtcCaps.height,
                                   webrtcCaps.maxFPS,
                                   webrtcCaps.expectedCaptureDelay,
                                   webrtcCaps.rawType,
                                   webrtcCaps.codecType,
                                   webrtcCaps.interlaced);
          LOG(("Capability: %u %u %u %u %d %d",
               webrtcCaps.width,
               webrtcCaps.height,
               webrtcCaps.maxFPS,
               webrtcCaps.expectedCaptureDelay,
               webrtcCaps.rawType,
               webrtcCaps.codecType));
          if (error) {
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
          Unused << self->SendReplyGetCaptureCapability(capCap);
          return NS_OK;
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvGetCaptureDevice(const CaptureEngine& aCapEngine,
                                    const int& aListNumber)
{
  LOG((__PRETTY_FUNCTION__));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, aListNumber]() -> nsresult {
      char deviceName[MediaEngineSource::kMaxDeviceNameLength];
      char deviceUniqueId[MediaEngineSource::kMaxUniqueIdLength];
      nsCString name;
      nsCString uniqueId;
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
          error = self->mEngines[aCapEngine].mPtrViECapture->GetCaptureDevice(aListNumber,
                                                                              deviceName,
                                                                              sizeof(deviceName),
                                                                              deviceUniqueId,
                                                                              sizeof(deviceUniqueId));
      }
      if (!error) {
        name.Assign(deviceName);
        uniqueId.Assign(deviceUniqueId);
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error, name, uniqueId]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (error) {
            LOG(("GetCaptureDevice failed: %d", error));
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }

          LOG(("Returning %s name %s id", name.get(), uniqueId.get()));
          Unused << self->SendReplyGetCaptureDevice(name, uniqueId);
          return NS_OK;
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

static nsresult
GetPrincipalFromOrigin(const nsACString& aOrigin, nsIPrincipal** aPrincipal)
{
  nsAutoCString originNoSuffix;
  mozilla::PrincipalOriginAttributes attrs;
  if (!attrs.PopulateFromOrigin(aOrigin, originNoSuffix)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal = mozilla::BasePrincipal::CreateCodebasePrincipal(uri, attrs);
  principal.forget(aPrincipal);
  return NS_OK;
}

// Find out whether the given origin has permission to use the
// camera. If the permission is not persistent, we'll make it
// a one-shot by removing the (session) permission.
static bool
HasCameraPermission(const nsCString& aOrigin)
{
  // Name used with nsIPermissionManager
  static const char* cameraPermission = "MediaManagerVideo";
  bool allowed = false;
  nsresult rv;
  nsCOMPtr<nsIPermissionManager> mgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIIOService> ioServ(do_GetIOService());
    nsCOMPtr<nsIURI> uri;
    rv = ioServ->NewURI(aOrigin, nullptr, nullptr, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      // Permanent permissions are only retrievable via principal, not uri
      nsCOMPtr<nsIPrincipal> principal;
      rv = GetPrincipalFromOrigin(aOrigin, getter_AddRefs(principal));
      if (NS_SUCCEEDED(rv)) {
        uint32_t video = nsIPermissionManager::UNKNOWN_ACTION;
        rv = mgr->TestExactPermissionFromPrincipal(principal,
                                                   cameraPermission,
                                                   &video);
        if (NS_SUCCEEDED(rv)) {
          allowed = (video == nsIPermissionManager::ALLOW_ACTION);
        }
        // Session permissions are removed after one use.
        if (allowed) {
          mgr->RemoveFromPrincipal(principal, cameraPermission);
        }
      }
    }
  }
  return allowed;
}

bool
CamerasParent::RecvAllocateCaptureDevice(const CaptureEngine& aCapEngine,
                                         const nsCString& unique_id,
                                         const nsCString& aOrigin)
{
  LOG(("%s: Verifying permissions for %s", __PRETTY_FUNCTION__, aOrigin.get()));
  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> mainthread_runnable =
    media::NewRunnableFrom([self, aCapEngine, unique_id, aOrigin]() -> nsresult {
      // Verify whether the claimed origin has received permission
      // to use the camera, either persistently or this session (one shot).
      bool allowed = HasCameraPermission(aOrigin);
      if (!allowed) {
        // Developer preference for turning off permission check.
        if (Preferences::GetBool("media.navigator.permission.disabled", false)
            || Preferences::GetBool("media.navigator.permission.fake")) {
          allowed = true;
          LOG(("No permission but checks are disabled or fake sources active"));
        } else {
          LOG(("No camera permission for this origin"));
        }
      }
      // After retrieving the permission (or not) on the main thread,
      // bounce to the WebRTC thread to allocate the device (or not),
      // then bounce back to the IPC thread for the reply to content.
      RefPtr<Runnable> webrtc_runnable =
      media::NewRunnableFrom([self, allowed, aCapEngine, unique_id]() -> nsresult {
        int numdev = -1;
        int error = -1;
        if (allowed && self->EnsureInitialized(aCapEngine)) {
          error = self->mEngines[aCapEngine].mPtrViECapture->AllocateCaptureDevice(
                    unique_id.get(), MediaEngineSource::kMaxUniqueIdLength, numdev);
        }
        RefPtr<nsIRunnable> ipc_runnable =
          media::NewRunnableFrom([self, numdev, error]() -> nsresult {
            if (self->IsShuttingDown()) {
              return NS_ERROR_FAILURE;
            }
            if (error) {
              Unused << self->SendReplyFailure();
              return NS_ERROR_FAILURE;
            } else {
              LOG(("Allocated device nr %d", numdev));
              Unused << self->SendReplyAllocateCaptureDevice(numdev);
              return NS_OK;
            }
          });
        self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
        return NS_OK;
        });
      self->DispatchToVideoCaptureThread(webrtc_runnable);
      return NS_OK;
    });
  NS_DispatchToMainThread(mainthread_runnable);
  return true;
}

int
CamerasParent::ReleaseCaptureDevice(const CaptureEngine& aCapEngine,
                                    const int& capnum)
{
  int error = -1;
  if (EnsureInitialized(aCapEngine)) {
    error = mEngines[aCapEngine].mPtrViECapture->ReleaseCaptureDevice(capnum);
  }
  return error;
}

bool
CamerasParent::RecvReleaseCaptureDevice(const CaptureEngine& aCapEngine,
                                        const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("RecvReleaseCamera device nr %d", numdev));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, numdev]() -> nsresult {
      int error = self->ReleaseCaptureDevice(aCapEngine, numdev);
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error, numdev]() -> nsresult {
          if (self->IsShuttingDown()) {
            LOG(("In Shutdown, not Releasing"));
            return NS_ERROR_FAILURE;
          }
          if (error) {
            Unused << self->SendReplyFailure();
            LOG(("Failed to free device nr %d", numdev));
            return NS_ERROR_FAILURE;
          } else {
            Unused << self->SendReplySuccess();
            LOG(("Freed device nr %d", numdev));
            return NS_OK;
          }
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvStartCapture(const CaptureEngine& aCapEngine,
                                const int& capnum,
                                const CaptureCapability& ipcCaps)
{
  LOG((__PRETTY_FUNCTION__));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, capnum, ipcCaps]() -> nsresult {
      CallbackHelper** cbh;
      webrtc::ExternalRenderer* render;
      EngineHelper* helper = nullptr;
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        cbh = self->mCallbacks.AppendElement(
          new CallbackHelper(static_cast<CaptureEngine>(aCapEngine), capnum, self));
        render = static_cast<webrtc::ExternalRenderer*>(*cbh);

        helper = &self->mEngines[aCapEngine];
        error =
          helper->mPtrViERender->AddRenderer(capnum, webrtc::kVideoI420, render);
        if (!error) {
          error = helper->mPtrViERender->StartRender(capnum);
        }

        webrtc::CaptureCapability capability;
        capability.width = ipcCaps.width();
        capability.height = ipcCaps.height();
        capability.maxFPS = ipcCaps.maxFPS();
        capability.expectedCaptureDelay = ipcCaps.expectedCaptureDelay();
        capability.rawType = static_cast<webrtc::RawVideoType>(ipcCaps.rawType());
        capability.codecType = static_cast<webrtc::VideoCodecType>(ipcCaps.codecType());
        capability.interlaced = ipcCaps.interlaced();

        if (!error) {
          error = helper->mPtrViECapture->StartCapture(capnum, capability);
        }
        if (!error) {
          helper->mEngineIsRunning = true;
        }
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (!error) {
            Unused << self->SendReplySuccess();
            return NS_OK;
          } else {
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

void
CamerasParent::StopCapture(const CaptureEngine& aCapEngine,
                           const int& capnum)
{
  if (EnsureInitialized(aCapEngine)) {
    mEngines[aCapEngine].mPtrViECapture->StopCapture(capnum);
    mEngines[aCapEngine].mPtrViERender->StopRender(capnum);
    mEngines[aCapEngine].mPtrViERender->RemoveRenderer(capnum);
    mEngines[aCapEngine].mEngineIsRunning = false;

    for (size_t i = 0; i < mCallbacks.Length(); i++) {
      if (mCallbacks[i]->mCapEngine == aCapEngine
          && mCallbacks[i]->mCapturerId == capnum) {
        delete mCallbacks[i];
        mCallbacks.RemoveElementAt(i);
        break;
      }
    }
  }
}

bool
CamerasParent::RecvStopCapture(const CaptureEngine& aCapEngine,
                               const int& capnum)
{
  LOG((__PRETTY_FUNCTION__));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, capnum]() -> nsresult {
      self->StopCapture(aCapEngine, capnum);
      return NS_OK;
    });
  nsresult rv = DispatchToVideoCaptureThread(webrtc_runnable);
  if (self->IsShuttingDown()) {
    return NS_SUCCEEDED(rv);
  } else {
    if (NS_SUCCEEDED(rv)) {
      return SendReplySuccess();
    } else {
      return SendReplyFailure();
    }
  }
}

void
CamerasParent::StopIPC()
{
  MOZ_ASSERT(!mDestroyed);
  // Release shared memory now, it's our last chance
  mShmemPool.Cleanup(this);
  // We don't want to receive callbacks or anything if we can't
  // forward them anymore anyway.
  mChildIsAlive = false;
  mDestroyed = true;
}

bool
CamerasParent::RecvAllDone()
{
  LOG((__PRETTY_FUNCTION__));
  // Don't try to send anything to the child now
  mChildIsAlive = false;
  return Send__delete__(this);
}

void
CamerasParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // No more IPC from here
  LOG((__PRETTY_FUNCTION__));
  StopIPC();
  // Shut down WebRTC (if we're not in full shutdown, else this
  // will already have happened)
  StopVideoCapture();
}

CamerasParent::CamerasParent()
  : mShmemPool(CaptureEngine::MaxEngine),
    mThreadMonitor("CamerasParent::mThreadMonitor"),
    mVideoCaptureThread(nullptr),
    mChildIsAlive(true),
    mDestroyed(false),
    mWebRTCAlive(true)
{
  LOG(("CamerasParent: %p", this));

  mPBackgroundThread = NS_GetCurrentThread();
  MOZ_ASSERT(mPBackgroundThread != nullptr, "GetCurrentThread failed");

  LOG(("Spinning up WebRTC Cameras Thread"));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> threadStart =
    media::NewRunnableFrom([self]() -> nsresult {
      // Register thread shutdown observer
      nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
      if (NS_WARN_IF(!obs)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv =
        obs->AddObserver(self, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // Start the thread
      MonitorAutoLock lock(self->mThreadMonitor);
      self->mVideoCaptureThread = new base::Thread("VideoCapture");
      base::Thread::Options options;
#if defined(_WIN32)
      options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINUITHREAD;
#else

      options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINTHREAD;
#endif
      if (!self->mVideoCaptureThread->StartWithOptions(options)) {
        MOZ_CRASH();
      }
      self->mThreadMonitor.NotifyAll();
      return NS_OK;
    });
  NS_DispatchToMainThread(threadStart);

  MOZ_COUNT_CTOR(CamerasParent);
}

CamerasParent::~CamerasParent()
{
  LOG(("~CamerasParent: %p", this));

  MOZ_COUNT_DTOR(CamerasParent);
#ifdef DEBUG
  // Verify we have shut down the webrtc engines, this is
  // supposed to happen in ActorDestroy.
  // That runnable takes a ref to us, so it must have finished
  // by the time we get here.
  for (int i = 0; i < CaptureEngine::MaxEngine; i++) {
    MOZ_ASSERT(!mEngines[i].mEngine);
  }
#endif
}

already_AddRefed<CamerasParent>
CamerasParent::Create() {
  mozilla::ipc::AssertIsOnBackgroundThread();
  RefPtr<CamerasParent> camerasParent = new CamerasParent();
  return camerasParent.forget();
}

}
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasParent.h"
#include "CamerasUtils.h"
#include "MediaEngine.h"
#include "MediaUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/unused.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#undef LOG
#undef LOG_ENABLED
PRLogModuleInfo *gCamerasParentLog;
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

class FrameSizeChangeRunnable : public nsRunnable {
public:
  FrameSizeChangeRunnable(CamerasParent *aParent, CaptureEngine capEngine,
                          int cap_id, unsigned int aWidth, unsigned int aHeight)
    : mParent(aParent), mCapEngine(capEngine), mCapId(cap_id),
      mWidth(aWidth), mHeight(aHeight) {}

  NS_IMETHOD Run() {
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
  nsRefPtr<CamerasParent> mParent;
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
  nsRefPtr<FrameSizeChangeRunnable> runnable =
    new FrameSizeChangeRunnable(mParent, mCapEngine, mCapturerId, w, h);
  MOZ_ASSERT(mParent);
  nsIThread * thread = mParent->GetBackgroundThread();
  MOZ_ASSERT(thread != nullptr);
  thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  return 0;
}

class DeliverFrameRunnable : public nsRunnable {
public:
  DeliverFrameRunnable(CamerasParent *aParent,
                       CaptureEngine engine,
                       int cap_id,
                       ShmemBuffer buffer,
                       unsigned char* altbuffer,
                       int size,
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

  NS_IMETHOD Run() {
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
  nsRefPtr<CamerasParent> mParent;
  CaptureEngine mCapEngine;
  int mCapId;
  ShmemBuffer mBuffer;
  mozilla::UniquePtr<unsigned char[]> mAlternateBuffer;
  int mSize;
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
CamerasParent::DispatchToVideoCaptureThread(nsRunnable *event)
{
  MonitorAutoLock lock(mThreadMonitor);

  while(mChildIsAlive && mWebRTCAlive &&
        (!mVideoCaptureThread || !mVideoCaptureThread->IsRunning())) {
    mThreadMonitor.Wait();
  }
  if (!mVideoCaptureThread || !mVideoCaptureThread->IsRunning()) {
    return NS_ERROR_FAILURE;
  }
  mVideoCaptureThread->message_loop()->PostTask(FROM_HERE,
                                                new RunnableTask(event));
  return NS_OK;
}

void
CamerasParent::StopVideoCapture()
{
  // We are called from the main thread (xpcom-shutdown) or
  // from PBackground (when the Actor shuts down).
  // Shut down the WebRTC stack (on the capture thread)
  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self]() -> nsresult {
        MonitorAutoLock lock(self->mThreadMonitor);
        self->CloseEngines();
        self->mThreadMonitor.NotifyAll();
        return NS_OK;
      });
  DispatchToVideoCaptureThread(webrtc_runnable);
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
    nsRefPtr<nsRunnable> threadShutdown =
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
                                   int size,
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
                             int size,
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
  nsRefPtr<DeliverFrameRunnable> runnable =
    new DeliverFrameRunnable(mParent, mCapEngine, mCapturerId,
                             Move(shMemBuffer), buffer, size, time_stamp,
                             ntp_time, render_time);
  MOZ_ASSERT(mParent);
  nsIThread* thread = mParent->GetBackgroundThread();
  MOZ_ASSERT(thread != nullptr);
  thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  return 0;
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
  if (!mWebRTCAlive) {
    return;
  }
  MOZ_ASSERT(mVideoCaptureThread->thread_id() == PlatformThread::CurrentId());

  // Stop the callers
  while (mCallbacks.Length()) {
    auto capEngine = mCallbacks[0]->mCapEngine;
    auto capNum = mCallbacks[0]->mCapturerId;
    LOG(("Forcing shutdown of engine %d, capturer %d", capEngine, capNum));
    RecvStopCapture(capEngine, capNum);
    RecvReleaseCaptureDevice(capEngine, capNum);
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
CamerasParent::RecvNumberOfCaptureDevices(const int& aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine]() -> nsresult {
      int num = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        num = self->mEngines[aCapEngine].mPtrViECapture->NumberOfCaptureDevices();
      }
      nsRefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, num]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (num < 0) {
            LOG(("RecvNumberOfCaptureDevices couldn't find devices"));
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            LOG(("RecvNumberOfCaptureDevices: %d", num));
            unused << self->SendReplyNumberOfCaptureDevices(num);
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
CamerasParent::RecvNumberOfCapabilities(const int& aCapEngine,
                                        const nsCString& unique_id)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("Getting caps for %s", unique_id.get()));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self, unique_id, aCapEngine]() -> nsresult {
      int num = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        num =
          self->mEngines[aCapEngine].mPtrViECapture->NumberOfCapabilities(
            unique_id.get(),
            MediaEngineSource::kMaxUniqueIdLength);
      }
      nsRefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, num]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (num < 0) {
            LOG(("RecvNumberOfCapabilities couldn't find capabilities"));
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            LOG(("RecvNumberOfCapabilities: %d", num));
          }
          unused << self->SendReplyNumberOfCapabilities(num);
          return NS_OK;
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvGetCaptureCapability(const int &aCapEngine,
                                        const nsCString& unique_id,
                                        const int& num)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("RecvGetCaptureCapability: %s %d", unique_id.get(), num));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self, unique_id, aCapEngine, num]() -> nsresult {
      webrtc::CaptureCapability webrtcCaps;
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        error = self->mEngines[aCapEngine].mPtrViECapture->GetCaptureCapability(
          unique_id.get(), MediaEngineSource::kMaxUniqueIdLength, num, webrtcCaps);
      }
      nsRefPtr<nsIRunnable> ipc_runnable =
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
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
          unused << self->SendReplyGetCaptureCapability(capCap);
          return NS_OK;
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvGetCaptureDevice(const int& aCapEngine,
                                    const int& aListNumber)
{
  LOG((__PRETTY_FUNCTION__));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
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
      nsRefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error, name, uniqueId]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (error) {
            LOG(("GetCaptureDevice failed: %d", error));
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }

          LOG(("Returning %s name %s id", name.get(), uniqueId.get()));
          unused << self->SendReplyGetCaptureDevice(name, uniqueId);
          return NS_OK;
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvAllocateCaptureDevice(const int& aCapEngine,
                                         const nsCString& unique_id)
{
  LOG((__PRETTY_FUNCTION__));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, unique_id]() -> nsresult {
      int numdev = -1;
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        error = self->mEngines[aCapEngine].mPtrViECapture->AllocateCaptureDevice(
          unique_id.get(), MediaEngineSource::kMaxUniqueIdLength, numdev);
      }
      nsRefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, numdev, error]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (error) {
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            LOG(("Allocated device nr %d", numdev));
            unused << self->SendReplyAllocateCaptureDevice(numdev);
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
CamerasParent::RecvReleaseCaptureDevice(const int& aCapEngine,
                                        const int& numdev)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("RecvReleaseCamera device nr %d", numdev));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, numdev]() -> nsresult {
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        error = self->mEngines[aCapEngine].mPtrViECapture->ReleaseCaptureDevice(numdev);
      }
      nsRefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error, numdev]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (error) {
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            unused << self->SendReplySuccess();
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
CamerasParent::RecvStartCapture(const int& aCapEngine,
                                const int& capnum,
                                const CaptureCapability& ipcCaps)
{
  LOG((__PRETTY_FUNCTION__));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
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
      nsRefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (!error) {
            unused << self->SendReplySuccess();
            return NS_OK;
          } else {
            unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
        });
      self->mPBackgroundThread->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return true;
}

bool
CamerasParent::RecvStopCapture(const int& aCapEngine,
                               const int& capnum)
{
  LOG((__PRETTY_FUNCTION__));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, capnum]() -> nsresult {
      if (self->EnsureInitialized(aCapEngine)) {
        self->mEngines[aCapEngine].mPtrViECapture->StopCapture(capnum);
        self->mEngines[aCapEngine].mPtrViERender->StopRender(capnum);
        self->mEngines[aCapEngine].mPtrViERender->RemoveRenderer(capnum);
        self->mEngines[aCapEngine].mEngineIsRunning = false;

        for (size_t i = 0; i < self->mCallbacks.Length(); i++) {
          if (self->mCallbacks[i]->mCapEngine == aCapEngine
              && self->mCallbacks[i]->mCapturerId == capnum) {
            delete self->mCallbacks[i];
            self->mCallbacks.RemoveElementAt(i);
            break;
          }
        }
      }
      return NS_OK;
    });
  if (NS_SUCCEEDED(DispatchToVideoCaptureThread(webrtc_runnable))) {
    return SendReplySuccess();
  } else {
    return SendReplyFailure();
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
  if (!gCamerasParentLog) {
    gCamerasParentLog = PR_NewLogModule("CamerasParent");
  }
  LOG(("CamerasParent: %p", this));

  mPBackgroundThread = NS_GetCurrentThread();
  MOZ_ASSERT(mPBackgroundThread != nullptr, "GetCurrentThread failed");

  LOG(("Spinning up WebRTC Cameras Thread"));

  nsRefPtr<CamerasParent> self(this);
  nsRefPtr<nsRunnable> threadStart =
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
  nsRefPtr<CamerasParent> camerasParent = new CamerasParent();
  return camerasParent.forget();
}

}
}

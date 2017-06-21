/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasParent.h"
#include "MediaEngine.h"
#include "MediaUtils.h"
#include "VideoFrameUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "mozilla/Services.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/Preferences.h"
#include "nsIPermissionManager.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsNetUtil.h"

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

#if defined(_WIN32)
#include <process.h>
#define getpid() _getpid()
#endif

#undef LOG
#undef LOG_VERBOSE
#undef LOG_ENABLED
mozilla::LazyLogModule gCamerasParentLog("CamerasParent");
#define LOG(args) MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Debug, args)
#define LOG_VERBOSE(args) MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Verbose, args)
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

// InputObserver is owned by CamerasParent, and it has a ref to CamerasParent
void InputObserver::OnDeviceChange() {
  LOG((__PRETTY_FUNCTION__));
  MOZ_ASSERT(mParent);

  RefPtr<InputObserver> self(this);
  RefPtr<nsIRunnable> ipc_runnable =
    media::NewRunnableFrom([self]() -> nsresult {
      if (self->mParent->IsShuttingDown()) {
        return NS_ERROR_FAILURE;
      }
      Unused << self->mParent->SendDeviceChange();
      return NS_OK;
    });

  nsIEventTarget* target = mParent->GetBackgroundEventTarget();
  MOZ_ASSERT(target != nullptr);
  target->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
};

class DeliverFrameRunnable : public ::mozilla::Runnable {
public:
  DeliverFrameRunnable(CamerasParent *aParent, CaptureEngine aEngine,
      uint32_t aStreamId, const webrtc::VideoFrame& aFrame,
      const VideoFrameProperties& aProperties)
      : mParent(aParent), mCapEngine(aEngine), mStreamId(aStreamId),
      mProperties(aProperties)
  {
    // No ShmemBuffer (of the right size) was available, so make an
    // extra buffer here.  We have no idea when we are going to run and
    // it will be potentially long after the webrtc frame callback has
    // returned, so the copy needs to be no later than here.
    // We will need to copy this back into a Shmem later on so we prefer
    // using ShmemBuffers to avoid the extra copy.
    mAlternateBuffer.reset(new unsigned char[aProperties.bufferSize()]);
    VideoFrameUtils::CopyVideoFrameBuffers(mAlternateBuffer.get(),
                                           aProperties.bufferSize(), aFrame);
  }

  DeliverFrameRunnable(CamerasParent* aParent, CaptureEngine aEngine,
      uint32_t aStreamId, ShmemBuffer aBuffer, VideoFrameProperties& aProperties)
      : mParent(aParent), mCapEngine(aEngine), mStreamId(aStreamId),
      mBuffer(Move(aBuffer)), mProperties(aProperties)
  {};

  NS_IMETHOD Run() override {
    if (mParent->IsShuttingDown()) {
      // Communication channel is being torn down
      mResult = 0;
      return NS_OK;
    }
    if (!mParent->DeliverFrameOverIPC(mCapEngine, mStreamId, Move(mBuffer),
                                      mAlternateBuffer.get(), mProperties)) {
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
  uint32_t mStreamId;
  ShmemBuffer mBuffer;
  mozilla::UniquePtr<unsigned char[]> mAlternateBuffer;
  VideoFrameProperties mProperties;
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
CamerasParent::DeliverFrameOverIPC(CaptureEngine capEng,
                          uint32_t aStreamId,
                          ShmemBuffer buffer,
                          unsigned char* altbuffer,
                          VideoFrameProperties& aProps)
{
  // No ShmemBuffers were available, so construct one now of the right size
  // and copy into it. That is an extra copy, but we expect this to be
  // the exceptional case, because we just assured the next call *will* have a
  // buffer of the right size.
  if (altbuffer != nullptr) {
    // Get a shared memory buffer from the pool, at least size big
    ShmemBuffer shMemBuff = mShmemPool.Get(this, aProps.bufferSize());

    if (!shMemBuff.Valid()) {
      LOG(("No usable Video shmem in DeliverFrame (out of buffers?)"));
      // We can skip this frame if we run out of buffers, it's not a real error.
      return 0;
    }

    // get() and Size() check for proper alignment of the segment
    memcpy(shMemBuff.GetBytes(), altbuffer, aProps.bufferSize());

    if (!SendDeliverFrame(capEng, aStreamId,
                          shMemBuff.Get(), aProps)) {
      return -1;
    }
  } else {
    MOZ_ASSERT(buffer.Valid());
    // ShmemBuffer was available, we're all good. A single copy happened
    // in the original webrtc callback.
    if (!SendDeliverFrame(capEng, aStreamId,
                          buffer.Get(), aProps)) {
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

void
CallbackHelper::OnFrame(const webrtc::VideoFrame& aVideoFrame)
{
  LOG_VERBOSE((__PRETTY_FUNCTION__));
  RefPtr<DeliverFrameRunnable> runnable = nullptr;
  // Get frame properties
  camera::VideoFrameProperties properties;
  VideoFrameUtils::InitFrameBufferProperties(aVideoFrame, properties);
  // Get a shared memory buffer to copy the frame data into
  ShmemBuffer shMemBuffer = mParent->GetBuffer(properties.bufferSize());
  if (!shMemBuffer.Valid()) {
    // Either we ran out of buffers or they're not the right size yet
    LOG(("Correctly sized Video shmem not available in DeliverFrame"));
    // We will do the copy into a(n extra) temporary buffer inside
    // the DeliverFrameRunnable constructor.
  } else {
    // Shared memory buffers of the right size are available, do the copy here.
    VideoFrameUtils::CopyVideoFrameBuffers(shMemBuffer.GetBytes(),
                                           properties.bufferSize(), aVideoFrame);
    runnable = new DeliverFrameRunnable(mParent, mCapEngine, mStreamId,
                                        Move(shMemBuffer), properties);
  }
  if (!runnable.get()) {
    runnable = new DeliverFrameRunnable(mParent, mCapEngine, mStreamId,
                                        aVideoFrame, properties);
  }
  MOZ_ASSERT(mParent);
  nsIEventTarget* target = mParent->GetBackgroundEventTarget();
  MOZ_ASSERT(target != nullptr);
  target->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

mozilla::ipc::IPCResult
CamerasParent::RecvReleaseFrame(mozilla::ipc::Shmem&& s) {
  mShmemPool.Put(ShmemBuffer(s));
  return IPC_OK();
}

bool
CamerasParent::SetupEngine(CaptureEngine aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));
  MOZ_ASSERT(mVideoCaptureThread->thread_id() == PlatformThread::CurrentId());
  RefPtr<mozilla::camera::VideoEngine>* engine = &mEngines[aCapEngine];

  // Already initialized
  if (engine->get()) {
    return true;
  }

  webrtc::CaptureDeviceInfo *captureDeviceInfo = nullptr;
  UniquePtr<webrtc::Config> config(new webrtc::Config);

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

  config->Set<webrtc::CaptureDeviceInfo>(captureDeviceInfo);
  *engine = mozilla::camera::VideoEngine::Create(UniquePtr<const webrtc::Config>(config.release()));

  if (!engine->get()) {
    LOG(("VideoEngine::Create failed"));
    return false;
  }

  RefPtr<InputObserver>* observer = mObservers.AppendElement(new InputObserver(this));
  auto device_info = engine->get()->GetOrCreateVideoCaptureDeviceInfo();
  MOZ_ASSERT(device_info);
  if (device_info) {
    device_info->RegisterVideoInputFeedBack(*(observer->get()));
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
    auto streamNum = mCallbacks[0]->mStreamId;
    LOG(("Forcing shutdown of engine %d, capturer %d", capEngine, streamNum));
    StopCapture(capEngine, streamNum);
    Unused << ReleaseCaptureDevice(capEngine, streamNum);
  }

  for (int i = 0; i < CaptureEngine::MaxEngine; i++) {
    if (auto engine = mEngines[i].get() ){
      if (engine->IsRunning()) {
        LOG(("Being closed down while engine %d is running!", i));
      }

      auto device_info = engine->GetOrCreateVideoCaptureDeviceInfo();
      MOZ_ASSERT(device_info);
      if (device_info) {
        device_info->DeRegisterVideoInputFeedBack();
      }
      mozilla::camera::VideoEngine::Delete(engine);
      mEngines[i] = nullptr;
    }
  }

  // the observers hold references to us
  mObservers.Clear();

  mWebRTCAlive = false;
}

VideoEngine *
CamerasParent::EnsureInitialized(int aEngine)
{
  LOG_VERBOSE((__PRETTY_FUNCTION__));
  // We're shutting down, don't try to do new WebRTC ops.
  if (!mWebRTCAlive) {
    return nullptr;
  }
  CaptureEngine capEngine = static_cast<CaptureEngine>(aEngine);
  if (!SetupEngine(capEngine)) {
    LOG(("CamerasParent failed to initialize engine"));
    return nullptr;
  }

  return mEngines[aEngine];
}

// Dispatch the runnable to do the camera operation on the
// specific Cameras thread, preventing us from blocking, and
// chain a runnable to send back the result on the IPC thread.
// It would be nice to get rid of the code duplication here,
// perhaps via Promises.
mozilla::ipc::IPCResult
CamerasParent::RecvNumberOfCaptureDevices(const CaptureEngine& aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("CaptureEngine=%d", aCapEngine));
  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine]() -> nsresult {
      int num = -1;
      if (auto engine = self->EnsureInitialized(aCapEngine)) {
        if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
          num = devInfo->NumberOfDevices();
        }
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
        self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasParent::RecvEnsureInitialized(const CaptureEngine& aCapEngine)
{
  LOG((__PRETTY_FUNCTION__));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine]() -> nsresult {
      bool result = self->EnsureInitialized(aCapEngine);

      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, result]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (!result) {
            LOG(("RecvEnsureInitialized failed"));
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          } else {
            LOG(("RecvEnsureInitialized succeeded"));
            Unused << self->SendReplySuccess();
            return NS_OK;
          }
        });
        self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasParent::RecvNumberOfCapabilities(const CaptureEngine& aCapEngine,
                                        const nsCString& unique_id)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("Getting caps for %s", unique_id.get()));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, unique_id, aCapEngine]() -> nsresult {
      int num = -1;
      if (auto engine = self->EnsureInitialized(aCapEngine)) {
        if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
          num = devInfo->NumberOfCapabilities(unique_id.get());
        }
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
      self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasParent::RecvGetCaptureCapability(const CaptureEngine& aCapEngine,
                                        const nsCString& unique_id,
                                        const int& num)
{
  LOG((__PRETTY_FUNCTION__));
  LOG(("RecvGetCaptureCapability: %s %d", unique_id.get(), num));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, unique_id, aCapEngine, num]() -> nsresult {
      webrtc::VideoCaptureCapability webrtcCaps;
      int error = -1;
      if (auto engine = self->EnsureInitialized(aCapEngine)) {
        if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()){
          error = devInfo->GetCapability(unique_id.get(), num, webrtcCaps);
        }
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, webrtcCaps, error]() -> nsresult {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          VideoCaptureCapability capCap(webrtcCaps.width,
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
      self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult
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
      pid_t devicePid = 0;
      int error = -1;
      if (auto engine = self->EnsureInitialized(aCapEngine)) {
        if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
          error = devInfo->GetDeviceName(aListNumber, deviceName, sizeof(deviceName),
                                         deviceUniqueId, sizeof(deviceUniqueId),
                                         nullptr, 0,
                                         &devicePid);
        }
      }
      if (!error) {
        name.Assign(deviceName);
        uniqueId.Assign(deviceUniqueId);
      }
      RefPtr<nsIRunnable> ipc_runnable =
        media::NewRunnableFrom([self, error, name, uniqueId, devicePid]() {
          if (self->IsShuttingDown()) {
            return NS_ERROR_FAILURE;
          }
          if (error) {
            LOG(("GetCaptureDevice failed: %d", error));
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
          bool scary = (devicePid == getpid());

          LOG(("Returning %s name %s id (pid = %d)%s", name.get(),
               uniqueId.get(), devicePid, (scary? " (scary)" : "")));
          Unused << self->SendReplyGetCaptureDevice(name, uniqueId, scary);
          return NS_OK;
        });
      self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

// Find out whether the given principal has permission to use the
// camera. If the permission is not persistent, we'll make it
// a one-shot by removing the (session) permission.
static bool
HasCameraPermission(const ipc::PrincipalInfo& aPrincipalInfo)
{
  if (aPrincipalInfo.type() == ipc::PrincipalInfo::TNullPrincipalInfo) {
    return false;
  }

  if (aPrincipalInfo.type() == ipc::PrincipalInfo::TSystemPrincipalInfo) {
    return true;
  }

  MOZ_ASSERT(aPrincipalInfo.type() == ipc::PrincipalInfo::TContentPrincipalInfo);

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Name used with nsIPermissionManager
  static const char* cameraPermission = "MediaManagerVideo";
  nsCOMPtr<nsIPermissionManager> mgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t video = nsIPermissionManager::UNKNOWN_ACTION;
  rv = mgr->TestExactPermissionFromPrincipal(principal, cameraPermission,
                                             &video);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  bool allowed = (video == nsIPermissionManager::ALLOW_ACTION);

  // Session permissions are removed after one use.
  if (allowed) {
    mgr->RemoveFromPrincipal(principal, cameraPermission);
  }

  return allowed;
}

mozilla::ipc::IPCResult
CamerasParent::RecvAllocateCaptureDevice(const CaptureEngine& aCapEngine,
                                         const nsCString& unique_id,
                                         const PrincipalInfo& aPrincipalInfo)
{
  LOG(("%s: Verifying permissions", __PRETTY_FUNCTION__));
  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> mainthread_runnable =
    media::NewRunnableFrom([self, aCapEngine, unique_id, aPrincipalInfo]() -> nsresult {
      // Verify whether the claimed origin has received permission
      // to use the camera, either persistently or this session (one shot).
      bool allowed = HasCameraPermission(aPrincipalInfo);
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
          auto engine = self->mEngines[aCapEngine].get();
          engine->CreateVideoCapture(numdev, unique_id.get());
          engine->WithEntry(numdev, [&error](VideoEngine::CaptureEntry& cap) {
            if (cap.VideoCapture()) {
              error = 0;
            }
          });
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
        self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
        return NS_OK;
        });
      self->DispatchToVideoCaptureThread(webrtc_runnable);
      return NS_OK;
    });
  NS_DispatchToMainThread(mainthread_runnable);
  return IPC_OK();
}

int
CamerasParent::ReleaseCaptureDevice(const CaptureEngine& aCapEngine,
                                    const int& capnum)
{
  int error = -1;
  if (auto engine = EnsureInitialized(aCapEngine)) {
    error = engine->ReleaseVideoCapture(capnum);
  }
  return error;
}

mozilla::ipc::IPCResult
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
      self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CamerasParent::RecvStartCapture(const CaptureEngine& aCapEngine,
                                const int& capnum,
                                const VideoCaptureCapability& ipcCaps)
{
  LOG((__PRETTY_FUNCTION__));

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
    media::NewRunnableFrom([self, aCapEngine, capnum, ipcCaps]() -> nsresult {
      LOG((__PRETTY_FUNCTION__));
      CallbackHelper** cbh;
      VideoEngine* engine = nullptr;
      int error = -1;
      if (self->EnsureInitialized(aCapEngine)) {
        cbh = self->mCallbacks.AppendElement(
          new CallbackHelper(static_cast<CaptureEngine>(aCapEngine), capnum, self));

        engine = self->mEngines[aCapEngine];
        engine->WithEntry(capnum, [capnum, &engine, &error, &ipcCaps, &cbh](VideoEngine::CaptureEntry& cap) {
          error = 0;
          webrtc::VideoCaptureCapability capability;
          capability.width = ipcCaps.width();
          capability.height = ipcCaps.height();
          capability.maxFPS = ipcCaps.maxFPS();
          capability.expectedCaptureDelay = ipcCaps.expectedCaptureDelay();
          capability.rawType = static_cast<webrtc::RawVideoType>(ipcCaps.rawType());
          capability.codecType = static_cast<webrtc::VideoCodecType>(ipcCaps.codecType());
          capability.interlaced = ipcCaps.interlaced();

          if (!error) {
            error = cap.VideoCapture()->StartCapture(capability);
          }
          if (!error) {
            engine->Startup();
            cap.VideoCapture()->RegisterCaptureDataCallback(static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(*cbh));
          }
        });
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
      self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
      return NS_OK;
    });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

void
CamerasParent::StopCapture(const CaptureEngine& aCapEngine,
                           const int& capnum)
{
  if (auto engine = EnsureInitialized(aCapEngine)) {
    engine->WithEntry(capnum,[capnum](VideoEngine::CaptureEntry& cap){
      if (cap.VideoCapture()) {
        cap.VideoCapture()->StopCapture();
        cap.VideoCapture()->DeRegisterCaptureDataCallback();
      }
    });
    // we're removing elements, iterate backwards
    for (size_t i = mCallbacks.Length(); i > 0; i--) {
      if (mCallbacks[i-1]->mCapEngine == aCapEngine
          && mCallbacks[i-1]->mStreamId == (uint32_t) capnum) {
        delete mCallbacks[i-1];
        mCallbacks.RemoveElementAt(i-1);
        break;
      }
    }
    engine->Shutdown();
  }
}

mozilla::ipc::IPCResult
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
    if (NS_FAILED(rv)) {
      return IPC_FAIL_NO_REASON(this);
    }
  } else {
    if (NS_SUCCEEDED(rv)) {
      if (!SendReplySuccess()) {
        return IPC_FAIL_NO_REASON(this);
      }
    } else {
      if (!SendReplyFailure()) {
        return IPC_FAIL_NO_REASON(this);
      }
    }
  }
  return IPC_OK();
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

mozilla::ipc::IPCResult
CamerasParent::RecvAllDone()
{
  LOG((__PRETTY_FUNCTION__));
  // Don't try to send anything to the child now
  mChildIsAlive = false;
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
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

  mPBackgroundEventTarget = GetCurrentThreadSerialEventTarget();
  MOZ_ASSERT(mPBackgroundEventTarget != nullptr,
             "GetCurrentThreadEventTarget failed");

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
}

CamerasParent::~CamerasParent()
{
  LOG(("~CamerasParent: %p", this));

#ifdef DEBUG
  // Verify we have shut down the webrtc engines, this is
  // supposed to happen in ActorDestroy.
  // That runnable takes a ref to us, so it must have finished
  // by the time we get here.
  for (int i = 0; i < CaptureEngine::MaxEngine; i++) {
    MOZ_ASSERT(!mEngines[i]);
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

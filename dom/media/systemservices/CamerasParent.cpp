/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasParent.h"
#include "MediaEngineSource.h"
#include "MediaUtils.h"
#include "VideoFrameUtils.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Unused.h"
#include "mozilla/Services.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "nsIPermissionManager.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

#include "common_video/libyuv/include/webrtc_libyuv.h"

#if defined(_WIN32)
#  include <process.h>
#  define getpid() _getpid()
#endif

#undef LOG
#undef LOG_VERBOSE
#undef LOG_ENABLED
mozilla::LazyLogModule gCamerasParentLog("CamerasParent");
#define LOG(...) \
  MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOG_VERBOSE(...) \
  MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasParentLog, mozilla::LogLevel::Debug)

namespace mozilla {
using media::MustGetShutdownBarrier;
using media::NewRunnableFrom;
namespace camera {

std::map<uint32_t, const char*> sDeviceUniqueIDs;
std::map<uint32_t, webrtc::VideoCaptureCapability> sAllRequestedCapabilities;

uint32_t ResolutionFeasibilityDistance(int32_t candidate, int32_t requested) {
  // The purpose of this function is to find a smallest resolution
  // which is larger than all requested capabilities.
  // Then we can use down-scaling to fulfill each request.

  MOZ_DIAGNOSTIC_ASSERT(candidate >= 0, "Candidate unexpectedly negative");
  MOZ_DIAGNOSTIC_ASSERT(requested >= 0, "Requested unexpectedly negative");

  if (candidate == 0) {
    // Treat width|height capability of 0 as "can do any".
    // This allows for orthogonal capabilities that are not in discrete steps.
    return 0;
  }

  uint32_t distance =
      std::abs(candidate - requested) * 1000 / std::max(candidate, requested);
  if (candidate >= requested) {
    // This is a good case, the candidate covers the requested resolution.
    return distance;
  }

  // This is a bad case, the candidate is lower than the requested resolution.
  // This is penalized with an added weight of 10000.
  return 10000 + distance;
}

uint32_t FeasibilityDistance(int32_t candidate, int32_t requested) {
  MOZ_DIAGNOSTIC_ASSERT(candidate >= 0, "Candidate unexpectedly negative");
  MOZ_DIAGNOSTIC_ASSERT(requested >= 0, "Requested unexpectedly negative");

  if (candidate == 0) {
    // Treat maxFPS capability of 0 as "can do any".
    // This allows for orthogonal capabilities that are not in discrete steps.
    return 0;
  }

  return std::abs(candidate - requested) * 1000 /
         std::max(candidate, requested);
}

StaticRefPtr<VideoEngine> CamerasParent::sEngines[CaptureEngine::MaxEngine];
int32_t CamerasParent::sNumOfOpenCamerasParentEngines = 0;
int32_t CamerasParent::sNumOfCamerasParents = 0;
base::Thread* CamerasParent::sVideoCaptureThread = nullptr;
Monitor* CamerasParent::sThreadMonitor = nullptr;
StaticMutex CamerasParent::sMutex;

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
  LOG("%s", __PRETTY_FUNCTION__);
  MOZ_ASSERT(mParent);

  RefPtr<InputObserver> self(this);
  RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self]() {
    if (self->mParent->IsShuttingDown()) {
      LOG("OnDeviceChanged failure: parent shutting down.");
      return NS_ERROR_FAILURE;
    }
    Unused << self->mParent->SendDeviceChange();
    return NS_OK;
  });

  nsIEventTarget* target = mParent->GetBackgroundEventTarget();
  MOZ_ASSERT(target != nullptr);
  target->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
};

class DeliverFrameRunnable : public mozilla::Runnable {
 public:
  DeliverFrameRunnable(CamerasParent* aParent, CaptureEngine aEngine,
                       uint32_t aStreamId, const webrtc::VideoFrame& aFrame,
                       const VideoFrameProperties& aProperties)
      : Runnable("camera::DeliverFrameRunnable"),
        mParent(aParent),
        mCapEngine(aEngine),
        mStreamId(aStreamId),
        mProperties(aProperties),
        mResult(0) {
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
                       uint32_t aStreamId, ShmemBuffer aBuffer,
                       VideoFrameProperties& aProperties)
      : Runnable("camera::DeliverFrameRunnable"),
        mParent(aParent),
        mCapEngine(aEngine),
        mStreamId(aStreamId),
        mBuffer(std::move(aBuffer)),
        mProperties(aProperties),
        mResult(0){};

  NS_IMETHOD Run() override {
    // runs on BackgroundEventTarget
    MOZ_ASSERT(GetCurrentSerialEventTarget() ==
               mParent->mPBackgroundEventTarget);
    if (mParent->IsShuttingDown()) {
      // Communication channel is being torn down
      mResult = 0;
      return NS_OK;
    }
    if (!mParent->DeliverFrameOverIPC(mCapEngine, mStreamId, std::move(mBuffer),
                                      mAlternateBuffer.get(), mProperties)) {
      mResult = -1;
    } else {
      mResult = 0;
    }
    return NS_OK;
  }

  int GetResult() { return mResult; }

 private:
  RefPtr<CamerasParent> mParent;
  CaptureEngine mCapEngine;
  uint32_t mStreamId;
  ShmemBuffer mBuffer;
  UniquePtr<unsigned char[]> mAlternateBuffer;
  VideoFrameProperties mProperties;
  int mResult;
};

NS_IMPL_ISUPPORTS(CamerasParent, nsIAsyncShutdownBlocker)

nsresult CamerasParent::DispatchToVideoCaptureThread(RefPtr<Runnable> event) {
  // Don't try to dispatch if we're already on the right thread.
  // There's a potential deadlock because the sThreadMonitor is likely
  // to be taken already.
  MonitorAutoLock lock(*sThreadMonitor);
  if (!sVideoCaptureThread) {
    LOG("Can't dispatch to video capture thread: thread not present");
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(sVideoCaptureThread->thread_id() != PlatformThread::CurrentId());

  sVideoCaptureThread->message_loop()->PostTask(event.forget());
  return NS_OK;
}

void CamerasParent::StopVideoCapture() {
  LOG("%s", __PRETTY_FUNCTION__);
  // Called when the actor is destroyed.
  ipc::AssertIsOnBackgroundThread();
  // Shut down the WebRTC stack (on the capture thread)
  RefPtr<CamerasParent> self(this);
  DebugOnly<nsresult> rv =
      DispatchToVideoCaptureThread(NewRunnableFrom([self]() {
        MonitorAutoLock lock(*(self->sThreadMonitor));
        self->CloseEngines();
        // After closing the WebRTC stack, clean up the
        // VideoCapture thread.
        base::Thread* thread = nullptr;
        if (sNumOfOpenCamerasParentEngines == 0 && self->sVideoCaptureThread) {
          thread = self->sVideoCaptureThread;
          self->sVideoCaptureThread = nullptr;
        }
        nsresult rv = NS_DispatchToMainThread(NewRunnableFrom([self, thread]() {
          if (thread) {
            thread->Stop();
            delete thread;
          }
          // May fail if already removed after RecvPCamerasConstructor().
          (void)MustGetShutdownBarrier()->RemoveBlocker(self);
          return NS_OK;
        }));
        MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "dispatch for video thread shutdown");
        return rv;
      }));
#ifdef DEBUG
  // It's ok for the dispatch to fail if the cleanup it has to do
  // has been done already.
  MOZ_ASSERT(NS_SUCCEEDED(rv) || !mWebRTCAlive);
#endif
}

int CamerasParent::DeliverFrameOverIPC(CaptureEngine capEng, uint32_t aStreamId,
                                       ShmemBuffer buffer,
                                       unsigned char* altbuffer,
                                       VideoFrameProperties& aProps) {
  // No ShmemBuffers were available, so construct one now of the right size
  // and copy into it. That is an extra copy, but we expect this to be
  // the exceptional case, because we just assured the next call *will* have a
  // buffer of the right size.
  if (altbuffer != nullptr) {
    // Get a shared memory buffer from the pool, at least size big
    ShmemBuffer shMemBuff = mShmemPool.Get(this, aProps.bufferSize());

    if (!shMemBuff.Valid()) {
      LOG("No usable Video shmem in DeliverFrame (out of buffers?)");
      // We can skip this frame if we run out of buffers, it's not a real error.
      return 0;
    }

    // get() and Size() check for proper alignment of the segment
    memcpy(shMemBuff.GetBytes(), altbuffer, aProps.bufferSize());

    if (!SendDeliverFrame(capEng, aStreamId, std::move(shMemBuff.Get()),
                          aProps)) {
      return -1;
    }
  } else {
    MOZ_ASSERT(buffer.Valid());
    // ShmemBuffer was available, we're all good. A single copy happened
    // in the original webrtc callback.
    if (!SendDeliverFrame(capEng, aStreamId, std::move(buffer.Get()), aProps)) {
      return -1;
    }
  }

  return 0;
}

ShmemBuffer CamerasParent::GetBuffer(size_t aSize) {
  return mShmemPool.GetIfAvailable(aSize);
}

void CallbackHelper::OnFrame(const webrtc::VideoFrame& aVideoFrame) {
  LOG_VERBOSE("%s", __PRETTY_FUNCTION__);
  RefPtr<DeliverFrameRunnable> runnable = nullptr;
  // Get frame properties
  camera::VideoFrameProperties properties;
  VideoFrameUtils::InitFrameBufferProperties(aVideoFrame, properties);
  // Get a shared memory buffer to copy the frame data into
  ShmemBuffer shMemBuffer = mParent->GetBuffer(properties.bufferSize());
  if (!shMemBuffer.Valid()) {
    // Either we ran out of buffers or they're not the right size yet
    LOG("Correctly sized Video shmem not available in DeliverFrame");
    // We will do the copy into a(n extra) temporary buffer inside
    // the DeliverFrameRunnable constructor.
  } else {
    // Shared memory buffers of the right size are available, do the copy here.
    VideoFrameUtils::CopyVideoFrameBuffers(
        shMemBuffer.GetBytes(), properties.bufferSize(), aVideoFrame);
    runnable = new DeliverFrameRunnable(mParent, mCapEngine, mStreamId,
                                        std::move(shMemBuffer), properties);
  }
  if (!runnable) {
    runnable = new DeliverFrameRunnable(mParent, mCapEngine, mStreamId,
                                        aVideoFrame, properties);
  }
  MOZ_ASSERT(mParent);
  nsIEventTarget* target = mParent->GetBackgroundEventTarget();
  MOZ_ASSERT(target != nullptr);
  target->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

mozilla::ipc::IPCResult CamerasParent::RecvReleaseFrame(
    mozilla::ipc::Shmem&& s) {
  mShmemPool.Put(ShmemBuffer(s));
  return IPC_OK();
}

bool CamerasParent::SetupEngine(CaptureEngine aCapEngine) {
  LOG("%s", __PRETTY_FUNCTION__);
  StaticRefPtr<VideoEngine>& engine = sEngines[aCapEngine];

  if (!engine) {
    CaptureDeviceType captureDeviceType = CaptureDeviceType::Camera;
    switch (aCapEngine) {
      case ScreenEngine:
        captureDeviceType = CaptureDeviceType::Screen;
        break;
      case BrowserEngine:
        captureDeviceType = CaptureDeviceType::Browser;
        break;
      case WinEngine:
        captureDeviceType = CaptureDeviceType::Window;
        break;
      case CameraEngine:
        captureDeviceType = CaptureDeviceType::Camera;
        break;
      default:
        LOG("Invalid webrtc Video engine");
        MOZ_CRASH();
        break;
    }

    engine = VideoEngine::Create(captureDeviceType);

    if (!engine) {
      LOG("VideoEngine::Create failed");
      return false;
    }
  }

  if (aCapEngine == CameraEngine && !mCameraObserver) {
    mCameraObserver = new InputObserver(this);
    auto device_info = engine->GetOrCreateVideoCaptureDeviceInfo();
    MOZ_ASSERT(device_info);
    if (device_info) {
      device_info->RegisterVideoInputFeedBack(mCameraObserver);
    }
  }

  return true;
}

void CamerasParent::CloseEngines() {
  sThreadMonitor->AssertCurrentThreadOwns();
  LOG("%s", __PRETTY_FUNCTION__);
  if (!mWebRTCAlive) {
    return;
  }
  MOZ_ASSERT(sVideoCaptureThread->thread_id() == PlatformThread::CurrentId());

  // Stop the callers
  while (mCallbacks.Length()) {
    auto capEngine = mCallbacks[0]->mCapEngine;
    auto streamNum = mCallbacks[0]->mStreamId;
    LOG("Forcing shutdown of engine %d, capturer %d", capEngine, streamNum);
    StopCapture(capEngine, streamNum);
    Unused << ReleaseCapture(capEngine, streamNum);
  }

  StaticRefPtr<VideoEngine>& engine = sEngines[CameraEngine];
  if (engine && mCameraObserver) {
    auto device_info = engine->GetOrCreateVideoCaptureDeviceInfo();
    MOZ_ASSERT(device_info);
    if (device_info) {
      device_info->DeRegisterVideoInputFeedBack(mCameraObserver);
    }
    mCameraObserver = nullptr;
  }

  // CloseEngines() is protected by sThreadMonitor
  sNumOfOpenCamerasParentEngines--;
  if (sNumOfOpenCamerasParentEngines == 0) {
    for (StaticRefPtr<VideoEngine>& engine : sEngines) {
      if (engine) {
        VideoEngine::Delete(engine);
        engine = nullptr;
      }
    }
  }

  mWebRTCAlive = false;
}

VideoEngine* CamerasParent::EnsureInitialized(int aEngine) {
  LOG_VERBOSE("%s", __PRETTY_FUNCTION__);
  // We're shutting down, don't try to do new WebRTC ops.
  if (!mWebRTCAlive) {
    return nullptr;
  }
  CaptureEngine capEngine = static_cast<CaptureEngine>(aEngine);
  if (!SetupEngine(capEngine)) {
    LOG("CamerasParent failed to initialize engine");
    return nullptr;
  }

  return sEngines[aEngine];
}

// Dispatch the runnable to do the camera operation on the
// specific Cameras thread, preventing us from blocking, and
// chain a runnable to send back the result on the IPC thread.
// It would be nice to get rid of the code duplication here,
// perhaps via Promises.
mozilla::ipc::IPCResult CamerasParent::RecvNumberOfCaptureDevices(
    const CaptureEngine& aCapEngine) {
  LOG("%s", __PRETTY_FUNCTION__);
  LOG("CaptureEngine=%d", aCapEngine);
  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom([self, aCapEngine]() {
    int num = -1;
    if (auto engine = self->EnsureInitialized(aCapEngine)) {
      if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
        num = devInfo->NumberOfDevices();
      }
    }
    RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self, num]() {
      if (!self->mChildIsAlive) {
        LOG("RecvNumberOfCaptureDevices failure: child not alive");
        return NS_ERROR_FAILURE;
      }

      if (num < 0) {
        LOG("RecvNumberOfCaptureDevices couldn't find devices");
        Unused << self->SendReplyFailure();
        return NS_ERROR_FAILURE;
      }

      LOG("RecvNumberOfCaptureDevices: %d", num);
      Unused << self->SendReplyNumberOfCaptureDevices(num);
      return NS_OK;
    });
    self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
    return NS_OK;
  });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasParent::RecvEnsureInitialized(
    const CaptureEngine& aCapEngine) {
  LOG("%s", __PRETTY_FUNCTION__);

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom([self, aCapEngine]() {
    bool result = self->EnsureInitialized(aCapEngine);

    RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self, result]() {
      if (!self->mChildIsAlive) {
        LOG("RecvEnsureInitialized: child not alive");
        return NS_ERROR_FAILURE;
      }

      if (!result) {
        LOG("RecvEnsureInitialized failed");
        Unused << self->SendReplyFailure();
        return NS_ERROR_FAILURE;
      }

      LOG("RecvEnsureInitialized succeeded");
      Unused << self->SendReplySuccess();
      return NS_OK;
    });
    self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
    return NS_OK;
  });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasParent::RecvNumberOfCapabilities(
    const CaptureEngine& aCapEngine, const nsACString& unique_id) {
  LOG("%s", __PRETTY_FUNCTION__);
  LOG("Getting caps for %s", PromiseFlatCString(unique_id).get());

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
      NewRunnableFrom([self, unique_id = nsCString(unique_id), aCapEngine]() {
        int num = -1;
        if (auto engine = self->EnsureInitialized(aCapEngine)) {
          if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
            num = devInfo->NumberOfCapabilities(unique_id.get());
          }
        }
        RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self, num]() {
          if (!self->mChildIsAlive) {
            LOG("RecvNumberOfCapabilities: child not alive");
            return NS_ERROR_FAILURE;
          }

          if (num < 0) {
            LOG("RecvNumberOfCapabilities couldn't find capabilities");
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }

          LOG("RecvNumberOfCapabilities: %d", num);
          Unused << self->SendReplyNumberOfCapabilities(num);
          return NS_OK;
        });
        self->mPBackgroundEventTarget->Dispatch(ipc_runnable,
                                                NS_DISPATCH_NORMAL);
        return NS_OK;
      });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasParent::RecvGetCaptureCapability(
    const CaptureEngine& aCapEngine, const nsACString& unique_id,
    const int& num) {
  LOG("%s", __PRETTY_FUNCTION__);
  LOG("RecvGetCaptureCapability: %s %d", PromiseFlatCString(unique_id).get(),
      num);

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom(
      [self, unique_id = nsCString(unique_id), aCapEngine, num]() {
        webrtc::VideoCaptureCapability webrtcCaps;
        int error = -1;
        if (auto engine = self->EnsureInitialized(aCapEngine)) {
          if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
            error = devInfo->GetCapability(unique_id.get(), num, webrtcCaps);
          }

          if (!error && aCapEngine == CameraEngine) {
            auto iter = self->mAllCandidateCapabilities.find(unique_id);
            if (iter == self->mAllCandidateCapabilities.end()) {
              std::map<uint32_t, webrtc::VideoCaptureCapability>
                  candidateCapabilities;
              candidateCapabilities.emplace(num, webrtcCaps);
              self->mAllCandidateCapabilities.emplace(nsCString(unique_id),
                                                      candidateCapabilities);
            } else {
              (iter->second).emplace(num, webrtcCaps);
            }
          }
        }
        RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self, webrtcCaps,
                                                            error]() {
          if (!self->mChildIsAlive) {
            LOG("RecvGetCaptureCapability: child not alive");
            return NS_ERROR_FAILURE;
          }
          VideoCaptureCapability capCap(
              webrtcCaps.width, webrtcCaps.height, webrtcCaps.maxFPS,
              static_cast<int>(webrtcCaps.videoType), webrtcCaps.interlaced);
          LOG("Capability: %u %u %u %d %d", webrtcCaps.width, webrtcCaps.height,
              webrtcCaps.maxFPS, static_cast<int>(webrtcCaps.videoType),
              webrtcCaps.interlaced);
          if (error) {
            LOG("RecvGetCaptureCapability: reply failure");
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
          Unused << self->SendReplyGetCaptureCapability(capCap);
          return NS_OK;
        });
        self->mPBackgroundEventTarget->Dispatch(ipc_runnable,
                                                NS_DISPATCH_NORMAL);
        return NS_OK;
      });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasParent::RecvGetCaptureDevice(
    const CaptureEngine& aCapEngine, const int& aDeviceIndex) {
  LOG("%s", __PRETTY_FUNCTION__);

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom([self, aCapEngine,
                                                      aDeviceIndex]() {
    char deviceName[MediaEngineSource::kMaxDeviceNameLength];
    char deviceUniqueId[MediaEngineSource::kMaxUniqueIdLength];
    nsCString name;
    nsCString uniqueId;
    pid_t devicePid = 0;
    int error = -1;
    if (auto engine = self->EnsureInitialized(aCapEngine)) {
      if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo()) {
        error = devInfo->GetDeviceName(
            aDeviceIndex, deviceName, sizeof(deviceName), deviceUniqueId,
            sizeof(deviceUniqueId), nullptr, 0, &devicePid);
      }
    }
    if (!error) {
      name.Assign(deviceName);
      uniqueId.Assign(deviceUniqueId);
    }
    RefPtr<nsIRunnable> ipc_runnable =
        NewRunnableFrom([self, error, name, uniqueId, devicePid]() {
          if (!self->mChildIsAlive) {
            return NS_ERROR_FAILURE;
          }
          if (error) {
            LOG("GetCaptureDevice failed: %d", error);
            Unused << self->SendReplyFailure();
            return NS_ERROR_FAILURE;
          }
          bool scary = (devicePid == getpid());

          LOG("Returning %s name %s id (pid = %d)%s", name.get(),
              uniqueId.get(), devicePid, (scary ? " (scary)" : ""));
          Unused << self->SendReplyGetCaptureDevice(name, uniqueId, scary);
          return NS_OK;
        });
    self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
    return NS_OK;
  });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

// Find out whether the given window with id has permission to use the
// camera. If the permission is not persistent, we'll make it a one-shot by
// removing the (session) permission.
static bool HasCameraPermission(const uint64_t& aWindowId) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<dom::WindowGlobalParent> window =
      dom::WindowGlobalParent::GetByInnerWindowId(aWindowId);
  if (!window) {
    // Could not find window by id
    return false;
  }

  // If we delegate permission from first party, we should use the top level
  // window
  if (StaticPrefs::permissions_delegation_enabled()) {
    RefPtr<dom::BrowsingContext> topBC = window->BrowsingContext()->Top();
    window = topBC->Canonical()->GetCurrentWindowGlobal();
  }

  // Return false if the window is not the currently-active window for its
  // BrowsingContext.
  if (!window || !window->IsCurrentGlobal()) {
    return false;
  }

  nsIPrincipal* principal = window->DocumentPrincipal();
  if (principal->GetIsNullPrincipal()) {
    return false;
  }

  if (principal->IsSystemPrincipal()) {
    return true;
  }

  MOZ_ASSERT(principal->GetIsContentPrincipal());

  nsresult rv;
  // Name used with nsIPermissionManager
  static const nsLiteralCString cameraPermission = "MediaManagerVideo"_ns;
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

mozilla::ipc::IPCResult CamerasParent::RecvAllocateCapture(
    const CaptureEngine& aCapEngine, const nsACString& unique_id,
    const uint64_t& aWindowID) {
  LOG("%s: Verifying permissions", __PRETTY_FUNCTION__);
  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> mainthread_runnable = NewRunnableFrom(
      [self, aCapEngine, unique_id = nsCString(unique_id), aWindowID]() {
        // Verify whether the claimed origin has received permission
        // to use the camera, either persistently or this session (one shot).
        bool allowed = HasCameraPermission(aWindowID);
        if (!allowed) {
          // Developer preference for turning off permission check.
          if (Preferences::GetBool("media.navigator.permission.disabled",
                                   false)) {
            allowed = true;
            LOG("No permission but checks are disabled");
          } else {
            LOG("No camera permission for this origin");
          }
        }
        // After retrieving the permission (or not) on the main thread,
        // bounce to the WebRTC thread to allocate the device (or not),
        // then bounce back to the IPC thread for the reply to content.
        RefPtr<Runnable> webrtc_runnable =
            NewRunnableFrom([self, allowed, aCapEngine, unique_id]() {
              int captureId = -1;
              int error = -1;
              if (allowed && self->EnsureInitialized(aCapEngine)) {
                StaticRefPtr<VideoEngine>& engine = self->sEngines[aCapEngine];
                captureId = engine->CreateVideoCapture(unique_id.get());
                engine->WithEntry(captureId,
                                  [&error](VideoEngine::CaptureEntry& cap) {
                                    if (cap.VideoCapture()) {
                                      error = 0;
                                    }
                                  });
              }
              RefPtr<nsIRunnable> ipc_runnable =
                  NewRunnableFrom([self, captureId, error]() {
                    if (!self->mChildIsAlive) {
                      LOG("RecvAllocateCapture: child not alive");
                      return NS_ERROR_FAILURE;
                    }

                    if (error) {
                      Unused << self->SendReplyFailure();
                      LOG("RecvAllocateCapture: WithEntry error");
                      return NS_ERROR_FAILURE;
                    }

                    LOG("Allocated device nr %d", captureId);
                    Unused << self->SendReplyAllocateCapture(captureId);
                    return NS_OK;
                  });
              self->mPBackgroundEventTarget->Dispatch(ipc_runnable,
                                                      NS_DISPATCH_NORMAL);
              return NS_OK;
            });
        self->DispatchToVideoCaptureThread(webrtc_runnable);
        return NS_OK;
      });
  NS_DispatchToMainThread(mainthread_runnable);
  return IPC_OK();
}

int CamerasParent::ReleaseCapture(const CaptureEngine& aCapEngine,
                                  int aCaptureId) {
  int error = -1;
  if (auto engine = EnsureInitialized(aCapEngine)) {
    error = engine->ReleaseVideoCapture(aCaptureId);
  }
  return error;
}

mozilla::ipc::IPCResult CamerasParent::RecvReleaseCapture(
    const CaptureEngine& aCapEngine, const int& aCaptureId) {
  LOG("%s", __PRETTY_FUNCTION__);
  LOG("RecvReleaseCamera device nr %d", aCaptureId);

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom([self, aCapEngine,
                                                      aCaptureId]() {
    int error = self->ReleaseCapture(aCapEngine, aCaptureId);
    RefPtr<nsIRunnable> ipc_runnable =
        NewRunnableFrom([self, error, aCaptureId]() {
          if (!self->mChildIsAlive) {
            LOG("RecvReleaseCapture: child not alive");
            return NS_ERROR_FAILURE;
          }

          if (error) {
            Unused << self->SendReplyFailure();
            LOG("RecvReleaseCapture: Failed to free device nr %d", aCaptureId);
            return NS_ERROR_FAILURE;
          }

          Unused << self->SendReplySuccess();
          LOG("Freed device nr %d", aCaptureId);
          return NS_OK;
        });
    self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
    return NS_OK;
  });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasParent::RecvStartCapture(
    const CaptureEngine& aCapEngine, const int& aCaptureId,
    const VideoCaptureCapability& ipcCaps) {
  LOG("%s", __PRETTY_FUNCTION__);

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom([self, aCapEngine,
                                                      aCaptureId, ipcCaps]() {
    LOG("%s", __PRETTY_FUNCTION__);
    CallbackHelper** cbh;
    int error = -1;
    if (self->EnsureInitialized(aCapEngine)) {
      cbh = self->mCallbacks.AppendElement(new CallbackHelper(
          static_cast<CaptureEngine>(aCapEngine), aCaptureId, self));

      self->sEngines[aCapEngine]->WithEntry(
          aCaptureId, [&aCaptureId, &aCapEngine, &error, &ipcCaps, &cbh,
                       self](VideoEngine::CaptureEntry& cap) {
            webrtc::VideoCaptureCapability capability;
            capability.width = ipcCaps.width();
            capability.height = ipcCaps.height();
            capability.maxFPS = ipcCaps.maxFPS();
            capability.videoType =
                static_cast<webrtc::VideoType>(ipcCaps.videoType());
            capability.interlaced = ipcCaps.interlaced();

            MOZ_DIAGNOSTIC_ASSERT(sDeviceUniqueIDs.find(aCaptureId) ==
                                  sDeviceUniqueIDs.end());
            sDeviceUniqueIDs.emplace(aCaptureId,
                                     cap.VideoCapture()->CurrentDeviceName());

            MOZ_DIAGNOSTIC_ASSERT(sAllRequestedCapabilities.find(aCaptureId) ==
                                  sAllRequestedCapabilities.end());
            sAllRequestedCapabilities.emplace(aCaptureId, capability);

            if (aCapEngine == CameraEngine) {
              for (const auto& it : sDeviceUniqueIDs) {
                if (strcmp(it.second,
                           cap.VideoCapture()->CurrentDeviceName()) == 0) {
                  capability.width =
                      std::max(capability.width,
                               sAllRequestedCapabilities[it.first].width);
                  capability.height =
                      std::max(capability.height,
                               sAllRequestedCapabilities[it.first].height);
                  capability.maxFPS =
                      std::max(capability.maxFPS,
                               sAllRequestedCapabilities[it.first].maxFPS);
                }
              }

              auto candidateCapabilities = self->mAllCandidateCapabilities.find(
                  nsCString(cap.VideoCapture()->CurrentDeviceName()));
              if ((candidateCapabilities !=
                   self->mAllCandidateCapabilities.end()) &&
                  (!candidateCapabilities->second.empty())) {
                int32_t minIdx = -1;
                uint64_t minDistance = UINT64_MAX;

                for (auto& candidateCapability :
                     candidateCapabilities->second) {
                  if (candidateCapability.second.videoType !=
                      capability.videoType) {
                    continue;
                  }
                  // The first priority is finding a suitable resolution.
                  // So here we raise the weight of width and height
                  uint64_t distance =
                      uint64_t(ResolutionFeasibilityDistance(
                          candidateCapability.second.width, capability.width)) +
                      uint64_t(ResolutionFeasibilityDistance(
                          candidateCapability.second.height,
                          capability.height)) +
                      uint64_t(
                          FeasibilityDistance(candidateCapability.second.maxFPS,
                                              capability.maxFPS));
                  if (distance < minDistance) {
                    minIdx = candidateCapability.first;
                    minDistance = distance;
                  }
                }
                MOZ_ASSERT(minIdx != -1);
                capability = candidateCapabilities->second[minIdx];
              }
            } else if (aCapEngine == ScreenEngine ||
                       aCapEngine == BrowserEngine || aCapEngine == WinEngine) {
              for (const auto& it : sDeviceUniqueIDs) {
                if (strcmp(it.second,
                           cap.VideoCapture()->CurrentDeviceName()) == 0) {
                  capability.maxFPS =
                      std::max(capability.maxFPS,
                               sAllRequestedCapabilities[it.first].maxFPS);
                }
              }
            }

            error = cap.VideoCapture()->StartCapture(capability);

            if (!error) {
              cap.VideoCapture()->RegisterCaptureDataCallback(
                  static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(
                      *cbh));
            } else {
              sDeviceUniqueIDs.erase(aCaptureId);
              sAllRequestedCapabilities.erase(aCaptureId);
            }
          });
    }
    RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self, error]() {
      if (!self->mChildIsAlive) {
        LOG("RecvStartCapture failure: child is not alive");
        return NS_ERROR_FAILURE;
      }

      if (!error) {
        Unused << self->SendReplySuccess();
        return NS_OK;
      }

      LOG("RecvStartCapture failure: StartCapture failed");
      Unused << self->SendReplyFailure();
      return NS_ERROR_FAILURE;
    });
    self->mPBackgroundEventTarget->Dispatch(ipc_runnable, NS_DISPATCH_NORMAL);
    return NS_OK;
  });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult CamerasParent::RecvFocusOnSelectedSource(
    const CaptureEngine& aCapEngine, const int& aCaptureId) {
  LOG("%s", __PRETTY_FUNCTION__);
  RefPtr<Runnable> webrtc_runnable = NewRunnableFrom(
      [self = RefPtr<CamerasParent>(this), aCapEngine, aCaptureId]() {
        if (auto engine = self->EnsureInitialized(aCapEngine)) {
          engine->WithEntry(aCaptureId, [self](VideoEngine::CaptureEntry& cap) {
            if (cap.VideoCapture()) {
              bool result = cap.VideoCapture()->FocusOnSelectedSource();
              RefPtr<nsIRunnable> ipc_runnable = NewRunnableFrom([self,
                                                                  result]() {
                if (!self->mChildIsAlive) {
                  LOG("RecvFocusOnSelectedSource failure: child is not alive");
                  return NS_ERROR_FAILURE;
                }

                if (result) {
                  Unused << self->SendReplySuccess();
                  return NS_OK;
                }

                Unused << self->SendReplyFailure();
                LOG("RecvFocusOnSelectedSource failure.");
                return NS_ERROR_FAILURE;
              });
              self->mPBackgroundEventTarget->Dispatch(ipc_runnable,
                                                      NS_DISPATCH_NORMAL);
            }
          });
        }
        LOG("RecvFocusOnSelectedSource CameraParent not initialized");
        return NS_ERROR_FAILURE;
      });
  DispatchToVideoCaptureThread(webrtc_runnable);
  return IPC_OK();
}

void CamerasParent::StopCapture(const CaptureEngine& aCapEngine,
                                int aCaptureId) {
  if (auto engine = EnsureInitialized(aCapEngine)) {
    // we're removing elements, iterate backwards
    for (size_t i = mCallbacks.Length(); i > 0; i--) {
      if (mCallbacks[i - 1]->mCapEngine == aCapEngine &&
          mCallbacks[i - 1]->mStreamId == (uint32_t)aCaptureId) {
        CallbackHelper* cbh = mCallbacks[i - 1];
        engine->WithEntry(aCaptureId, [cbh, &aCaptureId](
                                          VideoEngine::CaptureEntry& cap) {
          if (cap.VideoCapture()) {
            cap.VideoCapture()->DeRegisterCaptureDataCallback(
                static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(cbh));
            cap.VideoCapture()->StopCaptureIfAllClientsClose();

            sDeviceUniqueIDs.erase(aCaptureId);
            sAllRequestedCapabilities.erase(aCaptureId);
          }
        });

        delete mCallbacks[i - 1];
        mCallbacks.RemoveElementAt(i - 1);
        break;
      }
    }
  }
}

mozilla::ipc::IPCResult CamerasParent::RecvStopCapture(
    const CaptureEngine& aCapEngine, const int& aCaptureId) {
  LOG("%s", __PRETTY_FUNCTION__);

  RefPtr<CamerasParent> self(this);
  RefPtr<Runnable> webrtc_runnable =
      NewRunnableFrom([self, aCapEngine, aCaptureId]() {
        self->StopCapture(aCapEngine, aCaptureId);
        return NS_OK;
      });
  nsresult rv = DispatchToVideoCaptureThread(webrtc_runnable);
  if (!self->mChildIsAlive) {
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

void CamerasParent::StopIPC() {
  MOZ_ASSERT(!mDestroyed);
  // Release shared memory now, it's our last chance
  mShmemPool.Cleanup(this);
  // We don't want to receive callbacks or anything if we can't
  // forward them anymore anyway.
  mChildIsAlive = false;
  mDestroyed = true;
}

void CamerasParent::ActorDestroy(ActorDestroyReason aWhy) {
  // No more IPC from here
  LOG("%s", __PRETTY_FUNCTION__);
  StopIPC();
  // Shut down WebRTC (if we're not in full shutdown, else this
  // will already have happened)
  StopVideoCapture();
}

nsString CamerasParent::GetNewName() {
  static volatile uint64_t counter = 0;
  nsString name(u"CamerasParent "_ns);
  name.AppendInt(++counter);
  return name;
}

NS_IMETHODIMP CamerasParent::BlockShutdown(nsIAsyncShutdownClient*) {
  mPBackgroundEventTarget->Dispatch(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)]() {
        // Send__delete() can return failure if AddBlocker() registered this
        // CamerasParent while RecvPCamerasConstructor() called Send__delete()
        // because it noticed that AppShutdown had started.
        (void)Send__delete__(self);
      }));
  return NS_OK;
}

CamerasParent::CamerasParent()
    : mName(GetNewName()),
      mShmemPool(CaptureEngine::MaxEngine),
      mPBackgroundEventTarget(GetCurrentSerialEventTarget()),
      mChildIsAlive(true),
      mDestroyed(false),
      mWebRTCAlive(false) {
  MOZ_ASSERT(mPBackgroundEventTarget != nullptr,
             "GetCurrentThreadEventTarget failed");
  LOG("CamerasParent: %p", this);
  StaticMutexAutoLock slock(sMutex);

  if (sNumOfCamerasParents++ == 0) {
    sThreadMonitor = new Monitor("CamerasParent::sThreadMonitor");
  }
}

// RecvPCamerasConstructor() is used because IPC messages, for
// Send__delete__(), cannot be sent from AllocPCamerasParent().
ipc::IPCResult CamerasParent::RecvPCamerasConstructor() {
  ipc::AssertIsOnBackgroundThread();

  // A shutdown blocker must be added if sNumOfOpenCamerasParentEngines might
  // be incremented to indicate ownership of an sVideoCaptureThread.
  // If this task were queued after checking !IsInOrBeyond(AppShutdown), then
  // shutdown may have proceeded on the main thread and so the task may run
  // too late to add the blocker.
  // Don't dispatch from the constructor a runnable that may toggle the
  // reference count, because the IPC thread does not get a reference until
  // after the constructor returns.
  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)]() {
        nsresult rv = MustGetShutdownBarrier()->AddBlocker(
            self, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, u""_ns);
        LOG("AddBlocker returned 0x%x", static_cast<unsigned>(rv));
        // AddBlocker() will fail if called after all
        // AsyncShutdown.profileBeforeChange conditions have resolved or been
        // removed.
        //
        // The success of this AddBlocker() call is expected when an
        // sVideoCaptureThread is created based on the assumption that at
        // least one condition (e.g. nsIAsyncShutdownBlocker) added with
        // AsyncShutdown.profileBeforeChange.addBlocker() will not resolve or
        // be removed until it has queued a task and that task has run.
        // (AyncShutdown.jsm's Spinner#observe() makes a similar assumption
        // when it calls processNextEvent(), assuming that there will be some
        // other event generated, before checking whether its Barrier.wait()
        // promise has resolved.)
        //
        // If AppShutdown::IsInOrBeyond(AppShutdown) returned false,
        // then this main thread task was queued before AppShutdown's
        //   sCurrentShutdownPhase is set to AppShutdown,
        // which is before profile-before-change is notified,
        // which is when AsyncShutdown conditions are run,
        // which is when one condition would queue a task to resolve the
        //   condition or remove the blocker.
        // That task runs after this task and before AsyncShutdown prevents
        //   further conditions being added through AddBlocker().
        MOZ_ASSERT(NS_SUCCEEDED(rv) || !self->mWebRTCAlive);
      }));

  // AsyncShutdown barriers are available only for ShutdownPhases as late as
  // XPCOMWillShutdown.  The IPC background thread shuts down during
  // XPCOMShutdownThreads, so actors may be created when AsyncShutdown
  // barriers are no longer available.
  // IsInOrBeyond() checks sCurrentShutdownPhase, which is atomic.
  // ShutdownPhase::AppShutdown corresponds to profileBeforeChange used by
  // MustGetShutdownBarrier() in the parent process.
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdown)) {
    // The usual blocker removal path depends on the existence of the
    // `sVideoCaptureThread`, which is not ensured.  Queue removal now.
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(__func__, [self = RefPtr(this)]() {
          // May fail if AddBlocker() failed.
          (void)MustGetShutdownBarrier()->RemoveBlocker(self);
        }));
    return Send__delete__(this) ? IPC_OK() : IPC_FAIL(this, "Failed to send");
  }

  LOG("Spinning up WebRTC Cameras Thread");
  MonitorAutoLock lock(*sThreadMonitor);
  if (sVideoCaptureThread == nullptr) {
    MOZ_ASSERT(sNumOfOpenCamerasParentEngines == 0);
    sVideoCaptureThread = new base::Thread("VideoCapture");
    base::Thread::Options options;
#if defined(_WIN32)
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINUITHREAD;
#else
    options.message_loop_type = MessageLoop::TYPE_MOZILLA_NONMAINTHREAD;
#endif
    if (!sVideoCaptureThread->StartWithOptions(options)) {
      MOZ_CRASH();
    }
  }
  mWebRTCAlive = true;
  sNumOfOpenCamerasParentEngines++;
  return IPC_OK();
}

CamerasParent::~CamerasParent() {
  LOG("~CamerasParent: %p", this);
  StaticMutexAutoLock slock(sMutex);
  if (--sNumOfCamerasParents == 0) {
    delete sThreadMonitor;
    sThreadMonitor = nullptr;
  }
}

already_AddRefed<CamerasParent> CamerasParent::Create() {
  mozilla::ipc::AssertIsOnBackgroundThread();
  return MakeAndAddRef<CamerasParent>();
}

}  // namespace camera
}  // namespace mozilla

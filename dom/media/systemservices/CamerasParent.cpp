/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasParent.h"

#include <atomic>
#include "MediaEngineSource.h"
#include "PerformanceRecorder.h"
#include "VideoFrameUtils.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Unused.h"
#include "mozilla/Services.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "nsIPermissionManager.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"

#include "api/video/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"

#if defined(WEBRTC_USE_PIPEWIRE)
#  include "video_engine/pipewire_camera_impl.h"
#endif

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
#define LOG_FUNCTION()                                 \
  MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Debug, \
          ("CamerasParent(%p)::%s", this, __func__))
#define LOG_VERBOSE(...) \
  MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasParentLog, mozilla::LogLevel::Debug)

namespace mozilla {
using media::ShutdownBlockingTicket;
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

class CamerasParent::VideoEngineArray
    : public media::Refcountable<nsTArray<RefPtr<VideoEngine>>> {};

// Singleton video engines. The sEngines RefPtr is IPC background thread only
// and outlives the CamerasParent instances. The array elements are video
// capture thread only.
using VideoEngineArray = CamerasParent::VideoEngineArray;
static StaticRefPtr<VideoEngineArray> sEngines;
// Number of CamerasParents instances in the current process for which
// mVideoCaptureThread has been set. IPC background thread only.
static int32_t sNumCamerasParents = 0;
// Video processing thread - where webrtc.org capturer code runs. Outlives the
// CamerasParent instances. IPC background thread only.
static StaticRefPtr<nsIThread> sVideoCaptureThread;
// VideoCaptureOptions holding PipeWire session connected to PipeWire socket
// using the file descriptor we received from camera portal request. Set
// initially by IPC background thread only and later accessed by video capture
// thread only. Outlives the CamerasParent instances.
static StaticAutoPtr<webrtc::VideoCaptureOptions> sVideoCaptureOptions;

static already_AddRefed<nsISerialEventTarget>
MakeAndAddRefVideoCaptureThreadAndSingletons() {
  ipc::AssertIsOnBackgroundThread();

  MOZ_ASSERT_IF(sVideoCaptureThread, sNumCamerasParents > 0);
  MOZ_ASSERT_IF(!sVideoCaptureThread, sNumCamerasParents == 0);

  if (!sVideoCaptureThread) {
    LOG("Spinning up WebRTC Cameras Thread");
    nsIThreadManager::ThreadCreationOptions options;
#ifdef XP_WIN
    // Windows desktop capture needs a UI thread
    options.isUiThread = true;
#endif
    nsCOMPtr<nsIThread> videoCaptureThread;
    if (NS_FAILED(NS_NewNamedThread("VideoCapture",
                                    getter_AddRefs(videoCaptureThread), nullptr,
                                    options))) {
      return nullptr;
    }
    sVideoCaptureThread = videoCaptureThread.forget();

    sEngines = MakeRefPtr<VideoEngineArray>();
    sEngines->AppendElements(CaptureEngine::MaxEngine);
  }

  ++sNumCamerasParents;
  return do_AddRef(sVideoCaptureThread);
}

static void ReleaseVideoCaptureThreadAndSingletons() {
  ipc::AssertIsOnBackgroundThread();

  if (--sNumCamerasParents > 0) {
    // Other CamerasParent instances are using the singleton classes.
    return;
  }

  MOZ_ASSERT(sNumCamerasParents == 0, "Double release!");

  // No other CamerasParent instances alive. Clean up.
  LOG("Shutting down VideoEngines and the VideoCapture thread");
  MOZ_ALWAYS_SUCCEEDS(sVideoCaptureThread->Dispatch(
      NS_NewRunnableFunction(__func__, [engines = RefPtr(sEngines.forget())] {
        for (RefPtr<VideoEngine>& engine : *engines) {
          if (engine) {
            VideoEngine::Delete(engine);
            engine = nullptr;
          }
        }
      })));

  MOZ_ALWAYS_SUCCEEDS(RefPtr(sVideoCaptureThread.forget())->AsyncShutdown());
}

// 3 threads are involved in this code:
// - the main thread for some setups, and occassionally for video capture setup
//   calls that don't work correctly elsewhere.
// - the IPC thread on which PBackground is running and which receives and
//   sends messages
// - a thread which will execute the actual (possibly slow) camera access
//   called "VideoCapture". On Windows this is a thread with an event loop
//   suitable for UI access.

void CamerasParent::OnDeviceChange() {
  LOG_FUNCTION();

  mPBackgroundEventTarget->Dispatch(
      NS_NewRunnableFunction(__func__, [this, self = RefPtr(this)]() {
        if (IsShuttingDown()) {
          LOG("OnDeviceChanged failure: parent shutting down.");
          return;
        }
        Unused << SendDeviceChange();
      }));
};

class DeliverFrameRunnable : public mozilla::Runnable {
 public:
  DeliverFrameRunnable(CamerasParent* aParent, CaptureEngine aEngine,
                       uint32_t aStreamId, const TrackingId& aTrackingId,
                       const webrtc::VideoFrame& aFrame,
                       const VideoFrameProperties& aProperties)
      : Runnable("camera::DeliverFrameRunnable"),
        mParent(aParent),
        mCapEngine(aEngine),
        mStreamId(aStreamId),
        mTrackingId(aTrackingId),
        mProperties(aProperties),
        mResult(0) {
    // No ShmemBuffer (of the right size) was available, so make an
    // extra buffer here.  We have no idea when we are going to run and
    // it will be potentially long after the webrtc frame callback has
    // returned, so the copy needs to be no later than here.
    // We will need to copy this back into a Shmem later on so we prefer
    // using ShmemBuffers to avoid the extra copy.
    PerformanceRecorder<CopyVideoStage> rec(
        "CamerasParent::VideoFrameToAltBuffer"_ns, aTrackingId, aFrame.width(),
        aFrame.height());
    mAlternateBuffer.reset(new unsigned char[aProperties.bufferSize()]);
    VideoFrameUtils::CopyVideoFrameBuffers(mAlternateBuffer.get(),
                                           aProperties.bufferSize(), aFrame);
    rec.Record();
  }

  DeliverFrameRunnable(CamerasParent* aParent, CaptureEngine aEngine,
                       uint32_t aStreamId, const TrackingId& aTrackingId,
                       ShmemBuffer aBuffer, VideoFrameProperties& aProperties)
      : Runnable("camera::DeliverFrameRunnable"),
        mParent(aParent),
        mCapEngine(aEngine),
        mStreamId(aStreamId),
        mTrackingId(aTrackingId),
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
    if (!mParent->DeliverFrameOverIPC(mCapEngine, mStreamId, mTrackingId,
                                      std::move(mBuffer),
                                      mAlternateBuffer.get(), mProperties)) {
      mResult = -1;
    } else {
      mResult = 0;
    }
    return NS_OK;
  }

  int GetResult() { return mResult; }

 private:
  const RefPtr<CamerasParent> mParent;
  const CaptureEngine mCapEngine;
  const uint32_t mStreamId;
  const TrackingId mTrackingId;
  ShmemBuffer mBuffer;
  UniquePtr<unsigned char[]> mAlternateBuffer;
  const VideoFrameProperties mProperties;
  int mResult;
};

int CamerasParent::DeliverFrameOverIPC(CaptureEngine aCapEngine,
                                       uint32_t aStreamId,
                                       const TrackingId& aTrackingId,
                                       ShmemBuffer aBuffer,
                                       unsigned char* aAltBuffer,
                                       const VideoFrameProperties& aProps) {
  // No ShmemBuffers were available, so construct one now of the right size
  // and copy into it. That is an extra copy, but we expect this to be
  // the exceptional case, because we just assured the next call *will* have a
  // buffer of the right size.
  if (aAltBuffer != nullptr) {
    // Get a shared memory buffer from the pool, at least size big
    ShmemBuffer shMemBuff = mShmemPool.Get(this, aProps.bufferSize());

    if (!shMemBuff.Valid()) {
      LOG("No usable Video shmem in DeliverFrame (out of buffers?)");
      // We can skip this frame if we run out of buffers, it's not a real error.
      return 0;
    }

    PerformanceRecorder<CopyVideoStage> rec(
        "CamerasParent::AltBufferToShmem"_ns, aTrackingId, aProps.width(),
        aProps.height());
    // get() and Size() check for proper alignment of the segment
    memcpy(shMemBuff.GetBytes(), aAltBuffer, aProps.bufferSize());
    rec.Record();

    if (!SendDeliverFrame(aCapEngine, aStreamId, std::move(shMemBuff.Get()),
                          aProps)) {
      return -1;
    }
  } else {
    MOZ_ASSERT(aBuffer.Valid());
    // ShmemBuffer was available, we're all good. A single copy happened
    // in the original webrtc callback.
    if (!SendDeliverFrame(aCapEngine, aStreamId, std::move(aBuffer.Get()),
                          aProps)) {
      return -1;
    }
  }

  return 0;
}

ShmemBuffer CamerasParent::GetBuffer(size_t aSize) {
  return mShmemPool.GetIfAvailable(aSize);
}

void CallbackHelper::OnFrame(const webrtc::VideoFrame& aVideoFrame) {
  LOG_VERBOSE("CamerasParent(%p)::%s", mParent, __func__);
  if (profiler_thread_is_being_profiled_for_markers()) {
    PROFILER_MARKER_UNTYPED(
        nsPrintfCString("CaptureVideoFrame %dx%d %s %s", aVideoFrame.width(),
                        aVideoFrame.height(),
                        webrtc::VideoFrameBufferTypeToString(
                            aVideoFrame.video_frame_buffer()->type()),
                        mTrackingId.ToString().get()),
        MEDIA_RT);
  }
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
    PerformanceRecorder<CopyVideoStage> rec(
        "CamerasParent::VideoFrameToShmem"_ns, mTrackingId, aVideoFrame.width(),
        aVideoFrame.height());
    VideoFrameUtils::CopyVideoFrameBuffers(
        shMemBuffer.GetBytes(), properties.bufferSize(), aVideoFrame);
    rec.Record();
    runnable =
        new DeliverFrameRunnable(mParent, mCapEngine, mStreamId, mTrackingId,
                                 std::move(shMemBuffer), properties);
  }
  if (!runnable) {
    runnable = new DeliverFrameRunnable(mParent, mCapEngine, mStreamId,
                                        mTrackingId, aVideoFrame, properties);
  }
  MOZ_ASSERT(mParent);
  nsIEventTarget* target = mParent->GetBackgroundEventTarget();
  MOZ_ASSERT(target != nullptr);
  target->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

ipc::IPCResult CamerasParent::RecvReleaseFrame(ipc::Shmem&& aShmem) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());

  mShmemPool.Put(ShmemBuffer(aShmem));
  return IPC_OK();
}

void CamerasParent::CloseEngines() {
  MOZ_ASSERT(mVideoCaptureThread->IsOnCurrentThread());
  LOG_FUNCTION();

  // Stop the callers
  while (!mCallbacks.IsEmpty()) {
    auto capEngine = mCallbacks[0]->mCapEngine;
    auto streamNum = mCallbacks[0]->mStreamId;
    LOG("Forcing shutdown of engine %d, capturer %d", capEngine, streamNum);
    StopCapture(capEngine, streamNum);
    Unused << ReleaseCapture(capEngine, streamNum);
  }

  if (VideoEngine* engine = mEngines->ElementAt(CameraEngine); engine) {
    auto device_info =
        engine->GetOrCreateVideoCaptureDeviceInfo(sVideoCaptureOptions.get());
    MOZ_ASSERT(device_info);
    if (device_info) {
      device_info->DeRegisterVideoInputFeedBack(this);
    }
  }
}

VideoEngine* CamerasParent::EnsureInitialized(int aEngine) {
  MOZ_ASSERT(mVideoCaptureThread->IsOnCurrentThread());
  LOG_VERBOSE("CamerasParent(%p)::%s", this, __func__);
  CaptureEngine capEngine = static_cast<CaptureEngine>(aEngine);

  if (VideoEngine* engine = mEngines->ElementAt(capEngine); engine) {
    return engine;
  }

  CaptureDeviceType captureDeviceType = CaptureDeviceType::Camera;
  switch (capEngine) {
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
      return nullptr;
  }

  RefPtr<VideoEngine> engine = VideoEngine::Create(captureDeviceType);
  if (!engine) {
    LOG("VideoEngine::Create failed");
    return nullptr;
  }

  if (capEngine == CameraEngine) {
    auto device_info =
        engine->GetOrCreateVideoCaptureDeviceInfo(sVideoCaptureOptions.get());
    MOZ_ASSERT(device_info);
    if (device_info) {
      device_info->RegisterVideoInputFeedBack(this);
    }
  }

  return mEngines->ElementAt(capEngine) = std::move(engine);
}

// Dispatch the runnable to do the camera operation on the
// specific Cameras thread, preventing us from blocking, and
// chain a runnable to send back the result on the IPC thread.
// It would be nice to get rid of the code duplication here,
// perhaps via Promises.
ipc::IPCResult CamerasParent::RecvNumberOfCaptureDevices(
    const CaptureEngine& aCapEngine) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();
  LOG("CaptureEngine=%d", aCapEngine);

  using Promise = MozPromise<int, bool, true>;
  InvokeAsync(mVideoCaptureThread, __func__,
              [this, self = RefPtr(this), aCapEngine] {
                int num = -1;
                if (auto* engine = EnsureInitialized(aCapEngine)) {
                  if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo(
                          sVideoCaptureOptions.get())) {
                    num = static_cast<int>(devInfo->NumberOfDevices());
                  }
                }
                return Promise::CreateAndResolve(
                    num, "CamerasParent::RecvNumberOfCaptureDevices");
              })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            int nrDevices = aValue.ResolveValue();

            if (mDestroyed) {
              LOG("RecvNumberOfCaptureDevices failure: child not alive");
              return;
            }

            if (nrDevices < 0) {
              LOG("RecvNumberOfCaptureDevices couldn't find devices");
              Unused << SendReplyFailure();
              return;
            }

            LOG("RecvNumberOfCaptureDevices: %d", nrDevices);
            Unused << SendReplyNumberOfCaptureDevices(nrDevices);
          });
  return IPC_OK();
}

ipc::IPCResult CamerasParent::RecvEnsureInitialized(
    const CaptureEngine& aCapEngine) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();

  using Promise = MozPromise<bool, bool, true>;
  InvokeAsync(mVideoCaptureThread, __func__,
              [this, self = RefPtr(this), aCapEngine] {
                return Promise::CreateAndResolve(
                    EnsureInitialized(aCapEngine),
                    "CamerasParent::RecvEnsureInitialized");
              })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            bool result = aValue.ResolveValue();

            if (mDestroyed) {
              LOG("RecvEnsureInitialized: child not alive");
              return;
            }

            if (!result) {
              LOG("RecvEnsureInitialized failed");
              Unused << SendReplyFailure();
              return;
            }

            LOG("RecvEnsureInitialized succeeded");
            Unused << SendReplySuccess();
          });
  return IPC_OK();
}

ipc::IPCResult CamerasParent::RecvNumberOfCapabilities(
    const CaptureEngine& aCapEngine, const nsACString& aUniqueId) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();
  LOG("Getting caps for %s", PromiseFlatCString(aUniqueId).get());

  using Promise = MozPromise<int, bool, true>;
  InvokeAsync(
      mVideoCaptureThread, __func__,
      [this, self = RefPtr(this), id = nsCString(aUniqueId), aCapEngine]() {
        int num = -1;
        if (auto* engine = EnsureInitialized(aCapEngine)) {
          if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo(
                  sVideoCaptureOptions.get())) {
            num = devInfo->NumberOfCapabilities(id.get());
          }
        }
        return Promise::CreateAndResolve(
            num, "CamerasParent::RecvNumberOfCapabilities");
      })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            int aNrCapabilities = aValue.ResolveValue();

            if (mDestroyed) {
              LOG("RecvNumberOfCapabilities: child not alive");
              return;
            }

            if (aNrCapabilities < 0) {
              LOG("RecvNumberOfCapabilities couldn't find capabilities");
              Unused << SendReplyFailure();
              return;
            }

            LOG("RecvNumberOfCapabilities: %d", aNrCapabilities);
            Unused << SendReplyNumberOfCapabilities(aNrCapabilities);
          });
  return IPC_OK();
}

ipc::IPCResult CamerasParent::RecvGetCaptureCapability(
    const CaptureEngine& aCapEngine, const nsACString& aUniqueId,
    const int& aIndex) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();
  LOG("RecvGetCaptureCapability: %s %d", PromiseFlatCString(aUniqueId).get(),
      aIndex);

  using Promise = MozPromise<webrtc::VideoCaptureCapability, int, true>;
  InvokeAsync(
      mVideoCaptureThread, __func__,
      [this, self = RefPtr(this), id = nsCString(aUniqueId), aCapEngine,
       aIndex] {
        webrtc::VideoCaptureCapability webrtcCaps;
        int error = -1;
        if (auto* engine = EnsureInitialized(aCapEngine)) {
          if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo(
                  sVideoCaptureOptions.get())) {
            error = devInfo->GetCapability(id.get(), aIndex, webrtcCaps);
          }
        }

        if (!error && aCapEngine == CameraEngine) {
          auto iter = mAllCandidateCapabilities.find(id);
          if (iter == mAllCandidateCapabilities.end()) {
            std::map<uint32_t, webrtc::VideoCaptureCapability>
                candidateCapabilities;
            candidateCapabilities.emplace(aIndex, webrtcCaps);
            mAllCandidateCapabilities.emplace(id, candidateCapabilities);
          } else {
            (iter->second).emplace(aIndex, webrtcCaps);
          }
        }
        if (error) {
          return Promise::CreateAndReject(
              error, "CamerasParent::RecvGetCaptureCapability");
        }
        return Promise::CreateAndResolve(
            webrtcCaps, "CamerasParent::RecvGetCaptureCapability");
      })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            if (mDestroyed) {
              LOG("RecvGetCaptureCapability: child not alive");
              return;
            }

            if (aValue.IsReject()) {
              LOG("RecvGetCaptureCapability: reply failure");
              Unused << SendReplyFailure();
              return;
            }

            auto webrtcCaps = aValue.ResolveValue();
            VideoCaptureCapability capCap(
                webrtcCaps.width, webrtcCaps.height, webrtcCaps.maxFPS,
                static_cast<int>(webrtcCaps.videoType), webrtcCaps.interlaced);
            LOG("Capability: %u %u %u %d %d", webrtcCaps.width,
                webrtcCaps.height, webrtcCaps.maxFPS,
                static_cast<int>(webrtcCaps.videoType), webrtcCaps.interlaced);
            Unused << SendReplyGetCaptureCapability(capCap);
          });
  return IPC_OK();
}

ipc::IPCResult CamerasParent::RecvGetCaptureDevice(
    const CaptureEngine& aCapEngine, const int& aDeviceIndex) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();

  using Data = std::tuple<nsCString, nsCString, pid_t, int>;
  using Promise = MozPromise<Data, bool, true>;
  InvokeAsync(mVideoCaptureThread, __func__,
              [this, self = RefPtr(this), aCapEngine, aDeviceIndex] {
                char deviceName[MediaEngineSource::kMaxDeviceNameLength];
                char deviceUniqueId[MediaEngineSource::kMaxUniqueIdLength];
                nsCString name;
                nsCString uniqueId;
                pid_t devicePid = 0;
                int error = -1;
                if (auto* engine = EnsureInitialized(aCapEngine)) {
                  if (auto devInfo = engine->GetOrCreateVideoCaptureDeviceInfo(
                          sVideoCaptureOptions.get())) {
                    error = devInfo->GetDeviceName(
                        aDeviceIndex, deviceName, sizeof(deviceName),
                        deviceUniqueId, sizeof(deviceUniqueId), nullptr, 0,
                        &devicePid);
                  }
                }
                if (error == 0) {
                  name.Assign(deviceName);
                  uniqueId.Assign(deviceUniqueId);
                }
                return Promise::CreateAndResolve(
                    std::make_tuple(std::move(name), std::move(uniqueId),
                                    devicePid, error),
                    "CamerasParent::RecvGetCaptureDevice");
              })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            const auto& [name, uniqueId, devicePid, error] =
                aValue.ResolveValue();
            if (mDestroyed) {
              return;
            }
            if (error != 0) {
              LOG("GetCaptureDevice failed: %d", error);
              Unused << SendReplyFailure();
              return;
            }
            bool scary = (devicePid == getpid());

            LOG("Returning %s name %s id (pid = %d)%s", name.get(),
                uniqueId.get(), devicePid, (scary ? " (scary)" : ""));
            Unused << SendReplyGetCaptureDevice(name, uniqueId, scary);
          });
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

  // when we delegate permission from first party, we should use the top level
  // window
  RefPtr<dom::BrowsingContext> topBC = window->BrowsingContext()->Top();
  window = topBC->Canonical()->GetCurrentWindowGlobal();

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

ipc::IPCResult CamerasParent::RecvAllocateCapture(
    const CaptureEngine& aCapEngine, const nsACString& aUniqueIdUTF8,
    const uint64_t& aWindowID) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG("CamerasParent(%p)::%s: Verifying permissions", this, __func__);

  using Promise1 = MozPromise<bool, bool, true>;
  using Data = std::tuple<int, int>;
  using Promise2 = MozPromise<Data, bool, true>;
  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [aWindowID] {
                // Verify whether the claimed origin has received permission
                // to use the camera, either persistently or this session (one
                // shot).
                bool allowed = HasCameraPermission(aWindowID);
                if (!allowed) {
                  // Developer preference for turning off permission check.
                  if (Preferences::GetBool(
                          "media.navigator.permission.disabled", false)) {
                    allowed = true;
                    LOG("No permission but checks are disabled");
                  } else {
                    LOG("No camera permission for this origin");
                  }
                }
                return Promise1::CreateAndResolve(
                    allowed, "CamerasParent::RecvAllocateCapture");
              })
      ->Then(mVideoCaptureThread, __func__,
             [this, self = RefPtr(this), aCapEngine,
              unique_id = nsCString(aUniqueIdUTF8)](
                 Promise1::ResolveOrRejectValue&& aValue) {
               bool allowed = aValue.ResolveValue();
               int captureId = -1;
               int error = -1;
               if (allowed && EnsureInitialized(aCapEngine)) {
                 VideoEngine* engine = mEngines->ElementAt(aCapEngine);
                 captureId = engine->CreateVideoCapture(
                     unique_id.get(), sVideoCaptureOptions.get());
                 engine->WithEntry(captureId,
                                   [&error](VideoEngine::CaptureEntry& cap) {
                                     if (cap.VideoCapture()) {
                                       error = 0;
                                     }
                                   });
               }
               return Promise2::CreateAndResolve(
                   std::make_tuple(captureId, error),
                   "CamerasParent::RecvAllocateCapture");
             })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise2::ResolveOrRejectValue&& aValue) {
            const auto [captureId, error] = aValue.ResolveValue();
            if (mDestroyed) {
              LOG("RecvAllocateCapture: child not alive");
              return;
            }

            if (error != 0) {
              Unused << SendReplyFailure();
              LOG("RecvAllocateCapture: WithEntry error");
              return;
            }

            LOG("Allocated device nr %d", captureId);
            Unused << SendReplyAllocateCapture(captureId);
          });
  return IPC_OK();
}

int CamerasParent::ReleaseCapture(const CaptureEngine& aCapEngine,
                                  int aCaptureId) {
  MOZ_ASSERT(mVideoCaptureThread->IsOnCurrentThread());
  int error = -1;
  if (auto* engine = EnsureInitialized(aCapEngine)) {
    error = engine->ReleaseVideoCapture(aCaptureId);
  }
  return error;
}

ipc::IPCResult CamerasParent::RecvReleaseCapture(
    const CaptureEngine& aCapEngine, const int& aCaptureId) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();
  LOG("RecvReleaseCamera device nr %d", aCaptureId);

  using Promise = MozPromise<int, bool, true>;
  InvokeAsync(mVideoCaptureThread, __func__,
              [this, self = RefPtr(this), aCapEngine, aCaptureId] {
                return Promise::CreateAndResolve(
                    ReleaseCapture(aCapEngine, aCaptureId),
                    "CamerasParent::RecvReleaseCapture");
              })
      ->Then(mPBackgroundEventTarget, __func__,
             [this, self = RefPtr(this),
              aCaptureId](Promise::ResolveOrRejectValue&& aValue) {
               int error = aValue.ResolveValue();

               if (mDestroyed) {
                 LOG("RecvReleaseCapture: child not alive");
                 return;
               }

               if (error != 0) {
                 Unused << SendReplyFailure();
                 LOG("RecvReleaseCapture: Failed to free device nr %d",
                     aCaptureId);
                 return;
               }

               Unused << SendReplySuccess();
               LOG("Freed device nr %d", aCaptureId);
             });
  return IPC_OK();
}

ipc::IPCResult CamerasParent::RecvStartCapture(
    const CaptureEngine& aCapEngine, const int& aCaptureId,
    const VideoCaptureCapability& aIpcCaps) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();

  using Promise = MozPromise<int, bool, true>;
  InvokeAsync(
      mVideoCaptureThread, __func__,
      [this, self = RefPtr(this), aCapEngine, aCaptureId, aIpcCaps] {
        LOG_FUNCTION();
        CallbackHelper** cbh;
        int error = -1;

        if (!EnsureInitialized(aCapEngine)) {
          return Promise::CreateAndResolve(error,
                                           "CamerasParent::RecvStartCapture");
        }

        cbh = mCallbacks.AppendElement(new CallbackHelper(
            static_cast<CaptureEngine>(aCapEngine), aCaptureId, this));

        mEngines->ElementAt(aCapEngine)
            ->WithEntry(aCaptureId, [&](VideoEngine::CaptureEntry& cap) {
              webrtc::VideoCaptureCapability capability;
              capability.width = aIpcCaps.width();
              capability.height = aIpcCaps.height();
              capability.maxFPS = aIpcCaps.maxFPS();
              capability.videoType =
                  static_cast<webrtc::VideoType>(aIpcCaps.videoType());
              capability.interlaced = aIpcCaps.interlaced();

#ifndef FUZZING_SNAPSHOT
              MOZ_DIAGNOSTIC_ASSERT(sDeviceUniqueIDs.find(aCaptureId) ==
                                    sDeviceUniqueIDs.end());
#endif
              sDeviceUniqueIDs.emplace(aCaptureId,
                                       cap.VideoCapture()->CurrentDeviceName());

#ifndef FUZZING_SNAPSHOT
              MOZ_DIAGNOSTIC_ASSERT(
                  sAllRequestedCapabilities.find(aCaptureId) ==
                  sAllRequestedCapabilities.end());
#endif
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

                auto candidateCapabilities = mAllCandidateCapabilities.find(
                    nsCString(cap.VideoCapture()->CurrentDeviceName()));
                if ((candidateCapabilities !=
                     mAllCandidateCapabilities.end()) &&
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
                    uint64_t distance = uint64_t(ResolutionFeasibilityDistance(
                                            candidateCapability.second.width,
                                            capability.width)) +
                                        uint64_t(ResolutionFeasibilityDistance(
                                            candidateCapability.second.height,
                                            capability.height)) +
                                        uint64_t(FeasibilityDistance(
                                            candidateCapability.second.maxFPS,
                                            capability.maxFPS));
                    if (distance < minDistance) {
                      minIdx = static_cast<int32_t>(candidateCapability.first);
                      minDistance = distance;
                    }
                  }
                  MOZ_ASSERT(minIdx != -1);
                  capability = candidateCapabilities->second[minIdx];
                }
              } else if (aCapEngine == ScreenEngine ||
                         aCapEngine == BrowserEngine ||
                         aCapEngine == WinEngine) {
                for (const auto& it : sDeviceUniqueIDs) {
                  if (strcmp(it.second,
                             cap.VideoCapture()->CurrentDeviceName()) == 0) {
                    capability.maxFPS =
                        std::max(capability.maxFPS,
                                 sAllRequestedCapabilities[it.first].maxFPS);
                  }
                }
              }

              cap.VideoCapture()->SetTrackingId(
                  (*cbh)->mTrackingId.mUniqueInProcId);
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

        return Promise::CreateAndResolve(error,
                                         "CamerasParent::RecvStartCapture");
      })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            int error = aValue.ResolveValue();

            if (mDestroyed) {
              LOG("RecvStartCapture failure: child is not alive");
              return;
            }

            if (error != 0) {
              LOG("RecvStartCapture failure: StartCapture failed");
              Unused << SendReplyFailure();
              return;
            }

            Unused << SendReplySuccess();
          });
  return IPC_OK();
}

ipc::IPCResult CamerasParent::RecvFocusOnSelectedSource(
    const CaptureEngine& aCapEngine, const int& aCaptureId) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();

  using Promise = MozPromise<bool, bool, true>;
  InvokeAsync(mVideoCaptureThread, __func__,
              [this, self = RefPtr(this), aCapEngine, aCaptureId] {
                bool result = false;
                if (auto* engine = EnsureInitialized(aCapEngine)) {
                  engine->WithEntry(
                      aCaptureId, [&](VideoEngine::CaptureEntry& cap) {
                        if (cap.VideoCapture()) {
                          result = cap.VideoCapture()->FocusOnSelectedSource();
                        }
                      });
                }
                return Promise::CreateAndResolve(
                    result, "CamerasParent::RecvFocusOnSelectedSource");
              })
      ->Then(
          mPBackgroundEventTarget, __func__,
          [this, self = RefPtr(this)](Promise::ResolveOrRejectValue&& aValue) {
            bool result = aValue.ResolveValue();
            if (mDestroyed) {
              LOG("RecvFocusOnSelectedSource failure: child is not alive");
              return;
            }

            if (!result) {
              Unused << SendReplyFailure();
              LOG("RecvFocusOnSelectedSource failure.");
              return;
            }

            Unused << SendReplySuccess();
          });
  return IPC_OK();
}

void CamerasParent::StopCapture(const CaptureEngine& aCapEngine,
                                int aCaptureId) {
  MOZ_ASSERT(mVideoCaptureThread->IsOnCurrentThread());
  if (auto* engine = EnsureInitialized(aCapEngine)) {
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

ipc::IPCResult CamerasParent::RecvStopCapture(const CaptureEngine& aCapEngine,
                                              const int& aCaptureId) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  MOZ_ASSERT(!mDestroyed);

  LOG_FUNCTION();

  nsresult rv = mVideoCaptureThread->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr(this), aCapEngine, aCaptureId] {
        StopCapture(aCapEngine, aCaptureId);
      }));

  if (mDestroyed) {
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

void CamerasParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());
  LOG_FUNCTION();

  // Release shared memory now, it's our last chance
  mShmemPool.Cleanup(this);
  // We don't want to receive callbacks or anything if we can't
  // forward them anymore anyway.
  mDestroyed = true;
  // We don't need to listen for shutdown any longer. Disconnect the request.
  // This breaks the reference cycle between CamerasParent and the shutdown
  // promise's Then handler.
  mShutdownRequest.DisconnectIfExists();

  if (mVideoCaptureThread) {
    // Shut down the WebRTC stack, on the video capture thread.
    MOZ_ALWAYS_SUCCEEDS(mVideoCaptureThread->Dispatch(
        NewRunnableMethod(__func__, this, &CamerasParent::CloseEngines)));
  }
}

void CamerasParent::OnShutdown() {
  ipc::AssertIsOnBackgroundThread();
  LOG("CamerasParent(%p) ShutdownEvent", this);
  mShutdownRequest.Complete();
  (void)Send__delete__(this);
}

CamerasParent::CamerasParent()
    : mShutdownBlocker(ShutdownBlockingTicket::Create(
          u"CamerasParent"_ns, NS_LITERAL_STRING_FROM_CSTRING(__FILE__),
          __LINE__)),
      mVideoCaptureThread(mShutdownBlocker
                              ? MakeAndAddRefVideoCaptureThreadAndSingletons()
                              : nullptr),
      mEngines(sEngines),
      mShmemPool(CaptureEngine::MaxEngine),
      mPBackgroundEventTarget(GetCurrentSerialEventTarget()),
      mDestroyed(false) {
  MOZ_ASSERT(mPBackgroundEventTarget != nullptr,
             "GetCurrentThreadEventTarget failed");
  LOG("CamerasParent: %p", this);

  // Don't dispatch from the constructor a runnable that may toggle the
  // reference count, because the IPC thread does not get a reference until
  // after the constructor returns.
}

auto CamerasParent::RequestCameraAccess()
    -> RefPtr<CameraAccessRequestPromise> {
  ipc::AssertIsOnBackgroundThread();

#if defined(WEBRTC_USE_PIPEWIRE)
  const bool usePipeWire =
      mozilla::StaticPrefs::media_webrtc_camera_allow_pipewire_AtStartup();
#else
  const bool usePipeWire = false;
#endif

  if (!usePipeWire) {
    // For non-PipeWire implementations the access is automatically granted
    return CameraAccessRequestPromise::CreateAndResolve(
        NS_OK, "CamerasParent::RequestCameraAccess Resolve");
  }

#if defined(WEBRTC_USE_PIPEWIRE)
  static StaticRefPtr<CameraAccessRequestPromise> sCameraAccessRequestPromise;
  if (!sCameraAccessRequestPromise) {
    auto options = MakeRefPtr<VideoCaptureOptionsImpl>();
    auto portal = MakeRefPtr<CameraPortalImpl>();
    sCameraAccessRequestPromise = RefPtr<CameraAccessRequestPromise>(
        portal->Start()
            ->Then(
                GetCurrentSerialEventTarget(),
                "CamerasParent::RequestCameraAccess portal handler",
                [options, portal](int aFD) { return options->Init(aFD); },
                [](nsresult aRv) {
                  return VideoCaptureOptionsImpl::
                      VideoCaptureOptionsInitPromise::CreateAndReject(
                          aRv,
                          "CamerasParent::RequestCameraAccess Portal Reject");
                })
            ->Then(
                GetCurrentSerialEventTarget(),
                "CamerasParent::RequestCameraAccess options init handler",
                [options](nsresult aRv) {
                  MOZ_ASSERT(NS_SUCCEEDED(aRv));
                  MOZ_ASSERT(!sVideoCaptureOptions);
                  sVideoCaptureOptions = options->ReleaseOptions().release();
                  NS_DispatchToMainThread(NS_NewRunnableFunction(
                      "CamerasParent::RequestCameraAccess",
                      []() { ClearOnShutdown(&sVideoCaptureOptions); }));
                  return CameraAccessRequestPromise::CreateAndResolve(
                      aRv,
                      "CamerasParent::RequestCameraAccess options init "
                      "resolve");
                },
                [](nsresult aRv) {
                  MOZ_ASSERT(NS_FAILED(aRv));
                  return CameraAccessRequestPromise::CreateAndReject(
                      aRv,
                      "CamerasParent::RequestCameraAccess options init reject");
                }));
    static nsresult clearingRv = NS_DispatchToMainThread(NS_NewRunnableFunction(
        __func__, [] { ClearOnShutdown(&sCameraAccessRequestPromise); }));
    Unused << clearingRv;
  }

  // If camera acess is granted, all is jolly. But we need to handle rejection.
  return sCameraAccessRequestPromise->Then(
      GetCurrentSerialEventTarget(),
      "CamerasParent::CameraAccessRequestPromise rejection handler",
      [](nsresult aRv) {
        return CameraAccessRequestPromise::CreateAndResolve(
            aRv, "CamerasParent::RequestCameraAccess resolve");
      },
      [promise = RefPtr(sCameraAccessRequestPromise.get())](nsresult aRv) {
        if (promise == sCameraAccessRequestPromise) {
          sCameraAccessRequestPromise = nullptr;
          return CameraAccessRequestPromise::CreateAndReject(
              aRv, "CamerasParent::RequestCameraAccess reject");
        }
        return CamerasParent::RequestCameraAccess();
      });
#endif
}

// RecvPCamerasConstructor() is used because IPC messages, for
// Send__delete__(), cannot be sent from AllocPCamerasParent().
ipc::IPCResult CamerasParent::RecvPCamerasConstructor() {
  MOZ_ASSERT(mPBackgroundEventTarget->IsOnCurrentThread());

  // AsyncShutdown barriers are available only for ShutdownPhases as late as
  // XPCOMWillShutdown.  The IPC background thread shuts down during
  // XPCOMShutdownThreads, so actors may be created when AsyncShutdown barriers
  // are no longer available.  Should shutdown be past XPCOMWillShutdown we end
  // up with a null mShutdownBlocker.

  if (!mShutdownBlocker) {
    LOG("CamerasParent(%p) Got no ShutdownBlockingTicket. We are already in "
        "shutdown. Deleting.",
        this);
    return Send__delete__(this) ? IPC_OK() : IPC_FAIL(this, "Failed to send");
  }

  if (!mVideoCaptureThread) {
    return Send__delete__(this) ? IPC_OK() : IPC_FAIL(this, "Failed to send");
  }

  MOZ_ASSERT(mEngines);

  mShutdownBlocker->ShutdownPromise()
      ->Then(mPBackgroundEventTarget, "CamerasParent OnShutdown",
             [this, self = RefPtr(this)](
                 const ShutdownPromise::ResolveOrRejectValue& aValue) {
               MOZ_ASSERT(aValue.IsResolve(),
                          "ShutdownBlockingTicket must have been destroyed "
                          "without us disconnecting the shutdown request");
               OnShutdown();
             })
      ->Track(mShutdownRequest);

  return IPC_OK();
}

CamerasParent::~CamerasParent() {
  ipc::AssertIsOnBackgroundThread();
  LOG_FUNCTION();

  if (!mVideoCaptureThread) {
    // No video engines or video capture thread to shutdown here.
    return;
  }

  MOZ_ASSERT(mShutdownBlocker,
             "A ShutdownBlocker is a prerequisite for mVideoCaptureThread");

  ReleaseVideoCaptureThreadAndSingletons();
}

already_AddRefed<CamerasParent> CamerasParent::Create() {
  ipc::AssertIsOnBackgroundThread();
  return MakeAndAddRef<CamerasParent>();
}

}  // namespace camera
}  // namespace mozilla

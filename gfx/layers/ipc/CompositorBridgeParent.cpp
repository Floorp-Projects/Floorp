/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorBridgeParent.h"

#include <stdio.h>   // for fprintf, stdout
#include <stdint.h>  // for uint64_t
#include <map>       // for _Rb_tree_iterator, etc
#include <utility>   // for pair

#include "apz/src/APZCTreeManager.h"  // for APZCTreeManager
#include "LayerTransactionParent.h"   // for LayerTransactionParent
#include "RenderTrace.h"              // for RenderTraceLayers
#include "base/message_loop.h"        // for MessageLoop
#include "base/process.h"             // for ProcessId
#include "base/task.h"                // for CancelableTask, etc
#include "base/thread.h"              // for Thread
#include "gfxContext.h"               // for gfxContext
#include "gfxPlatform.h"              // for gfxPlatform
#include "TreeTraversal.h"            // for ForEachNode
#ifdef MOZ_WIDGET_GTK
#  include "gfxPlatformGtk.h"  // for gfxPlatform
#endif
#include "mozilla/AutoRestore.h"      // for AutoRestore
#include "mozilla/ClearOnShutdown.h"  // for ClearOnShutdown
#include "mozilla/DebugOnly.h"        // for DebugOnly
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/gfx/2D.h"         // for DrawTarget
#include "mozilla/gfx/Point.h"      // for IntSize
#include "mozilla/gfx/Rect.h"       // for IntSize
#include "mozilla/gfx/gfxVars.h"    // for gfxVars
#include "VRManager.h"              // for VRManager
#include "mozilla/ipc/Transport.h"  // for Transport
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/layers/AnimationHelper.h"  // for CompositorAnimationStorage
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZSampler.h"             // for APZSampler
#include "mozilla/layers/APZThreadUtils.h"         // for APZThreadUtils
#include "mozilla/layers/APZUpdater.h"             // for APZUpdater
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/BasicCompositor.h"          // for BasicCompositor
#include "mozilla/layers/CompositionRecorder.h"      // for CompositionRecorder
#include "mozilla/layers/Compositor.h"               // for Compositor
#include "mozilla/layers/CompositorManagerParent.h"  // for CompositorManagerParent
#include "mozilla/layers/CompositorOGL.h"            // for CompositorOGL
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/ContentCompositorBridgeParent.h"
#include "mozilla/layers/FrameUniformityData.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerManagerMLGPU.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/PLayerTransactionParent.h"
#include "mozilla/layers/RemoteContentController.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/media/MediaSystemResourceService.h"  // for MediaSystemResourceService
#include "mozilla/mozalloc.h"                          // for operator new, etc
#include "mozilla/PerfStats.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_WIDGET_GTK
#  include "basic/X11BasicCompositor.h"  // for X11BasicCompositor
#endif
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "nsIWidget.h"        // for nsIWidget
#include "nsTArray.h"         // for nsTArray
#include "nsThreadUtils.h"    // for NS_IsMainThread
#include "nsXULAppAPI.h"      // for XRE_GetIOMessageLoop
#ifdef XP_WIN
#  include "mozilla/layers/CompositorD3D11.h"
#  include "mozilla/widget/WinCompositorWidget.h"
#  include "mozilla/WindowsVersion.h"
#endif
#include "GeckoProfiler.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/Unused.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif
#include "mozilla/VsyncDispatcher.h"
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
#  include "VsyncSource.h"
#endif
#include "mozilla/widget/CompositorWidget.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
#  include "mozilla/widget/CompositorWidgetParent.h"
#endif
#ifdef XP_WIN
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif

#include "LayerScope.h"

namespace mozilla {

namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace std;

using base::ProcessId;
using base::Thread;

using mozilla::Telemetry::LABELS_CONTENT_FRAME_TIME_REASON;

/// Equivalent to asserting CompositorThreadHolder::IsInCompositorThread with
/// the addition that it doesn't assert if the compositor thread holder is
/// already gone during late shutdown.
static void AssertIsInCompositorThread() {
  MOZ_RELEASE_ASSERT(!CompositorThread() ||
                     CompositorThreadHolder::IsInCompositorThread());
}

CompositorBridgeParentBase::CompositorBridgeParentBase(
    CompositorManagerParent* aManager)
    : mCanSend(true), mCompositorManager(aManager) {}

CompositorBridgeParentBase::~CompositorBridgeParentBase() {}

ProcessId CompositorBridgeParentBase::GetChildProcessId() { return OtherPid(); }

void CompositorBridgeParentBase::NotifyNotUsed(PTextureParent* aTexture,
                                               uint64_t aTransactionId) {
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  if (!(texture->GetFlags() & TextureFlags::RECYCLE)) {
    return;
  }

  uint64_t textureId = TextureHost::GetTextureSerial(aTexture);
  mPendingAsyncMessage.push_back(OpNotifyNotUsed(textureId, aTransactionId));
}

void CompositorBridgeParentBase::SendAsyncMessage(
    const nsTArray<AsyncParentMessageData>& aMessage) {
  Unused << SendParentAsyncMessages(aMessage);
}

bool CompositorBridgeParentBase::AllocShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  return PCompositorBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool CompositorBridgeParentBase::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  return PCompositorBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

void CompositorBridgeParentBase::DeallocShmem(ipc::Shmem& aShmem) {
  PCompositorBridgeParent::DeallocShmem(aShmem);
}

static inline MessageLoop* CompositorLoop() {
  return CompositorThreadHolder::Loop();
}

base::ProcessId CompositorBridgeParentBase::RemotePid() { return OtherPid(); }

bool CompositorBridgeParentBase::StartSharingMetrics(
    ipc::SharedMemoryBasic::Handle aHandle,
    CrossProcessMutexHandle aMutexHandle, LayersId aLayersId,
    uint32_t aApzcId) {
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    MOZ_ASSERT(CompositorLoop());
    CompositorLoop()->PostTask(
        NewRunnableMethod<ipc::SharedMemoryBasic::Handle,
                          CrossProcessMutexHandle, LayersId, uint32_t>(
            "layers::CompositorBridgeParent::StartSharingMetrics", this,
            &CompositorBridgeParentBase::StartSharingMetrics, aHandle,
            aMutexHandle, aLayersId, aApzcId));
    return true;
  }

  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeParent::SendSharedCompositorFrameMetrics(
      aHandle, aMutexHandle, aLayersId, aApzcId);
}

bool CompositorBridgeParentBase::StopSharingMetrics(
    ScrollableLayerGuid::ViewID aScrollId, uint32_t aApzcId) {
  if (!CompositorThreadHolder::IsInCompositorThread()) {
    MOZ_ASSERT(CompositorLoop());
    CompositorLoop()->PostTask(
        NewRunnableMethod<ScrollableLayerGuid::ViewID, uint32_t>(
            "layers::CompositorBridgeParent::StopSharingMetrics", this,
            &CompositorBridgeParentBase::StopSharingMetrics, aScrollId,
            aApzcId));
    return true;
  }

  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeParent::SendReleaseSharedCompositorFrameMetrics(
      aScrollId, aApzcId);
}

CompositorBridgeParent::LayerTreeState::LayerTreeState()
    : mApzcTreeManagerParent(nullptr),
      mParent(nullptr),
      mLayerManager(nullptr),
      mContentCompositorBridgeParent(nullptr),
      mLayerTree(nullptr),
      mUpdatedPluginDataAvailable(false) {}

CompositorBridgeParent::LayerTreeState::~LayerTreeState() {
  if (mController) {
    mController->Destroy();
  }
}

typedef map<LayersId, CompositorBridgeParent::LayerTreeState> LayerTreeMap;
LayerTreeMap sIndirectLayerTrees;
StaticAutoPtr<mozilla::Monitor> sIndirectLayerTreesLock;

static void EnsureLayerTreeMapReady() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sIndirectLayerTreesLock) {
    sIndirectLayerTreesLock = new Monitor("IndirectLayerTree");
    mozilla::ClearOnShutdown(&sIndirectLayerTreesLock);
  }
}

template <typename Lambda>
inline void CompositorBridgeParent::ForEachIndirectLayerTree(
    const Lambda& aCallback) {
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  for (auto it = sIndirectLayerTrees.begin(); it != sIndirectLayerTrees.end();
       it++) {
    LayerTreeState* state = &it->second;
    if (state->mParent == this) {
      aCallback(state, it->first);
    }
  }
}

/**
 * A global map referencing each compositor by ID.
 *
 * This map is used by the ImageBridge protocol to trigger
 * compositions without having to keep references to the
 * compositor
 */
typedef map<uint64_t, CompositorBridgeParent*> CompositorMap;
static StaticAutoPtr<CompositorMap> sCompositorMap;

void CompositorBridgeParent::Setup() {
  EnsureLayerTreeMapReady();

  MOZ_ASSERT(!sCompositorMap);
  sCompositorMap = new CompositorMap;
}

void CompositorBridgeParent::FinishShutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sCompositorMap) {
    MOZ_ASSERT(sCompositorMap->empty());
    sCompositorMap = nullptr;
  }

  // TODO: this should be empty by now...
  sIndirectLayerTrees.clear();
}

#ifdef COMPOSITOR_PERFORMANCE_WARNING
static int32_t CalculateCompositionFrameRate() {
  // Used when layout.frame_rate is -1. Needs to be kept in sync with
  // DEFAULT_FRAME_RATE in nsRefreshDriver.cpp.
  // TODO: This should actually return the vsync rate.
  const int32_t defaultFrameRate = 60;
  int32_t compositionFrameRatePref =
      StaticPrefs::layers_offmainthreadcomposition_frame_rate();
  if (compositionFrameRatePref < 0) {
    // Use the same frame rate for composition as for layout.
    int32_t layoutFrameRatePref = StaticPrefs::layout_frame_rate();
    if (layoutFrameRatePref < 0) {
      // TODO: The main thread frame scheduling code consults the actual
      // monitor refresh rate in this case. We should do the same.
      return defaultFrameRate;
    }
    return layoutFrameRatePref;
  }
  return compositionFrameRatePref;
}
#endif

CompositorBridgeParent::CompositorBridgeParent(
    CompositorManagerParent* aManager, CSSToLayoutDeviceScale aScale,
    const TimeDuration& aVsyncRate, const CompositorOptions& aOptions,
    bool aUseExternalSurfaceSize, const gfx::IntSize& aSurfaceSize)
    : CompositorBridgeParentBase(aManager),
      mWidget(nullptr),
      mScale(aScale),
      mVsyncRate(aVsyncRate),
      mPendingTransaction{0},
      mPaused(false),
      mHaveCompositionRecorder(false),
      mUseExternalSurfaceSize(aUseExternalSurfaceSize),
      mEGLSurfaceSize(aSurfaceSize),
      mOptions(aOptions),
      mPauseCompositionMonitor("PauseCompositionMonitor"),
      mResumeCompositionMonitor("ResumeCompositionMonitor"),
      mCompositorBridgeID(0),
      mRootLayerTreeID{0},
      mOverrideComposeReadiness(false),
      mForceCompositionTask(nullptr),
      mCompositorScheduler(nullptr),
      mAnimationStorage(nullptr),
      mPaintTime(TimeDuration::Forever())
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
      ,
      mLastPluginUpdateLayerTreeId{0},
      mDeferPluginWindows(false),
      mPluginWindowsHidden(false)
#endif
{
}

void CompositorBridgeParent::InitSameProcess(widget::CompositorWidget* aWidget,
                                             const LayersId& aLayerTreeId) {
  MOZ_ASSERT(XRE_IsParentProcess() || recordreplay::IsRecordingOrReplaying());
  MOZ_ASSERT(NS_IsMainThread());

  mWidget = aWidget;
  mRootLayerTreeID = aLayerTreeId;

  Initialize();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvInitialize(
    const LayersId& aRootLayerTreeId) {
  MOZ_ASSERT(XRE_IsGPUProcess());

  mRootLayerTreeID = aRootLayerTreeId;

  Initialize();
  return IPC_OK();
}

void CompositorBridgeParent::Initialize() {
  MOZ_ASSERT(CompositorThread(),
             "The compositor thread must be Initialized before instanciating a "
             "CompositorBridgeParent.");

  if (mOptions.UseAPZ()) {
    MOZ_ASSERT(!mApzcTreeManager);
    MOZ_ASSERT(!mApzSampler);
    MOZ_ASSERT(!mApzUpdater);
    mApzcTreeManager = new APZCTreeManager(mRootLayerTreeID);
    mApzSampler = new APZSampler(mApzcTreeManager, mOptions.UseWebRender());
    mApzUpdater = new APZUpdater(mApzcTreeManager, mOptions.UseWebRender());
  }

  mPaused = mOptions.InitiallyPaused();

  mCompositorBridgeID = 0;
  // FIXME: This holds on the the fact that right now the only thing that
  // can destroy this instance is initialized on the compositor thread after
  // this task has been processed.
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableFunction(
      "AddCompositorRunnable", &AddCompositor, this, &mCompositorBridgeID));

  {  // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees[mRootLayerTreeID].mParent = this;
  }

  LayerScope::SetPixelScale(mScale.scale);

  if (!mOptions.UseWebRender()) {
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

LayersId CompositorBridgeParent::RootLayerTreeId() {
  MOZ_ASSERT(mRootLayerTreeID.IsValid());
  return mRootLayerTreeID;
}

CompositorBridgeParent::~CompositorBridgeParent() {
  nsTArray<PTextureParent*> textures;
  ManagedPTextureParent(textures);
  // We expect all textures to be destroyed by now.
  MOZ_DIAGNOSTIC_ASSERT(textures.Length() == 0);
  for (unsigned int i = 0; i < textures.Length(); ++i) {
    RefPtr<TextureHost> tex = TextureHost::AsTextureHost(textures[i]);
    tex->DeallocateDeviceData();
  }
}

void CompositorBridgeParent::ForceIsFirstPaint() {
  if (mWrBridge) {
    mWrBridge->ForceIsFirstPaint();
  } else {
    mCompositionManager->ForceIsFirstPaint();
  }
}

void CompositorBridgeParent::StopAndClearResources() {
  if (mForceCompositionTask) {
    mForceCompositionTask->Cancel();
    mForceCompositionTask = nullptr;
  }

  mPaused = true;

  // We need to clear the APZ tree before we destroy the WebRender API below,
  // because in the case of async scene building that will shut down the updater
  // thread and we need to run the task before that happens.
  MOZ_ASSERT((mApzSampler != nullptr) == (mApzcTreeManager != nullptr));
  MOZ_ASSERT((mApzUpdater != nullptr) == (mApzcTreeManager != nullptr));
  if (mApzUpdater) {
    mApzSampler->Destroy();
    mApzSampler = nullptr;
    mApzUpdater->ClearTree(mRootLayerTreeID);
    mApzUpdater = nullptr;
    mApzcTreeManager = nullptr;
  }

  // Ensure that the layer manager is destroyed before CompositorBridgeChild.
  if (mLayerManager) {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    ForEachIndirectLayerTree([this](LayerTreeState* lts, LayersId) -> void {
      mLayerManager->ClearCachedResources(lts->mRoot);
      lts->mLayerManager = nullptr;
      lts->mParent = nullptr;
    });
    mLayerManager->Destroy();
    mLayerManager = nullptr;
    mCompositionManager = nullptr;
  }

  if (mWrBridge) {
    // Ensure we are not holding the sIndirectLayerTreesLock when destroying
    // the WebRenderBridgeParent instances because it may block on WR.
    std::vector<RefPtr<WebRenderBridgeParent>> indirectBridgeParents;
    {  // scope lock
      MonitorAutoLock lock(*sIndirectLayerTreesLock);
      ForEachIndirectLayerTree([&](LayerTreeState* lts, LayersId) -> void {
        if (lts->mWrBridge) {
          indirectBridgeParents.emplace_back(lts->mWrBridge.forget());
        }
        lts->mParent = nullptr;
      });
    }
    for (const RefPtr<WebRenderBridgeParent>& bridge : indirectBridgeParents) {
      bridge->Destroy();
    }
    indirectBridgeParents.clear();

    RefPtr<wr::WebRenderAPI> api =
        mWrBridge->GetWebRenderAPI(wr::RenderRoot::Default);
    // Ensure we are not holding the sIndirectLayerTreesLock here because we
    // are going to block on WR threads in order to shut it down properly.
    mWrBridge->Destroy();
    mWrBridge = nullptr;

    if (api) {
      // Make extra sure we are done cleaning WebRender up before continuing.
      // After that we wont have a way to talk to a lot of the webrender parts.
      api->FlushSceneBuilder();
      api = nullptr;
    }

    if (mAsyncImageManager) {
      mAsyncImageManager->Destroy();
      // WebRenderAPI should be already destructed
      mAsyncImageManager = nullptr;
    }
  }

  if (mCompositor) {
    mCompositor->Destroy();
    mCompositor = nullptr;
  }

  // This must be destroyed now since it accesses the widget.
  if (mCompositorScheduler) {
    mCompositorScheduler->Destroy();
    mCompositorScheduler = nullptr;
  }

  // After this point, it is no longer legal to access the widget.
  mWidget = nullptr;

  // Clear mAnimationStorage here to ensure that the compositor thread
  // still exists when we destroy it.
  mAnimationStorage = nullptr;
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvWillClose() {
  StopAndClearResources();
  // Once we get the WillClose message, the client side is going to go away
  // soon and we can't be guaranteed that sending messages will work.
  mCanSend = false;
  return IPC_OK();
}

void CompositorBridgeParent::DeferredDestroy() {
  MOZ_ASSERT(!NS_IsMainThread());
  mSelfRef = nullptr;
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvPause() {
  PauseComposition();
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvResume() {
  ResumeComposition();
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvResumeAsync() {
  ResumeComposition();
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvMakeSnapshot(
    const SurfaceDescriptor& aInSnapshot, const gfx::IntRect& aRect) {
  RefPtr<DrawTarget> target =
      GetDrawTargetForDescriptor(aInSnapshot, gfx::BackendType::CAIRO);
  MOZ_ASSERT(target);
  if (!target) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }
  ForceComposeToTarget(target, &aRect);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvWaitOnTransactionProcessed() {
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvFlushRendering() {
  if (mWrBridge) {
    mWrBridge->FlushRendering();
    return IPC_OK();
  }

  if (mCompositorScheduler->NeedsComposite()) {
    CancelCurrentCompositeTask();
    ForceComposeToTarget(nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvFlushRenderingAsync() {
  if (mWrBridge) {
    mWrBridge->FlushRendering(false);
    return IPC_OK();
  }

  return RecvFlushRendering();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvForcePresent() {
  if (mWrBridge) {
    mWrBridge->ScheduleForcedGenerateFrame();
  }
  // During the shutdown sequence mLayerManager may be null
  if (mLayerManager) {
    mLayerManager->ForcePresent();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvNotifyRegionInvalidated(
    const nsIntRegion& aRegion) {
  if (mLayerManager) {
    mLayerManager->AddInvalidRegion(aRegion);
  }
  return IPC_OK();
}

void CompositorBridgeParent::Invalidate() {
  if (mLayerManager) {
    mLayerManager->InvalidateAll();
  }
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvStartFrameTimeRecording(
    const int32_t& aBufferSize, uint32_t* aOutStartIndex) {
  if (mLayerManager) {
    *aOutStartIndex = mLayerManager->StartFrameTimeRecording(aBufferSize);
  } else if (mWrBridge) {
    *aOutStartIndex = mWrBridge->StartFrameTimeRecording(aBufferSize);
  } else {
    *aOutStartIndex = 0;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvStopFrameTimeRecording(
    const uint32_t& aStartIndex, nsTArray<float>* intervals) {
  if (mLayerManager) {
    mLayerManager->StopFrameTimeRecording(aStartIndex, *intervals);
  } else if (mWrBridge) {
    mWrBridge->StopFrameTimeRecording(aStartIndex, *intervals);
  }
  return IPC_OK();
}

void CompositorBridgeParent::ActorDestroy(ActorDestroyReason why) {
  mCanSend = false;

  StopAndClearResources();

  RemoveCompositor(mCompositorBridgeID);

  mCompositionManager = nullptr;

  {  // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees.erase(mRootLayerTreeID);
  }

  // There are chances that the ref count reaches zero on the main thread
  // shortly after this function returns while some ipdl code still needs to run
  // on this thread. We must keep the compositor parent alive untill the code
  // handling message reception is finished on this thread.
  mSelfRef = this;
  MessageLoop::current()->PostTask(
      NewRunnableMethod("layers::CompositorBridgeParent::DeferredDestroy", this,
                        &CompositorBridgeParent::DeferredDestroy));
}

void CompositorBridgeParent::ScheduleRenderOnCompositorThread(
    const wr::RenderRootSet& aRenderRoots) {
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod<wr::RenderRootSet>(
      "layers::CompositorBridgeParent::ScheduleComposition", this,
      &CompositorBridgeParent::ScheduleComposition, aRenderRoots));
}

void CompositorBridgeParent::InvalidateOnCompositorThread() {
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(
      NewRunnableMethod("layers::CompositorBridgeParent::Invalidate", this,
                        &CompositorBridgeParent::Invalidate));
}

void CompositorBridgeParent::PauseComposition() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread(),
             "PauseComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mPauseCompositionMonitor);

  if (!mPaused) {
    mPaused = true;

    TimeStamp now = TimeStamp::Now();
    if (mCompositor) {
      mCompositor->Pause();
      DidComposite(VsyncId(), now, now);
    } else if (mWrBridge) {
      mWrBridge->Pause();
      NotifyPipelineRendered(mWrBridge->PipelineId(),
                             mWrBridge->GetCurrentEpoch(), VsyncId(), now, now,
                             now);
    }
  }

  // if anyone's waiting to make sure that composition really got paused, tell
  // them
  lock.NotifyAll();
}

void CompositorBridgeParent::ResumeComposition() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread(),
             "ResumeComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mResumeCompositionMonitor);

  bool resumed =
      mOptions.UseWebRender() ? mWrBridge->Resume() : mCompositor->Resume();
  if (!resumed) {
#ifdef MOZ_WIDGET_ANDROID
    // We can't get a surface. This could be because the activity changed
    // between the time resume was scheduled and now.
    __android_log_print(
        ANDROID_LOG_INFO, "CompositorBridgeParent",
        "Unable to renew compositor surface; remaining in paused state");
#endif
    lock.NotifyAll();
    return;
  }

  mPaused = false;

  Invalidate();
  mCompositorScheduler->ForceComposeToTarget(nullptr, nullptr);

  // if anyone's waiting to make sure that composition really got resumed, tell
  // them
  lock.NotifyAll();
}

void CompositorBridgeParent::ForceComposition() {
  // Cancel the orientation changed state to force composition
  mForceCompositionTask = nullptr;
  ScheduleRenderOnCompositorThread(wr::RenderRootSet());
}

void CompositorBridgeParent::CancelCurrentCompositeTask() {
  mCompositorScheduler->CancelCurrentCompositeTask();
}

void CompositorBridgeParent::SetEGLSurfaceRect(int x, int y, int width,
                                               int height) {
  NS_ASSERTION(mUseExternalSurfaceSize,
               "Compositor created without UseExternalSurfaceSize provided");
  mEGLSurfaceSize.SizeTo(width, height);
  if (mCompositor) {
    mCompositor->SetDestinationSurfaceSize(
        gfx::IntSize(mEGLSurfaceSize.width, mEGLSurfaceSize.height));
    if (mCompositor->AsCompositorOGL()) {
      mCompositor->AsCompositorOGL()->SetSurfaceOrigin(ScreenIntPoint(x, y));
    }
  }
}

void CompositorBridgeParent::ResumeCompositionAndResize(int x, int y, int width,
                                                        int height) {
  SetEGLSurfaceRect(x, y, width, height);
  ResumeComposition();
}

/*
 * This will execute a pause synchronously, waiting to make sure that the
 * compositor really is paused.
 */
void CompositorBridgeParent::SchedulePauseOnCompositorThread() {
  MonitorAutoLock lock(mPauseCompositionMonitor);

  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(
      NewRunnableMethod("layers::CompositorBridgeParent::PauseComposition",
                        this, &CompositorBridgeParent::PauseComposition));

  // Wait until the pause has actually been processed by the compositor thread
  lock.Wait();
}

bool CompositorBridgeParent::ScheduleResumeOnCompositorThread() {
  MonitorAutoLock lock(mResumeCompositionMonitor);

  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(
      NewRunnableMethod("layers::CompositorBridgeParent::ResumeComposition",
                        this, &CompositorBridgeParent::ResumeComposition));

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();

  return !mPaused;
}

bool CompositorBridgeParent::ScheduleResumeOnCompositorThread(int x, int y,
                                                              int width,
                                                              int height) {
  MonitorAutoLock lock(mResumeCompositionMonitor);

  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod<int, int, int, int>(
      "layers::CompositorBridgeParent::ResumeCompositionAndResize", this,
      &CompositorBridgeParent::ResumeCompositionAndResize, x, y, width,
      height));

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();

  return !mPaused;
}

void CompositorBridgeParent::ScheduleTask(
    already_AddRefed<CancelableRunnable> task, int time) {
  if (time == 0) {
    MessageLoop::current()->PostTask(std::move(task));
  } else {
    MessageLoop::current()->PostDelayedTask(std::move(task), time);
  }
}

void CompositorBridgeParent::UpdatePaintTime(LayerTransactionParent* aLayerTree,
                                             const TimeDuration& aPaintTime) {
  // We get a lot of paint timings for things with empty transactions.
  if (!mLayerManager || aPaintTime.ToMilliseconds() < 1.0) {
    return;
  }

  mLayerManager->SetPaintTime(aPaintTime);
}

void CompositorBridgeParent::RegisterPayloads(
    LayerTransactionParent* aLayerTree,
    const nsTArray<CompositionPayload>& aPayload) {
  // We get a lot of paint timings for things with empty transactions.
  if (!mLayerManager) {
    return;
  }

  mLayerManager->RegisterPayloads(aPayload);
}

void CompositorBridgeParent::NotifyShadowTreeTransaction(
    LayersId aId, bool aIsFirstPaint, const FocusTarget& aFocusTarget,
    bool aScheduleComposite, uint32_t aPaintSequenceNumber,
    bool aIsRepeatTransaction, bool aHitTestUpdate) {
  if (!aIsRepeatTransaction && mLayerManager && mLayerManager->GetRoot()) {
    // Process plugin data here to give time for them to update before the next
    // composition.
    bool pluginsUpdatedFlag = true;
    AutoResolveRefLayers resolve(mCompositionManager, this, nullptr,
                                 &pluginsUpdatedFlag);

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    // If plugins haven't been updated, stop waiting.
    if (!pluginsUpdatedFlag) {
      mWaitForPluginsUntil = TimeStamp();
      mHaveBlockedForPlugins = false;
    }
#endif

    if (mApzUpdater) {
      mApzUpdater->UpdateFocusState(mRootLayerTreeID,
                                    WRRootId::NonWebRender(aId), aFocusTarget);
      if (aHitTestUpdate) {
        mApzUpdater->UpdateHitTestingTree(
            mLayerManager->GetRoot(), aIsFirstPaint, aId, aPaintSequenceNumber);
      }
    }

    mLayerManager->NotifyShadowTreeTransaction();
  }
  if (aScheduleComposite) {
    ScheduleComposition();
  }
}

void CompositorBridgeParent::ScheduleComposition(
    const wr::RenderRootSet& aRenderRoots) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mPaused) {
    return;
  }

  if (mWrBridge) {
    mWrBridge->ScheduleGenerateFrame(aRenderRoots);
  } else {
    mCompositorScheduler->ScheduleComposition();
  }
}

// Go down the composite layer tree, setting properties to match their
// content-side counterparts.
/* static */
void CompositorBridgeParent::SetShadowProperties(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    if (Layer* maskLayer = layer->GetMaskLayer()) {
      SetShadowProperties(maskLayer);
    }
    for (size_t i = 0; i < layer->GetAncestorMaskLayerCount(); i++) {
      SetShadowProperties(layer->GetAncestorMaskLayerAt(i));
    }

    // FIXME: Bug 717688 -- Do these updates in
    // LayerTransactionParent::RecvUpdate.
    HostLayer* layerCompositor = layer->AsHostLayer();
    // Set the layerComposite's base transform to the layer's base transform.
    const auto& animations = layer->GetPropertyAnimationGroups();
    // If there is any animation, the animation value will override
    // non-animated value later, so we don't need to set the non-animated
    // value here.
    if (animations.IsEmpty()) {
      layerCompositor->SetShadowBaseTransform(layer->GetBaseTransform());
      layerCompositor->SetShadowTransformSetByAnimation(false);
      layerCompositor->SetShadowOpacity(layer->GetOpacity());
      layerCompositor->SetShadowOpacitySetByAnimation(false);
    }
    layerCompositor->SetShadowVisibleRegion(layer->GetVisibleRegion());
    layerCompositor->SetShadowClipRect(layer->GetClipRect());
  });
}

void CompositorBridgeParent::CompositeToTarget(VsyncId aId, DrawTarget* aTarget,
                                               const gfx::IntRect* aRect) {
  AUTO_PROFILER_TRACING("Paint", "Composite", GRAPHICS);
  AUTO_PROFILER_LABEL("CompositorBridgeParent::CompositeToTarget", GRAPHICS);
  PerfStats::AutoMetricRecording<PerfStats::Metric::Compositing> autoRecording;

  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread(),
             "Composite can only be called on the compositor thread");
  TimeStamp start = TimeStamp::Now();

  if (!CanComposite()) {
    TimeStamp end = TimeStamp::Now();
    DidComposite(aId, start, end);
    return;
  }

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  if (!mWaitForPluginsUntil.IsNull() && mWaitForPluginsUntil > start) {
    mHaveBlockedForPlugins = true;
    ScheduleComposition();
    return;
  }
#endif

  /*
   * AutoResolveRefLayers handles two tasks related to Windows and Linux
   * plugin window management:
   * 1) calculating if we have remote content in the view. If we do not have
   * remote content, all plugin windows for this CompositorBridgeParent (window)
   * can be hidden since we do not support plugins in chrome when running
   * under e10s.
   * 2) Updating plugin position, size, and clip. We do this here while the
   * remote layer tree is hooked up to to chrome layer tree. This is needed
   * since plugin clipping can depend on chrome (for example, due to tab modal
   * prompts). Updates in step 2 are applied via an async ipc message sent
   * to the main thread.
   */
  bool hasRemoteContent = false;
  bool updatePluginsFlag = true;
  AutoResolveRefLayers resolve(mCompositionManager, this, &hasRemoteContent,
                               &updatePluginsFlag);

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  // We do not support plugins in local content. When switching tabs
  // to local pages, hide every plugin associated with the window.
  if (!hasRemoteContent && gfxVars::BrowserTabsRemoteAutostart() &&
      mCachedPluginData.Length()) {
    Unused << SendHideAllPlugins(GetWidget()->GetWidgetKey());
    mCachedPluginData.Clear();
  }
#endif

  nsCString none;
  if (aTarget) {
    mLayerManager->BeginTransactionWithDrawTarget(aTarget, *aRect);
  } else {
    mLayerManager->BeginTransaction(none);
  }

  SetShadowProperties(mLayerManager->GetRoot());

  if (mForceCompositionTask && !mOverrideComposeReadiness) {
    if (mCompositionManager->ReadyForCompose()) {
      mForceCompositionTask->Cancel();
      mForceCompositionTask = nullptr;
    } else {
      return;
    }
  }

  mCompositionManager->ComputeRotation();

  TimeStamp time =
      mTestTime.valueOr(mCompositorScheduler->GetLastComposeTime());
  bool requestNextFrame =
      mCompositionManager->TransformShadowTree(time, mVsyncRate);

  // Don't eagerly schedule new compositions here when recording or replaying.
  // Recording/replaying processes schedule composites at the top of the main
  // thread's event loop rather than via PVsync, which can cause the composites
  // scheduled here to pile up without any drawing actually happening.
  if (requestNextFrame && !recordreplay::IsRecordingOrReplaying()) {
    ScheduleComposition();
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    // If we have visible windowed plugins then we need to wait for content (and
    // then the plugins) to have been updated by the active animation.
    if (!mPluginWindowsHidden && mCachedPluginData.Length()) {
      mWaitForPluginsUntil =
          mCompositorScheduler->GetLastComposeTime() + (mVsyncRate * 2);
    }
#endif
  }

  RenderTraceLayers(mLayerManager->GetRoot(), "0000");

  if (StaticPrefs::layers_dump_host_layers() || StaticPrefs::layers_dump()) {
    printf_stderr("Painting --- compositing layer tree:\n");
    mLayerManager->Dump(/* aSorted = */ true);
  }
  mLayerManager->SetDebugOverlayWantsNextFrame(false);
  mLayerManager->EndTransaction(time);

  if (!aTarget) {
    TimeStamp end = TimeStamp::Now();
    DidComposite(aId, start, end);
  }

  // We're not really taking advantage of the stored composite-again-time here.
  // We might be able to skip the next few composites altogether. However,
  // that's a bit complex to implement and we'll get most of the advantage
  // by skipping compositing when we detect there's nothing invalid. This is why
  // we do "composite until" rather than "composite again at".
  //
  // TODO(bug 1328602) Figure out what we should do here with the render thread.
  if (!mLayerManager->GetCompositeUntilTime().IsNull() ||
      mLayerManager->DebugOverlayWantsNextFrame()) {
    ScheduleComposition();
  }

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeDuration executionTime =
      TimeStamp::Now() - mCompositorScheduler->GetLastComposeTime();
  TimeDuration frameBudget = TimeDuration::FromMilliseconds(15);
  int32_t frameRate = CalculateCompositionFrameRate();
  if (frameRate > 0) {
    frameBudget = TimeDuration::FromSeconds(1.0 / frameRate);
  }
  if (executionTime > frameBudget) {
    printf_stderr("Compositor: Composite execution took %4.1f ms\n",
                  executionTime.ToMilliseconds());
  }
#endif

  // 0 -> Full-tilt composite
  if (StaticPrefs::layers_offmainthreadcomposition_frame_rate() == 0 ||
      mLayerManager->AlwaysScheduleComposite()) {
    // Special full-tilt composite mode for performance testing
    ScheduleComposition();
  }

  // TODO(bug 1328602) Need an equivalent that works with the rende thread.
  mLayerManager->SetCompositionTime(TimeStamp());

  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::COMPOSITE_TIME,
                                          start);
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvRemotePluginsReady() {
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  mWaitForPluginsUntil = TimeStamp();
  if (mHaveBlockedForPlugins) {
    mHaveBlockedForPlugins = false;
    ForceComposeToTarget(nullptr);
  } else {
    ScheduleComposition();
  }
  return IPC_OK();
#else
  MOZ_ASSERT_UNREACHABLE(
      "CompositorBridgeParent::RecvRemotePluginsReady calls "
      "unexpected on this platform.");
  return IPC_FAIL_NO_REASON(this);
#endif
}

void CompositorBridgeParent::ForceComposeToTarget(DrawTarget* aTarget,
                                                  const gfx::IntRect* aRect) {
  AUTO_PROFILER_LABEL("CompositorBridgeParent::ForceComposeToTarget", GRAPHICS);

  AutoRestore<bool> override(mOverrideComposeReadiness);
  mOverrideComposeReadiness = true;
  mCompositorScheduler->ForceComposeToTarget(aTarget, aRect);
}

PAPZCTreeManagerParent* CompositorBridgeParent::AllocPAPZCTreeManagerParent(
    const LayersId& aLayersId) {
  // This should only ever get called in the GPU process.
  MOZ_ASSERT(XRE_IsGPUProcess());
  // We should only ever get this if APZ is enabled in this compositor.
  MOZ_ASSERT(mOptions.UseAPZ());
  // The mApzcTreeManager and mApzUpdater should have been created via
  // RecvInitialize()
  MOZ_ASSERT(mApzcTreeManager);
  MOZ_ASSERT(mApzUpdater);
  // The main process should pass in 0 because we assume mRootLayerTreeID
  MOZ_ASSERT(!aLayersId.IsValid());

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state =
      sIndirectLayerTrees[mRootLayerTreeID];
  MOZ_ASSERT(state.mParent.get() == this);
  MOZ_ASSERT(!state.mApzcTreeManagerParent);
  state.mApzcTreeManagerParent = new APZCTreeManagerParent(
      WRRootId(mRootLayerTreeID, wr::RenderRoot::Default), mApzcTreeManager,
      mApzUpdater);

  return state.mApzcTreeManagerParent;
}

bool CompositorBridgeParent::DeallocPAPZCTreeManagerParent(
    PAPZCTreeManagerParent* aActor) {
  delete aActor;
  return true;
}

void CompositorBridgeParent::AllocateAPZCTreeManagerParent(
    const MonitorAutoLock& aProofOfLayerTreeStateLock,
    const WRRootId& aWrRootId, LayerTreeState& aState) {
  MOZ_ASSERT(aState.mParent == this);
  MOZ_ASSERT(mApzcTreeManager);
  MOZ_ASSERT(mApzUpdater);
  MOZ_ASSERT(!aState.mApzcTreeManagerParent);
  aState.mApzcTreeManagerParent =
      new APZCTreeManagerParent(aWrRootId, mApzcTreeManager, mApzUpdater);
}

PAPZParent* CompositorBridgeParent::AllocPAPZParent(const LayersId& aLayersId) {
  // The main process should pass in 0 because we assume mRootLayerTreeID
  MOZ_ASSERT(!aLayersId.IsValid());

  RemoteContentController* controller = new RemoteContentController();

  // Increment the controller's refcount before we return it. This will keep the
  // controller alive until it is released by IPDL in DeallocPAPZParent.
  controller->AddRef();

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state =
      sIndirectLayerTrees[mRootLayerTreeID];
  MOZ_ASSERT(!state.mController);
  state.mController = controller;

  return controller;
}

bool CompositorBridgeParent::DeallocPAPZParent(PAPZParent* aActor) {
  RemoteContentController* controller =
      static_cast<RemoteContentController*>(aActor);
  controller->Release();
  return true;
}

#if defined(MOZ_WIDGET_ANDROID)
AndroidDynamicToolbarAnimator*
CompositorBridgeParent::GetAndroidDynamicToolbarAnimator() {
  return mApzcTreeManager ? mApzcTreeManager->GetAndroidDynamicToolbarAnimator()
                          : nullptr;
}
#endif

RefPtr<APZSampler> CompositorBridgeParent::GetAPZSampler() {
  return mApzSampler;
}

RefPtr<APZUpdater> CompositorBridgeParent::GetAPZUpdater() {
  return mApzUpdater;
}

CompositorBridgeParent*
CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
    const LayersId& aLayersId) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  return sIndirectLayerTrees[aLayersId].mParent;
}

/*static*/
RefPtr<CompositorBridgeParent>
CompositorBridgeParent::GetCompositorBridgeParentFromWindowId(
    const wr::WindowId& aWindowId) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  for (auto it = sIndirectLayerTrees.begin(); it != sIndirectLayerTrees.end();
       it++) {
    LayerTreeState* state = &it->second;
    if (!state->mWrBridge) {
      continue;
    }
    // state->mWrBridge might be a root WebRenderBridgeParent or one of a
    // content process, but in either case the state->mParent will be the same.
    // So we don't need to distinguish between the two.
    if (RefPtr<wr::WebRenderAPI> api =
            state->mWrBridge->GetWebRenderAPI(wr::RenderRoot::Default)) {
      if (api->GetId() == aWindowId) {
        return state->mParent;
      }
    }
  }
  return nullptr;
}

bool CompositorBridgeParent::CanComposite() {
  return mLayerManager && mLayerManager->GetRoot() && !mPaused;
}

void CompositorBridgeParent::ScheduleRotationOnCompositorThread(
    const TargetConfig& aTargetConfig, bool aIsFirstPaint) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (!aIsFirstPaint && !mCompositionManager->IsFirstPaint() &&
      mCompositionManager->RequiresReorientation(aTargetConfig.orientation())) {
    if (mForceCompositionTask != nullptr) {
      mForceCompositionTask->Cancel();
    }
    RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod(
        "layers::CompositorBridgeParent::ForceComposition", this,
        &CompositorBridgeParent::ForceComposition);
    mForceCompositionTask = task;
    ScheduleTask(task.forget(), StaticPrefs::layers_orientation_sync_timeout());
  }
}

void CompositorBridgeParent::ShadowLayersUpdated(
    LayerTransactionParent* aLayerTree, const TransactionInfo& aInfo,
    bool aHitTestUpdate) {
  const TargetConfig& targetConfig = aInfo.targetConfig();

  ScheduleRotationOnCompositorThread(targetConfig, aInfo.isFirstPaint());

  // Instruct the LayerManager to update its render bounds now. Since all the
  // orientation change, dimension change would be done at the stage, update the
  // size here is free of race condition.
  mLayerManager->UpdateRenderBounds(targetConfig.naturalBounds());
  mLayerManager->SetRegionToClear(targetConfig.clearRegion());
  if (mLayerManager->GetCompositor()) {
    mLayerManager->GetCompositor()->SetScreenRotation(targetConfig.rotation());
  }

  mCompositionManager->Updated(aInfo.isFirstPaint(), targetConfig);
  Layer* root = aLayerTree->GetRoot();
  mLayerManager->SetRoot(root);

  if (mApzUpdater && !aInfo.isRepeatTransaction()) {
    mApzUpdater->UpdateFocusState(mRootLayerTreeID,
                                  WRRootId::NonWebRender(mRootLayerTreeID),
                                  aInfo.focusTarget());

    if (aHitTestUpdate) {
      AutoResolveRefLayers resolve(mCompositionManager);

      mApzUpdater->UpdateHitTestingTree(root, aInfo.isFirstPaint(),
                                        mRootLayerTreeID,
                                        aInfo.paintSequenceNumber());
    }
  }

  // The transaction ID might get reset to 1 if the page gets reloaded, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1145295#c41
  // Otherwise, it should be continually increasing.
  MOZ_ASSERT(aInfo.id() == TransactionId{1} ||
             aInfo.id() > mPendingTransaction);
  mPendingTransaction = aInfo.id();
  mRefreshStartTime = aInfo.refreshStart();
  mTxnStartTime = aInfo.transactionStart();
  mFwdTime = aInfo.fwdTime();
  RegisterPayloads(aLayerTree, aInfo.payload());

  if (root) {
    SetShadowProperties(root);
  }
  if (aInfo.scheduleComposite()) {
    ScheduleComposition();
    if (mPaused) {
      TimeStamp now = TimeStamp::Now();
      DidComposite(VsyncId(), now, now);
    }
  }
  mLayerManager->NotifyShadowTreeTransaction();
}

void CompositorBridgeParent::ScheduleComposite(
    LayerTransactionParent* aLayerTree) {
  ScheduleComposition();
}

bool CompositorBridgeParent::SetTestSampleTime(const LayersId& aId,
                                               const TimeStamp& aTime) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (aTime.IsNull()) {
    return false;
  }

  mTestTime = Some(aTime);
  if (mApzcTreeManager) {
    mApzcTreeManager->SetTestSampleTime(mTestTime);
  }

  if (mWrBridge) {
    mWrBridge->FlushRendering();
    return true;
  }

  bool testComposite =
      mCompositionManager && mCompositorScheduler->NeedsComposite();

  // Update but only if we were already scheduled to animate
  if (testComposite) {
    AutoResolveRefLayers resolve(mCompositionManager);
    bool requestNextFrame =
        mCompositionManager->TransformShadowTree(aTime, mVsyncRate);
    if (!requestNextFrame) {
      CancelCurrentCompositeTask();
      // Pretend we composited in case someone is wating for this event.
      TimeStamp now = TimeStamp::Now();
      DidComposite(VsyncId(), now, now);
    }
  }

  return true;
}

void CompositorBridgeParent::LeaveTestMode(const LayersId& aId) {
  mTestTime = Nothing();
  if (mApzcTreeManager) {
    mApzcTreeManager->SetTestSampleTime(mTestTime);
  }
}

void CompositorBridgeParent::ApplyAsyncProperties(
    LayerTransactionParent* aLayerTree, TransformsToSkip aSkip) {
  // NOTE: This should only be used for testing. For example, when mTestTime is
  // non-empty, or when called from test-only methods like
  // LayerTransactionParent::RecvGetAnimationTransform.

  // Synchronously update the layer tree
  if (aLayerTree->GetRoot()) {
    AutoResolveRefLayers resolve(mCompositionManager);
    SetShadowProperties(mLayerManager->GetRoot());

    TimeStamp time =
        mTestTime.valueOr(mCompositorScheduler->GetLastComposeTime());
    bool requestNextFrame =
        mCompositionManager->TransformShadowTree(time, mVsyncRate, aSkip);
    if (!requestNextFrame) {
      CancelCurrentCompositeTask();
      // Pretend we composited in case someone is waiting for this event.
      TimeStamp now = TimeStamp::Now();
      DidComposite(VsyncId(), now, now);
    }
  }
}

CompositorAnimationStorage* CompositorBridgeParent::GetAnimationStorage() {
  if (!mAnimationStorage) {
    mAnimationStorage = new CompositorAnimationStorage();
  }
  return mAnimationStorage;
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvGetFrameUniformity(
    FrameUniformityData* aOutData) {
  mCompositionManager->GetFrameUniformity(aOutData);
  return IPC_OK();
}

void CompositorBridgeParent::SetTestAsyncScrollOffset(
    const WRRootId& aWrRootId, const ScrollableLayerGuid::ViewID& aScrollId,
    const CSSPoint& aPoint) {
  if (mApzUpdater) {
    MOZ_ASSERT(aWrRootId.IsValid());
    mApzUpdater->SetTestAsyncScrollOffset(aWrRootId, aScrollId, aPoint);
  }
}

void CompositorBridgeParent::SetTestAsyncZoom(
    const WRRootId& aWrRootId, const ScrollableLayerGuid::ViewID& aScrollId,
    const LayerToParentLayerScale& aZoom) {
  if (mApzUpdater) {
    MOZ_ASSERT(aWrRootId.IsValid());
    mApzUpdater->SetTestAsyncZoom(aWrRootId, aScrollId, aZoom);
  }
}

void CompositorBridgeParent::FlushApzRepaints(const WRRootId& aWrRootId) {
  MOZ_ASSERT(mApzUpdater);
  MOZ_ASSERT(aWrRootId.IsValid());
  mApzUpdater->RunOnControllerThread(
      UpdaterQueueSelector(aWrRootId),
      NS_NewRunnableFunction(
          "layers::CompositorBridgeParent::FlushApzRepaints",
          [=]() { APZCTreeManager::FlushApzRepaints(aWrRootId.mLayersId); }));
}

void CompositorBridgeParent::GetAPZTestData(const WRRootId& aWrRootId,
                                            APZTestData* aOutData) {
  if (mApzUpdater) {
    MOZ_ASSERT(aWrRootId.IsValid());
    mApzUpdater->GetAPZTestData(aWrRootId, aOutData);
  }
}

void CompositorBridgeParent::SetConfirmedTargetAPZC(
    const LayersId& aLayersId, const uint64_t& aInputBlockId,
    const nsTArray<SLGuidAndRenderRoot>& aTargets) {
  if (!mApzcTreeManager || !mApzUpdater) {
    return;
  }
  // Need to specifically bind this since it's overloaded.
  void (APZCTreeManager::*setTargetApzcFunc)(
      uint64_t, const nsTArray<SLGuidAndRenderRoot>&) =
      &APZCTreeManager::SetTargetAPZC;
  RefPtr<Runnable> task = NewRunnableMethod<
      uint64_t, StoreCopyPassByConstLRef<nsTArray<SLGuidAndRenderRoot>>>(
      "layers::CompositorBridgeParent::SetConfirmedTargetAPZC",
      mApzcTreeManager.get(), setTargetApzcFunc, aInputBlockId, aTargets);
  UpdaterQueueSelector selector(aLayersId);
  for (size_t i = 0; i < aTargets.Length(); i++) {
    selector.mRenderRoots += aTargets[i].mRenderRoot;
  }
  mApzUpdater->RunOnControllerThread(selector, task.forget());
}

void CompositorBridgeParent::InitializeLayerManager(
    const nsTArray<LayersBackend>& aBackendHints) {
  NS_ASSERTION(!mLayerManager, "Already initialised mLayerManager");
  NS_ASSERTION(!mCompositor, "Already initialised mCompositor");

  if (!InitializeAdvancedLayers(aBackendHints, nullptr)) {
    mCompositor = NewCompositor(aBackendHints);
    if (!mCompositor) {
      return;
    }
    mLayerManager = new LayerManagerComposite(mCompositor);
  }
  mLayerManager->SetCompositorBridgeID(mCompositorBridgeID);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[mRootLayerTreeID].mLayerManager = mLayerManager;
}

bool CompositorBridgeParent::InitializeAdvancedLayers(
    const nsTArray<LayersBackend>& aBackendHints,
    TextureFactoryIdentifier* aOutIdentifier) {
#ifdef XP_WIN
  if (!mOptions.UseAdvancedLayers()) {
    return false;
  }

  // Currently LayerManagerMLGPU hardcodes a D3D11 device, so we reject using
  // AL if LAYERS_D3D11 isn't in the backend hints.
  if (!aBackendHints.Contains(LayersBackend::LAYERS_D3D11)) {
    return false;
  }

  RefPtr<LayerManagerMLGPU> manager = new LayerManagerMLGPU(mWidget);
  if (!manager->Initialize()) {
    return false;
  }

  if (aOutIdentifier) {
    *aOutIdentifier = manager->GetTextureFactoryIdentifier();
  }
  mLayerManager = manager;
  return true;
#else
  return false;
#endif
}

RefPtr<Compositor> CompositorBridgeParent::NewCompositor(
    const nsTArray<LayersBackend>& aBackendHints) {
  for (size_t i = 0; i < aBackendHints.Length(); ++i) {
    RefPtr<Compositor> compositor;
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
      compositor =
          new CompositorOGL(this, mWidget, mEGLSurfaceSize.width,
                            mEGLSurfaceSize.height, mUseExternalSurfaceSize);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_BASIC) {
#ifdef MOZ_WIDGET_GTK
      if (gfxVars::UseXRender()) {
        compositor = new X11BasicCompositor(this, mWidget);
      } else
#endif
      {
        compositor = new BasicCompositor(this, mWidget);
      }
#ifdef XP_WIN
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11) {
      compositor = new CompositorD3D11(this, mWidget);
#endif
    }
    nsCString failureReason;

    // Some software GPU emulation implementations will happily try to create
    // unreasonably big surfaces and then fail in awful ways.
    // Let's at least limit this to the default max texture size we use for
    // content, anything larger than that will fail to render on the content
    // side anyway. We can revisit this value and make it even tighter if need
    // be.
    const int max_fb_size = 32767;
    const LayoutDeviceIntSize size = mWidget->GetClientSize();
    if (size.width > max_fb_size || size.height > max_fb_size) {
      failureReason = "FEATURE_FAILURE_MAX_FRAMEBUFFER_SIZE";
      return nullptr;
    }

    MOZ_ASSERT(!gfxVars::UseWebRender() ||
               aBackendHints[i] == LayersBackend::LAYERS_BASIC);
    if (compositor && compositor->Initialize(&failureReason)) {
      if (failureReason.IsEmpty()) {
        failureReason = "SUCCESS";
      }

      // should only report success here
      if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
        Telemetry::Accumulate(Telemetry::OPENGL_COMPOSITING_FAILURE_ID,
                              failureReason);
      }
#ifdef XP_WIN
      else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11) {
        Telemetry::Accumulate(Telemetry::D3D11_COMPOSITING_FAILURE_ID,
                              failureReason);
      }
#endif

      return compositor;
    }

    // report any failure reasons here
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
      gfxCriticalNote << "[OPENGL] Failed to init compositor with reason: "
                      << failureReason.get();
      Telemetry::Accumulate(Telemetry::OPENGL_COMPOSITING_FAILURE_ID,
                            failureReason);
    }
#ifdef XP_WIN
    else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11) {
      gfxCriticalNote << "[D3D11] Failed to init compositor with reason: "
                      << failureReason.get();
      Telemetry::Accumulate(Telemetry::D3D11_COMPOSITING_FAILURE_ID,
                            failureReason);
    }
#endif
  }

  return nullptr;
}

PLayerTransactionParent* CompositorBridgeParent::AllocPLayerTransactionParent(
    const nsTArray<LayersBackend>& aBackendHints, const LayersId& aId) {
  MOZ_ASSERT(!aId.IsValid());

#ifdef XP_WIN
  // This is needed to avoid freezing the window on a device crash on double
  // buffering, see bug 1549674.
  if (gfxVars::UseDoubleBufferingWithCompositor() && XRE_IsGPUProcess()) {
    mWidget->AsWindows()->EnsureCompositorWindow();
  }
#endif

  InitializeLayerManager(aBackendHints);

  if (!mLayerManager) {
    NS_WARNING("Failed to initialise Compositor");
    LayerTransactionParent* p = new LayerTransactionParent(
        /* aManager */ nullptr, this, /* aAnimStorage */ nullptr,
        mRootLayerTreeID, mVsyncRate);
    p->AddIPDLReference();
    return p;
  }

  mCompositionManager = new AsyncCompositionManager(this, mLayerManager);

  LayerTransactionParent* p = new LayerTransactionParent(
      mLayerManager, this, GetAnimationStorage(), mRootLayerTreeID, mVsyncRate);
  p->AddIPDLReference();
  return p;
}

bool CompositorBridgeParent::DeallocPLayerTransactionParent(
    PLayerTransactionParent* actor) {
  static_cast<LayerTransactionParent*>(actor)->ReleaseIPDLReference();
  return true;
}

CompositorBridgeParent* CompositorBridgeParent::GetCompositorBridgeParent(
    uint64_t id) {
  AssertIsInCompositorThread();
  CompositorMap::iterator it = sCompositorMap->find(id);
  return it != sCompositorMap->end() ? it->second : nullptr;
}

void CompositorBridgeParent::AddCompositor(CompositorBridgeParent* compositor,
                                           uint64_t* outID) {
  AssertIsInCompositorThread();

  static uint64_t sNextID = 1;

  ++sNextID;
  (*sCompositorMap)[sNextID] = compositor;
  *outID = sNextID;
}

CompositorBridgeParent* CompositorBridgeParent::RemoveCompositor(uint64_t id) {
  AssertIsInCompositorThread();

  CompositorMap::iterator it = sCompositorMap->find(id);
  if (it == sCompositorMap->end()) {
    return nullptr;
  }
  CompositorBridgeParent* retval = it->second;
  sCompositorMap->erase(it);
  return retval;
}

void CompositorBridgeParent::NotifyVsync(const VsyncEvent& aVsync,
                                         const LayersId& aLayersId) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  auto it = sIndirectLayerTrees.find(aLayersId);
  if (it == sIndirectLayerTrees.end()) return;

  CompositorBridgeParent* cbp = it->second.mParent;
  if (!cbp || !cbp->mWidget) return;

  RefPtr<VsyncObserver> obs = cbp->mWidget->GetVsyncObserver();
  if (!obs) return;

  obs->NotifyVsync(aVsync);
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvNotifyChildCreated(
    const LayersId& child, CompositorOptions* aOptions) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  NotifyChildCreated(child);
  *aOptions = mOptions;
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvNotifyChildRecreated(
    const LayersId& aChild, CompositorOptions* aOptions) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);

  if (sIndirectLayerTrees.find(aChild) != sIndirectLayerTrees.end()) {
    NS_WARNING("Invalid to register the same layer tree twice");
    return IPC_FAIL_NO_REASON(this);
  }

  NotifyChildCreated(aChild);
  *aOptions = mOptions;
  return IPC_OK();
}

void CompositorBridgeParent::NotifyChildCreated(LayersId aChild) {
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  sIndirectLayerTrees[aChild].mParent = this;
  sIndirectLayerTrees[aChild].mLayerManager = mLayerManager;
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvMapAndNotifyChildCreated(
    const LayersId& aChild, const base::ProcessId& aOwnerPid,
    CompositorOptions* aOptions) {
  // We only use this message when the remote compositor is in the GPU process.
  // It is harmless to call it, though.
  MOZ_ASSERT(XRE_IsGPUProcess());

  LayerTreeOwnerTracker::Get()->Map(aChild, aOwnerPid);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  NotifyChildCreated(aChild);
  *aOptions = mOptions;
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvAdoptChild(
    const LayersId& child) {
  RefPtr<APZUpdater> oldApzUpdater;
  APZCTreeManagerParent* parent;
  bool scheduleComposition = false;
  RefPtr<ContentCompositorBridgeParent> cpcp;
  RefPtr<WebRenderBridgeParent> childWrBridge;

  // Before adopting the child, save the old compositor's root content
  // controller. We may need this to clear old layer transforms associated
  // with the child.
  // This is outside the lock because GetGeckoContentControllerForRoot()
  // does its own locking.
  RefPtr<GeckoContentController> oldRootController =
      GetGeckoContentControllerForRoot(child);

  {  // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    // If child is already belong to this CompositorBridgeParent,
    // no need to handle adopting child.
    if (sIndirectLayerTrees[child].mParent == this) {
      return IPC_OK();
    }

    if (sIndirectLayerTrees[child].mParent) {
      // We currently don't support adopting children from one compositor to
      // another if the two compositors don't have the same options.
      MOZ_ASSERT(sIndirectLayerTrees[child].mParent->mOptions == mOptions);
      oldApzUpdater = sIndirectLayerTrees[child].mParent->mApzUpdater;
    }
    NotifyChildCreated(child);
    if (sIndirectLayerTrees[child].mLayerTree) {
      sIndirectLayerTrees[child].mLayerTree->SetLayerManager(
          mLayerManager, GetAnimationStorage());
      // Trigger composition to handle a case that mLayerTree was not composited
      // yet by previous CompositorBridgeParent, since nsRefreshDriver might
      // wait composition complete.
      scheduleComposition = true;
    }
    if (mWrBridge) {
      childWrBridge = sIndirectLayerTrees[child].mWrBridge;
      cpcp = sIndirectLayerTrees[child].mContentCompositorBridgeParent;
    }
    parent = sIndirectLayerTrees[child].mApzcTreeManagerParent;
  }

  if (scheduleComposition) {
    ScheduleComposition();
  }

  if (childWrBridge) {
    MOZ_ASSERT(mWrBridge);
    nsTArray<RefPtr<wr::WebRenderAPI>> apis;
    DebugOnly<bool> cloneSuccess = mWrBridge->CloneWebRenderAPIs(apis);
    MOZ_ASSERT(cloneSuccess);
    wr::Epoch newEpoch = childWrBridge->UpdateWebRender(
        mWrBridge->CompositorScheduler(), std::move(apis),
        mWrBridge->AsyncImageManager(), GetAnimationStorage(),
        mWrBridge->GetTextureFactoryIdentifier());
    // Pretend we composited, since parent CompositorBridgeParent was replaced.
    TimeStamp now = TimeStamp::Now();
    NotifyPipelineRendered(childWrBridge->PipelineId(), newEpoch, VsyncId(),
                           now, now, now);
  }

  if (oldApzUpdater) {
    // We don't fully support moving a child from an APZ-enabled compositor to a
    // APZ-disabled compositor. The mOptions assertion above should already
    // ensure this, since APZ-ness is one of the things in mOptions. Note
    // however it is possible for mApzUpdater to be non-null here with
    // oldApzUpdater null, because the child may not have been previously
    // composited.
    MOZ_ASSERT(mApzUpdater);

    // As this nonetheless can happen (if e.g. a WebExtension moves a tab
    // into a popup window), try to handle it gracefully by clearing the
    // old layer transforms associated with the child. (Since the new compositor
    // is APZ-disabled, there will be nothing to update the transforms going
    // forward.)
    if (!mApzUpdater && oldRootController) {
      nsTArray<MatrixMessage> clear;
      clear.AppendElement(MatrixMessage(Nothing(), child));
      oldRootController->NotifyLayerTransforms(clear);
    }
  }
  if (mApzUpdater) {
    if (parent) {
      MOZ_ASSERT(mApzcTreeManager);
      parent->ChildAdopted(mApzcTreeManager, mApzUpdater);
    }
    mApzUpdater->NotifyLayerTreeAdopted(
        WRRootId(child, gfxUtils::GetContentRenderRoot()), oldApzUpdater);
  }
  return IPC_OK();
}

PWebRenderBridgeParent* CompositorBridgeParent::AllocPWebRenderBridgeParent(
    const wr::PipelineId& aPipelineId, const LayoutDeviceIntSize& aSize) {
  MOZ_ASSERT(wr::AsLayersId(aPipelineId) == mRootLayerTreeID);
  MOZ_ASSERT(!mWrBridge);
  MOZ_ASSERT(!mCompositor);
  MOZ_ASSERT(!mCompositorScheduler);
  MOZ_ASSERT(mWidget);

#ifdef XP_WIN
  if (mWidget && (DeviceManagerDx::Get()->CanUseDComp() ||
                  gfxVars::UseWebRenderFlipSequentialWin())) {
    mWidget->AsWindows()->EnsureCompositorWindow();
  }
#endif

  RefPtr<widget::CompositorWidget> widget = mWidget;
  wr::WrWindowId windowId = wr::NewWindowId();
  if (mApzUpdater) {
    // If APZ is enabled, we need to register the APZ updater with the window id
    // before the updater thread is created in WebRenderAPI::Create, so
    // that the callback from the updater thread can find the right APZUpdater.
    mApzUpdater->SetWebRenderWindowId(windowId);
  }
  if (mApzSampler) {
    // Same as for mApzUpdater, but for the sampler thread.
    mApzSampler->SetWebRenderWindowId(windowId);
  }
  nsTArray<RefPtr<wr::WebRenderAPI>> apis;
  apis.AppendElement(
      wr::WebRenderAPI::Create(this, std::move(widget), windowId, aSize));
  if (!apis[0]) {
    mWrBridge = WebRenderBridgeParent::CreateDestroyed(aPipelineId);
    mWrBridge.get()->AddRef();  // IPDL reference
    return mWrBridge;
  }

  if (StaticPrefs::gfx_webrender_split_render_roots_AtStartup()) {
    apis.AppendElement(
        apis[0]->CreateDocument(aSize, 1, wr::RenderRoot::Content));
    apis.AppendElement(
        apis[0]->CreateDocument(aSize, 2, wr::RenderRoot::Popover));
  }

  nsTArray<RefPtr<wr::WebRenderAPI>> clonedApis;
  for (auto& api : apis) {
    wr::TransactionBuilder txn;
    txn.SetRootPipeline(aPipelineId);
    api->SendTransaction(txn);
    clonedApis.AppendElement(api->Clone());
  }

  mAsyncImageManager = new AsyncImagePipelineManager(std::move(clonedApis));
  RefPtr<AsyncImagePipelineManager> asyncMgr = mAsyncImageManager;
  RefPtr<CompositorAnimationStorage> animStorage = GetAnimationStorage();
  mWrBridge = new WebRenderBridgeParent(this, aPipelineId, mWidget, nullptr,
                                        std::move(apis), std::move(asyncMgr),
                                        std::move(animStorage), mVsyncRate);
  mWrBridge.get()->AddRef();  // IPDL reference

  mCompositorScheduler = mWrBridge->CompositorScheduler();
  MOZ_ASSERT(mCompositorScheduler);
  {  // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    MOZ_ASSERT(sIndirectLayerTrees[mRootLayerTreeID].mWrBridge == nullptr);
    sIndirectLayerTrees[mRootLayerTreeID].mWrBridge = mWrBridge;
  }
  return mWrBridge;
}

bool CompositorBridgeParent::DeallocPWebRenderBridgeParent(
    PWebRenderBridgeParent* aActor) {
  WebRenderBridgeParent* parent = static_cast<WebRenderBridgeParent*>(aActor);
  {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    auto it = sIndirectLayerTrees.find(wr::AsLayersId(parent->PipelineId()));
    if (it != sIndirectLayerTrees.end()) {
      it->second.mWrBridge = nullptr;
    }
  }
  parent->Release();  // IPDL reference
  return true;
}

void CompositorBridgeParent::NotifyMemoryPressure() {
  if (mWrBridge) {
    RefPtr<wr::WebRenderAPI> api =
        mWrBridge->GetWebRenderAPI(wr::RenderRoot::Default);
    if (api) {
      api->NotifyMemoryPressure();
    }
  }
}

void CompositorBridgeParent::AccumulateMemoryReport(wr::MemoryReport* aReport) {
  if (mWrBridge) {
    RefPtr<wr::WebRenderAPI> api =
        mWrBridge->GetWebRenderAPI(wr::RenderRoot::Default);
    if (api) {
      api->AccumulateMemoryReport(aReport);
    }
  }
}

RefPtr<WebRenderBridgeParent> CompositorBridgeParent::GetWebRenderBridgeParent()
    const {
  return mWrBridge;
}

Maybe<TimeStamp> CompositorBridgeParent::GetTestingTimeStamp() const {
  return mTestTime;
}

void EraseLayerState(LayersId aId) {
  RefPtr<APZUpdater> apz;

  {  // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    auto iter = sIndirectLayerTrees.find(aId);
    if (iter != sIndirectLayerTrees.end()) {
      CompositorBridgeParent* parent = iter->second.mParent;
      if (parent) {
        apz = parent->GetAPZUpdater();
      }
      sIndirectLayerTrees.erase(iter);
    }
  }

  if (apz) {
    apz->NotifyLayerTreeRemoved(
        WRRootId(aId, gfxUtils::GetContentRenderRoot()));
  }
}

/*static*/
void CompositorBridgeParent::DeallocateLayerTreeId(LayersId aId) {
  MOZ_ASSERT(NS_IsMainThread());
  // Here main thread notifies compositor to remove an element from
  // sIndirectLayerTrees. This removed element might be queried soon.
  // Checking the elements of sIndirectLayerTrees exist or not before using.
  if (!CompositorLoop()) {
    gfxCriticalError() << "Attempting to post to a invalid Compositor Loop";
    return;
  }
  CompositorLoop()->PostTask(
      NewRunnableFunction("EraseLayerStateRunnable", &EraseLayerState, aId));
}

static void UpdateControllerForLayersId(LayersId aLayersId,
                                        GeckoContentController* aController) {
  // Adopt ref given to us by SetControllerForLayerTree()
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aLayersId].mController =
      already_AddRefed<GeckoContentController>(aController);
}

ScopedLayerTreeRegistration::ScopedLayerTreeRegistration(
    APZCTreeManager* aApzctm, LayersId aLayersId, Layer* aRoot,
    GeckoContentController* aController)
    : mLayersId(aLayersId) {
  EnsureLayerTreeMapReady();
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aLayersId].mRoot = aRoot;
  sIndirectLayerTrees[aLayersId].mController = aController;
}

ScopedLayerTreeRegistration::~ScopedLayerTreeRegistration() {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees.erase(mLayersId);
}

/*static*/
void CompositorBridgeParent::SetControllerForLayerTree(
    LayersId aLayersId, GeckoContentController* aController) {
  // This ref is adopted by UpdateControllerForLayersId().
  aController->AddRef();
  CompositorLoop()->PostTask(NewRunnableFunction(
      "UpdateControllerForLayersIdRunnable", &UpdateControllerForLayersId,
      aLayersId, aController));
}

/*static*/
already_AddRefed<IAPZCTreeManager> CompositorBridgeParent::GetAPZCTreeManager(
    LayersId aLayersId) {
  EnsureLayerTreeMapReady();
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  LayerTreeMap::iterator cit = sIndirectLayerTrees.find(aLayersId);
  if (sIndirectLayerTrees.end() == cit) {
    return nullptr;
  }
  LayerTreeState* lts = &cit->second;

  RefPtr<IAPZCTreeManager> apzctm =
      lts->mParent ? lts->mParent->mApzcTreeManager.get() : nullptr;
  return apzctm.forget();
}

#if defined(MOZ_GECKO_PROFILER)
static void InsertVsyncProfilerMarker(TimeStamp aVsyncTimestamp) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (profiler_thread_is_being_profiled()) {
    profiler_add_marker("VsyncTimestamp", JS::ProfilingCategoryPair::GRAPHICS,
                        MakeUnique<VsyncMarkerPayload>(aVsyncTimestamp));
  }
}
#endif

/*static */
void CompositorBridgeParent::PostInsertVsyncProfilerMarker(
    TimeStamp aVsyncTimestamp) {
#if defined(MOZ_GECKO_PROFILER)
  // Called in the vsync thread
  if (profiler_is_active() && CompositorThreadHolder::IsActive()) {
    CompositorLoop()->PostTask(
        NewRunnableFunction("InsertVsyncProfilerMarkerRunnable",
                            InsertVsyncProfilerMarker, aVsyncTimestamp));
  }
#endif
}

widget::PCompositorWidgetParent*
CompositorBridgeParent::AllocPCompositorWidgetParent(
    const CompositorWidgetInitData& aInitData) {
#if defined(MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING)
  if (mWidget) {
    // Should not create two widgets on the same compositor.
    return nullptr;
  }

  widget::CompositorWidgetParent* widget =
      new widget::CompositorWidgetParent(aInitData, mOptions);
  widget->AddRef();

  // Sending the constructor acts as initialization as well.
  mWidget = widget;
  return widget;
#else
  return nullptr;
#endif
}

bool CompositorBridgeParent::DeallocPCompositorWidgetParent(
    PCompositorWidgetParent* aActor) {
#if defined(MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING)
  static_cast<widget::CompositorWidgetParent*>(aActor)->Release();
  return true;
#else
  return false;
#endif
}

bool CompositorBridgeParent::IsPendingComposite() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mCompositor) {
    return false;
  }
  return mCompositor->IsPendingComposite();
}

void CompositorBridgeParent::FinishPendingComposite() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mCompositor) {
    return;
  }
  return mCompositor->FinishPendingComposite();
}

CompositorController*
CompositorBridgeParent::LayerTreeState::GetCompositorController() const {
  return mParent;
}

MetricsSharingController*
CompositorBridgeParent::LayerTreeState::CrossProcessSharingController() const {
  return mContentCompositorBridgeParent;
}

MetricsSharingController*
CompositorBridgeParent::LayerTreeState::InProcessSharingController() const {
  return mParent;
}

void CompositorBridgeParent::DidComposite(const VsyncId& aId,
                                          TimeStamp& aCompositeStart,
                                          TimeStamp& aCompositeEnd) {
  if (mWrBridge) {
    MOZ_ASSERT(false);  // This should never get called for a WR compositor
  } else {
    NotifyDidComposite(mPendingTransaction, aId, aCompositeStart,
                       aCompositeEnd);
#if defined(ENABLE_FRAME_LATENCY_LOG)
    if (mPendingTransaction.IsValid()) {
      if (mRefreshStartTime) {
        int32_t latencyMs =
            lround((aCompositeEnd - mRefreshStartTime).ToMilliseconds());
        printf_stderr(
            "From transaction start to end of generate frame latencyMs %d this "
            "%p\n",
            latencyMs, this);
      }
      if (mFwdTime) {
        int32_t latencyMs = lround((aCompositeEnd - mFwdTime).ToMilliseconds());
        printf_stderr(
            "From forwarding transaction to end of generate frame latencyMs %d "
            "this %p\n",
            latencyMs, this);
      }
    }
    mRefreshStartTime = TimeStamp();
    mTxnStartTime = TimeStamp();
    mFwdTime = TimeStamp();
#endif
    mPendingTransaction = TransactionId{0};
  }
}

void CompositorBridgeParent::NotifyDidSceneBuild(
    const nsTArray<wr::RenderRoot>& aRenderRoots,
    RefPtr<wr::WebRenderPipelineInfo> aInfo) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mPaused) {
    return;
  }

  if (mWrBridge) {
    mWrBridge->NotifyDidSceneBuild(aRenderRoots, aInfo);
  } else {
    mCompositorScheduler->ScheduleComposition();
  }
}

void CompositorBridgeParent::NotifyPipelineRendered(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch,
    const VsyncId& aCompositeStartId, TimeStamp& aCompositeStart,
    TimeStamp& aRenderStart, TimeStamp& aCompositeEnd,
    wr::RendererStats* aStats) {
  if (!mWrBridge || !mAsyncImageManager) {
    return;
  }

  nsTArray<FrameStats> stats;

  RefPtr<UiCompositorControllerParent> uiController =
      UiCompositorControllerParent::GetFromRootLayerTreeId(mRootLayerTreeID);

  if (mWrBridge->PipelineId() == aPipelineId) {
    mWrBridge->RemoveEpochDataPriorTo(aEpoch);

    if (!mPaused) {
      TransactionId transactionId = mWrBridge->FlushTransactionIdsForEpoch(
          aEpoch, aCompositeStartId, aCompositeStart, aRenderStart,
          aCompositeEnd, uiController);
      Unused << SendDidComposite(LayersId{0}, transactionId, aCompositeStart,
                                 aCompositeEnd);

      nsTArray<ImageCompositeNotificationInfo> notifications;
      mWrBridge->ExtractImageCompositeNotifications(&notifications);
      if (!notifications.IsEmpty()) {
        Unused << ImageBridgeParent::NotifyImageComposites(notifications);
      }
    }
    return;
  }

  auto wrBridge = mAsyncImageManager->GetWrBridge(aPipelineId);
  if (wrBridge && wrBridge->GetCompositorBridge()) {
    MOZ_ASSERT(!wrBridge->IsRootWebRenderBridgeParent());
    wrBridge->RemoveEpochDataPriorTo(aEpoch);
    if (!mPaused) {
      TransactionId transactionId = wrBridge->FlushTransactionIdsForEpoch(
          aEpoch, aCompositeStartId, aCompositeStart, aRenderStart,
          aCompositeEnd, uiController, aStats, &stats);
      Unused << wrBridge->GetCompositorBridge()->SendDidComposite(
          wrBridge->GetLayersId(), transactionId, aCompositeStart,
          aCompositeEnd);
    }
  }

  if (!stats.IsEmpty()) {
    Unused << SendNotifyFrameStats(stats);
  }
}

RefPtr<AsyncImagePipelineManager>
CompositorBridgeParent::GetAsyncImagePipelineManager() const {
  return mAsyncImageManager;
}

void CompositorBridgeParent::NotifyDidComposite(TransactionId aTransactionId,
                                                VsyncId aId,
                                                TimeStamp& aCompositeStart,
                                                TimeStamp& aCompositeEnd) {
  MOZ_ASSERT(
      !mWrBridge);  // We should be going through NotifyPipelineRendered instead

  Unused << SendDidComposite(LayersId{0}, aTransactionId, aCompositeStart,
                             aCompositeEnd);

  if (mLayerManager) {
    nsTArray<ImageCompositeNotificationInfo> notifications;
    mLayerManager->ExtractImageCompositeNotifications(&notifications);
    if (!notifications.IsEmpty()) {
      Unused << ImageBridgeParent::NotifyImageComposites(notifications);
    }
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  ForEachIndirectLayerTree([&](LayerTreeState* lts,
                               const LayersId& aLayersId) -> void {
    if (lts->mContentCompositorBridgeParent && lts->mParent == this) {
      ContentCompositorBridgeParent* cpcp = lts->mContentCompositorBridgeParent;
      cpcp->DidCompositeLocked(aLayersId, aId, aCompositeStart, aCompositeEnd);
    }
  });
}

void CompositorBridgeParent::InvalidateRemoteLayers() {
  MOZ_ASSERT(CompositorLoop() == MessageLoop::current());

  Unused << PCompositorBridgeParent::SendInvalidateLayers(LayersId{0});

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  ForEachIndirectLayerTree([](LayerTreeState* lts,
                              const LayersId& aLayersId) -> void {
    if (lts->mContentCompositorBridgeParent) {
      ContentCompositorBridgeParent* cpcp = lts->mContentCompositorBridgeParent;
      Unused << cpcp->SendInvalidateLayers(aLayersId);
    }
  });
}

void UpdateIndirectTree(LayersId aId, Layer* aRoot,
                        const TargetConfig& aTargetConfig) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aId].mRoot = aRoot;
  sIndirectLayerTrees[aId].mTargetConfig = aTargetConfig;
}

/* static */ CompositorBridgeParent::LayerTreeState*
CompositorBridgeParent::GetIndirectShadowTree(LayersId aId) {
  // Only the compositor thread should use this method variant
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  LayerTreeMap::iterator cit = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() == cit) {
    return nullptr;
  }
  return &cit->second;
}

/* static */
bool CompositorBridgeParent::CallWithIndirectShadowTree(
    LayersId aId,
    const std::function<void(CompositorBridgeParent::LayerTreeState&)>& aFunc) {
  // Note that this does not make things universally threadsafe just because the
  // sIndirectLayerTreesLock mutex is held. This is because the compositor
  // thread can mutate the LayerTreeState outside the lock. It does however
  // ensure that the *storage* for the LayerTreeState remains stable, since we
  // should always hold the lock when adding/removing entries to the map.
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  LayerTreeMap::iterator cit = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() == cit) {
    return false;
  }
  aFunc(cit->second);
  return true;
}

static CompositorBridgeParent::LayerTreeState* GetStateForRoot(
    LayersId aContentLayersId, const MonitorAutoLock& aProofOfLock) {
  CompositorBridgeParent::LayerTreeState* state = nullptr;
  LayerTreeMap::iterator itr = sIndirectLayerTrees.find(aContentLayersId);
  if (sIndirectLayerTrees.end() != itr) {
    state = &itr->second;
  }

  // |state| is the state for the content process, but we want the APZCTMParent
  // for the parent process owning that content process. So we have to jump to
  // the LayerTreeState for the root layer tree id for that layer tree, and use
  // the mApzcTreeManagerParent from that. This should also work with nested
  // content processes, because RootLayerTreeId() will bypass any intermediate
  // processes' ids and go straight to the root.
  if (state) {
    LayersId rootLayersId = state->mParent->RootLayerTreeId();
    itr = sIndirectLayerTrees.find(rootLayersId);
    state = (sIndirectLayerTrees.end() != itr) ? &itr->second : nullptr;
  }

  return state;
}

/* static */
APZCTreeManagerParent* CompositorBridgeParent::GetApzcTreeManagerParentForRoot(
    LayersId aContentLayersId) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState* state =
      GetStateForRoot(aContentLayersId, lock);
  return state ? state->mApzcTreeManagerParent : nullptr;
}

/* static */
GeckoContentController*
CompositorBridgeParent::GetGeckoContentControllerForRoot(
    LayersId aContentLayersId) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState* state =
      GetStateForRoot(aContentLayersId, lock);
  return state ? state->mController.get() : nullptr;
}

PTextureParent* CompositorBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const LayersId& aId, const uint64_t& aSerial,
    const wr::MaybeExternalImageId& aExternalImageId) {
  return TextureHost::CreateIPDLActor(this, aSharedData, aReadLock,
                                      aLayersBackend, aFlags, aSerial,
                                      aExternalImageId);
}

bool CompositorBridgeParent::DeallocPTextureParent(PTextureParent* actor) {
  return TextureHost::DestroyIPDLActor(actor);
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvInitPCanvasParent(
    Endpoint<PCanvasParent>&& aEndpoint) {
  MOZ_CRASH("PCanvasParent shouldn't be created via CompositorBridgeParent.");
}

bool CompositorBridgeParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void CompositorBridgeParent::NotifyWebRenderContextPurge() {
  MOZ_ASSERT(CompositorLoop() == MessageLoop::current());
  RefPtr<wr::WebRenderAPI> api =
      mWrBridge->GetWebRenderAPI(wr::RenderRoot::Default);
  api->ClearAllCaches();
}

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
//#define PLUGINS_LOG(...) printf_stderr("CP [%s]: ", __FUNCTION__);
//                         printf_stderr(__VA_ARGS__);
//                         printf_stderr("\n");
#  define PLUGINS_LOG(...)

bool CompositorBridgeParent::UpdatePluginWindowState(LayersId aId) {
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& lts = sIndirectLayerTrees[aId];
  if (!lts.mParent) {
    PLUGINS_LOG("[%" PRIu64 "] layer tree compositor parent pointer is null",
                aId);
    return false;
  }

  // Check if this layer tree has received any shadow layer updates
  if (!lts.mUpdatedPluginDataAvailable) {
    PLUGINS_LOG("[%" PRIu64 "] no plugin data", aId);
    return false;
  }

  // pluginMetricsChanged tracks whether we need to send plugin update
  // data to the main thread. If we do we'll have to block composition,
  // which we want to avoid if at all possible.
  bool pluginMetricsChanged = false;

  // Same layer tree checks
  if (mLastPluginUpdateLayerTreeId == aId) {
    // no plugin data and nothing has changed, bail.
    if (!mCachedPluginData.Length() && !lts.mPluginData.Length()) {
      PLUGINS_LOG("[%" PRIu64 "] no data, no changes", aId);
      return false;
    }

    if (mCachedPluginData.Length() == lts.mPluginData.Length()) {
      // check for plugin data changes
      for (uint32_t idx = 0; idx < lts.mPluginData.Length(); idx++) {
        if (!(mCachedPluginData[idx] == lts.mPluginData[idx])) {
          pluginMetricsChanged = true;
          break;
        }
      }
    } else {
      // array lengths don't match, need to update
      pluginMetricsChanged = true;
    }
  } else {
    // exchanging layer trees, we need to update
    pluginMetricsChanged = true;
  }

  // Check if plugin windows are currently hidden due to scrolling
  if (mDeferPluginWindows) {
    PLUGINS_LOG("[%" PRIu64 "] suppressing", aId);
    return false;
  }

  // If the plugin windows were hidden but now are not, we need to force
  // update the metrics to make sure they are visible again.
  if (mPluginWindowsHidden) {
    PLUGINS_LOG("[%" PRIu64 "] re-showing", aId);
    mPluginWindowsHidden = false;
    pluginMetricsChanged = true;
  }

  if (!lts.mPluginData.Length()) {
    // Don't hide plugins if the previous remote layer tree didn't contain any.
    if (!mCachedPluginData.Length()) {
      PLUGINS_LOG("[%" PRIu64 "] nothing to hide", aId);
      return false;
    }

    uintptr_t parentWidget = GetWidget()->GetWidgetKey();

    // We will pass through here in cases where the previous shadow layer
    // tree contained visible plugins and the new tree does not. All we need
    // to do here is hide the plugins for the old tree, so don't waste time
    // calculating clipping.
    mPluginsLayerOffset = nsIntPoint(0, 0);
    mPluginsLayerVisibleRegion.SetEmpty();
    Unused << lts.mParent->SendHideAllPlugins(parentWidget);
    lts.mUpdatedPluginDataAvailable = false;
    PLUGINS_LOG("[%" PRIu64 "] hide all", aId);
  } else {
    // Retrieve the offset and visible region of the layer that hosts
    // the plugins, CompositorBridgeChild needs these in calculating proper
    // plugin clipping.
    LayerTransactionParent* layerTree = lts.mLayerTree;
    Layer* contentRoot = layerTree->GetRoot();
    if (contentRoot) {
      nsIntPoint offset;
      nsIntRegion visibleRegion;
      if (contentRoot->GetVisibleRegionRelativeToRootLayer(visibleRegion,
                                                           &offset)) {
        // Check to see if these values have changed, if so we need to
        // update plugin window position within the window.
        if (!pluginMetricsChanged &&
            mPluginsLayerVisibleRegion == visibleRegion &&
            mPluginsLayerOffset == offset) {
          PLUGINS_LOG("[%" PRIu64 "] no change", aId);
          return false;
        }
        mPluginsLayerOffset = offset;
        mPluginsLayerVisibleRegion = visibleRegion;
        Unused << lts.mParent->SendUpdatePluginConfigurations(
            LayoutDeviceIntPoint::FromUnknownPoint(offset),
            LayoutDeviceIntRegion::FromUnknownRegion(visibleRegion),
            lts.mPluginData);
        lts.mUpdatedPluginDataAvailable = false;
        PLUGINS_LOG("[%" PRIu64 "] updated", aId);
      } else {
        PLUGINS_LOG("[%" PRIu64 "] no visibility data", aId);
        return false;
      }
    } else {
      PLUGINS_LOG("[%" PRIu64 "] no content root", aId);
      return false;
    }
  }

  mLastPluginUpdateLayerTreeId = aId;
  mCachedPluginData = lts.mPluginData;
  return true;
}

void CompositorBridgeParent::ScheduleShowAllPluginWindows() {
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(
      NewRunnableMethod("layers::CompositorBridgeParent::ShowAllPluginWindows",
                        this, &CompositorBridgeParent::ShowAllPluginWindows));
}

void CompositorBridgeParent::ShowAllPluginWindows() {
  MOZ_ASSERT(!NS_IsMainThread());
  mDeferPluginWindows = false;
  ScheduleComposition();
}

void CompositorBridgeParent::ScheduleHideAllPluginWindows() {
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(
      NewRunnableMethod("layers::CompositorBridgeParent::HideAllPluginWindows",
                        this, &CompositorBridgeParent::HideAllPluginWindows));
}

void CompositorBridgeParent::HideAllPluginWindows() {
  MOZ_ASSERT(!NS_IsMainThread());
  // No plugins in the cache implies no plugins to manage
  // in this content.
  if (!mCachedPluginData.Length() || mDeferPluginWindows) {
    return;
  }

  uintptr_t parentWidget = GetWidget()->GetWidgetKey();

  mDeferPluginWindows = true;
  mPluginWindowsHidden = true;

#  if defined(XP_WIN)
  // We will get an async reply that this has happened and then send hide.
  mWaitForPluginsUntil = TimeStamp::Now() + mVsyncRate;
  Unused << SendCaptureAllPlugins(parentWidget);
#  else
  Unused << SendHideAllPlugins(parentWidget);
  ScheduleComposition();
#  endif
}
#endif  // #if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)

mozilla::ipc::IPCResult CompositorBridgeParent::RecvAllPluginsCaptured() {
#if defined(XP_WIN)
  mWaitForPluginsUntil = TimeStamp();
  mHaveBlockedForPlugins = false;
  ForceComposeToTarget(nullptr);
  Unused << SendHideAllPlugins(GetWidget()->GetWidgetKey());
  return IPC_OK();
#else
  MOZ_ASSERT_UNREACHABLE(
      "CompositorBridgeParent::RecvAllPluginsCaptured calls unexpected.");
  return IPC_FAIL_NO_REASON(this);
#endif
}

int32_t RecordContentFrameTime(
    const VsyncId& aTxnId, const TimeStamp& aVsyncStart,
    const TimeStamp& aTxnStart, const VsyncId& aCompositeId,
    const TimeStamp& aCompositeEnd, const TimeDuration& aFullPaintTime,
    const TimeDuration& aVsyncRate, bool aContainsSVGGroup,
    bool aRecordUploadStats, wr::RendererStats* aStats /* = nullptr */) {
  double latencyMs = (aCompositeEnd - aTxnStart).ToMilliseconds();
  double latencyNorm = latencyMs / aVsyncRate.ToMilliseconds();
  int32_t fracLatencyNorm = lround(latencyNorm * 100.0);

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    class ContentFramePayload : public ProfilerMarkerPayload {
     public:
      ContentFramePayload(const mozilla::TimeStamp& aStartTime,
                          const mozilla::TimeStamp& aEndTime)
          : ProfilerMarkerPayload(aStartTime, aEndTime) {}
      virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                                 const TimeStamp& aProcessStartTime,
                                 UniqueStacks& aUniqueStacks) override {
        StreamCommonProps("CONTENT_FRAME_TIME", aWriter, aProcessStartTime,
                          aUniqueStacks);
      }
    };
    profiler_add_marker_for_thread(
        profiler_current_thread_id(), JS::ProfilingCategoryPair::GRAPHICS,
        "CONTENT_FRAME_TIME",
        MakeUnique<ContentFramePayload>(aTxnStart, aCompositeEnd));
  }
#endif

  Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME, fracLatencyNorm);

  if (!(aTxnId == VsyncId()) && aVsyncStart) {
    latencyMs = (aCompositeEnd - aVsyncStart).ToMilliseconds();
    latencyNorm = latencyMs / aVsyncRate.ToMilliseconds();
    fracLatencyNorm = lround(latencyNorm * 100.0);
    int32_t result = fracLatencyNorm;
    Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME_VSYNC, fracLatencyNorm);

    if (aContainsSVGGroup) {
      Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME_WITH_SVG,
                            fracLatencyNorm);
    }

    // Record CONTENT_FRAME_TIME_REASON.
    //
    // Note that deseralizing a layers update (RecvUpdate) can delay the receipt
    // of the composite vsync message
    // (CompositorBridgeParent::CompositeToTarget), since they're using the same
    // thread. This can mean that compositing might start significantly late,
    // but this code will still detect it as having successfully started on the
    // right vsync (which is somewhat correct). We'd now have reduced time left
    // in the vsync interval to finish compositing, so the chances of a missed
    // frame increases. This is effectively including the RecvUpdate work as
    // part of the 'compositing' phase for this metric, but it isn't included in
    // COMPOSITE_TIME, and *is* included in CONTENT_FULL_PAINT_TIME.
    //
    // Also of note is that when the root WebRenderBridgeParent decides to
    // skip a composite (due to the Renderer being busy), that won't notify
    // child WebRenderBridgeParents. That failure will show up as the
    // composite starting late (since it did), but it's really a fault of a
    // slow composite on the previous frame, not a slow
    // CONTENT_FULL_PAINT_TIME. It would be nice to have a separate bucket for
    // this category (scene was ready on the next vsync, but we chose not to
    // composite), but I can't find a way to locate the right child
    // WebRenderBridgeParents from the root. WebRender notifies us of the
    // child pipelines contained within a render, after it finishes, but I
    // can't see how to query what child pipeline would have been rendered,
    // when we choose to not do it.
    if (fracLatencyNorm < 200) {
      // Success
      Telemetry::AccumulateCategorical(
          LABELS_CONTENT_FRAME_TIME_REASON::OnTime);
    } else {
      if (aCompositeId == VsyncId()) {
        // aCompositeId is 0, possibly something got trigged from
        // outside vsync?
        Telemetry::AccumulateCategorical(
            LABELS_CONTENT_FRAME_TIME_REASON::NoVsyncNoId);
      } else if (aTxnId >= aCompositeId) {
        // Vsync ids are nonsensical, maybe we're trying to catch up?
        Telemetry::AccumulateCategorical(
            LABELS_CONTENT_FRAME_TIME_REASON::NoVsync);
      } else if (aCompositeId - aTxnId > 1) {
        // Composite started late (and maybe took too long as well)
        if (aFullPaintTime >= TimeDuration::FromMilliseconds(20)) {
          Telemetry::AccumulateCategorical(
              LABELS_CONTENT_FRAME_TIME_REASON::MissedCompositeLong);
        } else if (aFullPaintTime >= TimeDuration::FromMilliseconds(10)) {
          Telemetry::AccumulateCategorical(
              LABELS_CONTENT_FRAME_TIME_REASON::MissedCompositeMid);
        } else if (aFullPaintTime >= TimeDuration::FromMilliseconds(5)) {
          Telemetry::AccumulateCategorical(
              LABELS_CONTENT_FRAME_TIME_REASON::MissedCompositeLow);
        } else {
          Telemetry::AccumulateCategorical(
              LABELS_CONTENT_FRAME_TIME_REASON::MissedComposite);
        }
      } else {
        // Composite started on time, but must have taken too long.
        Telemetry::AccumulateCategorical(
            LABELS_CONTENT_FRAME_TIME_REASON::SlowComposite);
      }
    }

    if (aRecordUploadStats) {
      if (aStats) {
        latencyMs -= (double(aStats->resource_upload_time) / 1000000.0);
        latencyNorm = latencyMs / aVsyncRate.ToMilliseconds();
        fracLatencyNorm = lround(latencyNorm * 100.0);
      }
      Telemetry::Accumulate(
          Telemetry::CONTENT_FRAME_TIME_WITHOUT_RESOURCE_UPLOAD,
          fracLatencyNorm);

      if (aStats) {
        latencyMs -= (double(aStats->gpu_cache_upload_time) / 1000000.0);
        latencyNorm = latencyMs / aVsyncRate.ToMilliseconds();
        fracLatencyNorm = lround(latencyNorm * 100.0);
      }
      Telemetry::Accumulate(Telemetry::CONTENT_FRAME_TIME_WITHOUT_UPLOAD,
                            fracLatencyNorm);
    }
    return result;
  }

  return 0;
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvBeginRecording(
    const TimeStamp& aRecordingStart, BeginRecordingResolver&& aResolve) {
  if (mHaveCompositionRecorder) {
    aResolve(false);
    return IPC_OK();
  }

  if (mLayerManager) {
    mLayerManager->SetCompositionRecorder(
        MakeUnique<CompositionRecorder>(aRecordingStart));
  } else if (mWrBridge) {
    mWrBridge->SetCompositionRecorder(MakeUnique<WebRenderCompositionRecorder>(
        aRecordingStart, mWrBridge->PipelineId()));
  }

  mHaveCompositionRecorder = true;
  aResolve(true);

  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorBridgeParent::RecvEndRecording(
    bool* aOutSuccess) {
  if (!mHaveCompositionRecorder) {
    *aOutSuccess = false;
    return IPC_OK();
  }

  if (mLayerManager) {
    mLayerManager->WriteCollectedFrames();
  } else if (mWrBridge) {
    mWrBridge->WriteCollectedFrames();
  }

  mHaveCompositionRecorder = false;
  *aOutSuccess = true;

  return IPC_OK();
}

}  // namespace layers
}  // namespace mozilla

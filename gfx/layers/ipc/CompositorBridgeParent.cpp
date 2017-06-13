/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorBridgeParent.h"
#include <stdio.h>                      // for fprintf, stdout
#include <stdint.h>                     // for uint64_t
#include <map>                          // for _Rb_tree_iterator, etc
#include <utility>                      // for pair
#include "LayerTransactionParent.h"     // for LayerTransactionParent
#include "RenderTrace.h"                // for RenderTraceLayers
#include "base/message_loop.h"          // for MessageLoop
#include "base/process.h"               // for ProcessId
#include "base/task.h"                  // for CancelableTask, etc
#include "base/thread.h"                // for Thread
#include "gfxContext.h"                 // for gfxContext
#include "gfxPlatform.h"                // for gfxPlatform
#include "TreeTraversal.h"              // for ForEachNode
#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"             // for gfxPlatform
#endif
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/AutoRestore.h"        // for AutoRestore
#include "mozilla/ClearOnShutdown.h"    // for ClearOnShutdown
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/gfx/2D.h"          // for DrawTarget
#include "mozilla/gfx/GPUChild.h"       // for GfxPrefValue
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"          // for IntSize
#include "mozilla/gfx/gfxVars.h"        // for gfxVars
#include "VRManager.h"                  // for VRManager
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/AnimationHelper.h" // for CompositorAnimationStorage
#include "mozilla/layers/APZCTreeManager.h"  // for APZCTreeManager
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZThreadUtils.h"  // for APZCTreeManager
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/BasicCompositor.h"  // for BasicCompositor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorManagerParent.h" // for CompositorManagerParent
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/CrossProcessCompositorBridgeParent.h"
#include "mozilla/layers/FrameUniformityData.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/PLayerTransactionParent.h"
#include "mozilla/layers/RemoteContentController.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderCompositableHolder.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/media/MediaSystemResourceService.h" // for MediaSystemResourceService
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/Telemetry.h"
#ifdef MOZ_WIDGET_GTK
#include "basic/X11BasicCompositor.h" // for X11BasicCompositor
#endif
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsTArray.h"                   // for nsTArray
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#ifdef XP_WIN
#include "mozilla/layers/CompositorD3D11.h"
#endif
#include "GeckoProfiler.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/Unused.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_GECKO_PROFILER
#include "ProfilerMarkerPayload.h"
#endif
#include "mozilla/VsyncDispatcher.h"
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
#include "VsyncSource.h"
#endif
#include "mozilla/widget/CompositorWidget.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
# include "mozilla/widget/CompositorWidgetParent.h"
#endif

#include "LayerScope.h"

namespace mozilla {

namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace std;

using base::ProcessId;
using base::Thread;

CompositorBridgeParentBase::CompositorBridgeParentBase(CompositorManagerParent* aManager)
  : mCanSend(true)
  , mCompositorManager(aManager)
{
}

CompositorBridgeParentBase::~CompositorBridgeParentBase()
{
}

ProcessId
CompositorBridgeParentBase::GetChildProcessId()
{
  return OtherPid();
}

void
CompositorBridgeParentBase::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  if (!(texture->GetFlags() & TextureFlags::RECYCLE)) {
    return;
  }

  uint64_t textureId = TextureHost::GetTextureSerial(aTexture);
  mPendingAsyncMessage.push_back(
    OpNotifyNotUsed(textureId, aTransactionId));
}

void
CompositorBridgeParentBase::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  Unused << SendParentAsyncMessages(aMessage);
}

bool
CompositorBridgeParentBase::AllocShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem)
{
  return PCompositorBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool
CompositorBridgeParentBase::AllocUnsafeShmem(size_t aSize,
                                         ipc::SharedMemory::SharedMemoryType aType,
                                         ipc::Shmem* aShmem)
{
  return PCompositorBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

void
CompositorBridgeParentBase::DeallocShmem(ipc::Shmem& aShmem)
{
  PCompositorBridgeParent::DeallocShmem(aShmem);
}

base::ProcessId
CompositorBridgeParentBase::RemotePid()
{
  return OtherPid();
}

bool
CompositorBridgeParentBase::StartSharingMetrics(ipc::SharedMemoryBasic::Handle aHandle,
                                                CrossProcessMutexHandle aMutexHandle,
                                                uint64_t aLayersId,
                                                uint32_t aApzcId)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeParent::SendSharedCompositorFrameMetrics(
    aHandle, aMutexHandle, aLayersId, aApzcId);
}

bool
CompositorBridgeParentBase::StopSharingMetrics(FrameMetrics::ViewID aScrollId,
                                               uint32_t aApzcId)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeParent::SendReleaseSharedCompositorFrameMetrics(
    aScrollId, aApzcId);
}

CompositorBridgeParent::LayerTreeState::LayerTreeState()
  : mApzcTreeManagerParent(nullptr)
  , mParent(nullptr)
  , mLayerManager(nullptr)
  , mCrossProcessParent(nullptr)
  , mLayerTree(nullptr)
  , mUpdatedPluginDataAvailable(false)
{
}

CompositorBridgeParent::LayerTreeState::~LayerTreeState()
{
  if (mController) {
    mController->Destroy();
  }
}

typedef map<uint64_t, CompositorBridgeParent::LayerTreeState> LayerTreeMap;
LayerTreeMap sIndirectLayerTrees;
StaticAutoPtr<mozilla::Monitor> sIndirectLayerTreesLock;

static void EnsureLayerTreeMapReady()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sIndirectLayerTreesLock) {
    sIndirectLayerTreesLock = new Monitor("IndirectLayerTree");
    mozilla::ClearOnShutdown(&sIndirectLayerTreesLock);
  }
}

template <typename Lambda>
inline void
CompositorBridgeParent::ForEachIndirectLayerTree(const Lambda& aCallback)
{
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  for (auto it = sIndirectLayerTrees.begin(); it != sIndirectLayerTrees.end(); it++) {
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
typedef map<uint64_t,CompositorBridgeParent*> CompositorMap;
static StaticAutoPtr<CompositorMap> sCompositorMap;

void
CompositorBridgeParent::Setup()
{
  EnsureLayerTreeMapReady();

  MOZ_ASSERT(!sCompositorMap);
  sCompositorMap = new CompositorMap;

  gfxPrefs::SetWebRenderProfilerEnabledChangeCallback(
    [](const GfxPrefValue& aValue) -> void {
      CompositorBridgeParent::SetWebRenderProfilerEnabled(aValue.get_bool());
  });
}

void
CompositorBridgeParent::Shutdown()
{
  MOZ_ASSERT(sCompositorMap);
  MOZ_ASSERT(sCompositorMap->empty());
  sCompositorMap = nullptr;
  gfxPrefs::SetWebRenderProfilerEnabledChangeCallback(nullptr);
}

void
CompositorBridgeParent::FinishShutdown()
{
  // TODO: this should be empty by now...
  sIndirectLayerTrees.clear();
}

static void SetThreadPriority()
{
  hal::SetCurrentThreadPriority(hal::THREAD_PRIORITY_COMPOSITOR);
}

#ifdef COMPOSITOR_PERFORMANCE_WARNING
static int32_t
CalculateCompositionFrameRate()
{
  // Used when layout.frame_rate is -1. Needs to be kept in sync with
  // DEFAULT_FRAME_RATE in nsRefreshDriver.cpp.
  // TODO: This should actually return the vsync rate.
  const int32_t defaultFrameRate = 60;
  int32_t compositionFrameRatePref = gfxPrefs::LayersCompositionFrameRate();
  if (compositionFrameRatePref < 0) {
    // Use the same frame rate for composition as for layout.
    int32_t layoutFrameRatePref = gfxPrefs::LayoutFrameRate();
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

static inline MessageLoop*
CompositorLoop()
{
  return CompositorThreadHolder::Loop();
}

CompositorBridgeParent::CompositorBridgeParent(CompositorManagerParent* aManager,
                                               CSSToLayoutDeviceScale aScale,
                                               const TimeDuration& aVsyncRate,
                                               const CompositorOptions& aOptions,
                                               bool aUseExternalSurfaceSize,
                                               const gfx::IntSize& aSurfaceSize)
  : CompositorBridgeParentBase(aManager)
  , mWidget(nullptr)
  , mScale(aScale)
  , mVsyncRate(aVsyncRate)
  , mIsTesting(false)
  , mPendingTransaction(0)
  , mPaused(false)
  , mUseExternalSurfaceSize(aUseExternalSurfaceSize)
  , mEGLSurfaceSize(aSurfaceSize)
  , mOptions(aOptions)
  , mPauseCompositionMonitor("PauseCompositionMonitor")
  , mResumeCompositionMonitor("ResumeCompositionMonitor")
  , mRootLayerTreeID(0)
  , mOverrideComposeReadiness(false)
  , mForceCompositionTask(nullptr)
  , mCompositorThreadHolder(CompositorThreadHolder::GetSingleton())
  , mCompositorScheduler(nullptr)
  , mAnimationStorage(nullptr)
  , mPaintTime(TimeDuration::Forever())
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  , mLastPluginUpdateLayerTreeId(0)
  , mDeferPluginWindows(false)
  , mPluginWindowsHidden(false)
#endif
{
}

void
CompositorBridgeParent::InitSameProcess(widget::CompositorWidget* aWidget,
                                        const uint64_t& aLayerTreeId)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mWidget = aWidget;
  mRootLayerTreeID = aLayerTreeId;
  if (mOptions.UseAPZ()) {
    mApzcTreeManager = new APZCTreeManager();
  }

  Initialize();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvInitialize(const uint64_t& aRootLayerTreeId)
{
  mRootLayerTreeID = aRootLayerTreeId;

  Initialize();
  return IPC_OK();
}

void
CompositorBridgeParent::Initialize()
{
  MOZ_ASSERT(CompositorThread(),
             "The compositor thread must be Initialized before instanciating a CompositorBridgeParent.");

  mCompositorID = 0;
  // FIXME: This holds on the the fact that right now the only thing that
  // can destroy this instance is initialized on the compositor thread after
  // this task has been processed.
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableFunction(&AddCompositor,
                                                 this, &mCompositorID));

  CompositorLoop()->PostTask(NewRunnableFunction(SetThreadPriority));


  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees[mRootLayerTreeID].mParent = this;
  }

  LayerScope::SetPixelScale(mScale.scale);

  if (!mOptions.UseWebRender()) {
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

uint64_t
CompositorBridgeParent::RootLayerTreeId()
{
  MOZ_ASSERT(mRootLayerTreeID);
  return mRootLayerTreeID;
}

CompositorBridgeParent::~CompositorBridgeParent()
{
  InfallibleTArray<PTextureParent*> textures;
  ManagedPTextureParent(textures);
  // We expect all textures to be destroyed by now.
  MOZ_DIAGNOSTIC_ASSERT(textures.Length() == 0);
  for (unsigned int i = 0; i < textures.Length(); ++i) {
    RefPtr<TextureHost> tex = TextureHost::AsTextureHost(textures[i]);
    tex->DeallocateDeviceData();
  }
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvForceIsFirstPaint()
{
  mCompositionManager->ForceIsFirstPaint();
  return IPC_OK();
}

void
CompositorBridgeParent::StopAndClearResources()
{
  if (mForceCompositionTask) {
    mForceCompositionTask->Cancel();
    mForceCompositionTask = nullptr;
  }

  mPaused = true;

  // Ensure that the layer manager is destroyed before CompositorBridgeChild.
  if (mLayerManager) {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    ForEachIndirectLayerTree([this] (LayerTreeState* lts, uint64_t) -> void {
      mLayerManager->ClearCachedResources(lts->mRoot);
      lts->mLayerManager = nullptr;
      lts->mParent = nullptr;
    });
    mLayerManager->Destroy();
    mLayerManager = nullptr;
    mCompositionManager = nullptr;
  }

  if (mWrBridge) {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    ForEachIndirectLayerTree([] (LayerTreeState* lts, uint64_t) -> void {
      if (lts->mWrBridge) {
        lts->mWrBridge->Destroy();
        lts->mWrBridge = nullptr;
      }
      lts->mParent = nullptr;
    });
    mWrBridge->Destroy();
    mWrBridge = nullptr;
  }

  if (mCompositor) {
    mCompositor->DetachWidget();
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

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvWillClose()
{
  StopAndClearResources();
  return IPC_OK();
}

void CompositorBridgeParent::DeferredDestroy()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mCompositorThreadHolder);
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvPause()
{
  PauseComposition();
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvResume()
{
  ResumeComposition();
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                         const gfx::IntRect& aRect)
{
  RefPtr<DrawTarget> target = GetDrawTargetForDescriptor(aInSnapshot, gfx::BackendType::CAIRO);
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
CompositorBridgeParent::RecvWaitOnTransactionProcessed()
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvFlushRendering()
{
  if (gfxVars::UseWebRender()) {
    mWrBridge->FlushRendering(/* aSync */ true);
    return IPC_OK();
  }

  if (mCompositorScheduler->NeedsComposite()) {
    CancelCurrentCompositeTask();
    ForceComposeToTarget(nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvFlushRenderingAsync()
{
  if (gfxVars::UseWebRender()) {
    mWrBridge->FlushRendering(/* aSync */ false);
    return IPC_OK();
  }

  return RecvFlushRendering();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvForcePresent()
{
  // During the shutdown sequence mLayerManager may be null
  if (mLayerManager) {
    mLayerManager->ForcePresent();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvNotifyRegionInvalidated(const nsIntRegion& aRegion)
{
  if (mLayerManager) {
    mLayerManager->AddInvalidRegion(aRegion);
  }
  return IPC_OK();
}

void
CompositorBridgeParent::Invalidate()
{
  if (mLayerManager && mLayerManager->GetRoot()) {
    mLayerManager->AddInvalidRegion(
                                    mLayerManager->GetRoot()->GetLocalVisibleRegion().ToUnknownRegion().GetBounds());
  }
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex)
{
  if (mLayerManager) {
    *aOutStartIndex = mLayerManager->StartFrameTimeRecording(aBufferSize);
  } else {
    *aOutStartIndex = 0;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvStopFrameTimeRecording(const uint32_t& aStartIndex,
                                                   InfallibleTArray<float>* intervals)
{
  if (mLayerManager) {
    mLayerManager->StopFrameTimeRecording(aStartIndex, *intervals);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                                             const uint32_t& aPresShellId)
{
  ClearApproximatelyVisibleRegions(aLayersId, Some(aPresShellId));
  return IPC_OK();
}

void
CompositorBridgeParent::ClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                                         const Maybe<uint32_t>& aPresShellId)
{
  if (mLayerManager) {
    mLayerManager->ClearApproximatelyVisibleRegions(aLayersId, aPresShellId);

    // We need to recomposite to update the minimap.
    ScheduleComposition();
  }
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvNotifyApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                             const CSSIntRegion& aRegion)
{
  if (mLayerManager) {
    mLayerManager->UpdateApproximatelyVisibleRegion(aGuid, aRegion);

    // We need to recomposite to update the minimap.
    ScheduleComposition();
  }
  return IPC_OK();
}

void
CompositorBridgeParent::ActorDestroy(ActorDestroyReason why)
{
  mCanSend = false;

  StopAndClearResources();

  RemoveCompositor(mCompositorID);

  mCompositionManager = nullptr;

  if (mApzcTreeManager) {
    mApzcTreeManager->ClearTree();
    mApzcTreeManager = nullptr;
  }

  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees.erase(mRootLayerTreeID);
  }

  // There are chances that the ref count reaches zero on the main thread shortly
  // after this function returns while some ipdl code still needs to run on
  // this thread.
  // We must keep the compositor parent alive untill the code handling message
  // reception is finished on this thread.
  mSelfRef = this;
  MessageLoop::current()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::DeferredDestroy));
}

void
CompositorBridgeParent::ScheduleRenderOnCompositorThread()
{
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::ScheduleComposition));
}

void
CompositorBridgeParent::InvalidateOnCompositorThread()
{
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::Invalidate));
}

void
CompositorBridgeParent::PauseComposition()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread(),
             "PauseComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mPauseCompositionMonitor);

  if (!mPaused) {
    mPaused = true;

    if (!gfxVars::UseWebRender()) {
      mCompositor->Pause();
    } else {
      mWrBridge->Pause();
    }
    TimeStamp now = TimeStamp::Now();
    DidComposite(now, now);
  }

  // if anyone's waiting to make sure that composition really got paused, tell them
  lock.NotifyAll();
}

void
CompositorBridgeParent::ResumeComposition()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread(),
             "ResumeComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mResumeCompositionMonitor);

  bool resumed = gfxVars::UseWebRender() ? mWrBridge->Resume() : mCompositor->Resume();
  if (!resumed) {
#ifdef MOZ_WIDGET_ANDROID
    // We can't get a surface. This could be because the activity changed between
    // the time resume was scheduled and now.
    __android_log_print(ANDROID_LOG_INFO, "CompositorBridgeParent", "Unable to renew compositor surface; remaining in paused state");
#endif
    lock.NotifyAll();
    return;
  }

  mPaused = false;

  Invalidate();
  mCompositorScheduler->ResumeComposition();

  // if anyone's waiting to make sure that composition really got resumed, tell them
  lock.NotifyAll();
}

void
CompositorBridgeParent::ForceComposition()
{
  // Cancel the orientation changed state to force composition
  mForceCompositionTask = nullptr;
  ScheduleRenderOnCompositorThread();
}

void
CompositorBridgeParent::CancelCurrentCompositeTask()
{
  mCompositorScheduler->CancelCurrentCompositeTask();
}

void
CompositorBridgeParent::SetEGLSurfaceSize(int width, int height)
{
  NS_ASSERTION(mUseExternalSurfaceSize, "Compositor created without UseExternalSurfaceSize provided");
  mEGLSurfaceSize.SizeTo(width, height);
  if (mCompositor) {
    mCompositor->SetDestinationSurfaceSize(gfx::IntSize(mEGLSurfaceSize.width, mEGLSurfaceSize.height));
  }
}

void
CompositorBridgeParent::ResumeCompositionAndResize(int width, int height)
{
  SetEGLSurfaceSize(width, height);
  ResumeComposition();
}

/*
 * This will execute a pause synchronously, waiting to make sure that the compositor
 * really is paused.
 */
void
CompositorBridgeParent::SchedulePauseOnCompositorThread()
{
  MonitorAutoLock lock(mPauseCompositionMonitor);

  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::PauseComposition));

  // Wait until the pause has actually been processed by the compositor thread
  lock.Wait();
}

bool
CompositorBridgeParent::ScheduleResumeOnCompositorThread()
{
  MonitorAutoLock lock(mResumeCompositionMonitor);

  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::ResumeComposition));

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();

  return !mPaused;
}

bool
CompositorBridgeParent::ScheduleResumeOnCompositorThread(int width, int height)
{
  MonitorAutoLock lock(mResumeCompositionMonitor);

  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod
                             <int, int>(this,
                                        &CompositorBridgeParent::ResumeCompositionAndResize,
                                        width, height));

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();

  return !mPaused;
}

void
CompositorBridgeParent::ScheduleTask(already_AddRefed<CancelableRunnable> task, int time)
{
  if (time == 0) {
    MessageLoop::current()->PostTask(Move(task));
  } else {
    MessageLoop::current()->PostDelayedTask(Move(task), time);
  }
}

void
CompositorBridgeParent::UpdatePaintTime(LayerTransactionParent* aLayerTree,
                                        const TimeDuration& aPaintTime)
{
  // We get a lot of paint timings for things with empty transactions.
  if (!mLayerManager || aPaintTime.ToMilliseconds() < 1.0) {
    return;
  }

  mLayerManager->SetPaintTime(aPaintTime);
}

void
CompositorBridgeParent::NotifyShadowTreeTransaction(uint64_t aId, bool aIsFirstPaint,
    bool aScheduleComposite, uint32_t aPaintSequenceNumber,
    bool aIsRepeatTransaction, bool aHitTestUpdate)
{
  if (!aIsRepeatTransaction &&
      mLayerManager &&
      mLayerManager->GetRoot()) {
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

    if (mApzcTreeManager && aHitTestUpdate) {
      mApzcTreeManager->UpdateHitTestingTree(mRootLayerTreeID,
          mLayerManager->GetRoot(), aIsFirstPaint, aId, aPaintSequenceNumber);
    }

    mLayerManager->NotifyShadowTreeTransaction();
  }
  if (aScheduleComposite) {
    ScheduleComposition();
  }
}

void
CompositorBridgeParent::ScheduleComposition()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (mPaused) {
    return;
  }

  mCompositorScheduler->ScheduleComposition();
}

// Go down the composite layer tree, setting properties to match their
// content-side counterparts.
/* static */ void
CompositorBridgeParent::SetShadowProperties(Layer* aLayer)
{
  ForEachNode<ForwardIterator>(
      aLayer,
      [] (Layer *layer)
      {
        if (Layer* maskLayer = layer->GetMaskLayer()) {
          SetShadowProperties(maskLayer);
        }
        for (size_t i = 0; i < layer->GetAncestorMaskLayerCount(); i++) {
          SetShadowProperties(layer->GetAncestorMaskLayerAt(i));
        }

        // FIXME: Bug 717688 -- Do these updates in LayerTransactionParent::RecvUpdate.
        HostLayer* layerCompositor = layer->AsHostLayer();
        // Set the layerComposite's base transform to the layer's base transform.
        layerCompositor->SetShadowBaseTransform(layer->GetBaseTransform());
        layerCompositor->SetShadowTransformSetByAnimation(false);
        layerCompositor->SetShadowVisibleRegion(layer->GetVisibleRegion());
        layerCompositor->SetShadowClipRect(layer->GetClipRect());
        layerCompositor->SetShadowOpacity(layer->GetOpacity());
        layerCompositor->SetShadowOpacitySetByAnimation(false);
      }
    );
}

void
CompositorBridgeParent::CompositeToTarget(DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  AutoProfilerTracing tracing("Paint", "Composite");
  PROFILER_LABEL("CompositorBridgeParent", "Composite",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread(),
             "Composite can only be called on the compositor thread");
  TimeStamp start = TimeStamp::Now();

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeDuration scheduleDelta = TimeStamp::Now() - mCompositorScheduler->GetExpectedComposeStartTime();
  if (scheduleDelta > TimeDuration::FromMilliseconds(2) ||
      scheduleDelta < TimeDuration::FromMilliseconds(-2)) {
    printf_stderr("Compositor: Compose starting off schedule by %4.1f ms\n",
                  scheduleDelta.ToMilliseconds());
  }
#endif

  if (!CanComposite()) {
    TimeStamp end = TimeStamp::Now();
    DidComposite(start, end);
    return;
  }

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  if (!mWaitForPluginsUntil.IsNull() &&
      mWaitForPluginsUntil > start) {
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
  AutoResolveRefLayers resolve(mCompositionManager, this,
                               &hasRemoteContent,
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

  if (aTarget) {
    mLayerManager->BeginTransactionWithDrawTarget(aTarget, *aRect);
  } else {
    mLayerManager->BeginTransaction();
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

  TimeStamp time = mIsTesting ? mTestTime : mCompositorScheduler->GetLastComposeTime();
  bool requestNextFrame = mCompositionManager->TransformShadowTree(time, mVsyncRate);
  if (requestNextFrame) {
    ScheduleComposition();
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    // If we have visible windowed plugins then we need to wait for content (and
    // then the plugins) to have been updated by the active animation.
    if (!mPluginWindowsHidden && mCachedPluginData.Length()) {
      mWaitForPluginsUntil = mCompositorScheduler->GetLastComposeTime() + (mVsyncRate * 2);
    }
#endif
  }

  RenderTraceLayers(mLayerManager->GetRoot(), "0000");

#ifdef MOZ_DUMP_PAINTING
  if (gfxPrefs::DumpHostLayers()) {
    printf_stderr("Painting --- compositing layer tree:\n");
    mLayerManager->Dump(/* aSorted = */ true);
  }
#endif
  mLayerManager->SetDebugOverlayWantsNextFrame(false);
  mLayerManager->EndTransaction(time);

  if (!aTarget) {
    TimeStamp end = TimeStamp::Now();
    DidComposite(start, end);
  }

  // We're not really taking advantage of the stored composite-again-time here.
  // We might be able to skip the next few composites altogether. However,
  // that's a bit complex to implement and we'll get most of the advantage
  // by skipping compositing when we detect there's nothing invalid. This is why
  // we do "composite until" rather than "composite again at".
  //
  // TODO(bug 1328602) Figure out what we should do here with the render thread.
  if (!mLayerManager->GetCompositeUntilTime().IsNull() ||
      mLayerManager->DebugOverlayWantsNextFrame())
  {
      ScheduleComposition();
  }

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeDuration executionTime = TimeStamp::Now() - mCompositorScheduler->GetLastComposeTime();
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
  if (gfxPrefs::LayersCompositionFrameRate() == 0 ||
      mLayerManager->AlwaysScheduleComposite())
  {
    // Special full-tilt composite mode for performance testing
    ScheduleComposition();
  }

  // TODO(bug 1328602) Need an equivalent that works with the rende thread.
  mLayerManager->SetCompositionTime(TimeStamp());

  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::COMPOSITE_TIME, start);
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvRemotePluginsReady()
{
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
  NS_NOTREACHED("CompositorBridgeParent::RecvRemotePluginsReady calls "
                "unexpected on this platform.");
  return IPC_FAIL_NO_REASON(this);
#endif
}

void
CompositorBridgeParent::ForceComposeToTarget(DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  PROFILER_LABEL("CompositorBridgeParent", "ForceComposeToTarget",
    js::ProfileEntry::Category::GRAPHICS);

  AutoRestore<bool> override(mOverrideComposeReadiness);
  mOverrideComposeReadiness = true;
  mCompositorScheduler->ForceComposeToTarget(aTarget, aRect);
}

PAPZCTreeManagerParent*
CompositorBridgeParent::AllocPAPZCTreeManagerParent(const uint64_t& aLayersId)
{
  // We should only ever get this if APZ is enabled in this compositor.
  MOZ_ASSERT(mOptions.UseAPZ());

  // The main process should pass in 0 because we assume mRootLayerTreeID
  MOZ_ASSERT(aLayersId == 0);

  // This message doubles as initialization
  MOZ_ASSERT(!mApzcTreeManager);
  mApzcTreeManager = new APZCTreeManager();

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state = sIndirectLayerTrees[mRootLayerTreeID];
  MOZ_ASSERT(state.mParent);
  MOZ_ASSERT(!state.mApzcTreeManagerParent);
  state.mApzcTreeManagerParent = new APZCTreeManagerParent(mRootLayerTreeID, state.mParent->GetAPZCTreeManager());

  return state.mApzcTreeManagerParent;
}

bool
CompositorBridgeParent::DeallocPAPZCTreeManagerParent(PAPZCTreeManagerParent* aActor)
{
  delete aActor;
  return true;
}

PAPZParent*
CompositorBridgeParent::AllocPAPZParent(const uint64_t& aLayersId)
{
  // The main process should pass in 0 because we assume mRootLayerTreeID
  MOZ_ASSERT(aLayersId == 0);

  RemoteContentController* controller = new RemoteContentController();

  // Increment the controller's refcount before we return it. This will keep the
  // controller alive until it is released by IPDL in DeallocPAPZParent.
  controller->AddRef();

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state = sIndirectLayerTrees[mRootLayerTreeID];
  MOZ_ASSERT(!state.mController);
  state.mController = controller;

  return controller;
}

bool
CompositorBridgeParent::DeallocPAPZParent(PAPZParent* aActor)
{
  RemoteContentController* controller = static_cast<RemoteContentController*>(aActor);
  controller->Release();
  return true;
}

RefPtr<APZCTreeManager>
CompositorBridgeParent::GetAPZCTreeManager()
{
  return mApzcTreeManager;
}

CompositorBridgeParent*
CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(const uint64_t& aLayersId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  return sIndirectLayerTrees[aLayersId].mParent;
}

bool
CompositorBridgeParent::CanComposite()
{
  return mLayerManager &&
         mLayerManager->GetRoot() &&
         !mPaused;
}

void
CompositorBridgeParent::ScheduleRotationOnCompositorThread(const TargetConfig& aTargetConfig,
                                                     bool aIsFirstPaint)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (!aIsFirstPaint &&
      !mCompositionManager->IsFirstPaint() &&
      mCompositionManager->RequiresReorientation(aTargetConfig.orientation())) {
    if (mForceCompositionTask != nullptr) {
      mForceCompositionTask->Cancel();
    }
    RefPtr<CancelableRunnable> task =
      NewCancelableRunnableMethod(this, &CompositorBridgeParent::ForceComposition);
    mForceCompositionTask = task;
    ScheduleTask(task.forget(), gfxPrefs::OrientationSyncMillis());
  }
}

void
CompositorBridgeParent::ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                            const TransactionInfo& aInfo,
                                            bool aHitTestUpdate)
{
  const TargetConfig& targetConfig = aInfo.targetConfig();

  ScheduleRotationOnCompositorThread(targetConfig, aInfo.isFirstPaint());

  // Instruct the LayerManager to update its render bounds now. Since all the orientation
  // change, dimension change would be done at the stage, update the size here is free of
  // race condition.
  mLayerManager->UpdateRenderBounds(targetConfig.naturalBounds());
  mLayerManager->SetRegionToClear(targetConfig.clearRegion());
  if (mLayerManager->GetCompositor()) {
    mLayerManager->GetCompositor()->SetScreenRotation(targetConfig.rotation());
  }

  mCompositionManager->Updated(aInfo.isFirstPaint(), targetConfig);
  Layer* root = aLayerTree->GetRoot();
  mLayerManager->SetRoot(root);

  if (mApzcTreeManager && !aInfo.isRepeatTransaction() && aHitTestUpdate) {
    AutoResolveRefLayers resolve(mCompositionManager);

    mApzcTreeManager->UpdateHitTestingTree(
      mRootLayerTreeID, root, aInfo.isFirstPaint(),
      mRootLayerTreeID, aInfo.paintSequenceNumber());
  }

  // The transaction ID might get reset to 1 if the page gets reloaded, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1145295#c41
  // Otherwise, it should be continually increasing.
  MOZ_ASSERT(aInfo.id() == 1 || aInfo.id() > mPendingTransaction);
  mPendingTransaction = aInfo.id();

  if (root) {
    SetShadowProperties(root);
  }
  if (aInfo.scheduleComposite()) {
    ScheduleComposition();
    if (mPaused) {
      TimeStamp now = TimeStamp::Now();
      DidComposite(now, now);
    }
  }
  mLayerManager->NotifyShadowTreeTransaction();
}

void
CompositorBridgeParent::ForceComposite(LayerTransactionParent* aLayerTree)
{
  ScheduleComposition();
}

bool
CompositorBridgeParent::SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                          const TimeStamp& aTime)
{
  if (aTime.IsNull()) {
    return false;
  }

  mIsTesting = true;
  mTestTime = aTime;

  bool testComposite = mCompositionManager &&
                       mCompositorScheduler->NeedsComposite();

  // Update but only if we were already scheduled to animate
  if (testComposite) {
    AutoResolveRefLayers resolve(mCompositionManager);
    bool requestNextFrame = mCompositionManager->TransformShadowTree(aTime, mVsyncRate);
    if (!requestNextFrame) {
      CancelCurrentCompositeTask();
      // Pretend we composited in case someone is wating for this event.
      TimeStamp now = TimeStamp::Now();
      DidComposite(now, now);
    }
  }

  return true;
}

void
CompositorBridgeParent::LeaveTestMode(LayerTransactionParent* aLayerTree)
{
  mIsTesting = false;
}

void
CompositorBridgeParent::ApplyAsyncProperties(LayerTransactionParent* aLayerTree)
{
  // NOTE: This should only be used for testing. For example, when mIsTesting is
  // true or when called from test-only methods like
  // LayerTransactionParent::RecvGetAnimationTransform.

  // Synchronously update the layer tree
  if (aLayerTree->GetRoot()) {
    AutoResolveRefLayers resolve(mCompositionManager);
    SetShadowProperties(mLayerManager->GetRoot());

    TimeStamp time = mIsTesting ? mTestTime : mCompositorScheduler->GetLastComposeTime();
    bool requestNextFrame =
      mCompositionManager->TransformShadowTree(time, mVsyncRate,
        AsyncCompositionManager::TransformsToSkip::APZ);
    if (!requestNextFrame) {
      CancelCurrentCompositeTask();
      // Pretend we composited in case someone is waiting for this event.
      TimeStamp now = TimeStamp::Now();
      DidComposite(now, now);
    }
  }
}

CompositorAnimationStorage*
CompositorBridgeParent::GetAnimationStorage(const uint64_t& aId)
{
  MOZ_ASSERT(aId == 0);

  if (!mAnimationStorage) {
    mAnimationStorage = new CompositorAnimationStorage();
  }
  return mAnimationStorage;
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvGetFrameUniformity(FrameUniformityData* aOutData)
{
  mCompositionManager->GetFrameUniformity(aOutData);
  return IPC_OK();
}

void
CompositorBridgeParent::FlushApzRepaints(const uint64_t& aLayersId)
{
  MOZ_ASSERT(mApzcTreeManager);
  uint64_t layersId = aLayersId;
  if (layersId == 0) {
    // The request is coming from the parent-process layer tree, so we should
    // use the compositor's root layer tree id.
    layersId = mRootLayerTreeID;
  }
  RefPtr<CompositorBridgeParent> self = this;
  APZThreadUtils::RunOnControllerThread(NS_NewRunnableFunction([=] () {
    self->mApzcTreeManager->FlushApzRepaints(layersId);
  }));
}

void
CompositorBridgeParent::GetAPZTestData(const uint64_t& aLayersId,
                                       APZTestData* aOutData)
{
  MOZ_ASSERT(aLayersId == 0 || aLayersId == mRootLayerTreeID);
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  *aOutData = sIndirectLayerTrees[mRootLayerTreeID].mApzTestData;
}

void
CompositorBridgeParent::SetConfirmedTargetAPZC(const uint64_t& aLayersId,
                                               const uint64_t& aInputBlockId,
                                               const nsTArray<ScrollableLayerGuid>& aTargets)
{
  if (!mApzcTreeManager) {
    return;
  }
  // Need to specifically bind this since it's overloaded.
  void (APZCTreeManager::*setTargetApzcFunc)
        (uint64_t, const nsTArray<ScrollableLayerGuid>&) =
        &APZCTreeManager::SetTargetAPZC;
  RefPtr<Runnable> task = NewRunnableMethod
        <uint64_t, StoreCopyPassByConstLRef<nsTArray<ScrollableLayerGuid>>>
        (mApzcTreeManager.get(), setTargetApzcFunc, aInputBlockId, aTargets);
  APZThreadUtils::RunOnControllerThread(task.forget());
}

void
CompositorBridgeParent::InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints)
{
  NS_ASSERTION(!mLayerManager, "Already initialised mLayerManager");
  NS_ASSERTION(!mCompositor,   "Already initialised mCompositor");

  mCompositor = NewCompositor(aBackendHints);
  if (!mCompositor) {
    return;
  }

  mLayerManager = new LayerManagerComposite(mCompositor);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[mRootLayerTreeID].mLayerManager = mLayerManager;
}

RefPtr<Compositor>
CompositorBridgeParent::NewCompositor(const nsTArray<LayersBackend>& aBackendHints)
{
  for (size_t i = 0; i < aBackendHints.Length(); ++i) {
    RefPtr<Compositor> compositor;
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
      compositor = new CompositorOGL(this,
                                     mWidget,
                                     mEGLSurfaceSize.width,
                                     mEGLSurfaceSize.height,
                                     mUseExternalSurfaceSize);
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
    if (compositor && compositor->Initialize(&failureReason)) {
      if (failureReason.IsEmpty()){
        failureReason = "SUCCESS";
      }

      // should only report success here
      if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL){
        Telemetry::Accumulate(Telemetry::OPENGL_COMPOSITING_FAILURE_ID, failureReason);
      }
#ifdef XP_WIN
      else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11){
        Telemetry::Accumulate(Telemetry::D3D11_COMPOSITING_FAILURE_ID, failureReason);
      }
#endif

      compositor->SetCompositorID(mCompositorID);
      return compositor;
    }

    // report any failure reasons here
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL){
      gfxCriticalNote << "[OPENGL] Failed to init compositor with reason: "
                      << failureReason.get();
      Telemetry::Accumulate(Telemetry::OPENGL_COMPOSITING_FAILURE_ID, failureReason);
    }
#ifdef XP_WIN
    else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11){
      gfxCriticalNote << "[D3D11] Failed to init compositor with reason: "
                      << failureReason.get();
      Telemetry::Accumulate(Telemetry::D3D11_COMPOSITING_FAILURE_ID, failureReason);
    }
#endif
  }

  return nullptr;
}

PLayerTransactionParent*
CompositorBridgeParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                     const uint64_t& aId)
{
  MOZ_ASSERT(aId == 0);

  InitializeLayerManager(aBackendHints);

  if (!mLayerManager) {
    NS_WARNING("Failed to initialise Compositor");
    LayerTransactionParent* p = new LayerTransactionParent(nullptr, this, 0);
    p->AddIPDLReference();
    return p;
  }

  mCompositionManager = new AsyncCompositionManager(this, mLayerManager);

  LayerTransactionParent* p = new LayerTransactionParent(mLayerManager, this, 0);
  p->AddIPDLReference();
  return p;
}

bool
CompositorBridgeParent::DeallocPLayerTransactionParent(PLayerTransactionParent* actor)
{
  static_cast<LayerTransactionParent*>(actor)->ReleaseIPDLReference();
  return true;
}

CompositorBridgeParent* CompositorBridgeParent::GetCompositorBridgeParent(uint64_t id)
{
  CompositorMap::iterator it = sCompositorMap->find(id);
  return it != sCompositorMap->end() ? it->second : nullptr;
}

void CompositorBridgeParent::AddCompositor(CompositorBridgeParent* compositor, uint64_t* outID)
{
  static uint64_t sNextID = 1;

  ++sNextID;
  (*sCompositorMap)[sNextID] = compositor;
  *outID = sNextID;
}

CompositorBridgeParent* CompositorBridgeParent::RemoveCompositor(uint64_t id)
{
  CompositorMap::iterator it = sCompositorMap->find(id);
  if (it == sCompositorMap->end()) {
    return nullptr;
  }
  CompositorBridgeParent *retval = it->second;
  sCompositorMap->erase(it);
  return retval;
}

void
CompositorBridgeParent::NotifyVsync(const TimeStamp& aTimeStamp, const uint64_t& aLayersId)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  auto it = sIndirectLayerTrees.find(aLayersId);
  if (it == sIndirectLayerTrees.end())
    return;

  CompositorBridgeParent* cbp = it->second.mParent;
  if (!cbp || !cbp->mWidget)
    return;

  RefPtr<VsyncObserver> obs = cbp->mWidget->GetVsyncObserver();
  if (!obs)
    return;

  obs->NotifyVsync(aTimeStamp);
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvNotifyChildCreated(const uint64_t& child,
                                               CompositorOptions* aOptions)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  NotifyChildCreated(child);
  *aOptions = mOptions;
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvNotifyChildRecreated(const uint64_t& aChild,
                                                 CompositorOptions* aOptions)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);

  if (sIndirectLayerTrees.find(aChild) != sIndirectLayerTrees.end()) {
    // Invalid to register the same layer tree twice.
    return IPC_FAIL_NO_REASON(this);
  }

  NotifyChildCreated(aChild);
  *aOptions = mOptions;
  return IPC_OK();
}

void
CompositorBridgeParent::NotifyChildCreated(uint64_t aChild)
{
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  sIndirectLayerTrees[aChild].mParent = this;
  sIndirectLayerTrees[aChild].mLayerManager = mLayerManager;
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvMapAndNotifyChildCreated(const uint64_t& aChild,
                                                     const base::ProcessId& aOwnerPid,
                                                     CompositorOptions* aOptions)
{
  // We only use this message when the remote compositor is in the GPU process.
  // It is harmless to call it, though.
  MOZ_ASSERT(XRE_IsGPUProcess());

  LayerTreeOwnerTracker::Get()->Map(aChild, aOwnerPid);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  NotifyChildCreated(aChild);
  *aOptions = mOptions;
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvAdoptChild(const uint64_t& child)
{
  APZCTreeManagerParent* parent;
  {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    // We currently don't support adopting children from one compositor to
    // another if the two compositors don't have the same options.
    MOZ_ASSERT(sIndirectLayerTrees[child].mParent->mOptions == mOptions);
    NotifyChildCreated(child);
    if (sIndirectLayerTrees[child].mLayerTree) {
      sIndirectLayerTrees[child].mLayerTree->SetLayerManager(mLayerManager);
    }
    parent = sIndirectLayerTrees[child].mApzcTreeManagerParent;
  }

  if (mApzcTreeManager && parent) {
    parent->ChildAdopted(mApzcTreeManager);
  }
  return IPC_OK();
}

PWebRenderBridgeParent*
CompositorBridgeParent::AllocPWebRenderBridgeParent(const wr::PipelineId& aPipelineId,
                                                    const LayoutDeviceIntSize& aSize,
                                                    TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                    uint32_t* aIdNamespace)
{
#ifndef MOZ_BUILD_WEBRENDER
  // Extra guard since this in the parent process and we don't want a malicious
  // child process invoking this codepath before it's ready
  MOZ_RELEASE_ASSERT(false);
#endif
  MOZ_ASSERT(wr::AsUint64(aPipelineId) == mRootLayerTreeID);
  MOZ_ASSERT(!mWrBridge);
  MOZ_ASSERT(!mCompositor);
  MOZ_ASSERT(!mCompositorScheduler);


  MOZ_ASSERT(mWidget);
  RefPtr<widget::CompositorWidget> widget = mWidget;
  RefPtr<wr::WebRenderAPI> api = wr::WebRenderAPI::Create(
    gfxPrefs::WebRenderProfilerEnabled(), this, Move(widget), aSize);
  RefPtr<WebRenderCompositableHolder> holder =
    new WebRenderCompositableHolder(WebRenderBridgeParent::AllocIdNameSpace());
  MOZ_ASSERT(api); // TODO have a fallback
  api->SetRootPipeline(aPipelineId);
  mWrBridge = new WebRenderBridgeParent(this, aPipelineId, mWidget, nullptr, Move(api), Move(holder));
  *aIdNamespace = mWrBridge->GetIdNameSpace();

  mCompositorScheduler = mWrBridge->CompositorScheduler();
  MOZ_ASSERT(mCompositorScheduler);
  mWrBridge.get()->AddRef(); // IPDL reference
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  MOZ_ASSERT(sIndirectLayerTrees[mRootLayerTreeID].mWrBridge == nullptr);
  sIndirectLayerTrees[mRootLayerTreeID].mWrBridge = mWrBridge;
  *aTextureFactoryIdentifier = mWrBridge->GetTextureFactoryIdentifier();
  return mWrBridge;
}

bool
CompositorBridgeParent::DeallocPWebRenderBridgeParent(PWebRenderBridgeParent* aActor)
{
#ifndef MOZ_BUILD_WEBRENDER
  // Extra guard since this in the parent process and we don't want a malicious
  // child process invoking this codepath before it's ready
  MOZ_RELEASE_ASSERT(false);
#endif
  WebRenderBridgeParent* parent = static_cast<WebRenderBridgeParent*>(aActor);
  {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    auto it = sIndirectLayerTrees.find(parent->PipelineId().mHandle);
    if (it != sIndirectLayerTrees.end()) {
      it->second.mWrBridge = nullptr;
    }
  }
  parent->Release(); // IPDL reference
  return true;
}

RefPtr<WebRenderBridgeParent>
CompositorBridgeParent::GetWebRenderBridgeParent() const
{
  return mWrBridge;
}

void
CompositorBridgeParent::SetWebRenderProfilerEnabled(bool aEnabled)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  for (auto it = sIndirectLayerTrees.begin(); it != sIndirectLayerTrees.end(); it++) {
    LayerTreeState* state = &it->second;
    if (state->mWrBridge) {
      state->mWrBridge->SetWebRenderProfilerEnabled(aEnabled);
    }
  }
}

void
EraseLayerState(uint64_t aId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);

  auto iter = sIndirectLayerTrees.find(aId);
  if (iter != sIndirectLayerTrees.end()) {
    CompositorBridgeParent* parent = iter->second.mParent;
    if (parent) {
      parent->ClearApproximatelyVisibleRegions(aId, Nothing());
    }

    sIndirectLayerTrees.erase(iter);
  }
}

/*static*/ void
CompositorBridgeParent::DeallocateLayerTreeId(uint64_t aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Here main thread notifies compositor to remove an element from
  // sIndirectLayerTrees. This removed element might be queried soon.
  // Checking the elements of sIndirectLayerTrees exist or not before using.
  if (!CompositorLoop()) {
    gfxCriticalError() << "Attempting to post to a invalid Compositor Loop";
    return;
  }
  CompositorLoop()->PostTask(NewRunnableFunction(&EraseLayerState, aId));
}

static void
UpdateControllerForLayersId(uint64_t aLayersId,
                            GeckoContentController* aController)
{
  // Adopt ref given to us by SetControllerForLayerTree()
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aLayersId].mController =
    already_AddRefed<GeckoContentController>(aController);
}

ScopedLayerTreeRegistration::ScopedLayerTreeRegistration(APZCTreeManager* aApzctm,
                                                         uint64_t aLayersId,
                                                         Layer* aRoot,
                                                         GeckoContentController* aController)
    : mLayersId(aLayersId)
{
  EnsureLayerTreeMapReady();
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aLayersId].mRoot = aRoot;
  sIndirectLayerTrees[aLayersId].mController = aController;
}

ScopedLayerTreeRegistration::~ScopedLayerTreeRegistration()
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees.erase(mLayersId);
}

/*static*/ void
CompositorBridgeParent::SetControllerForLayerTree(uint64_t aLayersId,
                                                  GeckoContentController* aController)
{
  // This ref is adopted by UpdateControllerForLayersId().
  aController->AddRef();
  CompositorLoop()->PostTask(NewRunnableFunction(&UpdateControllerForLayersId,
                                                 aLayersId,
                                                 aController));
}

/*static*/ already_AddRefed<APZCTreeManager>
CompositorBridgeParent::GetAPZCTreeManager(uint64_t aLayersId)
{
  EnsureLayerTreeMapReady();
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  LayerTreeMap::iterator cit = sIndirectLayerTrees.find(aLayersId);
  if (sIndirectLayerTrees.end() == cit) {
    return nullptr;
  }
  LayerTreeState* lts = &cit->second;

  RefPtr<APZCTreeManager> apzctm = lts->mParent
                                   ? lts->mParent->mApzcTreeManager.get()
                                   : nullptr;
  return apzctm.forget();
}

static void
InsertVsyncProfilerMarker(TimeStamp aVsyncTimestamp)
{
#ifdef MOZ_GECKO_PROFILER
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  VsyncPayload* payload = new VsyncPayload(aVsyncTimestamp);
  PROFILER_MARKER_PAYLOAD("VsyncTimestamp", payload);
#endif
}

/*static */ void
CompositorBridgeParent::PostInsertVsyncProfilerMarker(TimeStamp aVsyncTimestamp)
{
  // Called in the vsync thread
  if (profiler_is_active() && CompositorThreadHolder::IsActive()) {
    CompositorLoop()->PostTask(
      NewRunnableFunction(InsertVsyncProfilerMarker, aVsyncTimestamp));
  }
}

widget::PCompositorWidgetParent*
CompositorBridgeParent::AllocPCompositorWidgetParent(const CompositorWidgetInitData& aInitData)
{
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

bool
CompositorBridgeParent::DeallocPCompositorWidgetParent(PCompositorWidgetParent* aActor)
{
#if defined(MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING)
  static_cast<widget::CompositorWidgetParent*>(aActor)->Release();
  return true;
#else
  return false;
#endif
}

bool
CompositorBridgeParent::IsPendingComposite()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mCompositor) {
    return false;
  }
  return mCompositor->IsPendingComposite();
}

void
CompositorBridgeParent::FinishPendingComposite()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!mCompositor) {
    return;
  }
  return mCompositor->FinishPendingComposite();
}

CompositorController*
CompositorBridgeParent::LayerTreeState::GetCompositorController() const
{
  return mParent;
}

MetricsSharingController*
CompositorBridgeParent::LayerTreeState::CrossProcessSharingController() const
{
  return mCrossProcessParent;
}

MetricsSharingController*
CompositorBridgeParent::LayerTreeState::InProcessSharingController() const
{
  return mParent;
}

void
CompositorBridgeParent::DidComposite(TimeStamp& aCompositeStart,
                                     TimeStamp& aCompositeEnd)
{
  if (mWrBridge) {
    NotifyDidComposite(mWrBridge->FlushPendingTransactionIds(), aCompositeStart, aCompositeEnd);
  } else {
    NotifyDidComposite(mPendingTransaction, aCompositeStart, aCompositeEnd);
    mPendingTransaction = 0;
  }
}

void
CompositorBridgeParent::NotifyDidCompositeToPipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd)
{
  if (!mWrBridge) {
    return;
  }
  mWrBridge->CompositableHolder()->Update(aPipelineId, aEpoch);

  if (mPaused) {
    return;
  }

  if (mWrBridge->PipelineId() == aPipelineId) {
    uint64_t transactionId = mWrBridge->FlushTransactionIdsForEpoch(aEpoch);
    Unused << SendDidComposite(0, transactionId, aCompositeStart, aCompositeEnd);

    nsTArray<ImageCompositeNotificationInfo> notifications;
    mWrBridge->ExtractImageCompositeNotifications(&notifications);
    if (!notifications.IsEmpty()) {
      Unused << ImageBridgeParent::NotifyImageComposites(notifications);
    }
    return;
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  ForEachIndirectLayerTree([&] (LayerTreeState* lts, const uint64_t& aLayersId) -> void {
    if (lts->mCrossProcessParent &&
        lts->mWrBridge &&
        lts->mWrBridge->PipelineId() == aPipelineId) {
      CrossProcessCompositorBridgeParent* cpcp = lts->mCrossProcessParent;
      uint64_t transactionId = lts->mWrBridge->FlushTransactionIdsForEpoch(aEpoch);
      Unused << cpcp->SendDidComposite(aLayersId, transactionId, aCompositeStart, aCompositeEnd);
    }
  });
}

void
CompositorBridgeParent::NotifyDidComposite(uint64_t aTransactionId, TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd)
{
  Unused << SendDidComposite(0, aTransactionId, aCompositeStart, aCompositeEnd);

  if (mLayerManager) {
    nsTArray<ImageCompositeNotificationInfo> notifications;
    mLayerManager->ExtractImageCompositeNotifications(&notifications);
    if (!notifications.IsEmpty()) {
      Unused << ImageBridgeParent::NotifyImageComposites(notifications);
    }
  }

  if (mWrBridge) {
    nsTArray<ImageCompositeNotificationInfo> notifications;
    mWrBridge->ExtractImageCompositeNotifications(&notifications);
    if (!notifications.IsEmpty()) {
      Unused << ImageBridgeParent::NotifyImageComposites(notifications);
    }
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  ForEachIndirectLayerTree([&] (LayerTreeState* lts, const uint64_t& aLayersId) -> void {
    if (lts->mCrossProcessParent && lts->mParent == this) {
      CrossProcessCompositorBridgeParent* cpcp = lts->mCrossProcessParent;
      cpcp->DidComposite(aLayersId, aCompositeStart, aCompositeEnd);
    }
  });
}

void
CompositorBridgeParent::InvalidateRemoteLayers()
{
  MOZ_ASSERT(CompositorLoop() == MessageLoop::current());

  Unused << PCompositorBridgeParent::SendInvalidateLayers(0);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  ForEachIndirectLayerTree([] (LayerTreeState* lts, const uint64_t& aLayersId) -> void {
    if (lts->mCrossProcessParent) {
      CrossProcessCompositorBridgeParent* cpcp = lts->mCrossProcessParent;
      Unused << cpcp->SendInvalidateLayers(aLayersId);
    }
  });
}

void
UpdateIndirectTree(uint64_t aId, Layer* aRoot, const TargetConfig& aTargetConfig)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aId].mRoot = aRoot;
  sIndirectLayerTrees[aId].mTargetConfig = aTargetConfig;
}

/* static */ CompositorBridgeParent::LayerTreeState*
CompositorBridgeParent::GetIndirectShadowTree(uint64_t aId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  LayerTreeMap::iterator cit = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() == cit) {
    return nullptr;
  }
  return &cit->second;
}

static CompositorBridgeParent::LayerTreeState*
GetStateForRoot(uint64_t aContentLayersId, const MonitorAutoLock& aProofOfLock)
{
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
    uint64_t rootLayersId = state->mParent->RootLayerTreeId();
    itr = sIndirectLayerTrees.find(rootLayersId);
    state = (sIndirectLayerTrees.end() != itr) ? &itr->second : nullptr;
  }

  return state;
}

/* static */ APZCTreeManagerParent*
CompositorBridgeParent::GetApzcTreeManagerParentForRoot(uint64_t aContentLayersId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState* state =
      GetStateForRoot(aContentLayersId, lock);
  return state ? state->mApzcTreeManagerParent : nullptr;
}

/* static */ GeckoContentController*
CompositorBridgeParent::GetGeckoContentControllerForRoot(uint64_t aContentLayersId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState* state =
      GetStateForRoot(aContentLayersId, lock);
  return state ? state->mController.get() : nullptr;
}

PTextureParent*
CompositorBridgeParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                            const LayersBackend& aLayersBackend,
                                            const TextureFlags& aFlags,
                                            const uint64_t& aId,
                                            const uint64_t& aSerial,
                                            const wr::MaybeExternalImageId& aExternalImageId)
{
  return TextureHost::CreateIPDLActor(this, aSharedData, aLayersBackend, aFlags, aSerial, aExternalImageId);
}

bool
CompositorBridgeParent::DeallocPTextureParent(PTextureParent* actor)
{
  return TextureHost::DestroyIPDLActor(actor);
}

bool
CompositorBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
//#define PLUGINS_LOG(...) printf_stderr("CP [%s]: ", __FUNCTION__);
//                         printf_stderr(__VA_ARGS__);
//                         printf_stderr("\n");
#define PLUGINS_LOG(...)

bool
CompositorBridgeParent::UpdatePluginWindowState(uint64_t aId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& lts = sIndirectLayerTrees[aId];
  if (!lts.mParent) {
    PLUGINS_LOG("[%" PRIu64 "] layer tree compositor parent pointer is null", aId);
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
    mPluginsLayerOffset = nsIntPoint(0,0);
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

void
CompositorBridgeParent::ScheduleShowAllPluginWindows()
{
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::ShowAllPluginWindows));
}

void
CompositorBridgeParent::ShowAllPluginWindows()
{
  MOZ_ASSERT(!NS_IsMainThread());
  mDeferPluginWindows = false;
  ScheduleComposition();
}

void
CompositorBridgeParent::ScheduleHideAllPluginWindows()
{
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(NewRunnableMethod(this, &CompositorBridgeParent::HideAllPluginWindows));
}

void
CompositorBridgeParent::HideAllPluginWindows()
{
  MOZ_ASSERT(!NS_IsMainThread());
  // No plugins in the cache implies no plugins to manage
  // in this content.
  if (!mCachedPluginData.Length() || mDeferPluginWindows) {
    return;
  }

  uintptr_t parentWidget = GetWidget()->GetWidgetKey();

  mDeferPluginWindows = true;
  mPluginWindowsHidden = true;

#if defined(XP_WIN)
  // We will get an async reply that this has happened and then send hide.
  mWaitForPluginsUntil = TimeStamp::Now() + mVsyncRate;
  Unused << SendCaptureAllPlugins(parentWidget);
#else
  Unused << SendHideAllPlugins(parentWidget);
  ScheduleComposition();
#endif
}
#endif // #if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)

mozilla::ipc::IPCResult
CompositorBridgeParent::RecvAllPluginsCaptured()
{
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

} // namespace layers
} // namespace mozilla

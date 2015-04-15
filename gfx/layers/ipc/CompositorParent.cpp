/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorParent.h"
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
#include "base/tracked.h"               // for FROM_HERE
#include "gfxContext.h"                 // for gfxContext
#include "gfxPlatform.h"                // for gfxPlatform
#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"             // for gfxPlatform
#endif
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/AutoRestore.h"        // for AutoRestore
#include "mozilla/ClearOnShutdown.h"    // for ClearOnShutdown
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/gfx/2D.h"          // for DrawTarget
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/layers/APZCTreeManager.h"  // for APZCTreeManager
#include "mozilla/layers/APZThreadUtils.h"  // for APZCTreeManager
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/BasicCompositor.h"  // for BasicCompositor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/PLayerTransactionParent.h"
#include "mozilla/layers/ShadowLayersManager.h" // for ShadowLayersManager
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/Telemetry.h"
#ifdef MOZ_WIDGET_GTK
#include "basic/X11BasicCompositor.h" // for X11BasicCompositor
#endif
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsRect.h"                     // for nsIntRect
#include "nsTArray.h"                   // for nsTArray
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#ifdef XP_WIN
#include "mozilla/layers/CompositorD3D11.h"
#include "mozilla/layers/CompositorD3D9.h"
#endif
#include "GeckoProfiler.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/unused.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/StaticPtr.h"
#ifdef MOZ_ENABLE_PROFILER_SPS
#include "ProfilerMarkers.h"
#endif
#include "mozilla/VsyncDispatcher.h"

#ifdef MOZ_WIDGET_GONK
#include "GeckoTouchDispatcher.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace std;

using base::ProcessId;
using base::Thread;

CompositorParent::LayerTreeState::LayerTreeState()
  : mParent(nullptr)
  , mLayerManager(nullptr)
  , mCrossProcessParent(nullptr)
  , mLayerTree(nullptr)
  , mUpdatedPluginDataAvailable(false)
{
}

CompositorParent::LayerTreeState::~LayerTreeState()
{
  if (mController) {
    mController->Destroy();
  }
}

typedef map<uint64_t, CompositorParent::LayerTreeState> LayerTreeMap;
static LayerTreeMap sIndirectLayerTrees;
static StaticAutoPtr<mozilla::Monitor> sIndirectLayerTreesLock;

static void EnsureLayerTreeMapReady()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sIndirectLayerTreesLock) {
    sIndirectLayerTreesLock = new Monitor("IndirectLayerTree");
    mozilla::ClearOnShutdown(&sIndirectLayerTreesLock);
  }
}

/**
  * A global map referencing each compositor by ID.
  *
  * This map is used by the ImageBridge protocol to trigger
  * compositions without having to keep references to the
  * compositor
  */
typedef map<uint64_t,CompositorParent*> CompositorMap;
static CompositorMap* sCompositorMap;

static void CreateCompositorMap()
{
  MOZ_ASSERT(!sCompositorMap);
  sCompositorMap = new CompositorMap;
}

static void DestroyCompositorMap()
{
  MOZ_ASSERT(sCompositorMap);
  MOZ_ASSERT(sCompositorMap->empty());
  delete sCompositorMap;
  sCompositorMap = nullptr;
}

// See ImageBridgeChild.cpp
void ReleaseImageBridgeParentSingleton();

CompositorThreadHolder::CompositorThreadHolder()
  : mCompositorThread(CreateCompositorThread())
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(CompositorThreadHolder);
}

CompositorThreadHolder::~CompositorThreadHolder()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_COUNT_DTOR(CompositorThreadHolder);

  DestroyCompositorThread(mCompositorThread);
}

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;
static bool sFinishedCompositorShutDown = false;

CompositorThreadHolder* GetCompositorThreadHolder()
{
  return sCompositorThreadHolder;
}

/* static */ Thread*
CompositorThreadHolder::CreateCompositorThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder, "The compositor thread has already been started!");

  Thread* compositorThread = new Thread("Compositor");

  Thread::Options options;
  /* Timeout values are powers-of-two to enable us get better data.
     128ms is chosen for transient hangs because 8Hz should be the minimally
     acceptable goal for Compositor responsiveness (normal goal is 60Hz). */
  options.transient_hang_timeout = 128; // milliseconds
  /* 2048ms is chosen for permanent hangs because it's longer than most
   * Compositor hangs seen in the wild, but is short enough to not miss getting
   * native hang stacks. */
  options.permanent_hang_timeout = 2048; // milliseconds
#if defined(_WIN32)
  /* With d3d9 the compositor thread creates native ui, see DeviceManagerD3D9. As
   * such the thread is a gui thread, and must process a windows message queue or
   * risk deadlocks. Chromium message loop TYPE_UI does exactly what we need. */
  options.message_loop_type = MessageLoop::TYPE_UI;
#endif

  if (!compositorThread->StartWithOptions(options)) {
    delete compositorThread;
    return nullptr;
  }

  EnsureLayerTreeMapReady();
  CreateCompositorMap();

  return compositorThread;
}

/* static */ void
CompositorThreadHolder::DestroyCompositorThread(Thread* aCompositorThread)
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sCompositorThreadHolder, "We shouldn't be destroying the compositor thread yet.");

  DestroyCompositorMap();
  delete aCompositorThread;
  sFinishedCompositorShutDown = true;
}

static Thread* CompositorThread() {
  return sCompositorThreadHolder ? sCompositorThreadHolder->GetCompositorThread() : nullptr;
}

static void SetThreadPriority()
{
  hal::SetCurrentThreadPriority(hal::THREAD_PRIORITY_COMPOSITOR);
}

CompositorVsyncObserver::CompositorVsyncObserver(CompositorParent* aCompositorParent, nsIWidget* aWidget)
  : mNeedsComposite(false)
  , mIsObservingVsync(false)
  , mVsyncNotificationsSkipped(0)
  , mCompositorParent(aCompositorParent)
  , mCurrentCompositeTaskMonitor("CurrentCompositeTaskMonitor")
  , mCurrentCompositeTask(nullptr)
  , mSetNeedsCompositeMonitor("SetNeedsCompositeMonitor")
  , mSetNeedsCompositeTask(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWidget != nullptr);
  mCompositorVsyncDispatcher = aWidget->GetCompositorVsyncDispatcher();
#ifdef MOZ_WIDGET_GONK
  GeckoTouchDispatcher::GetInstance()->SetCompositorVsyncObserver(this);
#endif
}

CompositorVsyncObserver::~CompositorVsyncObserver()
{
  MOZ_ASSERT(!mIsObservingVsync);
  // The CompositorVsyncDispatcher is cleaned up before this in the nsBaseWidget, which stops vsync listeners
  mCompositorParent = nullptr;
  mCompositorVsyncDispatcher = nullptr;
}

void
CompositorVsyncObserver::Destroy()
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  UnobserveVsync();
  CancelCurrentCompositeTask();
  CancelCurrentSetNeedsCompositeTask();
}

void
CompositorVsyncObserver::CancelCurrentSetNeedsCompositeTask()
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  MonitorAutoLock lock(mSetNeedsCompositeMonitor);
  if (mSetNeedsCompositeTask) {
    mSetNeedsCompositeTask->Cancel();
    mSetNeedsCompositeTask = nullptr;
  }
  mNeedsComposite = false;
}

/**
 * TODO Potential performance heuristics:
 * If a composite takes 17 ms, do we composite ASAP or wait until next vsync?
 * If a layer transaction comes after vsync, do we composite ASAP or wait until
 * next vsync?
 * How many skipped vsync events until we stop listening to vsync events?
 */
void
CompositorVsyncObserver::SetNeedsComposite(bool aNeedsComposite)
{
  if (!CompositorParent::IsInCompositorThread()) {
    MonitorAutoLock lock(mSetNeedsCompositeMonitor);
    mSetNeedsCompositeTask = NewRunnableMethod(this,
                                              &CompositorVsyncObserver::SetNeedsComposite,
                                              aNeedsComposite);
    MOZ_ASSERT(CompositorParent::CompositorLoop());
    CompositorParent::CompositorLoop()->PostTask(FROM_HERE, mSetNeedsCompositeTask);
    return;
  } else {
    MonitorAutoLock lock(mSetNeedsCompositeMonitor);
    mSetNeedsCompositeTask = nullptr;
  }

  mNeedsComposite = aNeedsComposite;
  if (!mIsObservingVsync && mNeedsComposite) {
    ObserveVsync();
  }
}

bool
CompositorVsyncObserver::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Called from the vsync dispatch thread
  MOZ_ASSERT(!CompositorParent::IsInCompositorThread());
  MOZ_ASSERT(!NS_IsMainThread());

  MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
  if (mCurrentCompositeTask == nullptr) {
    mCurrentCompositeTask = NewRunnableMethod(this,
                                              &CompositorVsyncObserver::Composite,
                                              aVsyncTimestamp);
    MOZ_ASSERT(CompositorParent::CompositorLoop());
    CompositorParent::CompositorLoop()->PostTask(FROM_HERE, mCurrentCompositeTask);
  }
  return true;
}

void
CompositorVsyncObserver::CancelCurrentCompositeTask()
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread() || NS_IsMainThread());
  MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
  if (mCurrentCompositeTask) {
    mCurrentCompositeTask->Cancel();
    mCurrentCompositeTask = nullptr;
  }
}

void
CompositorVsyncObserver::Composite(TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  {
    MonitorAutoLock lock(mCurrentCompositeTaskMonitor);
    mCurrentCompositeTask = nullptr;
  }

  DispatchTouchEvents(aVsyncTimestamp);

  if (mNeedsComposite && mCompositorParent) {
    mNeedsComposite = false;
    mCompositorParent->CompositeCallback(aVsyncTimestamp);
    mVsyncNotificationsSkipped = 0;
  } else if (mVsyncNotificationsSkipped++ > gfxPrefs::CompositorUnobserveCount()) {
    UnobserveVsync();
  }
}

void
CompositorVsyncObserver::OnForceComposeToTarget()
{
  /**
   * bug 1138502 - There are cases such as during long-running window resizing events
   * where we receive many sync RecvFlushComposites. We also get vsync notifications which
   * will increment mVsyncNotificationsSkipped because a composite just occurred. After
   * enough vsyncs and RecvFlushComposites occurred, we will disable vsync. Then at the next
   * ScheduleComposite, we will enable vsync, then get a RecvFlushComposite, which will
   * force us to unobserve vsync again. On some platforms, enabling/disabling vsync is not
   * free and this oscillating behavior causes a performance hit. In order to avoid this problem,
   * we reset the mVsyncNotificationsSkipped counter to keep vsync enabled.
   */
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  mVsyncNotificationsSkipped = 0;
}

bool
CompositorVsyncObserver::NeedsComposite()
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  return mNeedsComposite;
}

void
CompositorVsyncObserver::ObserveVsync()
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  mCompositorVsyncDispatcher->SetCompositorVsyncObserver(this);
  mIsObservingVsync = true;
}

void
CompositorVsyncObserver::UnobserveVsync()
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  mCompositorVsyncDispatcher->SetCompositorVsyncObserver(nullptr);
  mIsObservingVsync = false;
}

void
CompositorVsyncObserver::DispatchTouchEvents(TimeStamp aVsyncTimestamp)
{
#ifdef MOZ_WIDGET_GONK
  GeckoTouchDispatcher::GetInstance()->NotifyVsync(aVsyncTimestamp);
#endif
}

void CompositorParent::StartUp()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(!sCompositorThreadHolder, "The compositor thread has already been started!");

  sCompositorThreadHolder = new CompositorThreadHolder();
}

void CompositorParent::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main Thread!");
  MOZ_ASSERT(sCompositorThreadHolder, "The compositor thread has already been shut down!");

  ReleaseImageBridgeParentSingleton();

  sCompositorThreadHolder = nullptr;

  // No locking is needed around sFinishedCompositorShutDown because it is only
  // ever accessed on the main thread.
  while (!sFinishedCompositorShutDown) {
    NS_ProcessNextEvent(nullptr, true);
  }
}

MessageLoop* CompositorParent::CompositorLoop()
{
  return CompositorThread() ? CompositorThread()->message_loop() : nullptr;
}

static bool
IsInCompositorAsapMode()
{
  // Returns true if the compositor is allowed to be in ASAP mode
  // and layout is not in ASAP mode
  return gfxPrefs::LayersCompositionFrameRate() == 0 &&
            !gfxPlatform::IsInLayoutAsapMode();
}

static bool
UseVsyncComposition()
{
  return gfxPrefs::VsyncAlignedCompositor()
          && gfxPrefs::HardwareVsyncEnabled()
          && !IsInCompositorAsapMode()
          && !gfxPlatform::IsInLayoutAsapMode();
}

CompositorParent::CompositorParent(nsIWidget* aWidget,
                                   bool aUseExternalSurfaceSize,
                                   int aSurfaceWidth, int aSurfaceHeight)
  : mWidget(aWidget)
  , mCurrentCompositeTask(nullptr)
  , mIsTesting(false)
  , mPendingTransaction(0)
  , mPaused(false)
  , mUseExternalSurfaceSize(aUseExternalSurfaceSize)
  , mEGLSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mPauseCompositionMonitor("PauseCompositionMonitor")
  , mResumeCompositionMonitor("ResumeCompositionMonitor")
  , mOverrideComposeReadiness(false)
  , mForceCompositionTask(nullptr)
  , mCompositorThreadHolder(sCompositorThreadHolder)
  , mCompositorVsyncObserver(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(CompositorThread(),
             "The compositor thread must be Initialized before instanciating a CompositorParent.");
  MOZ_COUNT_CTOR(CompositorParent);
  mCompositorID = 0;
  // FIXME: This holds on the the fact that right now the only thing that
  // can destroy this instance is initialized on the compositor thread after
  // this task has been processed.
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(FROM_HERE, NewRunnableFunction(&AddCompositor,
                                                          this, &mCompositorID));

  CompositorLoop()->PostTask(FROM_HERE, NewRunnableFunction(SetThreadPriority));

  mRootLayerTreeID = AllocateLayerTreeId();

  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees[mRootLayerTreeID].mParent = this;
  }

  if (gfxPrefs::AsyncPanZoomEnabled()) {
    mApzcTreeManager = new APZCTreeManager();
  }

  if (UseVsyncComposition()) {
    NS_WARNING("Enabling vsync compositor\n");
    mCompositorVsyncObserver = new CompositorVsyncObserver(this, aWidget);
  }

  gfxPlatform::GetPlatform()->ComputeTileSize();
}

bool
CompositorParent::IsInCompositorThread()
{
  return CompositorThread() && CompositorThread()->thread_id() == PlatformThread::CurrentId();
}

uint64_t
CompositorParent::RootLayerTreeId()
{
  return mRootLayerTreeID;
}

CompositorParent::~CompositorParent()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(CompositorParent);
}

void
CompositorParent::Destroy()
{
  MOZ_ASSERT(ManagedPLayerTransactionParent().Length() == 0,
             "CompositorParent destroyed before managed PLayerTransactionParent");

  MOZ_ASSERT(mPaused); // Ensure RecvWillStop was called
  // Ensure that the layer manager is destructed on the compositor thread.
  mLayerManager = nullptr;
  if (mCompositor) {
    mCompositor->Destroy();
  }
  mCompositor = nullptr;

  mCompositionManager = nullptr;
  if (mApzcTreeManager) {
    mApzcTreeManager->ClearTree();
    mApzcTreeManager = nullptr;
  }
  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees.erase(mRootLayerTreeID);
  }
  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->Destroy();
    mCompositorVsyncObserver = nullptr;
  }
}

void
CompositorParent::ForceIsFirstPaint()
{
  mCompositionManager->ForceIsFirstPaint();
}

bool
CompositorParent::RecvWillStop()
{
  mPaused = true;
  RemoveCompositor(mCompositorID);

  // Ensure that the layer manager is destroyed before CompositorChild.
  if (mLayerManager) {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    for (LayerTreeMap::iterator it = sIndirectLayerTrees.begin();
         it != sIndirectLayerTrees.end(); it++)
    {
      LayerTreeState* lts = &it->second;
      if (lts->mParent == this) {
        mLayerManager->ClearCachedResources(lts->mRoot);
        lts->mLayerManager = nullptr;
        lts->mParent = nullptr;
      }
    }
    mLayerManager->Destroy();
    mLayerManager = nullptr;
    mCompositionManager = nullptr;
  }

  return true;
}

void CompositorParent::DeferredDestroy()
{
  MOZ_ASSERT(!NS_IsMainThread());
  mCompositorThreadHolder = nullptr;
  Release();
}

bool
CompositorParent::RecvStop()
{
  Destroy();
  // There are chances that the ref count reaches zero on the main thread shortly
  // after this function returns while some ipdl code still needs to run on
  // this thread.
  // We must keep the compositor parent alive untill the code handling message
  // reception is finished on this thread.
  this->AddRef(); // Corresponds to DeferredDestroy's Release
  MessageLoop::current()->PostTask(FROM_HERE,
                                   NewRunnableMethod(this,&CompositorParent::DeferredDestroy));
  return true;
}

bool
CompositorParent::RecvPause()
{
  PauseComposition();
  return true;
}

bool
CompositorParent::RecvResume()
{
  ResumeComposition();
  return true;
}

bool
CompositorParent::RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                   const nsIntRect& aRect)
{
  RefPtr<DrawTarget> target = GetDrawTargetForDescriptor(aInSnapshot, gfx::BackendType::CAIRO);
  ForceComposeToTarget(target, &aRect);
  return true;
}

bool
CompositorParent::RecvFlushRendering()
{
  if (mCompositorVsyncObserver && mCompositorVsyncObserver->NeedsComposite()) {
    mCompositorVsyncObserver->SetNeedsComposite(false);
    CancelCurrentCompositeTask();
    ForceComposeToTarget(nullptr);
  } else if (mCurrentCompositeTask) {
    // If we're waiting to do a composite, then cancel it
    // and do it immediately instead.
    CancelCurrentCompositeTask();
    ForceComposeToTarget(nullptr);
  }

  return true;
}

bool
CompositorParent::RecvGetTileSize(int32_t* aWidth, int32_t* aHeight)
{
  *aWidth = gfxPlatform::GetPlatform()->GetTileWidth();
  *aHeight = gfxPlatform::GetPlatform()->GetTileHeight();
  return true;
}

bool
CompositorParent::RecvNotifyRegionInvalidated(const nsIntRegion& aRegion)
{
  if (mLayerManager) {
    mLayerManager->AddInvalidRegion(aRegion);
  }
  return true;
}

bool
CompositorParent::RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex)
{
  if (mLayerManager) {
    *aOutStartIndex = mLayerManager->StartFrameTimeRecording(aBufferSize);
  } else {
    *aOutStartIndex = 0;
  }
  return true;
}

bool
CompositorParent::RecvStopFrameTimeRecording(const uint32_t& aStartIndex,
                                             InfallibleTArray<float>* intervals)
{
  if (mLayerManager) {
    mLayerManager->StopFrameTimeRecording(aStartIndex, *intervals);
  }
  return true;
}

void
CompositorParent::ActorDestroy(ActorDestroyReason why)
{
  CancelCurrentCompositeTask();
  if (mForceCompositionTask) {
    mForceCompositionTask->Cancel();
    mForceCompositionTask = nullptr;
  }
  mPaused = true;
  RemoveCompositor(mCompositorID);

  if (mLayerManager) {
    mLayerManager->Destroy();
    mLayerManager = nullptr;
    { // scope lock
      MonitorAutoLock lock(*sIndirectLayerTreesLock);
      sIndirectLayerTrees[mRootLayerTreeID].mLayerManager = nullptr;
    }
    mCompositionManager = nullptr;
    mCompositor = nullptr;
  }
}


void
CompositorParent::ScheduleRenderOnCompositorThread()
{
  CancelableTask *renderTask = NewRunnableMethod(this, &CompositorParent::ScheduleComposition);
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(FROM_HERE, renderTask);
}

void
CompositorParent::PauseComposition()
{
  MOZ_ASSERT(IsInCompositorThread(),
             "PauseComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mPauseCompositionMonitor);

  if (!mPaused) {
    mPaused = true;

    mCompositor->Pause();
    DidComposite();
  }

  // if anyone's waiting to make sure that composition really got paused, tell them
  lock.NotifyAll();
}

void
CompositorParent::ResumeComposition()
{
  MOZ_ASSERT(IsInCompositorThread(),
             "ResumeComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mResumeCompositionMonitor);

  if (!mCompositor->Resume()) {
#ifdef MOZ_WIDGET_ANDROID
    // We can't get a surface. This could be because the activity changed between
    // the time resume was scheduled and now.
    __android_log_print(ANDROID_LOG_INFO, "CompositorParent", "Unable to renew compositor surface; remaining in paused state");
#endif
    lock.NotifyAll();
    return;
  }

  mPaused = false;

  mLastCompose = TimeStamp::Now();
  CompositeToTarget(nullptr);

  // if anyone's waiting to make sure that composition really got resumed, tell them
  lock.NotifyAll();
}

void
CompositorParent::ForceComposition()
{
  // Cancel the orientation changed state to force composition
  mForceCompositionTask = nullptr;
  ScheduleRenderOnCompositorThread();
}

void
CompositorParent::CancelCurrentCompositeTask()
{
  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->CancelCurrentCompositeTask();
  } else if (mCurrentCompositeTask) {
    mCurrentCompositeTask->Cancel();
    mCurrentCompositeTask = nullptr;
  }
}

void
CompositorParent::SetEGLSurfaceSize(int width, int height)
{
  NS_ASSERTION(mUseExternalSurfaceSize, "Compositor created without UseExternalSurfaceSize provided");
  mEGLSurfaceSize.SizeTo(width, height);
  if (mCompositor) {
    mCompositor->SetDestinationSurfaceSize(gfx::IntSize(mEGLSurfaceSize.width, mEGLSurfaceSize.height));
  }
}

void
CompositorParent::ResumeCompositionAndResize(int width, int height)
{
  SetEGLSurfaceSize(width, height);
  ResumeComposition();
}

/*
 * This will execute a pause synchronously, waiting to make sure that the compositor
 * really is paused.
 */
void
CompositorParent::SchedulePauseOnCompositorThread()
{
  MonitorAutoLock lock(mPauseCompositionMonitor);

  CancelableTask *pauseTask = NewRunnableMethod(this,
                                                &CompositorParent::PauseComposition);
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(FROM_HERE, pauseTask);

  // Wait until the pause has actually been processed by the compositor thread
  lock.Wait();
}

bool
CompositorParent::ScheduleResumeOnCompositorThread()
{
  MonitorAutoLock lock(mResumeCompositionMonitor);

  CancelableTask *resumeTask =
    NewRunnableMethod(this, &CompositorParent::ResumeComposition);
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(FROM_HERE, resumeTask);

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();

  return !mPaused;
}

bool
CompositorParent::ScheduleResumeOnCompositorThread(int width, int height)
{
  MonitorAutoLock lock(mResumeCompositionMonitor);

  CancelableTask *resumeTask =
    NewRunnableMethod(this, &CompositorParent::ResumeCompositionAndResize, width, height);
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(FROM_HERE, resumeTask);

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();

  return !mPaused;
}

void
CompositorParent::ScheduleTask(CancelableTask* task, int time)
{
  if (time == 0) {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  } else {
    MessageLoop::current()->PostDelayedTask(FROM_HERE, task, time);
  }
}

void
CompositorParent::NotifyShadowTreeTransaction(uint64_t aId, bool aIsFirstPaint,
    bool aScheduleComposite, uint32_t aPaintSequenceNumber,
    bool aIsRepeatTransaction)
{
  if (mApzcTreeManager &&
      !aIsRepeatTransaction &&
      mLayerManager &&
      mLayerManager->GetRoot()) {
    AutoResolveRefLayers resolve(mCompositionManager);
    mApzcTreeManager->UpdateHitTestingTree(this, mLayerManager->GetRoot(),
        aIsFirstPaint, aId, aPaintSequenceNumber);

    mLayerManager->NotifyShadowTreeTransaction();
  }
  if (aScheduleComposite) {
    ScheduleComposition();
  }
}

// Used when layout.frame_rate is -1. Needs to be kept in sync with
// DEFAULT_FRAME_RATE in nsRefreshDriver.cpp.
static const int32_t kDefaultFrameRate = 60;

static int32_t
CalculateCompositionFrameRate()
{
  int32_t compositionFrameRatePref = gfxPrefs::LayersCompositionFrameRate();
  if (compositionFrameRatePref < 0) {
    // Use the same frame rate for composition as for layout.
    int32_t layoutFrameRatePref = gfxPrefs::LayoutFrameRate();
    if (layoutFrameRatePref < 0) {
      // TODO: The main thread frame scheduling code consults the actual
      // monitor refresh rate in this case. We should do the same.
      return kDefaultFrameRate;
    }
    return layoutFrameRatePref;
  }
  return compositionFrameRatePref;
}

void
CompositorParent::ScheduleSoftwareTimerComposition()
{
  MOZ_ASSERT(!mCompositorVsyncObserver);

  if (mCurrentCompositeTask) {
    return;
  }

  bool initialComposition = mLastCompose.IsNull();
  TimeDuration delta;
  if (!initialComposition)
    delta = TimeStamp::Now() - mLastCompose;

  int32_t rate = CalculateCompositionFrameRate();

  // If rate == 0 (ASAP mode), minFrameDelta must be 0 so there's no delay.
  TimeDuration minFrameDelta = TimeDuration::FromMilliseconds(
    rate == 0 ? 0.0 : std::max(0.0, 1000.0 / rate));

  mCurrentCompositeTask = NewRunnableMethod(this,
                                            &CompositorParent::CompositeCallback,
                                            TimeStamp::Now());

  if (!initialComposition && delta < minFrameDelta) {
    TimeDuration delay = minFrameDelta - delta;
#ifdef COMPOSITOR_PERFORMANCE_WARNING
    mExpectedComposeStartTime = TimeStamp::Now() + delay;
#endif
    ScheduleTask(mCurrentCompositeTask, delay.ToMilliseconds());
  } else {
#ifdef COMPOSITOR_PERFORMANCE_WARNING
    mExpectedComposeStartTime = TimeStamp::Now();
#endif
    ScheduleTask(mCurrentCompositeTask, 0);
  }
}

void
CompositorParent::ScheduleComposition()
{
  MOZ_ASSERT(IsInCompositorThread());
  if (mPaused) {
    return;
  }

  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->SetNeedsComposite(true);
  } else {
    ScheduleSoftwareTimerComposition();
  }
}

void
CompositorParent::CompositeCallback(TimeStamp aScheduleTime)
{
  if (mCompositorVsyncObserver) {
    // Align OMTA to vsync time.
    // TODO: ensure it aligns with the refresh / start time of
    // animations
    mLastCompose = aScheduleTime;
  } else {
    mLastCompose = TimeStamp::Now();
  }

  mCurrentCompositeTask = nullptr;
  CompositeToTarget(nullptr);
}

// Go down the composite layer tree, setting properties to match their
// content-side counterparts.
/* static */ void
CompositorParent::SetShadowProperties(Layer* aLayer)
{
  if (Layer* maskLayer = aLayer->GetMaskLayer()) {
    SetShadowProperties(maskLayer);
  }

  // FIXME: Bug 717688 -- Do these updates in LayerTransactionParent::RecvUpdate.
  LayerComposite* layerComposite = aLayer->AsLayerComposite();
  // Set the layerComposite's base transform to the layer's base transform.
  layerComposite->SetShadowTransform(aLayer->GetBaseTransform());
  layerComposite->SetShadowTransformSetByAnimation(false);
  layerComposite->SetShadowVisibleRegion(aLayer->GetVisibleRegion());
  layerComposite->SetShadowClipRect(aLayer->GetClipRect());
  layerComposite->SetShadowOpacity(aLayer->GetOpacity());

  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    SetShadowProperties(child);
  }
}

void
CompositorParent::CompositeToTarget(DrawTarget* aTarget, const nsIntRect* aRect)
{
  profiler_tracing("Paint", "Composite", TRACING_INTERVAL_START);
  PROFILER_LABEL("CompositorParent", "Composite",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(IsInCompositorThread(),
             "Composite can only be called on the compositor thread");
  TimeStamp start = TimeStamp::Now();

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeDuration scheduleDelta = TimeStamp::Now() - mExpectedComposeStartTime;
  if (scheduleDelta > TimeDuration::FromMilliseconds(2) ||
      scheduleDelta < TimeDuration::FromMilliseconds(-2)) {
    printf_stderr("Compositor: Compose starting off schedule by %4.1f ms\n",
                  scheduleDelta.ToMilliseconds());
  }
#endif

  if (!CanComposite()) {
    DidComposite();
    return;
  }

  AutoResolveRefLayers resolve(mCompositionManager);

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

  TimeStamp time = mIsTesting ? mTestTime : mLastCompose;
  bool requestNextFrame = mCompositionManager->TransformShadowTree(time);
  if (requestNextFrame) {
    ScheduleComposition();
  }

  RenderTraceLayers(mLayerManager->GetRoot(), "0000");

#ifdef MOZ_DUMP_PAINTING
  if (gfxPrefs::DumpHostLayers()) {
    printf_stderr("Painting --- compositing layer tree:\n");
    mLayerManager->Dump();
  }
#endif
  mLayerManager->SetDebugOverlayWantsNextFrame(false);
  mLayerManager->EndEmptyTransaction();

  if (!aTarget) {
    DidComposite();
  }

  if (mLayerManager->DebugOverlayWantsNextFrame()) {
    ScheduleComposition();
  }

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeDuration executionTime = TimeStamp::Now() - mLastCompose;
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
  if (gfxPrefs::LayersCompositionFrameRate() == 0
    || mLayerManager->GetCompositor()->GetDiagnosticTypes() & DiagnosticTypes::FLASH_BORDERS) {
    // Special full-tilt composite mode for performance testing
    ScheduleComposition();
  }

  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::COMPOSITE_TIME, start);
  profiler_tracing("Paint", "Composite", TRACING_INTERVAL_END);
}

void
CompositorParent::ForceComposeToTarget(DrawTarget* aTarget, const nsIntRect* aRect)
{
  PROFILER_LABEL("CompositorParent", "ForceComposeToTarget",
    js::ProfileEntry::Category::GRAPHICS);

  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->OnForceComposeToTarget();
  }

  AutoRestore<bool> override(mOverrideComposeReadiness);
  mOverrideComposeReadiness = true;

  mLastCompose = TimeStamp::Now();
  CompositeToTarget(aTarget, aRect);
}

bool
CompositorParent::CanComposite()
{
  return mLayerManager &&
         mLayerManager->GetRoot() &&
         !mPaused;
}

void
CompositorParent::ScheduleRotationOnCompositorThread(const TargetConfig& aTargetConfig,
                                                     bool aIsFirstPaint)
{
  MOZ_ASSERT(IsInCompositorThread());

  if (!aIsFirstPaint &&
      !mCompositionManager->IsFirstPaint() &&
      mCompositionManager->RequiresReorientation(aTargetConfig.orientation())) {
    if (mForceCompositionTask != nullptr) {
      mForceCompositionTask->Cancel();
    }
    mForceCompositionTask = NewRunnableMethod(this, &CompositorParent::ForceComposition);
    ScheduleTask(mForceCompositionTask, gfxPrefs::OrientationSyncMillis());
  }
}

void
CompositorParent::ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                      const uint64_t& aTransactionId,
                                      const TargetConfig& aTargetConfig,
                                      const InfallibleTArray<PluginWindowData>& aUnused,
                                      bool aIsFirstPaint,
                                      bool aScheduleComposite,
                                      uint32_t aPaintSequenceNumber,
                                      bool aIsRepeatTransaction)
{
  ScheduleRotationOnCompositorThread(aTargetConfig, aIsFirstPaint);

  // Instruct the LayerManager to update its render bounds now. Since all the orientation
  // change, dimension change would be done at the stage, update the size here is free of
  // race condition.
  mLayerManager->UpdateRenderBounds(aTargetConfig.naturalBounds());
  mLayerManager->SetRegionToClear(aTargetConfig.clearRegion());

  mCompositionManager->Updated(aIsFirstPaint, aTargetConfig);
  Layer* root = aLayerTree->GetRoot();
  mLayerManager->SetRoot(root);

  if (mApzcTreeManager && !aIsRepeatTransaction) {
    AutoResolveRefLayers resolve(mCompositionManager);
    mApzcTreeManager->UpdateHitTestingTree(this, root, aIsFirstPaint,
        mRootLayerTreeID, aPaintSequenceNumber);
  }

#ifdef DEBUG
  if (aTransactionId <= mPendingTransaction) {
    // Logging added to help diagnose why we're triggering the assert below.
    // See bug 1145295
    printf_stderr("CRASH: aTransactionId %" PRIu64 " <= mPendingTransaction %" PRIu64 "\n",
      aTransactionId, mPendingTransaction);
  }
#endif
  MOZ_ASSERT(aTransactionId > mPendingTransaction);
  mPendingTransaction = aTransactionId;

  if (root) {
    SetShadowProperties(root);
  }
  if (aScheduleComposite) {
    ScheduleComposition();
    if (mPaused) {
      DidComposite();
    }
    // When testing we synchronously update the shadow tree with the animated
    // values to avoid race conditions when calling GetAnimationTransform etc.
    // (since the above SetShadowProperties will remove animation effects).
    if (mIsTesting) {
      ApplyAsyncProperties(aLayerTree);
    }
  }
  mLayerManager->NotifyShadowTreeTransaction();
}

void
CompositorParent::ForceComposite(LayerTransactionParent* aLayerTree)
{
  ScheduleComposition();
}

bool
CompositorParent::SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                    const TimeStamp& aTime)
{
  if (aTime.IsNull()) {
    return false;
  }

  mIsTesting = true;
  mTestTime = aTime;

  bool testComposite = mCompositionManager && (mCurrentCompositeTask ||
                                               (mCompositorVsyncObserver
                                               && mCompositorVsyncObserver->NeedsComposite()));

  // Update but only if we were already scheduled to animate
  if (testComposite) {
    AutoResolveRefLayers resolve(mCompositionManager);
    bool requestNextFrame = mCompositionManager->TransformShadowTree(aTime);
    if (!requestNextFrame) {
      CancelCurrentCompositeTask();
      // Pretend we composited in case someone is wating for this event.
      DidComposite();
    }
  }

  return true;
}

void
CompositorParent::LeaveTestMode(LayerTransactionParent* aLayerTree)
{
  mIsTesting = false;
}

void
CompositorParent::ApplyAsyncProperties(LayerTransactionParent* aLayerTree)
{
  // NOTE: This should only be used for testing. For example, when mIsTesting is
  // true or when called from test-only methods like
  // LayerTransactionParent::RecvGetAnimationTransform.

  // Synchronously update the layer tree, but only if a composite was already
  // scehduled.
  if (aLayerTree->GetRoot() &&
      (mCurrentCompositeTask ||
       (mCompositorVsyncObserver &&
        mCompositorVsyncObserver->NeedsComposite()))) {
    AutoResolveRefLayers resolve(mCompositionManager);
    TimeStamp time = mIsTesting ? mTestTime : mLastCompose;
    bool requestNextFrame =
      mCompositionManager->TransformShadowTree(time);
    if (!requestNextFrame) {
      CancelCurrentCompositeTask();
      // Pretend we composited in case someone is waiting for this event.
      DidComposite();
    }
  }
}

bool
CompositorParent::RecvRequestOverfill()
{
  uint32_t overfillRatio = mCompositor->GetFillRatio();
  unused << SendOverfill(overfillRatio);
  return true;
}

void
CompositorParent::GetAPZTestData(const LayerTransactionParent* aLayerTree,
                                 APZTestData* aOutData)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  *aOutData = sIndirectLayerTrees[mRootLayerTreeID].mApzTestData;
}

class NotifyAPZConfirmedTargetTask : public Task
{
public:
  explicit NotifyAPZConfirmedTargetTask(const nsRefPtr<APZCTreeManager>& aAPZCTM,
                                        const uint64_t& aInputBlockId,
                                        const nsTArray<ScrollableLayerGuid>& aTargets)
   : mAPZCTM(aAPZCTM),
     mInputBlockId(aInputBlockId),
     mTargets(aTargets)
  {
  }

  virtual void Run() override {
    mAPZCTM->SetTargetAPZC(mInputBlockId, mTargets);
  }

private:
  nsRefPtr<APZCTreeManager> mAPZCTM;
  uint64_t mInputBlockId;
  nsTArray<ScrollableLayerGuid> mTargets;
};

void
CompositorParent::SetConfirmedTargetAPZC(const LayerTransactionParent* aLayerTree,
                                         const uint64_t& aInputBlockId,
                                         const nsTArray<ScrollableLayerGuid>& aTargets)
{
  if (!mApzcTreeManager) {
    return;
  }
  APZThreadUtils::RunOnControllerThread(new NotifyAPZConfirmedTargetTask(
    mApzcTreeManager, aInputBlockId, aTargets));
}

void
CompositorParent::InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints)
{
  NS_ASSERTION(!mLayerManager, "Already initialised mLayerManager");
  NS_ASSERTION(!mCompositor,   "Already initialised mCompositor");

  for (size_t i = 0; i < aBackendHints.Length(); ++i) {
    RefPtr<Compositor> compositor;
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
      compositor = new CompositorOGL(mWidget,
                                     mEGLSurfaceSize.width,
                                     mEGLSurfaceSize.height,
                                     mUseExternalSurfaceSize);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_BASIC) {
#ifdef MOZ_WIDGET_GTK
      if (gfxPlatformGtk::GetPlatform()->UseXRender()) {
        compositor = new X11BasicCompositor(mWidget);
      } else
#endif
      {
        compositor = new BasicCompositor(mWidget);
      }
#ifdef XP_WIN
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11) {
      compositor = new CompositorD3D11(mWidget);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D9) {
      compositor = new CompositorD3D9(this, mWidget);
#endif
    }

    if (!compositor) {
      // We passed a backend hint for which we can't create a compositor.
      // For example, we sometime pass LayersBackend::LAYERS_NONE as filler in aBackendHints.
      continue;
    }

    compositor->SetCompositorID(mCompositorID);
    RefPtr<LayerManagerComposite> layerManager = new LayerManagerComposite(compositor);

    if (layerManager->Initialize()) {
      mLayerManager = layerManager;
      MOZ_ASSERT(compositor);
      mCompositor = compositor;
      MonitorAutoLock lock(*sIndirectLayerTreesLock);
      sIndirectLayerTrees[mRootLayerTreeID].mLayerManager = layerManager;
      return;
    }
  }
}

PLayerTransactionParent*
CompositorParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                               const uint64_t& aId,
                                               TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                               bool *aSuccess)
{
  MOZ_ASSERT(aId == 0);

  // mWidget doesn't belong to the compositor thread, so it should be set to
  // nullptr before returning from this method, to avoid accessing it elsewhere.
  nsIntRect rect;
  mWidget->GetClientBounds(rect);
  InitializeLayerManager(aBackendHints);
  mWidget = nullptr;

  if (!mLayerManager) {
    NS_WARNING("Failed to initialise Compositor");
    *aSuccess = false;
    LayerTransactionParent* p = new LayerTransactionParent(nullptr, this, 0);
    p->AddIPDLReference();
    return p;
  }

  mCompositionManager = new AsyncCompositionManager(mLayerManager);
  *aSuccess = true;

  *aTextureFactoryIdentifier = mCompositor->GetTextureFactoryIdentifier();
  LayerTransactionParent* p = new LayerTransactionParent(mLayerManager, this, 0);
  p->AddIPDLReference();
  return p;
}

bool
CompositorParent::DeallocPLayerTransactionParent(PLayerTransactionParent* actor)
{
  static_cast<LayerTransactionParent*>(actor)->ReleaseIPDLReference();
  return true;
}

CompositorParent* CompositorParent::GetCompositor(uint64_t id)
{
  CompositorMap::iterator it = sCompositorMap->find(id);
  return it != sCompositorMap->end() ? it->second : nullptr;
}

void CompositorParent::AddCompositor(CompositorParent* compositor, uint64_t* outID)
{
  static uint64_t sNextID = 1;

  ++sNextID;
  (*sCompositorMap)[sNextID] = compositor;
  *outID = sNextID;
}

CompositorParent* CompositorParent::RemoveCompositor(uint64_t id)
{
  CompositorMap::iterator it = sCompositorMap->find(id);
  if (it == sCompositorMap->end()) {
    return nullptr;
  }
  CompositorParent *retval = it->second;
  sCompositorMap->erase(it);
  return retval;
}

bool
CompositorParent::RecvNotifyChildCreated(const uint64_t& child)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  NotifyChildCreated(child);
  return true;
}

void
CompositorParent::NotifyChildCreated(const uint64_t& aChild)
{
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  sIndirectLayerTrees[aChild].mParent = this;
  sIndirectLayerTrees[aChild].mLayerManager = mLayerManager;
}

bool
CompositorParent::RecvAdoptChild(const uint64_t& child)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  NotifyChildCreated(child);
  if (sIndirectLayerTrees[child].mLayerTree) {
    sIndirectLayerTrees[child].mLayerTree->mLayerManager = mLayerManager;
  }
  if (sIndirectLayerTrees[child].mRoot) {
    sIndirectLayerTrees[child].mRoot->AsLayerComposite()->SetLayerManager(mLayerManager);
  }
  return true;
}

/*static*/ uint64_t
CompositorParent::AllocateLayerTreeId()
{
  MOZ_ASSERT(CompositorLoop());
  MOZ_ASSERT(NS_IsMainThread());
  static uint64_t ids = 0;
  return ++ids;
}

static void
EraseLayerState(uint64_t aId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees.erase(aId);
}

/*static*/ void
CompositorParent::DeallocateLayerTreeId(uint64_t aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Here main thread notifies compositor to remove an element from
  // sIndirectLayerTrees. This removed element might be queried soon.
  // Checking the elements of sIndirectLayerTrees exist or not before using.
  MOZ_ASSERT(CompositorLoop());
  CompositorLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&EraseLayerState, aId));
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

ScopedLayerTreeRegistration::ScopedLayerTreeRegistration(uint64_t aLayersId,
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
CompositorParent::SetControllerForLayerTree(uint64_t aLayersId,
                                            GeckoContentController* aController)
{
  // This ref is adopted by UpdateControllerForLayersId().
  aController->AddRef();
  CompositorLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&UpdateControllerForLayersId,
                                                 aLayersId,
                                                 aController));
}

/*static*/ APZCTreeManager*
CompositorParent::GetAPZCTreeManager(uint64_t aLayersId)
{
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(aLayersId);
  if (state && state->mParent) {
    return state->mParent->mApzcTreeManager;
  }
  return nullptr;
}

float
CompositorParent::ComputeRenderIntegrity()
{
  if (mLayerManager) {
    return mLayerManager->ComputeRenderIntegrity();
  }

  return 1.0f;
}

static void
InsertVsyncProfilerMarker(TimeStamp aVsyncTimestamp)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  VsyncPayload* payload = new VsyncPayload(aVsyncTimestamp);
  PROFILER_MARKER_PAYLOAD("VsyncTimestamp", payload);
#endif
}

/*static */ void
CompositorParent::PostInsertVsyncProfilerMarker(TimeStamp aVsyncTimestamp)
{
  // Called in the vsync thread
  if (profiler_is_active() && sCompositorThreadHolder) {
    CompositorLoop()->PostTask(FROM_HERE,
      NewRunnableFunction(InsertVsyncProfilerMarker, aVsyncTimestamp));
  }
}

/* static */ void
CompositorParent::RequestNotifyLayerTreeReady(uint64_t aLayersId, CompositorUpdateObserver* aObserver)
{
  EnsureLayerTreeMapReady();
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aLayersId].mLayerTreeReadyObserver = aObserver;
}

/* static */ void
CompositorParent::RequestNotifyLayerTreeCleared(uint64_t aLayersId, CompositorUpdateObserver* aObserver)
{
  EnsureLayerTreeMapReady();
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aLayersId].mLayerTreeClearedObserver = aObserver;
}

/**
 * This class handles layer updates pushed directly from child
 * processes to the compositor thread.  It's associated with a
 * CompositorParent on the compositor thread.  While it uses the
 * PCompositor protocol to manage these updates, it doesn't actually
 * drive compositing itself.  For that it hands off work to the
 * CompositorParent it's associated with.
 */
class CrossProcessCompositorParent final : public PCompositorParent,
                                           public ShadowLayersManager
{
  friend class CompositorParent;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(CrossProcessCompositorParent)
public:
  explicit CrossProcessCompositorParent(Transport* aTransport)
    : mTransport(aTransport)
    , mCompositorThreadHolder(sCompositorThreadHolder)
    , mNotifyAfterRemotePaint(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
    gfxPlatform::GetPlatform()->ComputeTileSize();
  }

  // IToplevelProtocol::CloneToplevel()
  virtual IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // FIXME/bug 774388: work out what shutdown protocol we need.
  virtual bool RecvRequestOverfill() override { return true; }
  virtual bool RecvWillStop() override { return true; }
  virtual bool RecvStop() override { return true; }
  virtual bool RecvPause() override { return true; }
  virtual bool RecvResume() override { return true; }
  virtual bool RecvNotifyChildCreated(const uint64_t& child) override;
  virtual bool RecvAdoptChild(const uint64_t& child) override { return false; }
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const nsIntRect& aRect) override
  { return true; }
  virtual bool RecvFlushRendering() override { return true; }
  virtual bool RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) override { return true; }
  virtual bool RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) override { return true; }
  virtual bool RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) override  { return true; }
  virtual bool RecvGetTileSize(int32_t* aWidth, int32_t* aHeight) override
  {
    *aWidth = gfxPlatform::GetPlatform()->GetTileWidth();
    *aHeight = gfxPlatform::GetPlatform()->GetTileHeight();
    return true;
  }


  /**
   * Tells this CompositorParent to send a message when the compositor has received the transaction.
   */
  virtual bool RecvRequestNotifyAfterRemotePaint() override;

  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool *aSuccess) override;

  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) override;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& aTargetConfig,
                                   const InfallibleTArray<PluginWindowData>& aPlugins,
                                   bool aIsFirstPaint,
                                   bool aScheduleComposite,
                                   uint32_t aPaintSequenceNumber,
                                   bool aIsRepeatTransaction) override;
  virtual void ForceComposite(LayerTransactionParent* aLayerTree) override;
  virtual void NotifyClearCachedResources(LayerTransactionParent* aLayerTree) override;
  virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                 const TimeStamp& aTime) override;
  virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) override;
  virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                              APZTestData* aOutData) override;
  virtual void SetConfirmedTargetAPZC(const LayerTransactionParent* aLayerTree,
                                      const uint64_t& aInputBlockId,
                                      const nsTArray<ScrollableLayerGuid>& aTargets) override;

  virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aParent) override;

  void DidComposite(uint64_t aId);

private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~CrossProcessCompositorParent();

  void DeferredDestroy();

  // There can be many CPCPs, and IPDL-generated code doesn't hold a
  // reference to top-level actors.  So we hold a reference to
  // ourself.  This is released (deferred) in ActorDestroy().
  nsRefPtr<CrossProcessCompositorParent> mSelfRef;
  Transport* mTransport;

  nsRefPtr<CompositorThreadHolder> mCompositorThreadHolder;
  // If true, we should send a RemotePaintIsReady message when the layer transaction
  // is received
  bool mNotifyAfterRemotePaint;
};

void
CompositorParent::DidComposite()
{
  if (mPendingTransaction) {
    unused << SendDidComposite(0, mPendingTransaction);
    mPendingTransaction = 0;
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  for (LayerTreeMap::iterator it = sIndirectLayerTrees.begin();
       it != sIndirectLayerTrees.end(); it++) {
    LayerTreeState* lts = &it->second;
    if (lts->mParent == this && lts->mCrossProcessParent) {
      static_cast<CrossProcessCompositorParent*>(lts->mCrossProcessParent)->DidComposite(it->first);
    }
  }
}

static void
OpenCompositor(CrossProcessCompositorParent* aCompositor,
               Transport* aTransport, ProcessId aOtherPid,
               MessageLoop* aIOLoop)
{
  DebugOnly<bool> ok = aCompositor->Open(aTransport, aOtherPid, aIOLoop);
  MOZ_ASSERT(ok);
}

/*static*/ PCompositorParent*
CompositorParent::Create(Transport* aTransport, ProcessId aOtherPid)
{
  gfxPlatform::InitLayersIPC();

  nsRefPtr<CrossProcessCompositorParent> cpcp =
    new CrossProcessCompositorParent(aTransport);

  cpcp->mSelfRef = cpcp;
  CompositorLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(OpenCompositor, cpcp.get(),
                        aTransport, aOtherPid, XRE_GetIOMessageLoop()));
  // The return value is just compared to null for success checking,
  // we're not sharing a ref.
  return cpcp.get();
}

IToplevelProtocol*
CompositorParent::CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                                base::ProcessHandle aPeerProcess,
                                mozilla::ipc::ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == (unsigned)GetProtocolId()) {
      Transport* transport = OpenDescriptor(aFds[i].fd(),
                                            Transport::MODE_SERVER);
      PCompositorParent* compositor = Create(transport, base::GetProcId(aPeerProcess));
      compositor->CloneManagees(this, aCtx);
      compositor->IToplevelProtocol::SetTransport(transport);
      return compositor;
    }
  }
  return nullptr;
}

static void
UpdateIndirectTree(uint64_t aId, Layer* aRoot, const TargetConfig& aTargetConfig)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  sIndirectLayerTrees[aId].mRoot = aRoot;
  sIndirectLayerTrees[aId].mTargetConfig = aTargetConfig;
}

/* static */ CompositorParent::LayerTreeState*
CompositorParent::GetIndirectShadowTree(uint64_t aId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  LayerTreeMap::iterator cit = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() == cit) {
    return nullptr;
  }
  return &cit->second;
}

bool
CrossProcessCompositorParent::RecvRequestNotifyAfterRemotePaint()
{
  mNotifyAfterRemotePaint = true;
  return true;
}

void
CrossProcessCompositorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &CrossProcessCompositorParent::DeferredDestroy));
}

PLayerTransactionParent*
CrossProcessCompositorParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>&,
                                                           const uint64_t& aId,
                                                           TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                           bool *aSuccess)
{
  MOZ_ASSERT(aId != 0);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);

  CompositorParent::LayerTreeState* state = nullptr;
  LayerTreeMap::iterator itr = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() != itr) {
    state = &itr->second;
  }

  if (state && state->mLayerManager) {
    state->mCrossProcessParent = this;
    LayerManagerComposite* lm = state->mLayerManager;
    *aTextureFactoryIdentifier = lm->GetCompositor()->GetTextureFactoryIdentifier();
    *aSuccess = true;
    LayerTransactionParent* p = new LayerTransactionParent(lm, this, aId);
    p->AddIPDLReference();
    sIndirectLayerTrees[aId].mLayerTree = p;
    return p;
  }

  NS_WARNING("Created child without a matching parent?");
  // XXX: should be false, but that causes us to fail some tests on Mac w/ OMTC.
  // Bug 900745. change *aSuccess to false to see test failures.
  *aSuccess = true;
  LayerTransactionParent* p = new LayerTransactionParent(nullptr, this, aId);
  p->AddIPDLReference();
  return p;
}

bool
CrossProcessCompositorParent::DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers)
{
  LayerTransactionParent* slp = static_cast<LayerTransactionParent*>(aLayers);
  EraseLayerState(slp->GetId());
  static_cast<LayerTransactionParent*>(aLayers)->ReleaseIPDLReference();
  return true;
}

bool
CrossProcessCompositorParent::RecvNotifyChildCreated(const uint64_t& child)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  for (LayerTreeMap::iterator it = sIndirectLayerTrees.begin();
       it != sIndirectLayerTrees.end(); it++) {
    CompositorParent::LayerTreeState* lts = &it->second;
    if (lts->mParent && lts->mCrossProcessParent == this) {
      lts->mParent->NotifyChildCreated(child);
      return true;
    }
  }
  return false;
}

void
CrossProcessCompositorParent::ShadowLayersUpdated(
  LayerTransactionParent* aLayerTree,
  const uint64_t& aTransactionId,
  const TargetConfig& aTargetConfig,
  const InfallibleTArray<PluginWindowData>& aPlugins,
  bool aIsFirstPaint,
  bool aScheduleComposite,
  uint32_t aPaintSequenceNumber,
  bool aIsRepeatTransaction)
{
  uint64_t id = aLayerTree->GetId();

  MOZ_ASSERT(id != 0);

  CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }
  MOZ_ASSERT(state->mParent);
  state->mParent->ScheduleRotationOnCompositorThread(aTargetConfig, aIsFirstPaint);

  Layer* shadowRoot = aLayerTree->GetRoot();
  if (shadowRoot) {
    CompositorParent::SetShadowProperties(shadowRoot);
  }
  UpdateIndirectTree(id, shadowRoot, aTargetConfig);

  // Cache the plugin data for this remote layer tree
  state->mPluginData = aPlugins;
  state->mUpdatedPluginDataAvailable = true;

  state->mParent->NotifyShadowTreeTransaction(id, aIsFirstPaint, aScheduleComposite,
      aPaintSequenceNumber, aIsRepeatTransaction);

  // Send the 'remote paint ready' message to the content thread if it has already asked.
  if(mNotifyAfterRemotePaint)  {
    unused << SendRemotePaintIsReady();
    mNotifyAfterRemotePaint = false;
  }

  if (state->mLayerTreeReadyObserver) {
    nsRefPtr<CompositorUpdateObserver> observer = state->mLayerTreeReadyObserver;
    state->mLayerTreeReadyObserver = nullptr;
    observer->ObserveUpdate(id, true);
  }

  aLayerTree->SetPendingTransactionId(aTransactionId);
}

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
// Sends plugin window state changes to the main thread
static void
UpdatePluginWindowState(uint64_t aId)
{
  CompositorParent::LayerTreeState& lts = sIndirectLayerTrees[aId];
  if (!lts.mPluginData.Length() && !lts.mUpdatedPluginDataAvailable) {
    return;
  }

  bool shouldComposePlugin = !!lts.mRoot &&
                             !!lts.mRoot->GetParent();

  bool shouldHidePlugin = (!lts.mRoot ||
                           !lts.mRoot->GetParent()) &&
                          !lts.mUpdatedPluginDataAvailable;
  if (shouldComposePlugin) {
    if (!lts.mPluginData.Length()) {
      // We will pass through here in cases where the previous shadow layer
      // tree contained visible plugins and the new tree does not. All we need
      // to do here is hide the plugins for the old tree, so don't waste time
      // calculating clipping.
      nsTArray<uintptr_t> aVisibleIdList;
      unused << lts.mParent->SendUpdatePluginVisibility(aVisibleIdList);
      lts.mUpdatedPluginDataAvailable = false;
      return;
    }

    // Retrieve the offset and visible region of the layer that hosts
    // the plugins, CompositorChild needs these in calculating proper
    // plugin clipping.
    LayerTransactionParent* layerTree = lts.mLayerTree;
    Layer* contentRoot = layerTree->GetRoot();
    if (contentRoot) {
      nsIntPoint offset;
      nsIntRegion visibleRegion;
      if (contentRoot->GetVisibleRegionRelativeToRootLayer(visibleRegion,
                                                           &offset)) {
        unused <<
          lts.mParent->SendUpdatePluginConfigurations(offset, visibleRegion,
                                                      lts.mPluginData);
        lts.mUpdatedPluginDataAvailable = false;
      } else {
        shouldHidePlugin = true;
      }
    }
  }

  // Hide all plugins, this remote layer tree is no longer active
  if (shouldHidePlugin) {
    // hide all the plugins
    for (uint32_t pluginsIdx = 0; pluginsIdx < lts.mPluginData.Length();
         pluginsIdx++) {
      lts.mPluginData[pluginsIdx].visible() = false;
    }
    nsIntPoint offset;
    nsIntRegion region;
    unused << lts.mParent->SendUpdatePluginConfigurations(offset,
                                                          region,
                                                          lts.mPluginData);
    // Clear because there's no recovering from this until we receive
    // new shadow layer plugin data in ShadowLayersUpdated.
    lts.mPluginData.Clear();
  }
}
#endif // #if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)

void
CrossProcessCompositorParent::DidComposite(uint64_t aId)
{
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  LayerTransactionParent *layerTree = sIndirectLayerTrees[aId].mLayerTree;
  if (layerTree && layerTree->GetPendingTransactionId()) {
    unused << SendDidComposite(aId, layerTree->GetPendingTransactionId());
    layerTree->SetPendingTransactionId(0);
  }
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  UpdatePluginWindowState(aId);
#endif
}

void
CrossProcessCompositorParent::ForceComposite(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  CompositorParent* parent;
  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    parent = sIndirectLayerTrees[id].mParent;
  }
  if (parent) {
    parent->ForceComposite(aLayerTree);
  }
}

void
CrossProcessCompositorParent::NotifyClearCachedResources(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);

  nsRefPtr<CompositorUpdateObserver> observer;
  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    observer = sIndirectLayerTrees[id].mLayerTreeClearedObserver;
    sIndirectLayerTrees[id].mLayerTreeClearedObserver = nullptr;
  }
  if (observer) {
    observer->ObserveUpdate(id, false);
  }
}

bool
CrossProcessCompositorParent::SetTestSampleTime(
  LayerTransactionParent* aLayerTree, const TimeStamp& aTime)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(id);
  if (!state) {
    return false;
  }

  MOZ_ASSERT(state->mParent);
  return state->mParent->SetTestSampleTime(aLayerTree, aTime);
}

void
CrossProcessCompositorParent::LeaveTestMode(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->LeaveTestMode(aLayerTree);
}

void
CrossProcessCompositorParent::GetAPZTestData(const LayerTransactionParent* aLayerTree,
                                             APZTestData* aOutData)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  *aOutData = sIndirectLayerTrees[id].mApzTestData;
}

void
CrossProcessCompositorParent::SetConfirmedTargetAPZC(const LayerTransactionParent* aLayerTree,
                                                     const uint64_t& aInputBlockId,
                                                     const nsTArray<ScrollableLayerGuid>& aTargets)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  CompositorParent* parent = nullptr;
  {
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(id);
    if (!state || !state->mParent) {
      return;
    }
    parent = state->mParent;
  }
  parent->SetConfirmedTargetAPZC(aLayerTree, aInputBlockId, aTargets);
}

AsyncCompositionManager*
CrossProcessCompositorParent::GetCompositionManager(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(id);
  if (!state) {
    return nullptr;
  }

  MOZ_ASSERT(state->mParent);
  return state->mParent->GetCompositionManager(aLayerTree);
}

void
CrossProcessCompositorParent::DeferredDestroy()
{
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

CrossProcessCompositorParent::~CrossProcessCompositorParent()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetIOMessageLoop());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new DeleteTask<Transport>(mTransport));
}

IToplevelProtocol*
CrossProcessCompositorParent::CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                                            base::ProcessHandle aPeerProcess,
                                            mozilla::ipc::ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == (unsigned)GetProtocolId()) {
      Transport* transport = OpenDescriptor(aFds[i].fd(),
                                            Transport::MODE_SERVER);
      PCompositorParent* compositor =
        CompositorParent::Create(transport, base::GetProcId(aPeerProcess));
      compositor->CloneManagees(this, aCtx);
      compositor->IToplevelProtocol::SetTransport(transport);
      return compositor;
    }
  }
  return nullptr;
}

} // namespace layers
} // namespace mozilla

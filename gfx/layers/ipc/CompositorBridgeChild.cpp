/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include <stddef.h>                     // for size_t
#include "ClientLayerManager.h"         // for ClientLayerManager
#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "gfxPrefs.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/layers/TextureClient.h"// for TextureClient
#include "mozilla/layers/TextureClientPool.h"// for TextureClientPool
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsIObserver.h"                // for nsIObserver
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop, etc
#include "FrameLayerBuilder.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/Unused.h"
#include "mozilla/DebugOnly.h"
#if defined(XP_WIN)
#include "WinUtils.h"
#endif
#include "mozilla/widget/CompositorWidget.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
# include "mozilla/widget/CompositorWidgetChild.h"
#endif
#include "VsyncSource.h"

using mozilla::layers::LayerTransactionChild;
using mozilla::dom::TabChildBase;
using mozilla::Unused;
using mozilla::gfx::GPUProcessManager;

namespace mozilla {
namespace layers {

static StaticRefPtr<CompositorBridgeChild> sCompositorBridge;

Atomic<int32_t> CompositableForwarder::sSerialCounter(0);

CompositorBridgeChild::CompositorBridgeChild(ClientLayerManager *aLayerManager)
  : mLayerManager(aLayerManager)
  , mCanSend(false)
  , mFwdTransactionId(0)
  , mMessageLoop(MessageLoop::current())
  , mSectionAllocator(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorBridgeChild::~CompositorBridgeChild()
{
  if (mCanSend) {
    gfxCriticalError() << "CompositorBridgeChild was not deinitialized";
  }
}

bool
CompositorBridgeChild::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

static void DeferredDestroyCompositor(RefPtr<CompositorBridgeParent> aCompositorBridgeParent,
                                      RefPtr<CompositorBridgeChild> aCompositorBridgeChild)
{
  aCompositorBridgeChild->Close();

  if (sCompositorBridge == aCompositorBridgeChild) {
    sCompositorBridge = nullptr;
  }
}

void
CompositorBridgeChild::Destroy()
{
  // This must not be called from the destructor!
  mTexturesWaitingRecycled.Clear();

  if (!mCanSend) {
    return;
  }

  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Destroy();
  }

  if (mSectionAllocator) {
    delete mSectionAllocator;
    mSectionAllocator = nullptr;
  }

  // Destroying the layer manager may cause all sorts of things to happen, so
  // let's make sure there is still a reference to keep this alive whatever
  // happens.
  RefPtr<CompositorBridgeChild> selfRef = this;

  if (mLayerManager) {
    mLayerManager->Destroy();
    mLayerManager = nullptr;
  }

  AutoTArray<PLayerTransactionChild*, 16> transactions;
  ManagedPLayerTransactionChild(transactions);
  for (int i = transactions.Length() - 1; i >= 0; --i) {
    RefPtr<LayerTransactionChild> layers =
      static_cast<LayerTransactionChild*>(transactions[i]);
    layers->Destroy();
  }

  const ManagedContainer<PTextureChild>& textures = ManagedPTextureChild();
  for (auto iter = textures.ConstIter(); !iter.Done(); iter.Next()) {
    RefPtr<TextureClient> texture = TextureClient::AsTextureClient(iter.Get()->GetKey());

    if (texture) {
      texture->Destroy();
    }
  }

  SendWillClose();
  mCanSend = false;

  // We no longer care about unexpected shutdowns, in the remote process case.
  mProcessToken = 0;

  // The call just made to SendWillClose can result in IPC from the
  // CompositorBridgeParent to the CompositorBridgeChild (e.g. caused by the destruction
  // of shared memory). We need to ensure this gets processed by the
  // CompositorBridgeChild before it gets destroyed. It suffices to ensure that
  // events already in the MessageLoop get processed before the
  // CompositorBridgeChild is destroyed, so we add a task to the MessageLoop to
  // handle compositor desctruction.

  // From now on we can't send any message message.
  MessageLoop::current()->PostTask(
             NewRunnableFunction(DeferredDestroyCompositor, mCompositorBridgeParent, selfRef));
}

// static
void
CompositorBridgeChild::ShutDown()
{
  if (sCompositorBridge) {
    sCompositorBridge->Destroy();
    do {
      NS_ProcessNextEvent(nullptr, true);
    } while (sCompositorBridge);
  }
}

bool
CompositorBridgeChild::LookupCompositorFrameMetrics(const FrameMetrics::ViewID aId,
                                                    FrameMetrics& aFrame)
{
  SharedFrameMetricsData* data = mFrameMetricsTable.Get(aId);
  if (data) {
    data->CopyFrameMetrics(&aFrame);
    return true;
  }
  return false;
}

/* static */ bool
CompositorBridgeChild::InitForContent(Endpoint<PCompositorBridgeChild>&& aEndpoint)
{
  // There's only one compositor per child process.
  MOZ_ASSERT(!sCompositorBridge);

  RefPtr<CompositorBridgeChild> child(new CompositorBridgeChild(nullptr));
  if (!aEndpoint.Bind(child)) {
    NS_RUNTIMEABORT("Couldn't Open() Compositor channel.");
    return false;
  }

  child->mCanSend = true;

  // We release this ref in DeferredDestroyCompositor.
  sCompositorBridge = child;
  return true;
}

CompositorBridgeParent*
CompositorBridgeChild::InitSameProcess(widget::CompositorWidget* aWidget,
                                       const uint64_t& aLayerTreeId,
                                       CSSToLayoutDeviceScale aScale,
                                       bool aUseAPZ,
                                       bool aUseExternalSurface,
                                       const gfx::IntSize& aSurfaceSize)
{
  TimeDuration vsyncRate =
    gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay().GetVsyncRate();

  mCompositorBridgeParent =
    new CompositorBridgeParent(aScale, vsyncRate, aUseExternalSurface, aSurfaceSize);

  mCanSend = Open(mCompositorBridgeParent->GetIPCChannel(),
                  CompositorThreadHolder::Loop(),
                  ipc::ChildSide);
  MOZ_RELEASE_ASSERT(mCanSend);

  mCompositorBridgeParent->InitSameProcess(aWidget, aLayerTreeId, aUseAPZ);
  return mCompositorBridgeParent;
}

/* static */ RefPtr<CompositorBridgeChild>
CompositorBridgeChild::CreateRemote(const uint64_t& aProcessToken,
                                    ClientLayerManager* aLayerManager,
                                    Endpoint<PCompositorBridgeChild>&& aEndpoint)
{
  RefPtr<CompositorBridgeChild> child = new CompositorBridgeChild(aLayerManager);
  if (!aEndpoint.Bind(child)) {
    return nullptr;
  }

  child->mCanSend = true;
  child->mProcessToken = aProcessToken;
  return child;
}

/*static*/ CompositorBridgeChild*
CompositorBridgeChild::Get()
{
  // This is only expected to be used in child processes.
  MOZ_ASSERT(!XRE_IsParentProcess());
  return sCompositorBridge;
}

// static
bool
CompositorBridgeChild::ChildProcessHasCompositorBridge()
{
  return sCompositorBridge != nullptr;
}

PLayerTransactionChild*
CompositorBridgeChild::AllocPLayerTransactionChild(const nsTArray<LayersBackend>& aBackendHints,
                                                   const uint64_t& aId,
                                                   TextureFactoryIdentifier*,
                                                   bool*)
{
  LayerTransactionChild* c = new LayerTransactionChild(aId);
  c->AddIPDLReference();
  return c;
}

bool
CompositorBridgeChild::DeallocPLayerTransactionChild(PLayerTransactionChild* actor)
{
  uint64_t childId = static_cast<LayerTransactionChild*>(actor)->GetId();

  for (auto iter = mFrameMetricsTable.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<SharedFrameMetricsData>& data = iter.Data();
    if (data->GetLayersId() == childId) {
      iter.Remove();
    }
  }
  static_cast<LayerTransactionChild*>(actor)->ReleaseIPDLReference();
  return true;
}

bool
CompositorBridgeChild::RecvInvalidateLayers(const uint64_t& aLayersId)
{
  if (mLayerManager) {
    MOZ_ASSERT(aLayersId == 0);
    FrameLayerBuilder::InvalidateAllLayers(mLayerManager);
  } else if (aLayersId != 0) {
    if (dom::TabChild* child = dom::TabChild::GetFrom(aLayersId)) {
      child->InvalidateLayers();
    }
  }
  return true;
}

bool
CompositorBridgeChild::RecvCompositorUpdated(const uint64_t& aLayersId,
                                             const TextureFactoryIdentifier& aNewIdentifier)
{
  if (mLayerManager) {
    // This case is handled directly by nsBaseWidget.
    MOZ_ASSERT(aLayersId == 0);
  } else if (aLayersId != 0) {
    if (dom::TabChild* child = dom::TabChild::GetFrom(aLayersId)) {
      child->CompositorUpdated(aNewIdentifier);
    }
    if (!mCanSend) {
      return true;
    }
    SendAcknowledgeCompositorUpdate(aLayersId);
  }
  return true;
}

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
static void CalculatePluginClip(const LayoutDeviceIntRect& aBounds,
                                const nsTArray<LayoutDeviceIntRect>& aPluginClipRects,
                                const LayoutDeviceIntPoint& aContentOffset,
                                const LayoutDeviceIntRegion& aParentLayerVisibleRegion,
                                nsTArray<LayoutDeviceIntRect>& aResult,
                                LayoutDeviceIntRect& aVisibleBounds,
                                bool& aPluginIsVisible)
{
  aPluginIsVisible = true;
  LayoutDeviceIntRegion contentVisibleRegion;
  // aPluginClipRects (plugin widget origin) - contains *visible* rects
  for (uint32_t idx = 0; idx < aPluginClipRects.Length(); idx++) {
    LayoutDeviceIntRect rect = aPluginClipRects[idx];
    // shift to content origin
    rect.MoveBy(aBounds.x, aBounds.y);
    // accumulate visible rects
    contentVisibleRegion.OrWith(rect);
  }
  // apply layers clip (window origin)
  LayoutDeviceIntRegion region = aParentLayerVisibleRegion;
  region.MoveBy(-aContentOffset.x, -aContentOffset.y);
  contentVisibleRegion.AndWith(region);
  if (contentVisibleRegion.IsEmpty()) {
    aPluginIsVisible = false;
    return;
  }
  // shift to plugin widget origin
  contentVisibleRegion.MoveBy(-aBounds.x, -aBounds.y);
  for (auto iter = contentVisibleRegion.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect& rect = iter.Get();
    aResult.AppendElement(rect);
    aVisibleBounds.UnionRect(aVisibleBounds, rect);
  }
}
#endif

bool
CompositorBridgeChild::RecvUpdatePluginConfigurations(const LayoutDeviceIntPoint& aContentOffset,
                                                      const LayoutDeviceIntRegion& aParentLayerVisibleRegion,
                                                      nsTArray<PluginWindowData>&& aPlugins)
{
#if !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
  NS_NOTREACHED("CompositorBridgeChild::RecvUpdatePluginConfigurations calls "
                "unexpected on this platform.");
  return false;
#else
  // Now that we are on the main thread, update plugin widget config.
  // This should happen a little before we paint to the screen assuming
  // the main thread is running freely.
  DebugOnly<nsresult> rv;
  MOZ_ASSERT(NS_IsMainThread());

  // Tracks visible plugins we update, so we can hide any plugins we don't.
  nsTArray<uintptr_t> visiblePluginIds;
  nsIWidget* parent = nullptr;
  for (uint32_t pluginsIdx = 0; pluginsIdx < aPlugins.Length(); pluginsIdx++) {
    nsIWidget* widget =
      nsIWidget::LookupRegisteredPluginWindow(aPlugins[pluginsIdx].windowId());
    if (!widget) {
      NS_WARNING("Unexpected, plugin id not found!");
      continue;
    }
    if (!parent) {
      parent = widget->GetParent();
    }
    bool isVisible = aPlugins[pluginsIdx].visible();
    if (widget && !widget->Destroyed()) {
      LayoutDeviceIntRect bounds;
      LayoutDeviceIntRect visibleBounds;
      // If the plugin is visible update it's geometry.
      if (isVisible) {
        // Set bounds (content origin)
        bounds = aPlugins[pluginsIdx].bounds();
        nsTArray<LayoutDeviceIntRect> rectsOut;
        // This call may change the value of isVisible
        CalculatePluginClip(bounds, aPlugins[pluginsIdx].clip(),
                            aContentOffset,
                            aParentLayerVisibleRegion,
                            rectsOut, visibleBounds, isVisible);
        // content clipping region (widget origin)
        rv = widget->SetWindowClipRegion(rectsOut, false);
        NS_ASSERTION(NS_SUCCEEDED(rv), "widget call failure");
        // This will trigger a browser window paint event for areas uncovered
        // by a child window move, and will call invalidate on the plugin
        // parent window which the browser owns. The latter gets picked up in
        // our OnPaint handler and forwarded over to the plugin process async.
        rv = widget->Resize(aContentOffset.x + bounds.x,
                            aContentOffset.y + bounds.y,
                            bounds.width, bounds.height, true);
        NS_ASSERTION(NS_SUCCEEDED(rv), "widget call failure");
      }

      rv = widget->Enable(isVisible);
      NS_ASSERTION(NS_SUCCEEDED(rv), "widget call failure");

      // visible state - updated after clipping, prior to invalidating
      rv = widget->Show(isVisible);
      NS_ASSERTION(NS_SUCCEEDED(rv), "widget call failure");

      // Handle invalidation, this can be costly, avoid if it is not needed.
      if (isVisible) {
        // invalidate region (widget origin)
#if defined(XP_WIN)
        // Work around for flash's crummy sandbox. See bug 762948. This call
        // digs down into the window hirearchy, invalidating regions on
        // windows owned by other processes.
        mozilla::widget::WinUtils::InvalidatePluginAsWorkaround(
          widget, visibleBounds);
#else
        rv = widget->Invalidate(visibleBounds);
        NS_ASSERTION(NS_SUCCEEDED(rv), "widget call failure");
#endif
        visiblePluginIds.AppendElement(aPlugins[pluginsIdx].windowId());
      }
    }
  }
  // Any plugins we didn't update need to be hidden, as they are
  // not associated with visible content.
  nsIWidget::UpdateRegisteredPluginWindowVisibility((uintptr_t)parent, visiblePluginIds);
  if (!mCanSend) {
    return true;
  }
  SendRemotePluginsReady();
  return true;
#endif // !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
}

#if defined(XP_WIN)
static void
ScheduleSendAllPluginsCaptured(CompositorBridgeChild* aThis, MessageLoop* aLoop)
{
  aLoop->PostTask(NewNonOwningRunnableMethod(
    aThis, &CompositorBridgeChild::SendAllPluginsCaptured));
}
#endif

bool
CompositorBridgeChild::RecvCaptureAllPlugins(const uintptr_t& aParentWidget)
{
#if defined(XP_WIN)
  MOZ_ASSERT(NS_IsMainThread());
  nsIWidget::CaptureRegisteredPlugins(aParentWidget);

  // Bounce the call to SendAllPluginsCaptured off the ImageBridgeChild loop,
  // to make sure that the image updates on that thread have been processed.
  ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(
    NewRunnableFunction(&ScheduleSendAllPluginsCaptured, this,
                        MessageLoop::current()));
  return true;
#else
  MOZ_ASSERT_UNREACHABLE(
    "CompositorBridgeChild::RecvCaptureAllPlugins calls unexpected.");
  return false;
#endif
}

bool
CompositorBridgeChild::RecvHideAllPlugins(const uintptr_t& aParentWidget)
{
#if !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
  NS_NOTREACHED("CompositorBridgeChild::RecvHideAllPlugins calls "
                "unexpected on this platform.");
  return false;
#else
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<uintptr_t> list;
  nsIWidget::UpdateRegisteredPluginWindowVisibility(aParentWidget, list);
  if (!mCanSend) {
    return true;
  }
  SendRemotePluginsReady();
  return true;
#endif // !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
}

bool
CompositorBridgeChild::RecvDidComposite(const uint64_t& aId, const uint64_t& aTransactionId,
                                        const TimeStamp& aCompositeStart,
                                        const TimeStamp& aCompositeEnd)
{
  if (mLayerManager) {
    MOZ_ASSERT(aId == 0);
    RefPtr<ClientLayerManager> m = mLayerManager;
    m->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
  } else if (aId != 0) {
    RefPtr<dom::TabChild> child = dom::TabChild::GetFrom(aId);
    if (child) {
      child->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
    }
  }

  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->ReturnDeferredClients();
  }

  return true;
}

bool
CompositorBridgeChild::RecvOverfill(const uint32_t &aOverfill)
{
  for (size_t i = 0; i < mOverfillObservers.Length(); i++) {
    mOverfillObservers[i]->RunOverfillCallback(aOverfill);
  }
  mOverfillObservers.Clear();
  return true;
}

void
CompositorBridgeChild::AddOverfillObserver(ClientLayerManager* aLayerManager)
{
  MOZ_ASSERT(aLayerManager);
  mOverfillObservers.AppendElement(aLayerManager);
}

bool
CompositorBridgeChild::RecvClearCachedResources(const uint64_t& aId)
{
  dom::TabChild* child = dom::TabChild::GetFrom(aId);
  if (child) {
    child->ClearCachedResources();
  }
  return true;
}

void
CompositorBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy == AbnormalShutdown) {
#ifdef MOZ_B2G
  // Due to poor lifetime management of gralloc (and possibly shmems) we will
  // crash at some point in the future when we get destroyed due to abnormal
  // shutdown. Its better just to crash here. On desktop though, we have a chance
  // of recovering.
    NS_RUNTIMEABORT("ActorDestroy by IPC channel failure at CompositorBridgeChild");
#endif

    // If the parent side runs into a problem then the actor will be destroyed.
    // There is nothing we can do in the child side, here sets mCanSend as false.
    mCanSend = false;
    gfxCriticalNote << "Receive IPC close with reason=AbnormalShutdown";
  }

  if (mProcessToken && XRE_IsParentProcess()) {
    GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
  }
}

bool
CompositorBridgeChild::RecvSharedCompositorFrameMetrics(
    const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
    const CrossProcessMutexHandle& handle,
    const uint64_t& aLayersId,
    const uint32_t& aAPZCId)
{
  SharedFrameMetricsData* data = new SharedFrameMetricsData(
    metrics, handle, aLayersId, aAPZCId);
  mFrameMetricsTable.Put(data->GetViewID(), data);
  return true;
}

bool
CompositorBridgeChild::RecvReleaseSharedCompositorFrameMetrics(
    const ViewID& aId,
    const uint32_t& aAPZCId)
{
  SharedFrameMetricsData* data = mFrameMetricsTable.Get(aId);
  // The SharedFrameMetricsData may have been removed previously if
  // a SharedFrameMetricsData with the same ViewID but later APZCId had
  // been store and over wrote it.
  if (data && (data->GetAPZCId() == aAPZCId)) {
    mFrameMetricsTable.Remove(aId);
  }
  return true;
}

CompositorBridgeChild::SharedFrameMetricsData::SharedFrameMetricsData(
    const ipc::SharedMemoryBasic::Handle& metrics,
    const CrossProcessMutexHandle& handle,
    const uint64_t& aLayersId,
    const uint32_t& aAPZCId)
  : mMutex(nullptr)
  , mLayersId(aLayersId)
  , mAPZCId(aAPZCId)
{
  mBuffer = new ipc::SharedMemoryBasic;
  mBuffer->SetHandle(metrics);
  mBuffer->Map(sizeof(FrameMetrics));
  mMutex = new CrossProcessMutex(handle);
  MOZ_COUNT_CTOR(SharedFrameMetricsData);
}

CompositorBridgeChild::SharedFrameMetricsData::~SharedFrameMetricsData()
{
  // When the hash table deletes the class, delete
  // the shared memory and mutex.
  delete mMutex;
  mBuffer = nullptr;
  MOZ_COUNT_DTOR(SharedFrameMetricsData);
}

void
CompositorBridgeChild::SharedFrameMetricsData::CopyFrameMetrics(FrameMetrics* aFrame)
{
  FrameMetrics* frame = static_cast<FrameMetrics*>(mBuffer->memory());
  MOZ_ASSERT(frame);
  mMutex->Lock();
  *aFrame = *frame;
  mMutex->Unlock();
}

FrameMetrics::ViewID
CompositorBridgeChild::SharedFrameMetricsData::GetViewID()
{
  FrameMetrics* frame = static_cast<FrameMetrics*>(mBuffer->memory());
  MOZ_ASSERT(frame);
  // Not locking to read of mScrollId since it should not change after being
  // initially set.
  return frame->GetScrollId();
}

uint64_t
CompositorBridgeChild::SharedFrameMetricsData::GetLayersId() const
{
  return mLayersId;
}

uint32_t
CompositorBridgeChild::SharedFrameMetricsData::GetAPZCId()
{
  return mAPZCId;
}


bool
CompositorBridgeChild::RecvRemotePaintIsReady()
{
  // Used on the content thread, this bounces the message to the
  // TabParent (via the TabChild) if the notification was previously requested.
  // XPCOM gives a soup of compiler errors when trying to do_QueryReference
  // so I'm using static_cast<>
  MOZ_LAYERS_LOG(("[RemoteGfx] CompositorBridgeChild received RemotePaintIsReady"));
  RefPtr<nsISupports> iTabChildBase(do_QueryReferent(mWeakTabChild));
  if (!iTabChildBase) {
    MOZ_LAYERS_LOG(("[RemoteGfx] Note: TabChild was released before RemotePaintIsReady. "
        "MozAfterRemotePaint will not be sent to listener."));
    return true;
  }
  TabChildBase* tabChildBase = static_cast<TabChildBase*>(iTabChildBase.get());
  TabChild* tabChild = static_cast<TabChild*>(tabChildBase);
  MOZ_ASSERT(tabChild);
  Unused << tabChild->SendRemotePaintIsReady();
  mWeakTabChild = nullptr;
  return true;
}


void
CompositorBridgeChild::RequestNotifyAfterRemotePaint(TabChild* aTabChild)
{
  MOZ_ASSERT(aTabChild, "NULL TabChild not allowed in CompositorBridgeChild::RequestNotifyAfterRemotePaint");
  mWeakTabChild = do_GetWeakReference( static_cast<dom::TabChildBase*>(aTabChild) );
  if (!mCanSend) {
    return;
  }
  Unused << SendRequestNotifyAfterRemotePaint();
}

void
CompositorBridgeChild::CancelNotifyAfterRemotePaint(TabChild* aTabChild)
{
  RefPtr<nsISupports> iTabChildBase(do_QueryReferent(mWeakTabChild));
  if (!iTabChildBase) {
    return;
  }
  TabChildBase* tabChildBase = static_cast<TabChildBase*>(iTabChildBase.get());
  TabChild* tabChild = static_cast<TabChild*>(tabChildBase);
  if (tabChild == aTabChild) {
    mWeakTabChild = nullptr;
  }
}

bool
CompositorBridgeChild::SendWillClose()
{
  MOZ_RELEASE_ASSERT(mCanSend);
  return PCompositorBridgeChild::SendWillClose();
}

bool
CompositorBridgeChild::SendPause()
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendPause();
}

bool
CompositorBridgeChild::SendResume()
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendResume();
}

bool
CompositorBridgeChild::SendNotifyChildCreated(const uint64_t& id)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendNotifyChildCreated(id);
}

bool
CompositorBridgeChild::SendAdoptChild(const uint64_t& id)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendAdoptChild(id);
}

bool
CompositorBridgeChild::SendMakeSnapshot(const SurfaceDescriptor& inSnapshot, const gfx::IntRect& dirtyRect)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendMakeSnapshot(inSnapshot, dirtyRect);
}

bool
CompositorBridgeChild::SendFlushRendering()
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendFlushRendering();
}

bool
CompositorBridgeChild::SendStartFrameTimeRecording(const int32_t& bufferSize, uint32_t* startIndex)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendStartFrameTimeRecording(bufferSize, startIndex);
}

bool
CompositorBridgeChild::SendStopFrameTimeRecording(const uint32_t& startIndex, nsTArray<float>* intervals)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendStopFrameTimeRecording(startIndex, intervals);
}

bool
CompositorBridgeChild::SendNotifyRegionInvalidated(const nsIntRegion& region)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendNotifyRegionInvalidated(region);
}

bool
CompositorBridgeChild::SendRequestNotifyAfterRemotePaint()
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendRequestNotifyAfterRemotePaint();
}

bool
CompositorBridgeChild::SendClearApproximatelyVisibleRegions(uint64_t aLayersId,
                                                            uint32_t aPresShellId)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendClearApproximatelyVisibleRegions(aLayersId,
                                                                aPresShellId);
}

bool
CompositorBridgeChild::SendNotifyApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                            const CSSIntRegion& aRegion)
{
  if (!mCanSend) {
    return false;
  }
  return PCompositorBridgeChild::SendNotifyApproximatelyVisibleRegion(aGuid, aRegion);
}

PTextureChild*
CompositorBridgeChild::AllocPTextureChild(const SurfaceDescriptor&,
                                          const LayersBackend&,
                                          const TextureFlags&,
                                          const uint64_t&,
                                          const uint64_t& aSerial)
{
  return TextureClient::CreateIPDLActor();
}

bool
CompositorBridgeChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

bool
CompositorBridgeChild::RecvParentAsyncMessages(InfallibleTArray<AsyncParentMessageData>&& aMessages)
{
  for (AsyncParentMessageArray::index_type i = 0; i < aMessages.Length(); ++i) {
    const AsyncParentMessageData& message = aMessages[i];

    switch (message.type()) {
      case AsyncParentMessageData::TOpDeliverFence: {
        const OpDeliverFence& op = message.get_OpDeliverFence();
        FenceHandle fence = op.fence();
        DeliverFence(op.TextureId(), fence);
        break;
      }
      case AsyncParentMessageData::TOpNotifyNotUsed: {
        const OpNotifyNotUsed& op = message.get_OpNotifyNotUsed();
        NotifyNotUsed(op.TextureId(), op.fwdTransactionId());
        break;
      }
      default:
        NS_ERROR("unknown AsyncParentMessageData type");
        return false;
    }
  }
  return true;
}

void
CompositorBridgeChild::HoldUntilCompositableRefReleasedIfNecessary(TextureClient* aClient)
{
  if (!aClient) {
    return;
  }

  if (!(aClient->GetFlags() & TextureFlags::RECYCLE) &&
     !aClient->NeedsFenceHandle()) {
    return;
  }

  if (aClient->GetFlags() & TextureFlags::RECYCLE) {
    aClient->SetLastFwdTransactionId(GetFwdTransactionId());
    mTexturesWaitingRecycled.Put(aClient->GetSerial(), aClient);
    return;
  }
  MOZ_ASSERT(!(aClient->GetFlags() & TextureFlags::RECYCLE));
  MOZ_ASSERT(aClient->NeedsFenceHandle());
  // Handle a case of fence delivery via ImageBridge.
  // GrallocTextureData alwasys requests fence delivery if ANDROID_VERSION >= 17.
  ImageBridgeChild::GetSingleton()->HoldUntilFenceHandleDelivery(aClient, GetFwdTransactionId());
}

void
CompositorBridgeChild::NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  if (aFwdTransactionId < client->GetLastFwdTransactionId()) {
    // Released on host side, but client already requested newer use texture.
    return;
  }
  mTexturesWaitingRecycled.Remove(aTextureId);
}

void
CompositorBridgeChild::DeliverFence(uint64_t aTextureId, FenceHandle& aReleaseFenceHandle)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  client->SetReleaseFenceHandle(aReleaseFenceHandle);
}

void
CompositorBridgeChild::CancelWaitForRecycle(uint64_t aTextureId)
{
  RefPtr<TextureClient> client = mTexturesWaitingRecycled.Get(aTextureId);
  if (!client) {
    return;
  }
  mTexturesWaitingRecycled.Remove(aTextureId);
}

TextureClientPool*
CompositorBridgeChild::GetTexturePool(LayersBackend aBackend,
                                      SurfaceFormat aFormat,
                                      TextureFlags aFlags)
{
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    if (mTexturePools[i]->GetBackend() == aBackend &&
        mTexturePools[i]->GetFormat() == aFormat &&
        mTexturePools[i]->GetFlags() == aFlags) {
      return mTexturePools[i];
    }
  }

  mTexturePools.AppendElement(
      new TextureClientPool(aBackend, aFormat,
                            gfx::gfxVars::TileSize(),
                            aFlags,
                            gfxPrefs::LayersTilePoolShrinkTimeout(),
                            gfxPrefs::LayersTilePoolClearTimeout(),
                            gfxPrefs::LayersTileInitialPoolSize(),
                            gfxPrefs::LayersTilePoolUnusedSize(),
                            this));

  return mTexturePools.LastElement();
}

void
CompositorBridgeChild::HandleMemoryPressure()
{
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Clear();
  }
}

void
CompositorBridgeChild::ClearTexturePool()
{
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Clear();
  }
}

FixedSizeSmallShmemSectionAllocator*
CompositorBridgeChild::GetTileLockAllocator()
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return nullptr;
  }

  if (!mSectionAllocator) {
    mSectionAllocator = new FixedSizeSmallShmemSectionAllocator(this);
  }
  return mSectionAllocator;
}


PTextureChild*
CompositorBridgeChild::CreateTexture(const SurfaceDescriptor& aSharedData,
                                     LayersBackend aLayersBackend,
                                     TextureFlags aFlags,
                                     uint64_t aSerial)
{
  return PCompositorBridgeChild::SendPTextureConstructor(aSharedData, aLayersBackend, aFlags, 0 /* FIXME? */, aSerial);
}

bool
CompositorBridgeChild::AllocUnsafeShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem)
{
  return PCompositorBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool
CompositorBridgeChild::AllocShmem(size_t aSize,
                             ipc::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aShmem)
{
  return PCompositorBridgeChild::AllocShmem(aSize, aType, aShmem);
}

void
CompositorBridgeChild::DeallocShmem(ipc::Shmem& aShmem)
{
    PCompositorBridgeChild::DeallocShmem(aShmem);
}

widget::PCompositorWidgetChild*
CompositorBridgeChild::AllocPCompositorWidgetChild(const CompositorWidgetInitData& aInitData)
{
  // We send the constructor manually.
  MOZ_CRASH("Should not be called");
  return nullptr;
}

bool
CompositorBridgeChild::DeallocPCompositorWidgetChild(PCompositorWidgetChild* aActor)
{
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
  delete aActor;
  return true;
#else
  return false;
#endif
}

RefPtr<IAPZCTreeManager>
CompositorBridgeChild::GetAPZCTreeManager(uint64_t aLayerTreeId)
{
  bool apzEnabled = false;
  Unused << SendAsyncPanZoomEnabled(aLayerTreeId, &apzEnabled);

  if (!apzEnabled) {
    return nullptr;
  }

  PAPZCTreeManagerChild* child = SendPAPZCTreeManagerConstructor(aLayerTreeId);
  if (!child) {
    return nullptr;
  }
  APZCTreeManagerChild* parent = static_cast<APZCTreeManagerChild*>(child);

  return RefPtr<IAPZCTreeManager>(parent);
}

PAPZCTreeManagerChild*
CompositorBridgeChild::AllocPAPZCTreeManagerChild(const uint64_t& aLayersId)
{
  APZCTreeManagerChild* child = new APZCTreeManagerChild();
  child->AddRef();
  return child;
}

PAPZChild*
CompositorBridgeChild::AllocPAPZChild(const uint64_t& aLayersId)
{
  // We send the constructor manually.
  MOZ_CRASH("Should not be called");
  return nullptr;
}

bool
CompositorBridgeChild::DeallocPAPZChild(PAPZChild* aActor)
{
  delete aActor;
  return true;
}

bool
CompositorBridgeChild::DeallocPAPZCTreeManagerChild(PAPZCTreeManagerChild* aActor)
{
  APZCTreeManagerChild* parent = static_cast<APZCTreeManagerChild*>(aActor);
  parent->Release();
  return true;
}

void
CompositorBridgeChild::ProcessingError(Result aCode, const char* aReason)
{
  if (aCode != MsgDropped) {
    gfxDevCrash(gfx::LogReason::ProcessingError) << "Processing error in CompositorBridgeChild: " << int(aCode);
  }
}

} // namespace layers
} // namespace mozilla


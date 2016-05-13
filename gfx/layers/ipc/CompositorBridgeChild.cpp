/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include <stddef.h>                     // for size_t
#include "ClientLayerManager.h"         // for ClientLayerManager
#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsIObserver.h"                // for nsIObserver
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop, etc
#include "FrameLayerBuilder.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/unused.h"
#include "mozilla/DebugOnly.h"
#if defined(XP_WIN)
#include "WinUtils.h"
#endif

using mozilla::layers::LayerTransactionChild;
using mozilla::dom::TabChildBase;
using mozilla::Unused;

namespace mozilla {
namespace layers {

static StaticRefPtr<CompositorBridgeChild> sCompositorBridge;

Atomic<int32_t> CompositableForwarder::sSerialCounter(0);

CompositorBridgeChild::CompositorBridgeChild(ClientLayerManager *aLayerManager)
  : mLayerManager(aLayerManager)
  , mCanSend(false)
{
}

CompositorBridgeChild::~CompositorBridgeChild()
{
  RefPtr<DeleteTask<Transport>> task = new DeleteTask<Transport>(GetTransport());
  XRE_GetIOMessageLoop()->PostTask(task.forget());

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
  MOZ_ASSERT(mRefCnt != 0);

  if (!mCanSend) {
    return;
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

  SendWillClose();
  mCanSend = false;


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

  const ManagedContainer<PTextureChild>& textures = ManagedPTextureChild();
  for (auto iter = textures.ConstIter(); !iter.Done(); iter.Next()) {
    RefPtr<TextureClient> texture = TextureClient::AsTextureClient(iter.Get()->GetKey());

    if (texture) {
      texture->Destroy();
    }
  }

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

/*static*/ PCompositorBridgeChild*
CompositorBridgeChild::Create(Transport* aTransport, ProcessId aOtherPid)
{
  // There's only one compositor per child process.
  MOZ_ASSERT(!sCompositorBridge);

  RefPtr<CompositorBridgeChild> child(new CompositorBridgeChild(nullptr));
  if (!child->Open(aTransport, aOtherPid, XRE_GetIOMessageLoop(), ipc::ChildSide)) {
    NS_RUNTIMEABORT("Couldn't Open() Compositor channel.");
    return nullptr;
  }

  child->mCanSend = true;

  // We release this ref in DeferredDestroyCompositor.
  sCompositorBridge = child;

  int32_t width;
  int32_t height;
  sCompositorBridge->SendGetTileSize(&width, &height);
  gfxPlatform::GetPlatform()->SetTileSize(width, height);

  return sCompositorBridge;
}

bool
CompositorBridgeChild::OpenSameProcess(CompositorBridgeParent* aParent)
{
  MOZ_ASSERT(aParent);

  mCompositorBridgeParent = aParent;
  mCanSend = Open(mCompositorBridgeParent->GetIPCChannel(),
                  CompositorBridgeParent::CompositorLoop(),
                  ipc::ChildSide);
  return mCanSend;
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
  MOZ_ASSERT(mCanSend);
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
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  SendRemotePluginsReady();
#endif
  return true;
#endif // !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
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
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  SendRemotePluginsReady();
#endif
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
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendWillClose();
}

bool
CompositorBridgeChild::SendPause()
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendPause();
}

bool
CompositorBridgeChild::SendResume()
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendResume();
}

bool
CompositorBridgeChild::SendNotifyHidden(const uint64_t& id)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendNotifyHidden(id);
}

bool
CompositorBridgeChild::SendNotifyVisible(const uint64_t& id)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendNotifyVisible(id);
}

bool
CompositorBridgeChild::SendNotifyChildCreated(const uint64_t& id)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendNotifyChildCreated(id);
}

bool
CompositorBridgeChild::SendAdoptChild(const uint64_t& id)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendAdoptChild(id);
}

bool
CompositorBridgeChild::SendMakeSnapshot(const SurfaceDescriptor& inSnapshot, const gfx::IntRect& dirtyRect)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendMakeSnapshot(inSnapshot, dirtyRect);
}

bool
CompositorBridgeChild::SendFlushRendering()
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendFlushRendering();
}

bool
CompositorBridgeChild::SendGetTileSize(int32_t* tileWidth, int32_t* tileHeight)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendGetTileSize(tileWidth, tileHeight);
}

bool
CompositorBridgeChild::SendStartFrameTimeRecording(const int32_t& bufferSize, uint32_t* startIndex)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendStartFrameTimeRecording(bufferSize, startIndex);
}

bool
CompositorBridgeChild::SendStopFrameTimeRecording(const uint32_t& startIndex, nsTArray<float>* intervals)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendStopFrameTimeRecording(startIndex, intervals);
}

bool
CompositorBridgeChild::SendNotifyRegionInvalidated(const nsIntRegion& region)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendNotifyRegionInvalidated(region);
}

bool
CompositorBridgeChild::SendRequestNotifyAfterRemotePaint()
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendRequestNotifyAfterRemotePaint();
}

bool
CompositorBridgeChild::SendClearVisibleRegions(uint64_t aLayersId,
                                               uint32_t aPresShellId)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendClearVisibleRegions(aLayersId, aPresShellId);
}

bool
CompositorBridgeChild::SendUpdateVisibleRegion(VisibilityCounter aCounter,
                                               const ScrollableLayerGuid& aGuid,
                                               const CSSIntRegion& aRegion)
{
  MOZ_ASSERT(mCanSend);
  if (!mCanSend) {
    return true;
  }
  return PCompositorBridgeChild::SendUpdateVisibleRegion(aCounter, aGuid, aRegion);
}

PTextureChild*
CompositorBridgeChild::AllocPTextureChild(const SurfaceDescriptor&,
                                          const LayersBackend&,
                                          const TextureFlags&,
                                          const uint64_t&)
{
  return TextureClient::CreateIPDLActor();
}

bool
CompositorBridgeChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

} // namespace layers
} // namespace mozilla


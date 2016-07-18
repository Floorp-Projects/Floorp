/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessManager.h"
#include "GPUProcessHost.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/InProcessCompositorSession.h"
#include "mozilla/layers/RemoteCompositorSession.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
# include "mozilla/widget/CompositorWidgetChild.h"
#endif
#include "nsContentUtils.h"

namespace mozilla {
namespace gfx {

using namespace mozilla::layers;

static StaticAutoPtr<GPUProcessManager> sSingleton;

GPUProcessManager*
GPUProcessManager::Get()
{
  return sSingleton;
}

void
GPUProcessManager::Initialize()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  sSingleton = new GPUProcessManager();
}

void
GPUProcessManager::Shutdown()
{
  sSingleton = nullptr;
}

GPUProcessManager::GPUProcessManager()
 : mTaskFactory(this),
   mNextLayerTreeId(0),
   mProcess(nullptr),
   mGPUChild(nullptr)
{
  mObserver = new Observer(this);
  nsContentUtils::RegisterShutdownObserver(mObserver);
}

GPUProcessManager::~GPUProcessManager()
{
  // The GPU process should have already been shut down.
  MOZ_ASSERT(!mProcess && !mGPUChild);

  // We should have already removed observers.
  MOZ_ASSERT(!mObserver);
}

NS_IMPL_ISUPPORTS(GPUProcessManager::Observer, nsIObserver);

GPUProcessManager::Observer::Observer(GPUProcessManager* aManager)
 : mManager(aManager)
{
}

NS_IMETHODIMP
GPUProcessManager::Observer::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->OnXPCOMShutdown();
  }
  return NS_OK;
}

void
GPUProcessManager::OnXPCOMShutdown()
{
  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    mObserver = nullptr;
  }

  DestroyProcess();
}

void
GPUProcessManager::EnableGPUProcess()
{
  if (mProcess) {
    return;
  }

  // The subprocess is launched asynchronously, so we wait for a callback to
  // acquire the IPDL actor.
  mProcess = new GPUProcessHost(this);
  if (!mProcess->Launch()) {
    DisableGPUProcess("Failed to launch GPU process");
  }
}

void
GPUProcessManager::DisableGPUProcess(const char* aMessage)
{
  gfxConfig::SetFailed(Feature::GPU_PROCESS, FeatureStatus::Failed, aMessage);
  gfxCriticalNote << aMessage;

  DestroyProcess();
}

void
GPUProcessManager::EnsureGPUReady()
{
  if (mProcess && mProcess->IsConnected()) {
    if (!mProcess->WaitForLaunch()) {
      // If this fails, we should have fired OnProcessLaunchComplete and
      // removed the process.
      MOZ_ASSERT(!mProcess && !mGPUChild);
      return;
    }
  }
}

void
GPUProcessManager::OnProcessLaunchComplete(GPUProcessHost* aHost)
{
  MOZ_ASSERT(mProcess && mProcess == aHost);

  if (!mProcess->IsConnected()) {
    DisableGPUProcess("Failed to launch GPU process");
    return;
  }

  mGPUChild = mProcess->GetActor();
  mProcessToken = mProcess->GetProcessToken();
}

void
GPUProcessManager::OnProcessUnexpectedShutdown(GPUProcessHost* aHost)
{
  MOZ_ASSERT(mProcess && mProcess == aHost);

  DestroyProcess();
}

void
GPUProcessManager::NotifyRemoteActorDestroyed(const uint64_t& aProcessToken)
{
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> task = mTaskFactory.NewRunnableMethod(
      &GPUProcessManager::NotifyRemoteActorDestroyed, aProcessToken);
    NS_DispatchToMainThread(task.forget());
    return;
  }

  if (mProcessToken != aProcessToken) {
    // This token is for an older process; we can safely ignore it.
    return;
  }

  // One of the bridged top-level actors for the GPU process has been
  // prematurely terminated, and we're receiving a notification. This
  // can happen if the ActorDestroy for a bridged protocol fires
  // before the ActorDestroy for PGPUChild.
  DestroyProcess();
}

void
GPUProcessManager::DestroyProcess()
{
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcessToken = 0;
  mProcess = nullptr;
  mGPUChild = nullptr;
}

RefPtr<CompositorSession>
GPUProcessManager::CreateTopLevelCompositor(nsIWidget* aWidget,
                                            ClientLayerManager* aLayerManager,
                                            CSSToLayoutDeviceScale aScale,
                                            bool aUseAPZ,
                                            bool aUseExternalSurfaceSize,
                                            const gfx::IntSize& aSurfaceSize)
{
  uint64_t layerTreeId = AllocateLayerTreeId();

  if (mGPUChild) {
    RefPtr<CompositorSession> session = CreateRemoteSession(
      aWidget,
      aLayerManager,
      layerTreeId,
      aScale,
      aUseAPZ,
      aUseExternalSurfaceSize,
      aSurfaceSize);
    if (session) {
      return session;
    }

    // We couldn't create a remote compositor, so abort the process.
    DisableGPUProcess("Failed to create remote compositor");
  }

  return InProcessCompositorSession::Create(
    aWidget,
    aLayerManager,
    layerTreeId,
    aScale,
    aUseAPZ,
    aUseExternalSurfaceSize,
    aSurfaceSize);
}

RefPtr<CompositorSession>
GPUProcessManager::CreateRemoteSession(nsIWidget* aWidget,
                                       ClientLayerManager* aLayerManager,
                                       const uint64_t& aRootLayerTreeId,
                                       CSSToLayoutDeviceScale aScale,
                                       bool aUseAPZ,
                                       bool aUseExternalSurfaceSize,
                                       const gfx::IntSize& aSurfaceSize)
{
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
  ipc::Endpoint<PCompositorBridgeParent> parentPipe;
  ipc::Endpoint<PCompositorBridgeChild> childPipe;

  nsresult rv = PCompositorBridge::CreateEndpoints(
    mGPUChild->OtherPid(),
    base::GetCurrentProcId(),
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Failed to create PCompositorBridge endpoints: " << hexa(int(rv));
    return nullptr;
  }

  RefPtr<CompositorBridgeChild> child = CompositorBridgeChild::CreateRemote(
    mProcessToken,
    aLayerManager,
    Move(childPipe));
  if (!child) {
    gfxCriticalNote << "Failed to create CompositorBridgeChild";
    return nullptr;
  }

  CompositorWidgetInitData initData;
  aWidget->GetCompositorWidgetInitData(&initData);

  bool ok = mGPUChild->SendNewWidgetCompositor(
    Move(parentPipe),
    aScale,
    aUseExternalSurfaceSize,
    aSurfaceSize);
  if (!ok)
    return nullptr;

  CompositorWidgetChild* widget = new CompositorWidgetChild(aWidget);
  if (!child->SendPCompositorWidgetConstructor(widget, initData))
    return nullptr;
  if (!child->SendInitialize(aRootLayerTreeId))
    return nullptr;

  RefPtr<RemoteCompositorSession> session =
    new RemoteCompositorSession(child, widget, aRootLayerTreeId);
  return session.forget();
#else
  gfxCriticalNote << "Platform does not support out-of-process compositing";
  return nullptr;
#endif
}

bool
GPUProcessManager::CreateContentCompositorBridge(base::ProcessId aOtherProcess,
                                                 ipc::Endpoint<PCompositorBridgeChild>* aOutEndpoint)
{
  ipc::Endpoint<PCompositorBridgeParent> parentPipe;
  ipc::Endpoint<PCompositorBridgeChild> childPipe;

  base::ProcessId gpuPid = mGPUChild
                           ? mGPUChild->OtherPid()
                           : base::GetCurrentProcId();

  nsresult rv = PCompositorBridge::CreateEndpoints(
    gpuPid,
    aOtherProcess,
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: " << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentCompositorBridge(Move(parentPipe));
  } else {
    if (!CompositorBridgeParent::CreateForContent(Move(parentPipe)))
      return false;
  }

  *aOutEndpoint = Move(childPipe);
  return true;
}

already_AddRefed<APZCTreeManager>
GPUProcessManager::GetAPZCTreeManagerForLayers(uint64_t aLayersId)
{
  return CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
}

uint64_t
GPUProcessManager::AllocateLayerTreeId()
{
  MOZ_ASSERT(NS_IsMainThread());
  return ++mNextLayerTreeId;
}

void
GPUProcessManager::DeallocateLayerTreeId(uint64_t aLayersId)
{
  CompositorBridgeParent::DeallocateLayerTreeId(aLayersId);
}

void
GPUProcessManager::RequestNotifyLayerTreeReady(uint64_t aLayersId, CompositorUpdateObserver* aObserver)
{
  CompositorBridgeParent::RequestNotifyLayerTreeReady(aLayersId, aObserver);
}

void
GPUProcessManager::RequestNotifyLayerTreeCleared(uint64_t aLayersId, CompositorUpdateObserver* aObserver)
{
  CompositorBridgeParent::RequestNotifyLayerTreeCleared(aLayersId, aObserver);
}

void
GPUProcessManager::SwapLayerTreeObservers(uint64_t aLayer, uint64_t aOtherLayer)
{
  CompositorBridgeParent::SwapLayerTreeObservers(aLayer, aOtherLayer);
}

bool
GPUProcessManager::UpdateRemoteContentController(uint64_t aLayersId,
                                                 dom::ContentParent* aContentParent,
                                                 const dom::TabId& aTabId,
                                                 dom::TabParent* aBrowserParent)
{
  return CompositorBridgeParent::UpdateRemoteContentController(
    aLayersId,
    aContentParent,
    aTabId,
    aBrowserParent);
}

} // namespace gfx
} // namespace mozilla

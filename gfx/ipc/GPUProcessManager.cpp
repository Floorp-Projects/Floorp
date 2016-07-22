/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessManager.h"
#include "GPUProcessHost.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/InProcessCompositorSession.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/RemoteCompositorSession.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
# include "mozilla/widget/CompositorWidgetChild.h"
#endif
#include "nsBaseWidget.h"
#include "nsContentUtils.h"
#include "VRManagerChild.h"
#include "VRManagerParent.h"
#include "VsyncBridgeChild.h"
#include "VsyncIOThreadHolder.h"
#include "VsyncSource.h"

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

  LayerTreeOwnerTracker::Initialize();
}

GPUProcessManager::~GPUProcessManager()
{
  LayerTreeOwnerTracker::Shutdown();

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

  CleanShutdown();
}

void
GPUProcessManager::EnableGPUProcess()
{
  if (mProcess) {
    return;
  }

  // Start the Vsync I/O thread so can use it as soon as the process launches.
  EnsureVsyncIOThread();

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
  ShutdownVsyncIOThread();
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

  if (mGPUChild) {
    mGPUChild->EnsureGPUReady();
  }
}

void
GPUProcessManager::EnsureImageBridgeChild()
{
  if (ImageBridgeChild::GetSingleton()) {
    return;
  }

  EnsureGPUReady();

  if (!mGPUChild) {
    ImageBridgeChild::InitSameProcess();
    return;
  }

  ipc::Endpoint<PImageBridgeParent> parentPipe;
  ipc::Endpoint<PImageBridgeChild> childPipe;
  nsresult rv = PImageBridge::CreateEndpoints(
    mGPUChild->OtherPid(),
    base::GetCurrentProcId(),
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PImageBridge endpoints");
    return;
  }

  mGPUChild->SendInitImageBridge(Move(parentPipe));
  ImageBridgeChild::InitWithGPUProcess(Move(childPipe));
}

void
GPUProcessManager::EnsureVRManager()
{
  if (VRManagerChild::IsCreated()) {
    return;
  }

  EnsureGPUReady();

  if (!mGPUChild) {
    VRManagerChild::InitSameProcess();
    return;
  }

  ipc::Endpoint<PVRManagerParent> parentPipe;
  ipc::Endpoint<PVRManagerChild> childPipe;
  nsresult rv = PVRManager::CreateEndpoints(
    mGPUChild->OtherPid(),
    base::GetCurrentProcId(),
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PVRManager endpoints");
    return;
  }

  mGPUChild->SendInitVRManager(Move(parentPipe));
  VRManagerChild::InitWithGPUProcess(Move(childPipe));
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

  Endpoint<PVsyncBridgeParent> vsyncParent;
  Endpoint<PVsyncBridgeChild> vsyncChild;
  nsresult rv = PVsyncBridge::CreateEndpoints(
    mGPUChild->OtherPid(),
    base::GetCurrentProcId(),
    &vsyncParent,
    &vsyncChild);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PVsyncBridge endpoints");
    return;
  }

  mVsyncBridge = VsyncBridgeChild::Create(mVsyncIOThread, mProcessToken, Move(vsyncChild));
  mGPUChild->SendInitVsyncBridge(Move(vsyncParent));
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
GPUProcessManager::CleanShutdown()
{
  if (!mProcess) {
    return;
  }

#ifdef NS_FREE_PERMANENT_DATA
  mVsyncBridge->Close();
#endif
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
  mVsyncBridge = nullptr;
}

RefPtr<CompositorSession>
GPUProcessManager::CreateTopLevelCompositor(nsBaseWidget* aWidget,
                                            ClientLayerManager* aLayerManager,
                                            CSSToLayoutDeviceScale aScale,
                                            bool aUseAPZ,
                                            bool aUseExternalSurfaceSize,
                                            const gfx::IntSize& aSurfaceSize)
{
  uint64_t layerTreeId = AllocateLayerTreeId();

  EnsureGPUReady();
  EnsureImageBridgeChild();
  EnsureVRManager();

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
GPUProcessManager::CreateRemoteSession(nsBaseWidget* aWidget,
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

  TimeDuration vsyncRate =
    gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay().GetVsyncRate();

  bool ok = mGPUChild->SendNewWidgetCompositor(
    Move(parentPipe),
    aScale,
    vsyncRate,
    aUseExternalSurfaceSize,
    aSurfaceSize);
  if (!ok) {
    return nullptr;
  }

  RefPtr<CompositorVsyncDispatcher> dispatcher = aWidget->GetCompositorVsyncDispatcher();
  RefPtr<CompositorWidgetVsyncObserver> observer =
    new CompositorWidgetVsyncObserver(mVsyncBridge, aRootLayerTreeId);

  CompositorWidgetChild* widget = new CompositorWidgetChild(dispatcher, observer);
  if (!child->SendPCompositorWidgetConstructor(widget, initData)) {
    return nullptr;
  }
  if (!child->SendInitialize(aRootLayerTreeId)) {
    return nullptr;
  }

  RefPtr<APZCTreeManagerChild> apz = nullptr;
  if (aUseAPZ) {
    PAPZCTreeManagerChild* papz = child->SendPAPZCTreeManagerConstructor(0);
    if (!papz) {
      return nullptr;
    }
    apz = static_cast<APZCTreeManagerChild*>(papz);
  }

  RefPtr<RemoteCompositorSession> session =
    new RemoteCompositorSession(child, widget, apz, aRootLayerTreeId);
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
  EnsureGPUReady();

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
    if (!CompositorBridgeParent::CreateForContent(Move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = Move(childPipe);
  return true;
}

bool
GPUProcessManager::CreateContentImageBridge(base::ProcessId aOtherProcess,
                                            ipc::Endpoint<PImageBridgeChild>* aOutEndpoint)
{
  EnsureImageBridgeChild();

  base::ProcessId gpuPid = mGPUChild
                           ? mGPUChild->OtherPid()
                           : base::GetCurrentProcId();

  ipc::Endpoint<PImageBridgeParent> parentPipe;
  ipc::Endpoint<PImageBridgeChild> childPipe;
  nsresult rv = PImageBridge::CreateEndpoints(
    gpuPid,
    aOtherProcess,
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: " << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentImageBridge(Move(parentPipe));
  } else {
    if (!ImageBridgeParent::CreateForContent(Move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = Move(childPipe);
  return true;
}

bool
GPUProcessManager::CreateContentVRManager(base::ProcessId aOtherProcess,
                                          ipc::Endpoint<PVRManagerChild>* aOutEndpoint)
{
  EnsureVRManager();

  base::ProcessId gpuPid = mGPUChild
                           ? mGPUChild->OtherPid()
                           : base::GetCurrentProcId();

  ipc::Endpoint<PVRManagerParent> parentPipe;
  ipc::Endpoint<PVRManagerChild> childPipe;
  nsresult rv = PVRManager::CreateEndpoints(
    gpuPid,
    aOtherProcess,
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: " << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentVRManager(Move(parentPipe));
  } else {
    if (!VRManagerParent::CreateForContent(Move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = Move(childPipe);
  return true;
}

already_AddRefed<IAPZCTreeManager>
GPUProcessManager::GetAPZCTreeManagerForLayers(uint64_t aLayersId)
{
  return CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
}

void
GPUProcessManager::MapLayerTreeId(uint64_t aLayersId, base::ProcessId aOwningId)
{
  LayerTreeOwnerTracker::Get()->Map(aLayersId, aOwningId);

  if (mGPUChild) {
    mGPUChild->SendAddLayerTreeIdMapping(
        aLayersId,
        aOwningId);
  }
}

bool
GPUProcessManager::IsLayerTreeIdMapped(uint64_t aLayersId, base::ProcessId aRequestingId)
{
  return LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, aRequestingId);
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
  if (mGPUChild) {
    mGPUChild->SendDeallocateLayerTreeId(aLayersId);
    return;
  }
  CompositorBridgeParent::DeallocateLayerTreeId(aLayersId);
}

void
GPUProcessManager::EnsureVsyncIOThread()
{
  if (mVsyncIOThread) {
    return;
  }

  mVsyncIOThread = new VsyncIOThreadHolder();
  MOZ_RELEASE_ASSERT(mVsyncIOThread->Start());
}

void
GPUProcessManager::ShutdownVsyncIOThread()
{
  mVsyncIOThread = nullptr;
}

} // namespace gfx
} // namespace mozilla

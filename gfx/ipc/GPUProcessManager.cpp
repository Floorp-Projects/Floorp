/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessManager.h"
#include "GPUProcessHost.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace gfx {

using namespace mozilla::layers;

static StaticAutoPtr<GPUProcessManager> sSingleton;

GPUProcessManager*
GPUProcessManager::Get()
{
  MOZ_ASSERT(sSingleton);
  return sSingleton;
}

void
GPUProcessManager::Initialize()
{
  sSingleton = new GPUProcessManager();
}

void
GPUProcessManager::Shutdown()
{
  sSingleton = nullptr;
}

GPUProcessManager::GPUProcessManager()
 : mProcess(nullptr),
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
}

void
GPUProcessManager::OnProcessUnexpectedShutdown(GPUProcessHost* aHost)
{
  MOZ_ASSERT(mProcess && mProcess == aHost);

  DestroyProcess();
}

void
GPUProcessManager::DestroyProcess()
{
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcess = nullptr;
  mGPUChild = nullptr;
}

already_AddRefed<CompositorSession>
GPUProcessManager::CreateTopLevelCompositor(widget::CompositorWidgetProxy* aProxy,
                                            ClientLayerManager* aLayerManager,
                                            CSSToLayoutDeviceScale aScale,
                                            bool aUseAPZ,
                                            bool aUseExternalSurfaceSize,
                                            int aSurfaceWidth,
                                            int aSurfaceHeight)
{
  return CompositorSession::CreateInProcess(
    aProxy,
    aLayerManager,
    aScale,
    aUseAPZ,
    aUseExternalSurfaceSize,
    aSurfaceWidth,
    aSurfaceHeight);
}

PCompositorBridgeParent*
GPUProcessManager::CreateTabCompositorBridge(ipc::Transport* aTransport,
                                             base::ProcessId aOtherProcess,
                                             ipc::GeckoChildProcessHost* aSubprocess)
{
  return CompositorBridgeParent::Create(aTransport, aOtherProcess, aSubprocess);
}

already_AddRefed<APZCTreeManager>
GPUProcessManager::GetAPZCTreeManagerForLayers(uint64_t aLayersId)
{
  return CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
}

uint64_t
GPUProcessManager::AllocateLayerTreeId()
{
  return CompositorBridgeParent::AllocateLayerTreeId();
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

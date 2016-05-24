/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessManager.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/StaticPtr.h"

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
{
}

GPUProcessManager::~GPUProcessManager()
{
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

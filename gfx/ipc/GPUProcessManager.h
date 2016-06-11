/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_GPUProcessManager_h_
#define _include_mozilla_gfx_ipc_GPUProcessManager_h_

#include "base/basictypes.h"
#include "base/process.h"
#include "Units.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/gfx/GPUProcessHost.h"
#include "mozilla/ipc/Transport.h"
#include "nsIObserverService.h"

namespace mozilla {
namespace layers {
class APZCTreeManager;
class CompositorSession;
class ClientLayerManager;
class CompositorUpdateObserver;
class PCompositorBridgeParent;
} // namespace layers
namespace widget {
class CompositorWidgetProxy;
} // namespace widget
namespace dom {
class ContentParent;
class TabParent;
} // namespace dom
namespace ipc {
class GeckoChildProcessHost;
} // namespace ipc
namespace gfx {

class GPUChild;

// The GPUProcessManager is a singleton responsible for creating GPU-bound
// objects that may live in another process. Currently, it provides access
// to the compositor via CompositorBridgeParent.
class GPUProcessManager final : public GPUProcessHost::Listener
{
  typedef layers::APZCTreeManager APZCTreeManager;
  typedef layers::CompositorUpdateObserver CompositorUpdateObserver;

public:
  static void Initialize();
  static void Shutdown();
  static GPUProcessManager* Get();

  ~GPUProcessManager();

  // If not using a GPU process, launch a new GPU process asynchronously.
  void EnableGPUProcess();

  // Ensure that GPU-bound methods can be used. If no GPU process is being
  // used, or one is launched and ready, this function returns immediately.
  // Otherwise it blocks until the GPU process has finished launching.
  void EnsureGPUReady();

  already_AddRefed<layers::CompositorSession> CreateTopLevelCompositor(
    widget::CompositorWidgetProxy* aProxy,
    layers::ClientLayerManager* aLayerManager,
    CSSToLayoutDeviceScale aScale,
    bool aUseAPZ,
    bool aUseExternalSurfaceSize,
    int aSurfaceWidth,
    int aSurfaceHeight);

  layers::PCompositorBridgeParent* CreateTabCompositorBridge(
    ipc::Transport* aTransport,
    base::ProcessId aOtherProcess,
    ipc::GeckoChildProcessHost* aSubprocess);

  // This returns a reference to the APZCTreeManager to which
  // pan/zoom-related events can be sent.
  already_AddRefed<APZCTreeManager> GetAPZCTreeManagerForLayers(uint64_t aLayersId);

  // Allocate an ID that can be used to refer to a layer tree and
  // associated resources that live only on the compositor thread.
  //
  // Must run on the content main thread.
  uint64_t AllocateLayerTreeId();

  // Release compositor-thread resources referred to by |aID|.
  //
  // Must run on the content main thread.
  void DeallocateLayerTreeId(uint64_t aLayersId);

  void RequestNotifyLayerTreeReady(uint64_t aLayersId, CompositorUpdateObserver* aObserver);
  void RequestNotifyLayerTreeCleared(uint64_t aLayersId, CompositorUpdateObserver* aObserver);
  void SwapLayerTreeObservers(uint64_t aLayer, uint64_t aOtherLayer);

  // Creates a new RemoteContentController for aTabId. Should only be called on
  // the main thread.
  //
  // aLayersId      The layers id for the browser corresponding to aTabId.
  // aContentParent The ContentParent for the process that the TabChild for
  //                aTabId lives in.
  // aBrowserParent The toplevel TabParent for aTabId.
  bool UpdateRemoteContentController(uint64_t aLayersId,
                                     dom::ContentParent* aContentParent,
                                     const dom::TabId& aTabId,
                                     dom::TabParent* aBrowserParent);

  void OnProcessLaunchComplete(GPUProcessHost* aHost) override;

private:
  // Called from our xpcom-shutdown observer.
  void OnXPCOMShutdown();

private:
  GPUProcessManager();

  // Permanently disable the GPU process and record a message why.
  void DisableGPUProcess(const char* aMessage);

  // Shutdown the GPU process.
  void DestroyProcess();

  DISALLOW_COPY_AND_ASSIGN(GPUProcessManager);

  class Observer final : public nsIObserver {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(GPUProcessManager* aManager);

  protected:
    ~Observer() {}

    GPUProcessManager* mManager;
  };
  friend class Observer;

private:
  RefPtr<Observer> mObserver;
  GPUProcessHost* mProcess;
  GPUChild* mGPUChild;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_GPUProcessManager_h_

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
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/gfx/GPUProcessHost.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/TaskFactory.h"
#include "mozilla/ipc/Transport.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
class nsBaseWidget;


namespace mozilla {
namespace layers {
class IAPZCTreeManager;
class CompositorSession;
class ClientLayerManager;
class CompositorUpdateObserver;
class PCompositorBridgeChild;
class PImageBridgeChild;
class RemoteCompositorSession;
} // namespace layers
namespace widget {
class CompositorWidget;
} // namespace widget
namespace dom {
class ContentParent;
class TabParent;
class PVideoDecoderManagerChild;
} // namespace dom
namespace ipc {
class GeckoChildProcessHost;
} // namespace ipc
namespace gfx {

class GPUChild;
class GPUProcessListener;
class PVRManagerChild;
class VsyncBridgeChild;
class VsyncIOThreadHolder;

// The GPUProcessManager is a singleton responsible for creating GPU-bound
// objects that may live in another process. Currently, it provides access
// to the compositor via CompositorBridgeParent.
class GPUProcessManager final : public GPUProcessHost::Listener
{
  friend class layers::RemoteCompositorSession;

  typedef layers::ClientLayerManager ClientLayerManager;
  typedef layers::CompositorSession CompositorSession;
  typedef layers::IAPZCTreeManager IAPZCTreeManager;
  typedef layers::CompositorUpdateObserver CompositorUpdateObserver;
  typedef layers::PCompositorBridgeChild PCompositorBridgeChild;
  typedef layers::PImageBridgeChild PImageBridgeChild;
  typedef layers::RemoteCompositorSession RemoteCompositorSession;

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

  RefPtr<CompositorSession> CreateTopLevelCompositor(
    nsBaseWidget* aWidget,
    ClientLayerManager* aLayerManager,
    CSSToLayoutDeviceScale aScale,
    bool aUseAPZ,
    bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize);

  bool CreateContentBridges(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PCompositorBridgeChild>* aOutCompositor,
    ipc::Endpoint<PImageBridgeChild>* aOutImageBridge,
    ipc::Endpoint<PVRManagerChild>* aOutVRBridge);
  bool CreateContentVideoDecoderManager(base::ProcessId aOtherProcess,
                                        ipc::Endpoint<dom::PVideoDecoderManagerChild>* aOutEndPoint);

  // This returns a reference to the APZCTreeManager to which
  // pan/zoom-related events can be sent.
  already_AddRefed<IAPZCTreeManager> GetAPZCTreeManagerForLayers(uint64_t aLayersId);

  // Maps the layer tree and process together so that aOwningPID is allowed
  // to access aLayersId across process.
  void MapLayerTreeId(uint64_t aLayersId, base::ProcessId aOwningId);

  // Checks to see if aLayersId and aRequestingPID have been mapped by MapLayerTreeId
  bool IsLayerTreeIdMapped(uint64_t aLayersId, base::ProcessId aRequestingId);

  // Allocate an ID that can be used to refer to a layer tree and
  // associated resources that live only on the compositor thread.
  //
  // Must run on the content main thread.
  uint64_t AllocateLayerTreeId();

  // Release compositor-thread resources referred to by |aID|.
  //
  // Must run on the content main thread.
  void DeallocateLayerTreeId(uint64_t aLayersId);

  void OnProcessLaunchComplete(GPUProcessHost* aHost) override;
  void OnProcessUnexpectedShutdown(GPUProcessHost* aHost) override;

  // Notify the GPUProcessManager that a top-level PGPU protocol has been
  // terminated. This may be called from any thread.
  void NotifyRemoteActorDestroyed(const uint64_t& aProcessToken);

  void AddListener(GPUProcessListener* aListener);
  void RemoveListener(GPUProcessListener* aListener);

  // Returns access to the PGPU protocol if a GPU process is present.
  GPUChild* GetGPUChild() {
    return mGPUChild;
  }

private:
  // Called from our xpcom-shutdown observer.
  void OnXPCOMShutdown();

  bool CreateContentCompositorBridge(base::ProcessId aOtherProcess,
                                     ipc::Endpoint<PCompositorBridgeChild>* aOutEndpoint);
  bool CreateContentImageBridge(base::ProcessId aOtherProcess,
                                ipc::Endpoint<PImageBridgeChild>* aOutEndpoint);
  bool CreateContentVRManager(base::ProcessId aOtherProcess,
                              ipc::Endpoint<PVRManagerChild>* aOutEndpoint);

  // Called from RemoteCompositorSession. We track remote sessions so we can
  // notify their owning widgets that the session must be restarted.
  void RegisterSession(RemoteCompositorSession* aSession);
  void UnregisterSession(RemoteCompositorSession* aSession);

private:
  GPUProcessManager();

  // Permanently disable the GPU process and record a message why.
  void DisableGPUProcess(const char* aMessage);

  // Shutdown the GPU process.
  void CleanShutdown();
  void DestroyProcess();

  void EnsureVsyncIOThread();
  void ShutdownVsyncIOThread();

  void EnsureImageBridgeChild();
  void EnsureVRManager();

  RefPtr<CompositorSession> CreateRemoteSession(
    nsBaseWidget* aWidget,
    ClientLayerManager* aLayerManager,
    const uint64_t& aRootLayerTreeId,
    CSSToLayoutDeviceScale aScale,
    bool aUseAPZ,
    bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize);

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
  ipc::TaskFactory<GPUProcessManager> mTaskFactory;
  RefPtr<VsyncIOThreadHolder> mVsyncIOThread;
  uint64_t mNextLayerTreeId;

  nsTArray<RefPtr<RemoteCompositorSession>> mRemoteSessions;
  nsTArray<GPUProcessListener*> mListeners;

  // Fields that are associated with the current GPU process.
  GPUProcessHost* mProcess;
  MOZ_INIT_OUTSIDE_CTOR uint64_t mProcessToken;
  GPUChild* mGPUChild;
  RefPtr<VsyncBridgeChild> mVsyncBridge;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_GPUProcessManager_h_

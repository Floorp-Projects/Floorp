/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
class nsBaseWidget;

namespace mozilla {
class MemoryReportingProcess;
class PVideoDecoderManagerChild;
namespace layers {
class IAPZCTreeManager;
class CompositorOptions;
class CompositorSession;
class CompositorUpdateObserver;
class PCompositorBridgeChild;
class PCompositorManagerChild;
class PImageBridgeChild;
class RemoteCompositorSession;
class InProcessCompositorSession;
class UiCompositorControllerChild;
}  // namespace layers
namespace widget {
class CompositorWidget;
}  // namespace widget
namespace dom {
class ContentParent;
class BrowserParent;
}  // namespace dom
namespace ipc {
class GeckoChildProcessHost;
}  // namespace ipc
namespace gfx {

class GPUChild;
class GPUProcessListener;
class PVRManagerChild;
class VsyncBridgeChild;
class VsyncIOThreadHolder;

// The GPUProcessManager is a singleton responsible for creating GPU-bound
// objects that may live in another process. Currently, it provides access
// to the compositor via CompositorBridgeParent.
class GPUProcessManager final : public GPUProcessHost::Listener {
  friend class layers::RemoteCompositorSession;
  friend class layers::InProcessCompositorSession;

  typedef layers::CompositorOptions CompositorOptions;
  typedef layers::CompositorSession CompositorSession;
  typedef layers::CompositorUpdateObserver CompositorUpdateObserver;
  typedef layers::IAPZCTreeManager IAPZCTreeManager;
  typedef layers::LayerManager LayerManager;
  typedef layers::LayersId LayersId;
  typedef layers::PCompositorBridgeChild PCompositorBridgeChild;
  typedef layers::PCompositorManagerChild PCompositorManagerChild;
  typedef layers::PImageBridgeChild PImageBridgeChild;
  typedef layers::RemoteCompositorSession RemoteCompositorSession;
  typedef layers::InProcessCompositorSession InProcessCompositorSession;
  typedef layers::UiCompositorControllerChild UiCompositorControllerChild;

 public:
  static void Initialize();
  static void Shutdown();
  static GPUProcessManager* Get();

  ~GPUProcessManager();

  // If not using a GPU process, launch a new GPU process asynchronously.
  void LaunchGPUProcess();

  // Ensure that GPU-bound methods can be used. If no GPU process is being
  // used, or one is launched and ready, this function returns immediately.
  // Otherwise it blocks until the GPU process has finished launching.
  bool EnsureGPUReady();

  already_AddRefed<CompositorSession> CreateTopLevelCompositor(
      nsBaseWidget* aWidget, LayerManager* aLayerManager,
      CSSToLayoutDeviceScale aScale, const CompositorOptions& aOptions,
      bool aUseExternalSurfaceSize, const gfx::IntSize& aSurfaceSize,
      bool* aRetry);

  bool CreateContentBridges(
      base::ProcessId aOtherProcess,
      mozilla::ipc::Endpoint<PCompositorManagerChild>* aOutCompositor,
      mozilla::ipc::Endpoint<PImageBridgeChild>* aOutImageBridge,
      mozilla::ipc::Endpoint<PVRManagerChild>* aOutVRBridge,
      mozilla::ipc::Endpoint<PVideoDecoderManagerChild>* aOutVideoManager,
      nsTArray<uint32_t>* aNamespaces);

  // Maps the layer tree and process together so that aOwningPID is allowed
  // to access aLayersId across process.
  void MapLayerTreeId(LayersId aLayersId, base::ProcessId aOwningId);

  // Release compositor-thread resources referred to by |aID|.
  //
  // Must run on the content main thread.
  void UnmapLayerTreeId(LayersId aLayersId, base::ProcessId aOwningId);

  // Checks to see if aLayersId and aRequestingPID have been mapped by
  // MapLayerTreeId
  bool IsLayerTreeIdMapped(LayersId aLayersId, base::ProcessId aRequestingId);

  // Allocate an ID that can be used to refer to a layer tree and
  // associated resources that live only on the compositor thread.
  //
  // Must run on the browser main thread.
  LayersId AllocateLayerTreeId();

  // Allocate an ID that can be used as Namespace and
  // Must run on the browser main thread.
  uint32_t AllocateNamespace();

  // Allocate a layers ID and connect it to a compositor. If the compositor is
  // null, the connect operation will not be performed, but an ID will still be
  // allocated. This must be called from the browser main thread.
  //
  // Note that a layer tree id is always allocated, even if this returns false.
  bool AllocateAndConnectLayerTreeId(PCompositorBridgeChild* aCompositorBridge,
                                     base::ProcessId aOtherPid,
                                     LayersId* aOutLayersId,
                                     CompositorOptions* aOutCompositorOptions);

  // Destroy and recreate all of the compositors
  void ResetCompositors();

  void OnProcessLaunchComplete(GPUProcessHost* aHost) override;
  void OnProcessUnexpectedShutdown(GPUProcessHost* aHost) override;
  void SimulateDeviceReset();
  void DisableWebRender(wr::WebRenderError aError);
  void NotifyWebRenderError(wr::WebRenderError aError);
  void OnInProcessDeviceReset();
  void OnRemoteProcessDeviceReset(GPUProcessHost* aHost) override;
  void NotifyListenersOnCompositeDeviceReset();

  // Notify the GPUProcessManager that a top-level PGPU protocol has been
  // terminated. This may be called from any thread.
  void NotifyRemoteActorDestroyed(const uint64_t& aProcessToken);

  void AddListener(GPUProcessListener* aListener);
  void RemoveListener(GPUProcessListener* aListener);

  // Send a message to the GPU process observer service to broadcast. Returns
  // true if the message was sent, false if not.
  bool NotifyGpuObservers(const char* aTopic);

  // Used for tests and diagnostics
  void KillProcess();

  // Returns -1 if there is no GPU process, or the platform pid for it.
  base::ProcessId GPUProcessPid();

  // If a GPU process is present, create a MemoryReportingProcess object.
  // Otherwise, return null.
  RefPtr<MemoryReportingProcess> GetProcessMemoryReporter();

  // Returns access to the PGPU protocol if a GPU process is present.
  GPUChild* GetGPUChild() { return mGPUChild; }

  // Returns whether or not a GPU process was ever launched.
  bool AttemptedGPUProcess() const { return mNumProcessAttempts > 0; }

 private:
  // Called from our xpcom-shutdown observer.
  void OnXPCOMShutdown();

  bool CreateContentCompositorManager(
      base::ProcessId aOtherProcess,
      mozilla::ipc::Endpoint<PCompositorManagerChild>* aOutEndpoint);
  bool CreateContentImageBridge(
      base::ProcessId aOtherProcess,
      mozilla::ipc::Endpoint<PImageBridgeChild>* aOutEndpoint);
  bool CreateContentVRManager(
      base::ProcessId aOtherProcess,
      mozilla::ipc::Endpoint<PVRManagerChild>* aOutEndpoint);
  void CreateContentVideoDecoderManager(
      base::ProcessId aOtherProcess,
      mozilla::ipc::Endpoint<PVideoDecoderManagerChild>* aOutEndPoint);

  // Called from RemoteCompositorSession. We track remote sessions so we can
  // notify their owning widgets that the session must be restarted.
  void RegisterRemoteProcessSession(RemoteCompositorSession* aSession);
  void UnregisterRemoteProcessSession(RemoteCompositorSession* aSession);

  // Called from InProcessCompositorSession. We track in process sessino so we
  // can notify their owning widgets that the session must be restarted
  void RegisterInProcessSession(InProcessCompositorSession* aSession);
  void UnregisterInProcessSession(InProcessCompositorSession* aSession);

  void RebuildRemoteSessions();
  void RebuildInProcessSessions();

  void FallbackToSoftware(const char* aMessage);

 private:
  GPUProcessManager();

  // Permanently disable the GPU process and record a message why.
  void DisableGPUProcess(const char* aMessage);

  // Shutdown the GPU process.
  void CleanShutdown();
  void DestroyProcess();

  void HandleProcessLost();

  void EnsureVsyncIOThread();
  void ShutdownVsyncIOThread();

  void EnsureProtocolsReady();
  void EnsureCompositorManagerChild();
  void EnsureImageBridgeChild();
  void EnsureVRManager();

#if defined(MOZ_WIDGET_ANDROID)
  already_AddRefed<UiCompositorControllerChild> CreateUiCompositorController(
      nsBaseWidget* aWidget, const LayersId aId);
#endif  // defined(MOZ_WIDGET_ANDROID)

  RefPtr<CompositorSession> CreateRemoteSession(
      nsBaseWidget* aWidget, LayerManager* aLayerManager,
      const LayersId& aRootLayerTreeId, CSSToLayoutDeviceScale aScale,
      const CompositorOptions& aOptions, bool aUseExternalSurfaceSize,
      const gfx::IntSize& aSurfaceSize);

  DISALLOW_COPY_AND_ASSIGN(GPUProcessManager);

  class Observer final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(GPUProcessManager* aManager);

   protected:
    virtual ~Observer() = default;

    GPUProcessManager* mManager;
  };
  friend class Observer;

 private:
  bool mDecodeVideoOnGpuProcess = true;

  RefPtr<Observer> mObserver;
  mozilla::ipc::TaskFactory<GPUProcessManager> mTaskFactory;
  RefPtr<VsyncIOThreadHolder> mVsyncIOThread;
  uint32_t mNextNamespace;
  uint32_t mIdNamespace;
  uint32_t mResourceId;
  uint32_t mNumProcessAttempts;

  nsTArray<RefPtr<RemoteCompositorSession>> mRemoteSessions;
  nsTArray<RefPtr<InProcessCompositorSession>> mInProcessSessions;
  nsTArray<GPUProcessListener*> mListeners;

  uint32_t mDeviceResetCount;
  TimeStamp mDeviceResetLastTime;

  // Fields that are associated with the current GPU process.
  GPUProcessHost* mProcess;
  uint64_t mProcessToken;
  GPUChild* mGPUChild;
  RefPtr<VsyncBridgeChild> mVsyncBridge;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_GPUProcessManager_h_

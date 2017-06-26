/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUProcessManager.h"

#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "GPUProcessListener.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorOptions.h"
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
#include "mozilla/dom/VideoDecoderManagerChild.h"
#include "mozilla/dom/VideoDecoderManagerParent.h"
#include "MediaPrefs.h"

#ifdef MOZ_CRASHREPORTER
# include "nsExceptionHandler.h"
#endif

#if defined(MOZ_WIDGET_ANDROID)
#include "mozilla/widget/AndroidUiThread.h"
#include "mozilla/layers/UiCompositorControllerChild.h"
#endif // defined(MOZ_WIDGET_ANDROID)

namespace mozilla {
namespace gfx {

using namespace mozilla::layers;

enum class FallbackType : uint32_t
{
  NONE = 0,
  DECODINGDISABLED,
  DISABLED,
};

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
   mNextNamespace(0),
   mIdNamespace(0),
   mResourceId(0),
   mNumProcessAttempts(0),
   mDeviceResetCount(0),
   mProcess(nullptr),
   mGPUChild(nullptr)
{
  MOZ_COUNT_CTOR(GPUProcessManager);

  mIdNamespace = AllocateNamespace();
  mObserver = new Observer(this);
  nsContentUtils::RegisterShutdownObserver(mObserver);

  mDeviceResetLastTime = TimeStamp::Now();

  LayerTreeOwnerTracker::Initialize();
}

GPUProcessManager::~GPUProcessManager()
{
  MOZ_COUNT_DTOR(GPUProcessManager);

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
GPUProcessManager::LaunchGPUProcess()
{
  if (mProcess) {
    return;
  }

  // Start the Vsync I/O thread so can use it as soon as the process launches.
  EnsureVsyncIOThread();

  mNumProcessAttempts++;

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
  if (!gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    return;
  }

  gfxConfig::SetFailed(Feature::GPU_PROCESS, FeatureStatus::Failed, aMessage);
  gfxCriticalNote << aMessage;

  gfxPlatform::NotifyGPUProcessDisabled();

  Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                        uint32_t(FallbackType::DISABLED));

  DestroyProcess();
  ShutdownVsyncIOThread();
}

bool
GPUProcessManager::EnsureGPUReady()
{
  if (mProcess && !mProcess->IsConnected()) {
    if (!mProcess->WaitForLaunch()) {
      // If this fails, we should have fired OnProcessLaunchComplete and
      // removed the process.
      MOZ_ASSERT(!mProcess && !mGPUChild);
      return false;
    }
  }

  if (mGPUChild && mGPUChild->EnsureGPUReady()) {
    return true;
  }

  return false;
}

void
GPUProcessManager::EnsureCompositorManagerChild()
{
  base::ProcessId gpuPid = EnsureGPUReady()
                           ? mGPUChild->OtherPid()
                           : base::GetCurrentProcId();

  if (CompositorManagerChild::IsInitialized(gpuPid)) {
    return;
  }

  if (!EnsureGPUReady()) {
    CompositorManagerChild::InitSameProcess(AllocateNamespace());
    return;
  }

  ipc::Endpoint<PCompositorManagerParent> parentPipe;
  ipc::Endpoint<PCompositorManagerChild> childPipe;
  nsresult rv = PCompositorManager::CreateEndpoints(
    mGPUChild->OtherPid(),
    base::GetCurrentProcId(),
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PCompositorManager endpoints");
    return;
  }

  mGPUChild->SendInitCompositorManager(Move(parentPipe));
  CompositorManagerChild::Init(Move(childPipe), AllocateNamespace());
}

void
GPUProcessManager::EnsureImageBridgeChild()
{
  if (ImageBridgeChild::GetSingleton()) {
    return;
  }

  if (!EnsureGPUReady()) {
    ImageBridgeChild::InitSameProcess(AllocateNamespace());
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
  ImageBridgeChild::InitWithGPUProcess(Move(childPipe), AllocateNamespace());
}

void
GPUProcessManager::EnsureVRManager()
{
  if (VRManagerChild::IsCreated()) {
    return;
  }

  if (!EnsureGPUReady()) {
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

#if defined(MOZ_WIDGET_ANDROID)
already_AddRefed<UiCompositorControllerChild>
GPUProcessManager::CreateUiCompositorController(nsBaseWidget* aWidget, const uint64_t aId)
{
  RefPtr<UiCompositorControllerChild> result;

  if (!EnsureGPUReady()) {
    result = UiCompositorControllerChild::CreateForSameProcess(aId);
  } else {
    ipc::Endpoint<PUiCompositorControllerParent> parentPipe;
    ipc::Endpoint<PUiCompositorControllerChild> childPipe;
    nsresult rv = PUiCompositorController::CreateEndpoints(
      mGPUChild->OtherPid(),
      base::GetCurrentProcId(),
      &parentPipe,
      &childPipe);
    if (NS_FAILED(rv)) {
      DisableGPUProcess("Failed to create PUiCompositorController endpoints");
      return nullptr;
    }

    mGPUChild->SendInitUiCompositorController(aId, Move(parentPipe));
    result = UiCompositorControllerChild::CreateForGPUProcess(mProcessToken, Move(childPipe));
  }
  if (result) {
    result->SetBaseWidget(aWidget);
  }
  return result.forget();
}
#endif // defined(MOZ_WIDGET_ANDROID)

void
GPUProcessManager::OnProcessLaunchComplete(GPUProcessHost* aHost)
{
  MOZ_ASSERT(mProcess && mProcess == aHost);

  if (!mProcess->IsConnected()) {
    DisableGPUProcess("Failed to connect GPU process");
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

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("GPUProcessStatus"),
    NS_LITERAL_CSTRING("Running"));
#endif
}

static bool
ShouldLimitDeviceResets(uint32_t count, int32_t deltaMilliseconds)
{
  // We decide to limit by comparing the amount of resets that have happened
  // and time since the last reset to two prefs.
  int32_t timeLimit = gfxPrefs::DeviceResetThresholdMilliseconds();
  int32_t countLimit = gfxPrefs::DeviceResetLimitCount();

  bool hasTimeLimit = timeLimit >= 0;
  bool hasCountLimit = countLimit >= 0;

  bool triggeredTime = deltaMilliseconds < timeLimit;
  bool triggeredCount = count > (uint32_t)countLimit;

  // If we have both prefs set then it needs to trigger both limits,
  // otherwise we only test the pref that is set or none
  if (hasTimeLimit && hasCountLimit) {
    return triggeredTime && triggeredCount;
  } else if (hasTimeLimit) {
    return triggeredTime;
  } else if (hasCountLimit) {
    return triggeredCount;
  }

  return false;
}

void
GPUProcessManager::TriggerDeviceResetForTesting()
{
  if (mProcess) {
    OnRemoteProcessDeviceReset(mProcess);
  } else {
    OnInProcessDeviceReset();
  }
}

void
GPUProcessManager::OnInProcessDeviceReset()
{
  RebuildInProcessSessions();
  NotifyListenersOnCompositeDeviceReset();
}

void
GPUProcessManager::OnRemoteProcessDeviceReset(GPUProcessHost* aHost)
{
  // Detect whether the device is resetting too quickly or too much
  // indicating that we should give up and use software
  mDeviceResetCount++;

  auto newTime = TimeStamp::Now();
  auto delta = (int32_t)(newTime - mDeviceResetLastTime).ToMilliseconds();
  mDeviceResetLastTime = newTime;

  if (ShouldLimitDeviceResets(mDeviceResetCount, delta)) {
    DestroyProcess();
    DisableGPUProcess("GPU processed experienced too many device resets");

    HandleProcessLost();
    return;
  }

  RebuildRemoteSessions();
  NotifyListenersOnCompositeDeviceReset();
}

void
GPUProcessManager::NotifyListenersOnCompositeDeviceReset()
{
  for (const auto& listener : mListeners) {
    listener->OnCompositorDeviceReset();
  }
}

void
GPUProcessManager::OnProcessUnexpectedShutdown(GPUProcessHost* aHost)
{
  MOZ_ASSERT(mProcess && mProcess == aHost);

  DestroyProcess();

  if (mNumProcessAttempts > uint32_t(gfxPrefs::GPUProcessMaxRestarts())) {
    char disableMessage[64];
    SprintfLiteral(disableMessage, "GPU process disabled after %d attempts",
                   mNumProcessAttempts);
    DisableGPUProcess(disableMessage);
  } else if (mNumProcessAttempts > uint32_t(gfxPrefs::GPUProcessMaxRestartsWithDecoder()) &&
             mDecodeVideoOnGpuProcess) {
    mDecodeVideoOnGpuProcess = false;
    Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                                     uint32_t(FallbackType::DECODINGDISABLED));
  } else {
    Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                                     uint32_t(FallbackType::NONE));
  }

  HandleProcessLost();
}

void
GPUProcessManager::HandleProcessLost()
{
  if (gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    LaunchGPUProcess();
  }

  // The shutdown and restart sequence for the GPU process is as follows:
  //
  //  (1) The GPU process dies. IPDL will enqueue an ActorDestroy message on
  //      each channel owning a bridge to the GPU process, on the thread
  //      owning that channel.
  //
  //  (2) The first channel to process its ActorDestroy message will post a
  //      message to the main thread to call NotifyRemoteActorDestroyed on
  //      the GPUProcessManager, which calls OnProcessUnexpectedShutdown if
  //      it has not handled shutdown for this process yet.
  //
  //  (3) We then notify each widget that its session with the compositor is
  //      now invalid. The widget is responsible for destroying its layer
  //      manager and CompositorBridgeChild. Note that at this stage, not
  //      all actors may have received ActorDestroy yet. CompositorBridgeChild
  //      may attempt to send messages, and if this happens, it will probably
  //      report a MsgDropped error. This is okay.
  //
  //  (4) At this point, the UI process has a clean slate: no layers should
  //      exist for the old compositor. We may make a decision on whether or
  //      not to re-launch the GPU process. Currently, we do not relaunch it,
  //      and any new compositors will be created in-process and will default
  //      to software.
  //
  //  (5) Next we notify each ContentParent of the lost connection. It will
  //      request new endpoints from the GPUProcessManager and forward them
  //      to its ContentChild. The parent-side of these endpoints may come
  //      from the compositor thread of the UI process, or the compositor
  //      thread of the GPU process. However, no actual compositors should
  //      exist yet.
  //
  //  (6) Each ContentChild will receive new endpoints. It will destroy its
  //      Compositor/ImageBridgeChild singletons and recreate them, as well
  //      as invalidate all retained layers.
  //
  //  (7) In addition, each ContentChild will ask each of its TabChildren
  //      to re-request association with the compositor for the window
  //      owning the tab. The sequence of calls looks like:
  //        (a) [CONTENT] ContentChild::RecvReinitRendering
  //        (b) [CONTENT] TabChild::ReinitRendering
  //        (c) [CONTENT] TabChild::SendEnsureLayersConnected
  //        (d)      [UI] TabParent::RecvEnsureLayersConnected
  //        (e)      [UI] RenderFrameParent::EnsureLayersConnected
  //        (f)      [UI] CompositorBridgeChild::SendNotifyChildRecreated
  //
  //      Note that at step (e), RenderFrameParent will call GetLayerManager
  //      on the nsIWidget owning the tab. This step ensures that a compositor
  //      exists for the window. If we decided to launch a new GPU Process,
  //      at this point we block until the process has launched and we're
  //      able to create a new window compositor. Otherwise, if compositing
  //      is now in-process, this will simply create a new
  //      CompositorBridgeParent in the UI process. If there are multiple tabs
  //      in the same window, additional tabs will simply return the already-
  //      established compositor.
  //
  //      Finally, this step serves one other crucial function: tabs must be
  //      associated with a window compositor or else they can't forward
  //      layer transactions. So this step both ensures that a compositor
  //      exists, and that the tab can forward layers.
  //
  //  (8) Last, if the window had no remote tabs, step (7) will not have
  //      applied, and the window will not have a new compositor just yet.
  //      The next refresh tick and paint will ensure that one exists, again
  //      via nsIWidget::GetLayerManager.
  RebuildRemoteSessions();

  // Notify content. This will ensure that each content process re-establishes
  // a connection to the compositor thread (whether it's in-process or in a
  // newly launched GPU process).
  for (const auto& listener : mListeners) {
    listener->OnCompositorUnexpectedShutdown();
  }
}

void
GPUProcessManager::RebuildRemoteSessions()
{
  // Build a list of sessions to notify, since notification might delete
  // entries from the list.
  nsTArray<RefPtr<RemoteCompositorSession>> sessions;
  for (auto& session : mRemoteSessions) {
    sessions.AppendElement(session);
  }

  // Notify each widget that we have lost the GPU process. This will ensure
  // that each widget destroys its layer manager and CompositorBridgeChild.
  for (const auto& session : sessions) {
    session->NotifySessionLost();
  }
}

void
GPUProcessManager::RebuildInProcessSessions()
{
  // Build a list of sessions to notify, since notification might delete
  // entries from the list.
  nsTArray<RefPtr<InProcessCompositorSession>> sessions;
  for (auto& session : mInProcessSessions) {
    sessions.AppendElement(session);
  }

  // Notify each widget that we have lost the GPU process. This will ensure
  // that each widget destroys its layer manager and CompositorBridgeChild.
  for (const auto& session : sessions) {
    session->NotifySessionLost();
  }
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
  OnProcessUnexpectedShutdown(mProcess);
}

void
GPUProcessManager::CleanShutdown()
{
  DestroyProcess();
  mVsyncIOThread = nullptr;
}

void
GPUProcessManager::KillProcess()
{
  if (!mProcess) {
    return;
  }

  mProcess->KillProcess();
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
  if (mVsyncBridge) {
    mVsyncBridge->Close();
    mVsyncBridge = nullptr;
  }

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("GPUProcessStatus"),
    NS_LITERAL_CSTRING("Destroyed"));
#endif
}

RefPtr<CompositorSession>
GPUProcessManager::CreateTopLevelCompositor(nsBaseWidget* aWidget,
                                            LayerManager* aLayerManager,
                                            CSSToLayoutDeviceScale aScale,
                                            const CompositorOptions& aOptions,
                                            bool aUseExternalSurfaceSize,
                                            const gfx::IntSize& aSurfaceSize)
{
  uint64_t layerTreeId = AllocateLayerTreeId();

  EnsureCompositorManagerChild();
  EnsureImageBridgeChild();
  EnsureVRManager();

  RefPtr<CompositorSession> session;

  if (EnsureGPUReady()) {
    session = CreateRemoteSession(
      aWidget,
      aLayerManager,
      layerTreeId,
      aScale,
      aOptions,
      aUseExternalSurfaceSize,
      aSurfaceSize);
    if (!session) {
      // We couldn't create a remote compositor, so abort the process.
      DisableGPUProcess("Failed to create remote compositor");
    }
  }

  if (!session) {
    session = InProcessCompositorSession::Create(
      aWidget,
      aLayerManager,
      layerTreeId,
      aScale,
      aOptions,
      aUseExternalSurfaceSize,
      aSurfaceSize,
      AllocateNamespace());
  }

#if defined(MOZ_WIDGET_ANDROID)
  if (session) {
    // Nothing to do if controller gets a nullptr
    RefPtr<UiCompositorControllerChild> controller = CreateUiCompositorController(aWidget, session->RootLayerTreeId());
    session->SetUiCompositorControllerChild(controller);
  }
#endif // defined(MOZ_WIDGET_ANDROID)

  return session;
}

RefPtr<CompositorSession>
GPUProcessManager::CreateRemoteSession(nsBaseWidget* aWidget,
                                       LayerManager* aLayerManager,
                                       const uint64_t& aRootLayerTreeId,
                                       CSSToLayoutDeviceScale aScale,
                                       const CompositorOptions& aOptions,
                                       bool aUseExternalSurfaceSize,
                                       const gfx::IntSize& aSurfaceSize)
{
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
  CompositorWidgetInitData initData;
  aWidget->GetCompositorWidgetInitData(&initData);

  RefPtr<CompositorBridgeChild> child =
    CompositorManagerChild::CreateWidgetCompositorBridge(
      mProcessToken,
      aLayerManager,
      AllocateNamespace(),
      aScale,
      aOptions,
      aUseExternalSurfaceSize,
      aSurfaceSize);
  if (!child) {
    gfxCriticalNote << "Failed to create CompositorBridgeChild";
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
  if (aOptions.UseAPZ()) {
    PAPZCTreeManagerChild* papz = child->SendPAPZCTreeManagerConstructor(0);
    if (!papz) {
      return nullptr;
    }
    apz = static_cast<APZCTreeManagerChild*>(papz);
  }

  RefPtr<RemoteCompositorSession> session =
    new RemoteCompositorSession(aWidget, child, widget, apz, aRootLayerTreeId);
  return session.forget();
#else
  gfxCriticalNote << "Platform does not support out-of-process compositing";
  return nullptr;
#endif
}

bool
GPUProcessManager::CreateContentBridges(base::ProcessId aOtherProcess,
                                        ipc::Endpoint<PCompositorManagerChild>* aOutCompositor,
                                        ipc::Endpoint<PImageBridgeChild>* aOutImageBridge,
                                        ipc::Endpoint<PVRManagerChild>* aOutVRBridge,
                                        ipc::Endpoint<dom::PVideoDecoderManagerChild>* aOutVideoManager,
                                        nsTArray<uint32_t>* aNamespaces)
{
  if (!CreateContentCompositorManager(aOtherProcess, aOutCompositor) ||
      !CreateContentImageBridge(aOtherProcess, aOutImageBridge) ||
      !CreateContentVRManager(aOtherProcess, aOutVRBridge))
  {
    return false;
  }
  // VideoDeocderManager is only supported in the GPU process, so we allow this to be
  // fallible.
  CreateContentVideoDecoderManager(aOtherProcess, aOutVideoManager);
  // Allocates 3 namespaces(for CompositorManagerChild, CompositorBridgeChild and ImageBridgeChild)
  aNamespaces->AppendElement(AllocateNamespace());
  aNamespaces->AppendElement(AllocateNamespace());
  aNamespaces->AppendElement(AllocateNamespace());
  return true;
}

bool
GPUProcessManager::CreateContentCompositorManager(base::ProcessId aOtherProcess,
                                                  ipc::Endpoint<PCompositorManagerChild>* aOutEndpoint)
{
  ipc::Endpoint<PCompositorManagerParent> parentPipe;
  ipc::Endpoint<PCompositorManagerChild> childPipe;

  base::ProcessId gpuPid = EnsureGPUReady()
                           ? mGPUChild->OtherPid()
                           : base::GetCurrentProcId();

  nsresult rv = PCompositorManager::CreateEndpoints(
    gpuPid,
    aOtherProcess,
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor manager: " << hexa(int(rv));
    return false;
  }

  if (EnsureGPUReady()) {
    mGPUChild->SendNewContentCompositorManager(Move(parentPipe));
  } else {
    CompositorManagerParent::Create(Move(parentPipe));
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

  if (EnsureGPUReady()) {
    mGPUChild->SendNewContentImageBridge(Move(parentPipe));
  } else {
    if (!ImageBridgeParent::CreateForContent(Move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = Move(childPipe);
  return true;
}

base::ProcessId
GPUProcessManager::GPUProcessPid()
{
  base::ProcessId gpuPid = mGPUChild
                           ? mGPUChild->OtherPid()
                           : -1;
  return gpuPid;
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

  if (EnsureGPUReady()) {
    mGPUChild->SendNewContentVRManager(Move(parentPipe));
  } else {
    if (!VRManagerParent::CreateForContent(Move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = Move(childPipe);
  return true;
}

void
GPUProcessManager::CreateContentVideoDecoderManager(base::ProcessId aOtherProcess,
                                                    ipc::Endpoint<dom::PVideoDecoderManagerChild>* aOutEndpoint)
{
  if (!EnsureGPUReady() ||
      !MediaPrefs::PDMUseGPUDecoder() ||
      !mDecodeVideoOnGpuProcess) {
    return;
  }

  ipc::Endpoint<dom::PVideoDecoderManagerParent> parentPipe;
  ipc::Endpoint<dom::PVideoDecoderManagerChild> childPipe;

  nsresult rv = dom::PVideoDecoderManager::CreateEndpoints(
    mGPUChild->OtherPid(),
    aOtherProcess,
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content video decoder: " << hexa(int(rv));
    return;
  }

  mGPUChild->SendNewContentVideoDecoderManager(Move(parentPipe));

  *aOutEndpoint = Move(childPipe);
  return;
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

  if (EnsureGPUReady()) {
    mGPUChild->SendAddLayerTreeIdMapping(LayerTreeIdMapping(aLayersId, aOwningId));
  }
}

void
GPUProcessManager::UnmapLayerTreeId(uint64_t aLayersId, base::ProcessId aOwningId)
{
  LayerTreeOwnerTracker::Get()->Unmap(aLayersId, aOwningId);

  if (EnsureGPUReady()) {
    mGPUChild->SendRemoveLayerTreeIdMapping(LayerTreeIdMapping(aLayersId, aOwningId));
    return;
  }
  CompositorBridgeParent::DeallocateLayerTreeId(aLayersId);
}

bool
GPUProcessManager::IsLayerTreeIdMapped(uint64_t aLayersId, base::ProcessId aRequestingId)
{
  return LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, aRequestingId);
}

uint64_t
GPUProcessManager::AllocateLayerTreeId()
{
  // Allocate tree id by using id namespace.
  // By it, tree id does not conflict with external image id and
  // async image pipeline id.
  MOZ_ASSERT(NS_IsMainThread());
  ++mResourceId;
  if (mResourceId == UINT32_MAX) {
    // Move to next id namespace.
    mIdNamespace = AllocateNamespace();
    mResourceId = 1;
  }

  uint64_t layerTreeId = mIdNamespace;
  layerTreeId = (layerTreeId << 32) | mResourceId;
  return layerTreeId;
}

uint32_t
GPUProcessManager::AllocateNamespace()
{
  MOZ_ASSERT(NS_IsMainThread());
  return ++mNextNamespace;
}

bool
GPUProcessManager::AllocateAndConnectLayerTreeId(PCompositorBridgeChild* aCompositorBridge,
                                                 base::ProcessId aOtherPid,
                                                 uint64_t* aOutLayersId,
                                                 CompositorOptions* aOutCompositorOptions)
{
  uint64_t layersId = AllocateLayerTreeId();
  *aOutLayersId = layersId;

  if (!mGPUChild || !aCompositorBridge) {
    // If we're not remoting to another process, or there is no compositor,
    // then we'll send at most one message. In this case we can just keep
    // the old behavior of making sure the mapping occurs, and maybe sending
    // a creation notification.
    MapLayerTreeId(layersId, aOtherPid);
    if (!aCompositorBridge) {
      return false;
    }
    return aCompositorBridge->SendNotifyChildCreated(layersId, aOutCompositorOptions);
  }

  // Use the combined message path.
  LayerTreeOwnerTracker::Get()->Map(layersId, aOtherPid);
  return aCompositorBridge->SendMapAndNotifyChildCreated(layersId, aOtherPid, aOutCompositorOptions);
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

void
GPUProcessManager::RegisterRemoteProcessSession(RemoteCompositorSession* aSession)
{
  mRemoteSessions.AppendElement(aSession);
}

void
GPUProcessManager::UnregisterRemoteProcessSession(RemoteCompositorSession* aSession)
{
  mRemoteSessions.RemoveElement(aSession);
}

void
GPUProcessManager::RegisterInProcessSession(InProcessCompositorSession* aSession)
{
  mInProcessSessions.AppendElement(aSession);
}

void
GPUProcessManager::UnregisterInProcessSession(InProcessCompositorSession* aSession)
{
  mInProcessSessions.RemoveElement(aSession);
}

void
GPUProcessManager::AddListener(GPUProcessListener* aListener)
{
  mListeners.AppendElement(aListener);
}

void
GPUProcessManager::RemoveListener(GPUProcessListener* aListener)
{
  mListeners.RemoveElement(aListener);
}

bool
GPUProcessManager::NotifyGpuObservers(const char* aTopic)
{
  if (!EnsureGPUReady()) {
    return false;
  }
  nsCString topic(aTopic);
  mGPUChild->SendNotifyGpuObservers(topic);
  return true;
}

class GPUMemoryReporter : public MemoryReportingProcess
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GPUMemoryReporter, override)

  bool IsAlive() const override {
    if (GPUProcessManager* gpm = GPUProcessManager::Get()) {
      return !!gpm->GetGPUChild();
    }
    return false;
  }

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const dom::MaybeFileDesc& aDMDFile) override
  {
    GPUChild* child = GetChild();
    if (!child) {
      return false;
    }

    return child->SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile);
  }

  int32_t Pid() const override {
    if (GPUChild* child = GetChild()) {
      return (int32_t)child->OtherPid();
    }
    return 0;
  }

private:
  GPUChild* GetChild() const {
    if (GPUProcessManager* gpm = GPUProcessManager::Get()) {
      if (GPUChild* child = gpm->GetGPUChild()) {
        return child;
      }
    }
    return nullptr;
  }

protected:
  ~GPUMemoryReporter() = default;
};

RefPtr<MemoryReportingProcess>
GPUProcessManager::GetProcessMemoryReporter()
{
  if (!EnsureGPUReady()) {
    return nullptr;
  }
  return new GPUMemoryReporter();
}

} // namespace gfx
} // namespace mozilla

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/dom/ContentChild.h"   // for ContentChild
#include "mozilla/dom/TabChild.h"       // for TabChild
#include "mozilla/dom/TabGroup.h"       // for TabGroup
#include "VsyncSource.h"

namespace mozilla {
namespace layers {

StaticRefPtr<CompositorManagerChild> CompositorManagerChild::sInstance;

/* static */ bool
CompositorManagerChild::IsInitialized(base::ProcessId aGPUPid)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Since GPUChild and CompositorManagerChild will race on ActorDestroy, we
  // cannot know if the CompositorManagerChild is about to be released but has
  // yet to be. As such, we need to verify the GPU PID matches as well.
  return sInstance && sInstance->CanSend() && sInstance->OtherPid() == aGPUPid;
}

/* static */ bool
CompositorManagerChild::InitSameProcess(uint32_t aNamespace)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(IsInitialized(base::GetCurrentProcId()))) {
    MOZ_ASSERT_UNREACHABLE("Already initialized same process");
    return false;
  }

  RefPtr<CompositorManagerParent> parent =
    CompositorManagerParent::CreateSameProcess();
  sInstance = new CompositorManagerChild(parent, aNamespace);
  return true;
}

/* static */ bool
CompositorManagerChild::Init(Endpoint<PCompositorManagerChild>&& aEndpoint,
                             uint32_t aNamespace)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    // Since GPUChild and CompositorManagerChild will race on ActorDestroy, we
    // cannot know if the CompositorManagerChild has yet to be released or not.
    // To avoid an unnecessary reinitialization, we verify the GPU PID actually
    // changed.
    MOZ_ASSERT(sInstance->mNamespace != aNamespace);
    MOZ_RELEASE_ASSERT(sInstance->OtherPid() != aEndpoint.OtherPid());
  }

  sInstance = new CompositorManagerChild(Move(aEndpoint), aNamespace);
  return sInstance->CanSend();
}

/* static */ void
CompositorManagerChild::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  CompositorBridgeChild::ShutDown();

  if (!sInstance) {
    return;
  }

  sInstance->Close();
  sInstance = nullptr;
}

/* static */ bool
CompositorManagerChild::CreateContentCompositorBridge(uint32_t aNamespace)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance)) {
    return false;
  }

  CompositorBridgeOptions options = ContentCompositorOptions();
  PCompositorBridgeChild* pbridge =
    sInstance->SendPCompositorBridgeConstructor(options);
  if (NS_WARN_IF(!pbridge)) {
    return true;
  }

  auto bridge = static_cast<CompositorBridgeChild*>(pbridge);
  bridge->InitForContent(aNamespace);
  return true;
}

/* static */ already_AddRefed<CompositorBridgeChild>
CompositorManagerChild::CreateWidgetCompositorBridge(uint64_t aProcessToken,
                                                     LayerManager* aLayerManager,
                                                     uint32_t aNamespace,
                                                     CSSToLayoutDeviceScale aScale,
                                                     const CompositorOptions& aOptions,
                                                     bool aUseExternalSurfaceSize,
                                                     const gfx::IntSize& aSurfaceSize)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance)) {
    return nullptr;
  }

  TimeDuration vsyncRate =
    gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay().GetVsyncRate();

  CompositorBridgeOptions options =
    WidgetCompositorOptions(aScale, vsyncRate, aOptions,
                            aUseExternalSurfaceSize, aSurfaceSize);
  PCompositorBridgeChild* pbridge =
    sInstance->SendPCompositorBridgeConstructor(options);
  if (NS_WARN_IF(!pbridge)) {
    return nullptr;
  }

  RefPtr<CompositorBridgeChild> bridge =
    static_cast<CompositorBridgeChild*>(pbridge);
  bridge->InitForWidget(aProcessToken, aLayerManager, aNamespace);
  return bridge.forget();
}

/* static */ already_AddRefed<CompositorBridgeChild>
CompositorManagerChild::CreateSameProcessWidgetCompositorBridge(LayerManager* aLayerManager,
                                                                uint32_t aNamespace)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance)) {
    return nullptr;
  }

  CompositorBridgeOptions options = SameProcessWidgetCompositorOptions();
  PCompositorBridgeChild* pbridge =
    sInstance->SendPCompositorBridgeConstructor(options);
  if (NS_WARN_IF(!pbridge)) {
    return nullptr;
  }

  RefPtr<CompositorBridgeChild> bridge =
    static_cast<CompositorBridgeChild*>(pbridge);
  bridge->InitForWidget(1, aLayerManager, aNamespace);
  return bridge.forget();
}

CompositorManagerChild::CompositorManagerChild(CompositorManagerParent* aParent,
                                               uint32_t aNamespace)
  : mCanSend(false)
  , mNamespace(aNamespace)
  , mResourceId(0)
{
  MOZ_ASSERT(aParent);

  SetOtherProcessId(base::GetCurrentProcId());
  MessageLoop* loop = CompositorThreadHolder::Loop();
  ipc::MessageChannel* channel = aParent->GetIPCChannel();
  if (NS_WARN_IF(!Open(channel, loop, ipc::ChildSide))) {
    return;
  }

  mCanSend = true;
  AddRef();
  SetReplyTimeout();
}

CompositorManagerChild::CompositorManagerChild(Endpoint<PCompositorManagerChild>&& aEndpoint,
                                               uint32_t aNamespace)
  : mCanSend(false)
  , mNamespace(aNamespace)
  , mResourceId(0)
{
  if (NS_WARN_IF(!aEndpoint.Bind(this))) {
    return;
  }

  mCanSend = true;
  AddRef();
  SetReplyTimeout();
}

void
CompositorManagerChild::DeallocPCompositorManagerChild()
{
  MOZ_ASSERT(!mCanSend);
  Release();
}

void
CompositorManagerChild::ActorDestroy(ActorDestroyReason aReason)
{
  mCanSend = false;
  if (sInstance == this) {
    sInstance = nullptr;
  }
}

PCompositorBridgeChild*
CompositorManagerChild::AllocPCompositorBridgeChild(const CompositorBridgeOptions& aOptions)
{
  CompositorBridgeChild* child = new CompositorBridgeChild(this);
  child->AddRef();
  return child;
}

bool
CompositorManagerChild::DeallocPCompositorBridgeChild(PCompositorBridgeChild* aActor)
{
  static_cast<CompositorBridgeChild*>(aActor)->Release();
  return true;
}

void
CompositorManagerChild::HandleFatalError(const char* aName, const char* aMsg) const
{
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aName, aMsg, OtherPid());
}

void
CompositorManagerChild::ProcessingError(Result aCode, const char* aReason)
{
  if (aCode != MsgDropped) {
    gfxDevCrash(gfx::LogReason::ProcessingError) << "Processing error in CompositorBridgeChild: " << int(aCode);
  }
}

already_AddRefed<nsIEventTarget>
CompositorManagerChild::GetSpecificMessageEventTarget(const Message& aMsg)
{
  if (aMsg.type() != PCompositorBridge::Msg_DidComposite__ID) {
    return nullptr;
  }

  uint64_t layersId;
  PickleIterator iter(aMsg);
  if (!IPC::ReadParam(&aMsg, &iter, &layersId)) {
    return nullptr;
  }

  TabChild* tabChild = TabChild::GetFrom(layersId);
  if (!tabChild) {
    return nullptr;
  }

  return do_AddRef(tabChild->TabGroup()->EventTargetFor(TaskCategory::Other));
}

void
CompositorManagerChild::SetReplyTimeout()
{
#ifndef DEBUG
  // Add a timeout for release builds to kill GPU process when it hangs.
  // Don't apply timeout when using web render as it tend to timeout frequently.
  if (XRE_IsParentProcess() && !gfxVars::UseWebRender()) {
    int32_t timeout = gfxPrefs::GPUProcessIPCReplyTimeoutMs();
    SetReplyTimeoutMs(timeout);
  }
#endif
}

bool
CompositorManagerChild::ShouldContinueFromReplyTimeout()
{
  if (XRE_IsParentProcess()) {
    gfxCriticalNote << "Killing GPU process due to IPC reply timeout";
    GPUProcessManager::Get()->KillProcess();
  }
  return false;
}

} // namespace layers
} // namespace mozilla

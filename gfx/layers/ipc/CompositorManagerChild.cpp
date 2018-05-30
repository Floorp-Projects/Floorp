/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CompositorManagerChild.h"

#include "gfxPrefs.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/dom/ContentChild.h"   // for ContentChild
#include "mozilla/dom/TabChild.h"       // for TabChild
#include "mozilla/dom/TabGroup.h"       // for TabGroup
#include "VsyncSource.h"

namespace mozilla {
namespace layers {

using gfx::GPUProcessManager;

StaticRefPtr<CompositorManagerChild> CompositorManagerChild::sInstance;

/* static */ bool
CompositorManagerChild::IsInitialized(uint64_t aProcessToken)
{
  MOZ_ASSERT(NS_IsMainThread());
  return sInstance && sInstance->CanSend() &&
         sInstance->mProcessToken == aProcessToken;
}

/* static */ void
CompositorManagerChild::InitSameProcess(uint32_t aNamespace,
                                        uint64_t aProcessToken)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(IsInitialized(aProcessToken))) {
    MOZ_ASSERT_UNREACHABLE("Already initialized same process");
    return;
  }

  RefPtr<CompositorManagerParent> parent =
    CompositorManagerParent::CreateSameProcess();
  RefPtr<CompositorManagerChild> child =
    new CompositorManagerChild(parent, aProcessToken, aNamespace);
  if (NS_WARN_IF(!child->CanSend())) {
    MOZ_DIAGNOSTIC_ASSERT(false, "Failed to open same process protocol");
    return;
  }

  parent->BindComplete();
  sInstance = child.forget();
}

/* static */ bool
CompositorManagerChild::Init(Endpoint<PCompositorManagerChild>&& aEndpoint,
                             uint32_t aNamespace,
                             uint64_t aProcessToken /* = 0 */)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sInstance) {
    MOZ_ASSERT(sInstance->mNamespace != aNamespace);
  }

  sInstance = new CompositorManagerChild(std::move(aEndpoint), aProcessToken,
                                         aNamespace);
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

/* static */ void
CompositorManagerChild::OnGPUProcessLost(uint64_t aProcessToken)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Since GPUChild and CompositorManagerChild will race on ActorDestroy, we
  // cannot know if the CompositorManagerChild is about to be released but has
  // yet to be. As such, we want to pre-emptively set mCanSend to false.
  if (sInstance && sInstance->mProcessToken == aProcessToken) {
    sInstance->mCanSend = false;
  }
}

/* static */ bool
CompositorManagerChild::CreateContentCompositorBridge(uint32_t aNamespace)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!sInstance || !sInstance->CanSend())) {
    return false;
  }

  CompositorBridgeOptions options = ContentCompositorOptions();
  PCompositorBridgeChild* pbridge =
    sInstance->SendPCompositorBridgeConstructor(options);
  if (NS_WARN_IF(!pbridge)) {
    return false;
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
  if (NS_WARN_IF(!sInstance || !sInstance->CanSend())) {
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
  if (NS_WARN_IF(!sInstance || !sInstance->CanSend())) {
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
                                               uint64_t aProcessToken,
                                               uint32_t aNamespace)
  : mProcessToken(aProcessToken)
  , mNamespace(aNamespace)
  , mResourceId(0)
  , mCanSend(false)
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
                                               uint64_t aProcessToken,
                                               uint32_t aNamespace)
  : mProcessToken(aProcessToken)
  , mNamespace(aNamespace)
  , mResourceId(0)
  , mCanSend(false)
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
CompositorManagerChild::HandleFatalError(const char* aMsg) const
{
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
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
  if (aMsg.type() == PCompositorBridge::Msg_DidComposite__ID) {
    LayersId layersId;
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

  if (aMsg.type() == PCompositorBridge::Msg_ParentAsyncMessages__ID) {
    return do_AddRef(SystemGroup::EventTargetFor(TaskCategory::Other));
  }

  return nullptr;
}

void
CompositorManagerChild::SetReplyTimeout()
{
#ifndef DEBUG
  // Add a timeout for release builds to kill GPU process when it hangs.
  if (XRE_IsParentProcess() &&
      GPUProcessManager::Get()->GetGPUChild()) {
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
    MOZ_DIAGNOSTIC_ASSERT(GPUProcessManager::Get()->GetGPUChild());
    GPUProcessManager::Get()->KillProcess();
  }
  return false;
}

} // namespace layers
} // namespace mozilla

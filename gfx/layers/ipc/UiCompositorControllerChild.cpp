/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/UiCompositorControllerChild.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/UiCompositorControllerMessageTypes.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/StaticPtr.h"
#include "nsBaseWidget.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/widget/AndroidUiThread.h"

static RefPtr<nsThread> GetUiThread() { return mozilla::GetAndroidUiThread(); }
#else
static RefPtr<nsThread> GetUiThread() {
  MOZ_CRASH("Platform does not support UiCompositorController");
  return nullptr;
}
#endif  // defined(MOZ_WIDGET_ANDROID)

static bool IsOnUiThread() {
  return GetUiThread()->SerialEventTarget()->IsOnCurrentThread();
}

namespace mozilla {
namespace layers {

// public:
/* static */
RefPtr<UiCompositorControllerChild>
UiCompositorControllerChild::CreateForSameProcess(
    const LayersId& aRootLayerTreeId) {
  RefPtr<UiCompositorControllerChild> child =
      new UiCompositorControllerChild(0);
  child->mParent = new UiCompositorControllerParent(aRootLayerTreeId);
  GetUiThread()->Dispatch(
      NewRunnableMethod(
          "layers::UiCompositorControllerChild::OpenForSameProcess", child,
          &UiCompositorControllerChild::OpenForSameProcess),
      nsIThread::DISPATCH_NORMAL);
  return child;
}

/* static */
RefPtr<UiCompositorControllerChild>
UiCompositorControllerChild::CreateForGPUProcess(
    const uint64_t& aProcessToken,
    Endpoint<PUiCompositorControllerChild>&& aEndpoint) {
  RefPtr<UiCompositorControllerChild> child =
      new UiCompositorControllerChild(aProcessToken);

  RefPtr<nsIRunnable> task =
      NewRunnableMethod<Endpoint<PUiCompositorControllerChild>&&>(
          "layers::UiCompositorControllerChild::OpenForGPUProcess", child,
          &UiCompositorControllerChild::OpenForGPUProcess,
          std::move(aEndpoint));

  GetUiThread()->Dispatch(task.forget(), nsIThread::DISPATCH_NORMAL);
  return child;
}

bool UiCompositorControllerChild::Pause() {
  if (!mIsOpen) {
    return false;
  }
  return SendPause();
}

bool UiCompositorControllerChild::Resume() {
  if (!mIsOpen) {
    return false;
  }
  return SendResume();
}

bool UiCompositorControllerChild::ResumeAndResize(const int32_t& aX,
                                                  const int32_t& aY,
                                                  const int32_t& aWidth,
                                                  const int32_t& aHeight) {
  if (!mIsOpen) {
    mResize = Some(gfx::IntRect(aX, aY, aWidth, aHeight));
    // Since we are caching these values, pretend the call succeeded.
    return true;
  }
  return SendResumeAndResize(aX, aY, aWidth, aHeight);
}

bool UiCompositorControllerChild::InvalidateAndRender() {
  if (!mIsOpen) {
    return false;
  }
  return SendInvalidateAndRender();
}

bool UiCompositorControllerChild::SetMaxToolbarHeight(const int32_t& aHeight) {
  if (!mIsOpen) {
    mMaxToolbarHeight = Some(aHeight);
    // Since we are caching this value, pretend the call succeeded.
    return true;
  }
  return SendMaxToolbarHeight(aHeight);
}

bool UiCompositorControllerChild::SetFixedBottomOffset(int32_t aOffset) {
  return SendFixedBottomOffset(aOffset);
}

bool UiCompositorControllerChild::ToolbarAnimatorMessageFromUI(
    const int32_t& aMessage) {
  if (!mIsOpen) {
    return false;
  }

  if (aMessage == IS_COMPOSITOR_CONTROLLER_OPEN) {
    RecvToolbarAnimatorMessageFromCompositor(COMPOSITOR_CONTROLLER_OPEN);
  }

  return true;
}

bool UiCompositorControllerChild::SetDefaultClearColor(const uint32_t& aColor) {
  if (!mIsOpen) {
    mDefaultClearColor = Some(aColor);
    // Since we are caching this value, pretend the call succeeded.
    return true;
  }

  return SendDefaultClearColor(aColor);
}

bool UiCompositorControllerChild::RequestScreenPixels() {
  if (!mIsOpen) {
    return false;
  }

  return SendRequestScreenPixels();
}

bool UiCompositorControllerChild::EnableLayerUpdateNotifications(
    const bool& aEnable) {
  if (!mIsOpen) {
    mLayerUpdateEnabled = Some(aEnable);
    // Since we are caching this value, pretend the call succeeded.
    return true;
  }

  return SendEnableLayerUpdateNotifications(aEnable);
}

void UiCompositorControllerChild::Destroy() {
  if (!IsOnUiThread()) {
    GetUiThread()->Dispatch(
        NewRunnableMethod("layers::UiCompositorControllerChild::Destroy", this,
                          &UiCompositorControllerChild::Destroy),
        nsIThread::DISPATCH_SYNC);
    return;
  }

  if (mWidget) {
    // Dispatch mWidget to main thread to prevent it from being destructed by
    // the ui thread.
    RefPtr<nsIWidget> widget = std::move(mWidget);
    NS_ReleaseOnMainThread("UiCompositorControllerChild::mWidget",
                           widget.forget());
  }

  if (mIsOpen) {
    // Close the underlying IPC channel.
    PUiCompositorControllerChild::Close();
    mIsOpen = false;
  }
}

void UiCompositorControllerChild::SetBaseWidget(nsBaseWidget* aWidget) {
  mWidget = aWidget;
}

bool UiCompositorControllerChild::DeallocPixelBuffer(Shmem& aMem) {
  return DeallocShmem(aMem);
}

// protected:
void UiCompositorControllerChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIsOpen = false;
  mParent = nullptr;

  if (mProcessToken) {
    gfx::GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
    mProcessToken = 0;
  }
}

void UiCompositorControllerChild::ActorDealloc() {
  if (mParent) {
    mParent = nullptr;
  }
  Release();
}

void UiCompositorControllerChild::ProcessingError(Result aCode,
                                                  const char* aReason) {
  if (aCode != MsgDropped) {
    gfxDevCrash(gfx::LogReason::ProcessingError)
        << "Processing error in UiCompositorControllerChild: " << int(aCode);
  }
}

void UiCompositorControllerChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

mozilla::ipc::IPCResult
UiCompositorControllerChild::RecvToolbarAnimatorMessageFromCompositor(
    const int32_t& aMessage) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mWidget) {
    mWidget->RecvToolbarAnimatorMessageFromCompositor(aMessage);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerChild::RecvRootFrameMetrics(
    const ScreenPoint& aScrollOffset, const CSSToScreenScale& aZoom) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mWidget) {
    mWidget->UpdateRootFrameMetrics(aScrollOffset, aZoom);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

mozilla::ipc::IPCResult UiCompositorControllerChild::RecvScreenPixels(
    ipc::Shmem&& aMem, const ScreenIntSize& aSize) {
#if defined(MOZ_WIDGET_ANDROID)
  if (mWidget) {
    mWidget->RecvScreenPixels(std::move(aMem), aSize);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  return IPC_OK();
}

// private:
UiCompositorControllerChild::UiCompositorControllerChild(
    const uint64_t& aProcessToken)
    : mIsOpen(false), mProcessToken(aProcessToken), mWidget(nullptr) {}

UiCompositorControllerChild::~UiCompositorControllerChild() = default;

void UiCompositorControllerChild::OpenForSameProcess() {
  MOZ_ASSERT(IsOnUiThread());

  mIsOpen = Open(mParent->GetIPCChannel(),
                 mozilla::layers::CompositorThreadHolder::Loop(),
                 mozilla::ipc::ChildSide);

  if (!mIsOpen) {
    mParent = nullptr;
    return;
  }

  mParent->InitializeForSameProcess();
  AddRef();
  SendCachedValues();
  // Let Ui thread know the connection is open;
  RecvToolbarAnimatorMessageFromCompositor(COMPOSITOR_CONTROLLER_OPEN);
}

void UiCompositorControllerChild::OpenForGPUProcess(
    Endpoint<PUiCompositorControllerChild>&& aEndpoint) {
  MOZ_ASSERT(IsOnUiThread());

  mIsOpen = aEndpoint.Bind(this);

  if (!mIsOpen) {
    // The GPU Process Manager might be gone if we receive ActorDestroy very
    // late in shutdown.
    if (gfx::GPUProcessManager* gpm = gfx::GPUProcessManager::Get()) {
      gpm->NotifyRemoteActorDestroyed(mProcessToken);
    }
    return;
  }

  AddRef();
  SendCachedValues();
  // Let Ui thread know the connection is open;
  RecvToolbarAnimatorMessageFromCompositor(COMPOSITOR_CONTROLLER_OPEN);
}

void UiCompositorControllerChild::SendCachedValues() {
  MOZ_ASSERT(mIsOpen);
  if (mResize) {
    SendResumeAndResize(mResize.ref().x, mResize.ref().y, mResize.ref().width,
                        mResize.ref().height);
    mResize.reset();
  }
  if (mMaxToolbarHeight) {
    SendMaxToolbarHeight(mMaxToolbarHeight.ref());
    mMaxToolbarHeight.reset();
  }
  if (mDefaultClearColor) {
    SendDefaultClearColor(mDefaultClearColor.ref());
    mDefaultClearColor.reset();
  }
  if (mLayerUpdateEnabled) {
    SendEnableLayerUpdateNotifications(mLayerUpdateEnabled.ref());
    mLayerUpdateEnabled.reset();
  }
}

}  // namespace layers
}  // namespace mozilla

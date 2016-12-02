/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "CompositableHost.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/WebRenderCompositorOGL.h"
#include "mozilla/widget/CompositorWidget.h"

bool is_in_compositor_thread()
{
  return mozilla::layers::CompositorThreadHolder::IsInCompositorThread();
}

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

WebRenderBridgeParent::WebRenderBridgeParent(CompositorBridgeParentBase* aCompositorBridge,
                                             const uint64_t& aPipelineId,
                                             widget::CompositorWidget* aWidget,
                                             gl::GLContext* aGlContext,
                                             wrwindowstate* aWrWindowState,
                                             layers::Compositor* aCompositor)
  : mCompositorBridge(aCompositorBridge)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mWRState(nullptr)
  , mGLContext(aGlContext)
  , mWRWindowState(aWrWindowState)
  , mCompositor(aCompositor)
  , mChildLayerObserverEpoch(0)
  , mParentLayerObserverEpoch(0)
  , mPendingTransactionId(0)
  , mDestroyed(false)
  , mWREpoch(0)
{
  MOZ_ASSERT(mGLContext);
  MOZ_ASSERT(mCompositor);

  if (!mWRWindowState) {
    // mWRWindowState should only be null for the root WRBP of a layers tree,
    // i.e. the one created by the CompositorBridgeParent as opposed to the
    // CrossProcessCompositorBridgeParent
    MOZ_ASSERT(mWidget);
    mWRWindowState = wr_init_window(mPipelineId,
                                    gfxPrefs::WebRenderProfilerEnabled());
  }
  if (mWidget) {
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
    mCompositor->AsWebRenderCompositorOGL()->SetVsyncScheduler(mCompositorScheduler);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCreate(const uint32_t& aWidth,
                                  const uint32_t& aHeight)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  if (mWRState) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  mGLContext->MakeCurrent();
  mWRState = wr_create(mWRWindowState, aWidth, aHeight, mPipelineId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvShutdown()
{
  if (mDestroyed) {
    return IPC_OK();
  }
  Destroy();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

void
WebRenderBridgeParent::Destroy()
{
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(mWRState);
  mDestroyed = true;
  ClearResources();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddImage(const uint32_t& aWidth,
                                    const uint32_t& aHeight,
                                    const uint32_t& aStride,
                                    const WRImageFormat& aFormat,
                                    const ByteBuffer& aBuffer,
                                    WRImageKey* aOutImageKey)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  *aOutImageKey = wr_add_image(mWRWindowState, aWidth, aHeight, aStride, aFormat,
                               aBuffer.mData, aBuffer.mLength);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvUpdateImage(const WRImageKey& aImageKey,
                                       const uint32_t& aWidth,
                                       const uint32_t& aHeight,
                                       const WRImageFormat& aFormat,
                                       const ByteBuffer& aBuffer)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  wr_update_image(mWRWindowState, aImageKey, aWidth, aHeight, aFormat,
                  aBuffer.mData, aBuffer.mLength);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDeleteImage(const WRImageKey& aImageKey)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  mKeysToDelete.push_back(aImageKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPBegin(const uint32_t& aWidth,
                                   const uint32_t& aHeight,
                                   bool* aOutSuccess)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRState);
  wr_dp_begin(mWRWindowState, mWRState, aWidth, aHeight);
  *aOutSuccess = true;
  return IPC_OK();
}


mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                 const uint64_t& aTransactionId)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ProcessWebrenderCommands(aCommands);

  // The transaction ID might get reset to 1 if the page gets reloaded, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1145295#c41
  // Otherwise, it should be continually increasing.
  MOZ_ASSERT(aTransactionId == 1 || aTransactionId > mPendingTransactionId);
  mPendingTransactionId = aTransactionId;

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPSyncEnd(InfallibleTArray<WebRenderCommand>&& aCommands,
                                     const uint64_t& aTransactionId)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  ProcessWebrenderCommands(aCommands);

  // The transaction ID might get reset to 1 if the page gets reloaded, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1145295#c41
  // Otherwise, it should be continually increasing.
  MOZ_ASSERT(aTransactionId == 1 || aTransactionId > mPendingTransactionId);
  mPendingTransactionId = aTransactionId;

  return IPC_OK();
}

void
WebRenderBridgeParent::ProcessWebrenderCommands(InfallibleTArray<WebRenderCommand>& aCommands)
{
  MOZ_ASSERT(mWRState);
  for (InfallibleTArray<WebRenderCommand>::index_type i = 0; i < aCommands.Length(); ++i) {
    const WebRenderCommand& cmd = aCommands[i];

    switch (cmd.type()) {
      case WebRenderCommand::TOpPushDLBuilder: {
        wr_push_dl_builder(mWRState);
        break;
      }
      case WebRenderCommand::TOpPopDLBuilder: {
        const OpPopDLBuilder& op = cmd.get_OpPopDLBuilder();
        wr_pop_dl_builder(mWRState, op.bounds(), op.overflow(), &(op.matrix().components[0]));
        break;
      }
      case WebRenderCommand::TOpDPPushRect: {
        const OpDPPushRect& op = cmd.get_OpDPPushRect();
        wr_dp_push_rect(mWRState, op.bounds(), op.clip(), op.r(), op.g(), op.b(), op.a());
        break;
      }
      case WebRenderCommand::TOpDPPushImage: {
        const OpDPPushImage& op = cmd.get_OpDPPushImage();
        wr_dp_push_image(mWRState, op.bounds(), op.clip(), op.mask().ptrOr(nullptr), op.key());
        break;
      }
      case WebRenderCommand::TOpDPPushIframe: {
        const OpDPPushIframe& op = cmd.get_OpDPPushIframe();
        wr_dp_push_iframe(mWRState, op.bounds(), op.clip(), op.layersid());
        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
    }
  }
  wr_dp_end(mWRWindowState, mWRState, mWREpoch++);
  ScheduleComposition();
  DeleteOldImages();

  if (ShouldParentObserveEpoch()) {
    mCompositorBridge->ObserveLayerUpdate(mPipelineId, GetChildLayerObserverEpoch(), true);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPGetSnapshot(PTextureParent* aTexture,
                                         const IntRect& aRect)
{
  if (mDestroyed) {
    return IPC_OK();
  }

  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }

  // XXX Add other TextureHost supports.
  // Only BufferTextureHost is supported now.
  BufferTextureHost* bufferTexture = texture->AsBufferTextureHost();
  if (!bufferTexture) {
    // We kill the content process rather than have it continue with an invalid
    // snapshot, that may be too harsh and we could decide to return some sort
    // of error to the child process and let it deal with it...
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(bufferTexture->GetBufferDescriptor().type() == BufferDescriptor::TRGBDescriptor);
  uint32_t stride = ImageDataSerializer::GetRGBStride(bufferTexture->GetBufferDescriptor().get_RGBDescriptor());
  uint8_t* buffer = bufferTexture->GetBuffer();
  IntSize size = bufferTexture->GetSize();

  // We only support B8G8R8A8 for now.
  MOZ_ASSERT(buffer);
  MOZ_ASSERT(bufferTexture->GetFormat() == SurfaceFormat::B8G8R8A8);
  uint32_t buffer_size = size.width * size.height * 4;

  // Assert the size and stride of the buffer is what webrender expects
  MOZ_ASSERT(size == aRect.Size());
  MOZ_ASSERT((uint32_t)(size.width * 4) == stride);

  MOZ_ASSERT(mWRState);
  mGLContext->MakeCurrent();
  wr_readback_into_buffer(mWRWindowState, aRect.width, aRect.height, buffer, buffer_size);

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddExternalImageId(const uint64_t& aImageId,
                                              const uint64_t& aAsyncContainerId)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(!mExternalImageIds.Get(aImageId));

  PCompositableParent* compositableParent = CompositableMap::Get(aAsyncContainerId);
  if (!compositableParent) {
    NS_ERROR("CompositableParent not found in the map");
    return IPC_OK();
  }

  CompositableHost* host = CompositableHost::FromIPDLActor(compositableParent);
  if (host->GetType() != CompositableType::IMAGE) {
    NS_ERROR("Incompatible CompositableHost");
    return IPC_OK();
  }

  host->SetCompositor(mCompositor);
  mCompositor->AsWebRenderCompositorOGL()->AddExternalImageId(aImageId, host);
  mExternalImageIds.Put(aImageId, aImageId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvRemoveExternalImageId(const uint64_t& aImageId)
{
  if (mDestroyed) {
    return IPC_OK();
  }
  MOZ_ASSERT(mExternalImageIds.Get(aImageId));
  mExternalImageIds.Remove(aImageId);
  mCompositor->AsWebRenderCompositorOGL()->RemoveExternalImageId(aImageId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch)
{
  mChildLayerObserverEpoch = aLayerObserverEpoch;
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvClearCachedResources()
{
  mCompositorBridge->ObserveLayerUpdate(mPipelineId, GetChildLayerObserverEpoch(), false);
  return IPC_OK();
}

void
WebRenderBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  Destroy();
}

void
WebRenderBridgeParent::CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect)
{
  if (aTarget) {
    // XXX Add compositing to DrawTarget
    return;
  }
  if (!mWidget) {
    return;
  }

  TimeStamp start = TimeStamp::Now();

  mCompositor->SetCompositionTime(TimeStamp::Now());
  mCompositor->AsWebRenderCompositorOGL()->UpdateExternalImages();

  MOZ_ASSERT(mWRState);
  mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
  widgetContext.mGL = mGLContext;
#endif
  if (!mWidget->PreRender(&widgetContext)) {
    return;
  }
  mGLContext->MakeCurrent();
  wr_composite_window(mWRWindowState);
  mGLContext->SwapBuffers();
  mWidget->PostRender(&widgetContext);

  TimeStamp end = TimeStamp::Now();
  mCompositorBridge->NotifyDidComposite(mPendingTransactionId, start, end);
  mPendingTransactionId = 0;

  // Calls for TextureHosts recycling
  mCompositor->EndFrame();
  mCompositor->FlushPendingNotifyNotUsed();
}

WebRenderBridgeParent::~WebRenderBridgeParent()
{
}

void
WebRenderBridgeParent::DeleteOldImages()
{
  for (WRImageKey key : mKeysToDelete) {
    wr_delete_image(mWRWindowState, key);
  }
  mKeysToDelete.clear();
}

void
WebRenderBridgeParent::ScheduleComposition()
{
  mCompositor->AsWebRenderCompositorOGL()->ScheduleComposition();
}

void
WebRenderBridgeParent::ClearResources()
{
  DeleteOldImages();
  for (auto iter = mExternalImageIds.Iter(); !iter.Done(); iter.Next()) {
    uint64_t externalImageId = iter.Data();
    mCompositor->AsWebRenderCompositorOGL()->RemoveExternalImageId(externalImageId);
  }
  mExternalImageIds.Clear();

  if (mWRState) {
    wr_destroy(mWRState);
    mWRState = nullptr;
  }
  if (mCompositorScheduler) {
    mCompositorScheduler->Destroy();
    mCompositorScheduler = nullptr;
  }
  mGLContext = nullptr;
  mCompositorBridge = nullptr;
}

bool
WebRenderBridgeParent::ShouldParentObserveEpoch()
{
  if (mParentLayerObserverEpoch == mChildLayerObserverEpoch) {
    return false;
  }

  mParentLayerObserverEpoch = mChildLayerObserverEpoch;
  return true;
}

void
WebRenderBridgeParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderBridgeParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

base::ProcessId
WebRenderBridgeParent::GetChildProcessId()
{
  return OtherPid();
}

bool
WebRenderBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

PCompositableParent*
WebRenderBridgeParent::AllocPCompositableParent(const TextureInfo& aInfo)
{
  return nullptr;
}

bool
WebRenderBridgeParent::DeallocPCompositableParent(PCompositableParent* aActor)
{
  return true;
}

void
WebRenderBridgeParent::SetWebRenderProfilerEnabled(bool aEnabled)
{
  if (mWidget) {
    // Only set the flag to "root" WebRenderBridgeParent.
    wr_profiler_set_enabled(mWRWindowState, aEnabled);
  }
}

} // namespace layers
} // namespace mozilla

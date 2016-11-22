/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace layers {

WebRenderBridgeParent::WebRenderBridgeParent(WebRenderBridgeParent* aParent,
                                             const uint64_t& aPipelineId,
                                             widget::CompositorWidget* aWidget,
                                             gl::GLContext* aGlContext,
                                             wrwindowstate* aWrWindowState,
                                             layers::Compositor* aCompositor)
  : mParent(aParent)
  , mPipelineId(aPipelineId)
  , mWidget(aWidget)
  , mWRState(nullptr)
  , mGLContext(aGlContext)
  , mWRWindowState(aWrWindowState)
  , mCompositor(aCompositor)
{
  MOZ_ASSERT(mGLContext);
  MOZ_ASSERT(mCompositor);
  if (!mWRWindowState) {
    // mWRWindowState should only be null for the root WRBP of a layers tree,
    // i.e. the one created by the CompositorBridgeParent as opposed to the
    // CrossProcessCompositorBridgeParent
    MOZ_ASSERT(mWidget);
    mWRWindowState = wr_init_window(mPipelineId);
  }
  if (mWidget) {
    mCompositorScheduler = new CompositorVsyncScheduler(this, mWidget);
  }
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvCreate(const uint32_t& aWidth,
                                  const uint32_t& aHeight)
{
  if (mWRState) {
    return IPC_OK();
  }
  MOZ_ASSERT(mWRWindowState);
  mGLContext->MakeCurrent();
  mWRState = wr_create(mWRWindowState, aWidth, aHeight, mPipelineId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDestroy()
{
  MOZ_ASSERT(mWRState);
  MOZ_ASSERT(mCompositor);
  ClearResources();
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvAddImage(const uint32_t& aWidth,
                                    const uint32_t& aHeight,
                                    const uint32_t& aStride,
                                    const WRImageFormat& aFormat,
                                    const ByteBuffer& aBuffer,
                                    WRImageKey* aOutImageKey)
{
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
  MOZ_ASSERT(mWRWindowState);
  wr_update_image(mWRWindowState, aImageKey, aWidth, aHeight, aFormat,
                  aBuffer.mData, aBuffer.mLength);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDeleteImage(const WRImageKey& aImageKey)
{
  MOZ_ASSERT(mWRWindowState);
  mKeysToDelete.push_back(aImageKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPBegin(const uint32_t& aWidth,
                                   const uint32_t& aHeight,
                                   bool* aOutSuccess)
{
  MOZ_ASSERT(mWRState);
  wr_dp_begin(mWRWindowState, mWRState, aWidth, aHeight);
  *aOutSuccess = true;
  return IPC_OK();
}


mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPEnd(InfallibleTArray<WebRenderCommand>&& commands)
{
  ProcessWebrenderCommands(commands);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPSyncEnd(InfallibleTArray<WebRenderCommand>&& commands)
{
  ProcessWebrenderCommands(commands);
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
  wr_dp_end(mWRWindowState, mWRState);
  ScheduleComposition();
  DeleteOldImages();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPGetSnapshot(const uint32_t& aWidth,
                                         const uint32_t& aHeight,
                                         InfallibleTArray<uint8_t>* aOutImageSnapshot)
{
  MOZ_ASSERT(mWRState);
  mGLContext->MakeCurrent();

  uint32_t length = 0;
  uint32_t capacity = 0;
  const uint8_t* webrenderSnapshot = wr_readback_buffer(mWRWindowState, aWidth, aHeight, &length, &capacity);

  aOutImageSnapshot->ReplaceElementsAt(0, aOutImageSnapshot->Length(), webrenderSnapshot, length);

  wr_free_buffer(webrenderSnapshot, length, capacity);

  return IPC_OK();
}

void
WebRenderBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  ClearResources();
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

  mCompositor->SetCompositionTime(TimeStamp::Now());

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
  if (mWidget) {
    mCompositorScheduler->ScheduleComposition();
  } else {
    mParent->ScheduleComposition();
  }
}

void
WebRenderBridgeParent::ClearResources()
{
  if (mWRState) {
    wr_destroy(mWRState);
    mWRState = nullptr;
  }
  if (mWidget) {
    // Only the "root" WebRenderBridgeParent (the one with the widget ptr) owns
    // the compositor ref and needs to destroy it.
    mCompositor->Destroy();
  }
  if (mCompositorScheduler) {
    mCompositorScheduler->Destroy();
    mCompositorScheduler = nullptr;
  }
  mGLContext = nullptr;
  mParent = nullptr;
}

} // namespace layers
} // namespace mozilla

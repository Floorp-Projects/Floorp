/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeParent.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace layers {

WebRenderBridgeParent::WebRenderBridgeParent(const uint64_t& aPipelineId,
                                             const nsString* aResourcePath,
                                             widget::CompositorWidget* aWidget,
                                             gl::GLContext* aGlContext,
                                             wrwindowstate* aWrWindowState,
                                             layers::Compositor* aCompositor)
  : mPipelineId(aPipelineId)
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
    MOZ_ASSERT(aResourcePath);
    mWRWindowState = wr_init_window(mPipelineId,
                                    NS_ConvertUTF16toUTF8(*aResourcePath).get());
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
  wr_destroy(mWRState);
  mWRState = nullptr;
  if (mWidget) {
    mCompositor->Destroy();
  }
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
WebRenderBridgeParent::RecvPushDLBuilder()
{
  MOZ_ASSERT(mWRState);
  wr_push_dl_builder(mWRState);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvPopDLBuilder(const WRRect& aBounds,
                                        const WRRect& aOverflow,
                                        const gfx::Matrix4x4& aMatrix,
                                        const uint64_t& aScrollId)
{
  MOZ_ASSERT(mWRState);
  wr_pop_dl_builder(mWRWindowState, mWRState, aBounds, aOverflow, &aMatrix.components[0], aScrollId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPBegin(const uint32_t& aWidth,
                                   const uint32_t& aHeight,
                                   bool* aOutSuccess)
{
  MOZ_ASSERT(mWRState);
  if (mWidget) {
    mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
    widgetContext.mGL = mGLContext;
#endif
    if (!mWidget->PreRender(&widgetContext)) {
      *aOutSuccess = false;
      return IPC_OK();
    }
  }
  mGLContext->MakeCurrent();
  wr_dp_begin(mWRWindowState, mWRState, aWidth, aHeight);
  *aOutSuccess = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPEnd()
{
  MOZ_ASSERT(mWRState);
  mGLContext->MakeCurrent();
  wr_dp_end(mWRWindowState, mWRState);
  mGLContext->SwapBuffers();
  if (mWidget) {
    mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
    widgetContext.mGL = mGLContext;
#endif
    mWidget->PostRender(&widgetContext);
  }

  DeleteOldImages();
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPPushRect(const WRRect& aBounds,
                                      const WRRect& aClip,
                                      const float& r, const float& g,
                                      const float& b, const float& a)
{
  MOZ_ASSERT(mWRState);
  wr_dp_push_rect(mWRState, aBounds, aClip, r, g, b, a);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPPushImage(const WRRect& aBounds,
                                       const WRRect& aClip,
                                       const Maybe<WRImageMask>& aMask,
                                       const WRImageKey& aKey)
{
  MOZ_ASSERT(mWRState);
  wr_dp_push_image(mWRState, aBounds, aClip, aMask.ptrOr(nullptr), aKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebRenderBridgeParent::RecvDPPushIframe(const WRRect& aBounds,
                                        const WRRect& aClip,
                                        const uint64_t& aLayersId)
{
  MOZ_ASSERT(mWRState);
  wr_dp_push_iframe(mWRState, aBounds, aClip, aLayersId);
  return IPC_OK();
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

} // namespace layers
} // namespace mozilla

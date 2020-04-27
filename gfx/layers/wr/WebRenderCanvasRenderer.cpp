/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCanvasRenderer.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "SharedSurfaceGL.h"
#include "WebRenderBridgeChild.h"
#include "RenderRootStateManager.h"

namespace mozilla {
namespace layers {

CompositableForwarder* WebRenderCanvasRenderer::GetForwarder() {
  return mManager->WrBridge();
}

void WebRenderCanvasRenderer::Initialize(const CanvasInitializeData& aData) {
  ShareableCanvasRenderer::Initialize(aData);
}

WebRenderCanvasRendererAsync::~WebRenderCanvasRendererAsync() { Destroy(); }

void WebRenderCanvasRendererAsync::Initialize(
    const CanvasInitializeData& aData) {
  WebRenderCanvasRenderer::Initialize(aData);

  ClearCachedResources();
}

bool WebRenderCanvasRendererAsync::CreateCompositable() {
  if (!mCanvasClient) {
    TextureFlags flags = TextureFlags::DEFAULT;
    if (mOriginPos == gl::OriginPos::BottomLeft) {
      flags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
    }

    if (IsOpaque()) {
      flags |= TextureFlags::IS_OPAQUE;
    }

    if (!mIsAlphaPremultiplied) {
      flags |= TextureFlags::NON_PREMULTIPLIED;
    }

    mCanvasClient = CanvasClient::CreateCanvasClient(GetCanvasClientType(),
                                                     GetForwarder(), flags);
    if (!mCanvasClient) {
      return false;
    }

    mCanvasClient->Connect();
  }

  if (!mPipelineId) {
    // Alloc async image pipeline id.
    mPipelineId = Some(
        mManager->WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    mManager->AddPipelineIdForCompositable(mPipelineId.ref(),
                                           mCanvasClient->GetIPCHandle());
  }

  return true;
}

void WebRenderCanvasRendererAsync::ClearCachedResources() {
  if (mPipelineId.isSome()) {
    mManager->RemovePipelineIdForCompositable(mPipelineId.ref());
    mPipelineId.reset();
  }
}

void WebRenderCanvasRendererAsync::Destroy() {
  if (mPipelineId.isSome()) {
    mManager->RemovePipelineIdForCompositable(mPipelineId.ref());
    mPipelineId.reset();
  }
}

void WebRenderCanvasRendererAsync::
    UpdateCompositableClientForEmptyTransaction() {
  bool wasDirty = IsDirty();
  UpdateCompositableClient(mManager->GetRenderRoot());
  if (wasDirty && mPipelineId.isSome()) {
    // Notify an update of async image pipeline during empty transaction.
    // During non empty transaction, WebRenderBridgeParent receives
    // OpUpdateAsyncImagePipeline message, but during empty transaction, the
    // message is not sent to WebRenderBridgeParent. Then
    // OpUpdatedAsyncImagePipeline is used to notify the update.
    mManager->AddWebRenderParentCommand(
        OpUpdatedAsyncImagePipeline(mPipelineId.ref()));
  }
}

}  // namespace layers
}  // namespace mozilla

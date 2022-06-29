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

namespace mozilla {
namespace layers {

CompositableForwarder* WebRenderCanvasRenderer::GetForwarder() {
  return mManager->WrBridge();
}

WebRenderCanvasRendererAsync::~WebRenderCanvasRendererAsync() {
  if (mPipelineId.isSome()) {
    mManager->RemovePipelineIdForCompositable(mPipelineId.ref());
    mPipelineId.reset();
  }
}

void WebRenderCanvasRendererAsync::Initialize(const CanvasRendererData& aData) {
  WebRenderCanvasRenderer::Initialize(aData);

  ClearCachedResources();
}

bool WebRenderCanvasRendererAsync::CreateCompositable() {
  if (!mCanvasClient) {
    auto compositableFlags = TextureFlags::NO_FLAGS;
    if (!mData.mIsAlphaPremult) {
      // WR needs this flag marked on the compositable, not just the texture.
      compositableFlags |= TextureFlags::NON_PREMULTIPLIED;
    }
    mCanvasClient = new CanvasClient(GetForwarder(), compositableFlags);
    mCanvasClient->Connect();
  }

  if (!mPipelineId) {
    // Alloc async image pipeline id.
    mPipelineId = Some(
        mManager->WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    mManager->AddPipelineIdForCompositable(
        mPipelineId.ref(), mCanvasClient->GetIPCHandle(),
        CompositableHandleOwner::WebRenderBridge);
  }

  return true;
}

void WebRenderCanvasRendererAsync::ClearCachedResources() {
  if (mPipelineId.isSome()) {
    mManager->RemovePipelineIdForCompositable(mPipelineId.ref());
    mPipelineId.reset();
  }
}

void WebRenderCanvasRendererAsync::
    UpdateCompositableClientForEmptyTransaction() {
  bool wasDirty = IsDirty();
  UpdateCompositableClient();
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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCANVASRENDERER_H
#define GFX_WEBRENDERCANVASRENDERER_H

#include "mozilla/layers/RenderRootStateManager.h"
#include "ShareableCanvasRenderer.h"

namespace mozilla {
namespace layers {

class WebRenderCanvasRenderer : public ShareableCanvasRenderer {
 public:
  explicit WebRenderCanvasRenderer(RenderRootStateManager* aManager)
      : mManager(aManager) {}

  CompositableForwarder* GetForwarder() override;
  RenderRootStateManager* GetRenderRootStateManager() { return mManager; }

 protected:
  RefPtr<RenderRootStateManager> mManager;
};

class WebRenderCanvasRendererAsync final : public WebRenderCanvasRenderer {
 public:
  explicit WebRenderCanvasRendererAsync(RenderRootStateManager* aManager)
      : WebRenderCanvasRenderer(aManager) {}
  virtual ~WebRenderCanvasRendererAsync();

  WebRenderCanvasRendererAsync* AsWebRenderCanvasRendererAsync() override {
    return this;
  }

  void Initialize(const CanvasRendererData& aData) override;
  bool CreateCompositable() override;
  void EnsurePipeline(bool aIsAsync) override;

  void ClearCachedResources() override;

  void UpdateCompositableClientForEmptyTransaction();

  Maybe<wr::PipelineId> GetPipelineId() { return mPipelineId; }

 protected:
  Maybe<wr::PipelineId> mPipelineId;
  bool mIsAsync = false;
};

}  // namespace layers
}  // namespace mozilla

#endif

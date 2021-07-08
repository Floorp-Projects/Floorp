/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_SWGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_SWGL_H

#include "mozilla/gfx/2D.h"
#include "mozilla/webrender/RenderCompositor.h"

namespace mozilla {

namespace wr {

class RenderCompositorSWGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      const RefPtr<widget::CompositorWidget>& aWidget, nsACString& aError);

  RenderCompositorSWGL(const RefPtr<widget::CompositorWidget>& aWidget,
                       void* aContext);
  virtual ~RenderCompositorSWGL();

  void* swgl() const override { return mContext; }

  bool MakeCurrent() override;

  bool BeginFrame() override;
  void CancelFrame() override;
  RenderedFrameId EndFrame(const nsTArray<DeviceIntRect>& aDirtyRects) final;

  void StartCompositing(const wr::DeviceIntRect* aDirtyRects,
                        size_t aNumDirtyRects,
                        const wr::DeviceIntRect* aOpaqueRects,
                        size_t aNumOpaqueRects) override;

  bool UsePartialPresent() override { return true; }
  bool RequestFullRender() override;

  void Pause() override;
  bool Resume() override;

  layers::WebRenderBackend BackendType() const override {
    return layers::WebRenderBackend::SOFTWARE;
  }
  layers::WebRenderCompositor CompositorType() const override {
    return layers::WebRenderCompositor::SOFTWARE;
  }

  bool SurfaceOriginIsTopLeft() override { return true; }

  LayoutDeviceIntSize GetBufferSize() override;

  bool SupportsExternalBufferTextures() const override { return true; }

  // Interface for wr::Compositor
  void GetCompositorCapabilities(CompositorCapabilities* aCaps) override;

 private:
  void* mContext = nullptr;
  RefPtr<gfx::DrawTarget> mDT;
  LayoutDeviceIntRegion mDirtyRegion;
  RefPtr<gfx::DataSourceSurface> mSurface;
  uint8_t* mMappedData = nullptr;
  int32_t mMappedStride = 0;

  void ClearMappedBuffer();

  bool AllocateMappedBuffer(const wr::DeviceIntRect* aOpaqueRects,
                            size_t aNumOpaqueRects);

  void CommitMappedBuffer(bool aDirty = true);
};

}  // namespace wr
}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_SWGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_SWGL_H

#include "mozilla/webrender/RenderCompositor.h"

namespace mozilla {

namespace wr {

class RenderCompositorSWGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget);

  RenderCompositorSWGL(RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositorSWGL();

  bool BeginFrame() override;
  void CancelFrame() override;
  RenderedFrameId EndFrame(const nsTArray<DeviceIntRect>& aDirtyRects) final;

  bool GetMappedBuffer(uint8_t** aData, int32_t* aStride) override {
    if (mMappedData) {
      *aData = mMappedData;
      *aStride = mMappedStride;
      return true;
    }
    return false;
  }

  void Pause() override;
  bool Resume() override;

  LayoutDeviceIntSize GetBufferSize() override;

  // Interface for wr::Compositor
  CompositorCapabilities GetCompositorCapabilities() override;

 private:
  RefPtr<DrawTarget> mDT;
  LayoutDeviceIntRegion mRegion;
  RefPtr<DataSourceSurface> mSurface;
  uint8_t* mMappedData = nullptr;
  int32_t mMappedStride = 0;

  void ClearMappedBuffer();

  void CommitMappedBuffer(const nsTArray<DeviceIntRect>* aDirtyRects = nullptr);
};

}  // namespace wr
}  // namespace mozilla

#endif

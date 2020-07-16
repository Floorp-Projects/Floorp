/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H

#include "GLTypes.h"
#include "mozilla/webrender/RenderCompositor.h"

namespace mozilla {
namespace wr {

class RenderCompositorOGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget);

  RenderCompositorOGL(RefPtr<gl::GLContext>&& aGL,
                      RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositorOGL();

  bool BeginFrame() override;
  RenderedFrameId EndFrame(const nsTArray<DeviceIntRect>& aDirtyRects) final;
  void Pause() override;
  bool Resume() override;

  bool UsePartialPresent() override { return mUsePartialPresent; }
  uint32_t GetMaxPartialPresentRects() override;

  gl::GLContext* gl() const override { return mGL; }

  LayoutDeviceIntSize GetBufferSize() override;

  // Interface for wr::Compositor
  CompositorCapabilities GetCompositorCapabilities() override;

 protected:
  RefPtr<gl::GLContext> mGL;
  bool mUsePartialPresent;
};

}  // namespace wr
}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H

#include "mozilla/webrender/RenderCompositor.h"

namespace mozilla {
/*
namespace gl {
class GLContext;
}

namespace layers {
class SyncObjectHost;
}

namespace widget {
class CompositorWidget;
}
*/
namespace wr {

class RenderCompositorOGL : public RenderCompositor
{
public:
  static UniquePtr<RenderCompositor> Create(RefPtr<widget::CompositorWidget>&& aWidget);

  RenderCompositorOGL(RefPtr<gl::GLContext>&& aGL,
                        RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositorOGL();

  bool Destroy() override;
  bool BeginFrame() override;
  void EndFrame() override;
  void Pause() override;
  bool Resume() override;

  gl::GLContext* gl() const override { return mGL; }

  bool UseANGLE() const override;

  LayoutDeviceIntSize GetClientSize() override;

protected:
  RefPtr<gl::GLContext> mGL;
};

} // namespace wr
} // namespace mozilla

#endif

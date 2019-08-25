/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_H

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "Units.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace layers {
class NativeLayer;
class SyncObjectHost;
}  // namespace layers

namespace widget {
class CompositorWidget;
}

namespace wr {

class RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget);

  RenderCompositor(RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositor();

  virtual bool BeginFrame(layers::NativeLayer* aNativeLayer) = 0;
  virtual void EndFrame() = 0;
  // Returns false when waiting gpu tasks is failed.
  // It might happen when rendering context is lost.
  virtual bool WaitForGPU() { return true; }
  virtual void Pause() = 0;
  virtual bool Resume() = 0;

  virtual gl::GLContext* gl() const { return nullptr; }

  virtual bool MakeCurrent();

  virtual bool UseANGLE() const { return false; }

  virtual bool UseDComp() const { return false; }

  virtual bool UseTripleBuffering() const { return false; }

  virtual LayoutDeviceIntSize GetBufferSize() = 0;

  widget::CompositorWidget* GetWidget() const { return mWidget; }

  layers::SyncObjectHost* GetSyncObject() const { return mSyncObject.get(); }

  virtual bool IsContextLost();

 protected:
  RefPtr<widget::CompositorWidget> mWidget;
  RefPtr<layers::SyncObjectHost> mSyncObject;
};

}  // namespace wr
}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLManager.h"
#include "CompositorOGL.h"  // for CompositorOGL
#include "GLContext.h"      // for GLContext
#include "OGLShaderProgram.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/mozalloc.h"  // for operator new, etc

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class GLManagerCompositor : public GLManager {
 public:
  explicit GLManagerCompositor(CompositorOGL* aCompositor)
      : mImpl(aCompositor) {}

  GLContext* gl() const override { return mImpl->gl(); }

  void ActivateProgram(ShaderProgramOGL* aProg) override {
    mImpl->ActivateProgram(aProg);
  }

  ShaderProgramOGL* GetProgram(GLenum aTarget,
                               gfx::SurfaceFormat aFormat) override {
    ShaderConfigOGL config = ShaderConfigFromTargetAndFormat(aTarget, aFormat);
    return mImpl->GetShaderProgramFor(config);
  }

  const gfx::Matrix4x4& GetProjMatrix() const override {
    return mImpl->GetProjMatrix();
  }

  void BindAndDrawQuad(ShaderProgramOGL* aProg, const gfx::Rect& aLayerRect,
                       const gfx::Rect& aTextureRect) override {
    mImpl->BindAndDrawQuad(aProg, aLayerRect, aTextureRect);
  }

 private:
  RefPtr<CompositorOGL> mImpl;
};

/* static */
GLManager* GLManager::CreateGLManager(LayerManagerComposite* aManager) {
  if (aManager && aManager->GetCompositor()->GetBackendType() ==
                      LayersBackend::LAYERS_OPENGL) {
    return new GLManagerCompositor(
        aManager->GetCompositor()->AsCompositorOGL());
  }
  return nullptr;
}

}  // namespace layers
}  // namespace mozilla

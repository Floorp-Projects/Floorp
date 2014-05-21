/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLManager.h"
#include "CompositorOGL.h"              // for CompositorOGL
#include "GLContext.h"                  // for GLContext
#include "mozilla/Assertions.h"         // for MOZ_CRASH
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class GLManagerCompositor : public GLManager
{
public:
  GLManagerCompositor(CompositorOGL* aCompositor)
    : mImpl(aCompositor)
  {}

  virtual GLContext* gl() const MOZ_OVERRIDE
  {
    return mImpl->gl();
  }

  virtual ShaderProgramOGL* GetProgram(GLenum aTarget, gfx::SurfaceFormat aFormat) MOZ_OVERRIDE
  {
    ShaderConfigOGL config = ShaderConfigFromTargetAndFormat(aTarget, aFormat);
    return mImpl->GetShaderProgramFor(config);
  }

  virtual const gfx::Matrix4x4& GetProjMatrix() const MOZ_OVERRIDE
  {
    return mImpl->GetProjMatrix();
  }

  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg,
                               const gfx::Rect& aLayerRect,
                               const gfx::Rect& aTextureRect) MOZ_OVERRIDE
  {
    mImpl->BindAndDrawQuad(aProg, aLayerRect, aTextureRect);
  }

private:
  RefPtr<CompositorOGL> mImpl;
};

/* static */ GLManager*
GLManager::CreateGLManager(LayerManagerComposite* aManager)
{
  if (aManager &&
      Compositor::GetBackend() == LayersBackend::LAYERS_OPENGL) {
    return new GLManagerCompositor(static_cast<CompositorOGL*>(
      aManager->GetCompositor()));
  }
  return nullptr;
}

}
}

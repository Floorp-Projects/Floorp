/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLManager.h"
#include "CompositorOGL.h"              // for CompositorOGL
#include "GLContext.h"                  // for GLContext
#include "LayerManagerOGL.h"            // for LayerManagerOGL
#include "Layers.h"                     // for LayerManager
#include "mozilla/Assertions.h"         // for MOZ_CRASH
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"            // for LayerManager::AddRef, etc

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

inline ShaderProgramType
GetProgramTypeForSurfaceFormat(GLenum aTarget, gfx::SurfaceFormat aFormat)
{
  if (aTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
    switch (aFormat) {
    case gfx::FORMAT_R8G8B8X8:
    case gfx::FORMAT_R5G6B5:
      return RGBXRectLayerProgramType;
    case gfx::FORMAT_R8G8B8A8:
      return RGBARectLayerProgramType;
    default:
      MOZ_CRASH("unhandled program type");
    }
  }
  switch (aFormat) {
  case gfx::FORMAT_B8G8R8A8:
    return BGRALayerProgramType;
  case gfx::FORMAT_B8G8R8X8:
    return BGRXLayerProgramType;
  case gfx::FORMAT_R8G8B8X8:
  case gfx::FORMAT_R5G6B5:
    return RGBXLayerProgramType;
  case gfx::FORMAT_R8G8B8A8:
    return RGBALayerProgramType;
  default:
    MOZ_CRASH("unhandled program type");
  }
}

class GLManagerLayerManager : public GLManager
{
public:
  GLManagerLayerManager(LayerManagerOGL* aManager)
    : mImpl(aManager)
  {}

  virtual GLContext* gl() const MOZ_OVERRIDE
  {
    return mImpl->gl();
  }

  virtual ShaderProgramOGL* GetProgram(GLenum aTarget, gfx::SurfaceFormat aFormat) MOZ_OVERRIDE
  {
    return mImpl->GetProgram(GetProgramTypeForSurfaceFormat(aTarget, aFormat));
  }

  virtual gfx3DMatrix& GetProjMatrix() const MOZ_OVERRIDE
  {
    return mImpl->mProjMatrix;
  }

  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg) MOZ_OVERRIDE
  {
    mImpl->BindAndDrawQuad(aProg);
  }

private:
  nsRefPtr<LayerManagerOGL> mImpl;
};

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
    ShaderConfigOGL config = mImpl->GetShaderConfigFor(aTarget, aFormat);
    return mImpl->GetShaderProgramFor(config);
  }

  virtual const gfx3DMatrix& GetProjMatrix() const MOZ_OVERRIDE
  {
    return mImpl->GetProjMatrix();
  }

  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg) MOZ_OVERRIDE
  {
    mImpl->BindAndDrawQuad(aProg);
  }

private:
  RefPtr<CompositorOGL> mImpl;
};

/* static */ GLManager*
GLManager::CreateGLManager(LayerManager* aManager)
{
  if (!aManager) {
    return nullptr;
  } else if (aManager->GetBackendType() == LAYERS_OPENGL) {
    return new GLManagerLayerManager(static_cast<LayerManagerOGL*>(aManager));
  }
  if (aManager->GetBackendType() == LAYERS_NONE) {
    if (Compositor::GetBackend() == LAYERS_OPENGL) {
      return new GLManagerCompositor(static_cast<CompositorOGL*>(
        static_cast<LayerManagerComposite*>(aManager)->GetCompositor()));
    } else {
      return nullptr;
    }
  }

  MOZ_CRASH("Cannot create GLManager for non-GL layer manager");
}

}
}

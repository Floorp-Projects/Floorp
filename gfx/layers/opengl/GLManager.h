/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GLMANAGER_H
#define MOZILLA_GFX_GLMANAGER_H

#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "OGLShaderProgram.h"

namespace mozilla {
namespace gl {
class GLContext;
}

namespace layers {

class LayerManagerComposite;

/**
 * Minimal interface to allow widgets to draw using OpenGL. Abstracts
 * CompositorOGL. Call CreateGLManager with a LayerManagerComposite
 * backed by a CompositorOGL.
 */
class GLManager
{
public:
  static GLManager* CreateGLManager(LayerManagerComposite* aManager);

  virtual ~GLManager() {}

  virtual gl::GLContext* gl() const = 0;
  virtual ShaderProgramOGL* GetProgram(GLenum aTarget, gfx::SurfaceFormat aFormat) = 0;
  virtual void ActivateProgram(ShaderProgramOGL* aPrg) = 0;
  virtual const gfx::Matrix4x4& GetProjMatrix() const = 0;
  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg, const gfx::Rect& aLayerRect,
                               const gfx::Rect& aTextureRect) = 0;
};

}
}
#endif

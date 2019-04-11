/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GLMANAGER_H
#define MOZILLA_GFX_GLMANAGER_H

#include "GLDefs.h"
#include "mozilla/gfx/Types.h"  // for SurfaceFormat
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gl {
class GLContext;
}  // namespace gl

namespace layers {

class LayerManagerComposite;
class ShaderProgramOGL;

/**
 * Minimal interface to allow widgets to draw using OpenGL. Abstracts
 * CompositorOGL. Call CreateGLManager with a LayerManagerComposite
 * backed by a CompositorOGL.
 */
class GLManager {
 public:
  static GLManager* CreateGLManager(LayerManagerComposite* aManager);

  virtual ~GLManager() = default;

  virtual gl::GLContext* gl() const = 0;
  virtual ShaderProgramOGL* GetProgram(GLenum aTarget,
                                       gfx::SurfaceFormat aFormat) = 0;
  virtual void ActivateProgram(ShaderProgramOGL* aPrg) = 0;
  virtual const gfx::Matrix4x4& GetProjMatrix() const = 0;
  virtual void BindAndDrawQuad(ShaderProgramOGL* aProg,
                               const gfx::Rect& aLayerRect,
                               const gfx::Rect& aTextureRect) = 0;
};

}  // namespace layers
}  // namespace mozilla

#endif

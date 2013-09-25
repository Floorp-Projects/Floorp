/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CANVASLAYEROGL_H
#define GFX_CANVASLAYEROGL_H

#include "GLContextTypes.h"             // for GLContext
#include "GLDefs.h"                     // for GLuint, LOCAL_GL_TEXTURE_2D
#include "LayerManagerOGL.h"            // for LayerOGL::GLContext, etc
#include "Layers.h"                     // for CanvasLayer, etc
#include "gfxTypes.h"
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "opengl/LayerManagerOGLProgram.h"  // for ShaderProgramType, etc
#if defined(GL_PROVIDER_GLX)
#include "GLXLibrary.h"
#include "mozilla/X11Util.h"
#endif

struct nsIntPoint;
class gfxASurface;
class gfxImageSurface;

namespace mozilla {
namespace layers {

class CanvasLayerOGL :
  public CanvasLayer,
  public LayerOGL
{
public:
  CanvasLayerOGL(LayerManagerOGL *aManager);
  ~CanvasLayerOGL();

  // CanvasLayer implementation
  virtual void Initialize(const Data& aData);

  // LayerOGL implementation
  virtual void Destroy();
  virtual Layer* GetLayer() { return this; }
  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources();

protected:
  void UpdateSurface();

  nsRefPtr<gfxASurface> mCanvasSurface;
  nsRefPtr<GLContext> mGLContext;
  ShaderProgramType mLayerProgram;
  RefPtr<gfx::DrawTarget> mDrawTarget;

  GLuint mTexture;
  GLenum mTextureTarget;

  bool mDelayedUpdates;
  bool mIsGLAlphaPremult;
  bool mNeedsYFlip;
  bool mForceReadback;
  GLuint mUploadTexture;
#if defined(GL_PROVIDER_GLX)
  GLXPixmap mPixmap;
#endif

  nsRefPtr<gfxImageSurface> mCachedTempSurface;
  gfxIntSize mCachedSize;
  gfxImageFormat mCachedFormat;

  gfxImageSurface* GetTempSurface(const gfxIntSize& aSize,
                                  const gfxImageFormat aFormat);

  void DiscardTempSurface() {
    mCachedTempSurface = nullptr;
  }
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */

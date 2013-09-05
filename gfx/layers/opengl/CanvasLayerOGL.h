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
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "opengl/LayerManagerOGLProgram.h"  // for ShaderProgramType, etc
#include "./../mozilla-config.h"        // for GL_PROVIDER_GLX
#if defined(GL_PROVIDER_GLX)
#include "GLXLibrary.h"
#include "mozilla/X11Util.h"
#endif

struct nsIntPoint;


namespace mozilla {
namespace layers {

class CanvasLayerOGL :
  public CanvasLayer,
  public LayerOGL
{
public:
  CanvasLayerOGL(LayerManagerOGL *aManager)
    : CanvasLayer(aManager, nullptr)
    , LayerOGL(aManager)
    , mLayerProgram(RGBALayerProgramType)
    , mTexture(0)
    , mTextureTarget(LOCAL_GL_TEXTURE_2D)
    , mDelayedUpdates(false)
    , mIsGLAlphaPremult(false)
    , mUploadTexture(0)
#if defined(GL_PROVIDER_GLX)
    , mPixmap(0)
#endif
  { 
    mImplData = static_cast<LayerOGL*>(this);
    mForceReadback = Preferences::GetBool("webgl.force-layers-readback", false);
  }

  ~CanvasLayerOGL() {
    Destroy();
  }

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
  gfxASurface::gfxImageFormat mCachedFormat;

  gfxImageSurface* GetTempSurface(const gfxIntSize& aSize,
                                  const gfxASurface::gfxImageFormat aFormat)
  {
    if (!mCachedTempSurface ||
        aSize.width != mCachedSize.width ||
        aSize.height != mCachedSize.height ||
        aFormat != mCachedFormat)
    {
      mCachedTempSurface = new gfxImageSurface(aSize, aFormat);
      mCachedSize = aSize;
      mCachedFormat = aFormat;
    }

    return mCachedTempSurface;
  }

  void DiscardTempSurface() {
    mCachedTempSurface = nullptr;
  }
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */

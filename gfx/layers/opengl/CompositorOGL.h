/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include "GLContextTypes.h"             // for GLContext, etc
#include "GLDefs.h"                     // for GLuint, LOCAL_GL_TEXTURE_2D, etc
#include "LayerManagerOGLProgram.h"     // for ShaderProgramOGL, etc
#include "Units.h"                      // for ScreenPoint
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE, MOZ_FINAL
#include "mozilla/RefPtr.h"             // for TemporaryRef, RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect, IntRect
#include "mozilla/gfx/Types.h"          // for Float, SurfaceFormat, etc
#include "mozilla/layers/Compositor.h"  // for SurfaceInitMode, Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for MaskType::NumMaskTypes, etc
#include "mozilla/layers/LayersTypes.h"
#include "nsAutoPtr.h"                  // for nsRefPtr, nsAutoPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING
#include "nsSize.h"                     // for nsIntSize
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray, etc
#include "nsThreadUtils.h"              // for nsRunnable
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include "nsXULAppAPI.h"                // for XRE_GetProcessType
#include "nscore.h"                     // for NS_IMETHOD
#include "VBOArena.h"                   // for gl::VBOArena

class gfx3DMatrix;
class nsIWidget;
struct gfxMatrix;

namespace mozilla {
class TimeStamp;

namespace gfx {
class Matrix4x4;
}

namespace layers {

class CompositingRenderTarget;
class CompositingRenderTargetOGL;
class DataTextureSource;
class GLManagerCompositor;
class TextureSource;
struct Effect;
struct EffectChain;
struct FPSState;

class CompositorOGL : public Compositor
{
  typedef mozilla::gl::GLContext GLContext;
  typedef ShaderProgramType ProgramType;
  
  friend class GLManagerCompositor;

public:
  CompositorOGL(nsIWidget *aWidget, int aSurfaceWidth = -1, int aSurfaceHeight = -1,
                bool aUseExternalSurfaceSize = false);

  virtual ~CompositorOGL();

  virtual TemporaryRef<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = 0) MOZ_OVERRIDE;

  virtual bool Initialize() MOZ_OVERRIDE;

  virtual void Destroy() MOZ_OVERRIDE;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() MOZ_OVERRIDE
  {
    return TextureFactoryIdentifier(LAYERS_OPENGL,
                                    XRE_GetProcessType(),
                                    GetMaxTextureSize(),
                                    mFBOTextureTarget == LOCAL_GL_TEXTURE_2D,
                                    SupportsPartialTextureUpdate());
  }

  virtual TemporaryRef<CompositingRenderTarget> 
  CreateRenderTarget(const gfx::IntRect &aRect, SurfaceInitMode aInit) MOZ_OVERRIDE;

  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                               const CompositingRenderTarget *aSource) MOZ_OVERRIDE;

  virtual void SetRenderTarget(CompositingRenderTarget *aSurface) MOZ_OVERRIDE;
  virtual CompositingRenderTarget* GetCurrentRenderTarget() MOZ_OVERRIDE;

  virtual void DrawQuad(const gfx::Rect& aRect, const gfx::Rect& aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const gfx::Point& aOffset) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE;
  virtual void EndFrameForExternalComposition(const gfxMatrix& aTransform) MOZ_OVERRIDE;
  virtual void AbortFrame() MOZ_OVERRIDE;

  virtual bool SupportsPartialTextureUpdate() MOZ_OVERRIDE;

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) MOZ_OVERRIDE
  {
    if (!mGLContext)
      return false;
    int32_t maxSize = GetMaxTextureSize();
    return aSize <= gfx::IntSize(maxSize, maxSize);
  }

  virtual int32_t GetMaxTextureSize() const MOZ_OVERRIDE;

  /**
   * Set the size of the EGL surface we're rendering to, if we're rendering to
   * an EGL surface.
   */
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) MOZ_OVERRIDE;

  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) MOZ_OVERRIDE {
    mRenderOffset = aOffset;
  }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) MOZ_OVERRIDE;

  virtual void SetTargetContext(gfx::DrawTarget* aTarget) MOZ_OVERRIDE
  {
    mTarget = aTarget;
  }

  virtual void PrepareViewport(const gfx::IntSize& aSize,
                               const gfxMatrix& aWorldTransform) MOZ_OVERRIDE;


#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const MOZ_OVERRIDE { return "OGL"; }
#endif // MOZ_DUMP_PAINTING

  virtual void NotifyLayersTransaction() MOZ_OVERRIDE;

  virtual void Pause() MOZ_OVERRIDE;
  virtual bool Resume() MOZ_OVERRIDE;

  virtual nsIWidget* GetWidget() const MOZ_OVERRIDE { return mWidget; }
  virtual const nsIntSize& GetWidgetSize() MOZ_OVERRIDE {
    return mWidgetSize;
  }

  GLContext* gl() const { return mGLContext; }
  ShaderProgramType GetFBOLayerProgramType() const {
    return mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB ?
           RGBARectLayerProgramType : RGBALayerProgramType;
  }
  gfx::SurfaceFormat GetFBOFormat() const {
    return gfx::FORMAT_R8G8B8A8;
  }

  virtual void SaveState() MOZ_OVERRIDE;
  virtual void RestoreState() MOZ_OVERRIDE;

  /**
   * The compositor provides with temporary textures for use with direct
   * textruing like gralloc texture.
   * Doing so lets us use gralloc the way it has been designed to be used
   * (see https://wiki.mozilla.org/Platform/GFX/Gralloc)
   */
  GLuint GetTemporaryTexture(GLenum aUnit);
private:
  /** 
   * Context target, nullptr when drawing directly to our swap chain.
   */
  RefPtr<gfx::DrawTarget> mTarget;

  /** Widget associated with this compositor */
  nsIWidget *mWidget;
  nsIntSize mWidgetSize;
  nsRefPtr<GLContext> mGLContext;

  /** The size of the surface we are rendering to */
  nsIntSize mSurfaceSize;

  ScreenPoint mRenderOffset;

  /** Helper-class used by Initialize **/
  class ReadDrawFPSPref MOZ_FINAL : public nsRunnable {
  public:
    NS_IMETHOD Run() MOZ_OVERRIDE;
  };

  already_AddRefed<mozilla::gl::GLContext> CreateContext();

  /** Shader Programs */
  struct ShaderProgramVariations {
    nsAutoTArray<nsAutoPtr<ShaderProgramOGL>, NumMaskTypes> mVariations;
    ShaderProgramVariations() {
      MOZ_COUNT_CTOR(ShaderProgramVariations);
      mVariations.SetLength(NumMaskTypes);
    }
    ~ShaderProgramVariations() {
      MOZ_COUNT_DTOR(ShaderProgramVariations);
    }
  };
  nsTArray<ShaderProgramVariations> mPrograms;

  /** Texture target to use for FBOs */
  GLenum mFBOTextureTarget;

  /** Currently bound render target */
  RefPtr<CompositingRenderTargetOGL> mCurrentRenderTarget;
#ifdef DEBUG
  CompositingRenderTargetOGL* mWindowRenderTarget;
#endif

  /** VBO that has some basics in it for a textured quad,
   *  including vertex coords and texcoords for both
   *  flipped and unflipped textures */
  GLuint mQuadVBO;

  /**
   * When we can't use mQuadVBO, we allocate VBOs from this arena instead.
   */
  gl::VBOArena mVBOs;

  bool mHasBGRA;

  /**
   * When rendering to some EGL surfaces (e.g. on Android), we rely on being told
   * about size changes (via SetSurfaceSize) rather than pulling this information
   * from the widget.
   */
  bool mUseExternalSurfaceSize;

  /**
   * Have we had DrawQuad calls since the last frame was rendered?
   */
  bool mFrameInProgress;

  /* Start a new frame. If aClipRectIn is null and aClipRectOut is non-null,
   * sets *aClipRectOut to the screen dimensions.
   */
  virtual void BeginFrame(const gfx::Rect *aClipRectIn,
                          const gfxMatrix& aTransform,
                          const gfx::Rect& aRenderBounds, 
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) MOZ_OVERRIDE;

  ShaderProgramType GetProgramTypeForEffect(Effect* aEffect) const;

  /**
   * Updates all layer programs with a new projection matrix.
   */
  void SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix);

  /**
   * Helper method for Initialize, creates all valid variations of a program
   * and adds them to mPrograms
   */
  void AddPrograms(ShaderProgramType aType);

  ShaderProgramOGL* GetProgram(ShaderProgramType aType,
                               MaskType aMask = MaskNone) {
    MOZ_ASSERT(ProgramProfileOGL::ProgramExists(aType, aMask),
               "Invalid program type.");
    return mPrograms[aType].mVariations[aMask];
  }

  /**
   * Create a FBO backed by a texture.
   * Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(const gfx::IntRect& aRect, SurfaceInitMode aInit,
                            GLuint aSourceFrameBuffer,
                            GLuint *aFBO, GLuint *aTexture);

  GLintptr QuadVBOVertexOffset() { return 0; }
  GLintptr QuadVBOTexCoordOffset() { return sizeof(float)*4*2; }
  GLintptr QuadVBOFlippedTexCoordOffset() { return sizeof(float)*8*2; }

  void BindQuadVBO();
  void QuadVBOVerticesAttrib(GLuint aAttribIndex);
  void QuadVBOTexCoordsAttrib(GLuint aAttribIndex);
  void QuadVBOFlippedTexCoordsAttrib(GLuint aAttribIndex);
  void BindAndDrawQuad(GLuint aVertAttribIndex,
                       GLuint aTexCoordAttribIndex,
                       bool aFlipped = false);
  void BindAndDrawQuad(ShaderProgramOGL *aProg,
                       bool aFlipped = false);
  void BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                      const gfx::Rect& aTexCoordRect,
                                      TextureSource *aTexture);

  void CleanupResources();

  /**
   * Copies the content of our backbuffer to the set transaction target.
   * Does not restore the target FBO, so only call from EndFrame.
   */
  void CopyToTarget(gfx::DrawTarget* aTarget, const gfxMatrix& aWorldMatrix);

  /**
   * Records the passed frame timestamp and returns the current estimated FPS.
   */
  double AddFrameAndGetFps(const TimeStamp& timestamp);

  bool mDestroyed;

  nsAutoPtr<FPSState> mFPS;
  // Textures used for direct texturing of buffers like gralloc.
  // The index of the texture in this array must correspond to the texture unit.
  nsTArray<GLuint> mTextures;
  static bool sDrawFPS;
};

}
}

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */

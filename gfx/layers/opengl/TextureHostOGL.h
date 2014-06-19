/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint64_t
#include "CompositableHost.h"
#include "GLContextTypes.h"             // for GLContext
#include "GLDefs.h"                     // for GLenum, LOCAL_GL_CLAMP_TO_EDGE, etc
#include "GLTextureImage.h"             // for TextureImage
#include "gfxTypes.h"
#include "mozilla/GfxMessageUtils.h"    // for gfxContentType
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, IntPoint
#include "mozilla/gfx/Types.h"          // for SurfaceFormat, etc
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsISupportsImpl.h"            // for TextureImage::Release, etc
#include "OGLShaderProgram.h"           // for ShaderProgramType, etc
#ifdef MOZ_WIDGET_GONK
#include <ui/GraphicBuffer.h>
#if ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif
#endif

class gfxReusableSurfaceWrapper;
class nsIntRegion;
struct nsIntPoint;
struct nsIntRect;
struct nsIntSize;

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class SurfaceStream;
}

namespace layers {

class Compositor;
class CompositorOGL;
class TextureImageTextureSourceOGL;

/**
 * CompositableBackendSpecificData implementation for the Gonk OpenGL backend.
 * Share a same texture between TextureHosts in the same CompositableHost.
 * By shareing the texture among the TextureHosts, number of texture allocations
 * can be reduced than texture allocation in every TextureHosts.
 * From Bug 912134, use only one texture among all TextureHosts degrade
 * the rendering performance.
 * CompositableDataGonkOGL chooses in a middile of them.
 */
class CompositableDataGonkOGL : public CompositableBackendSpecificData
{
public:
  CompositableDataGonkOGL();
  virtual ~CompositableDataGonkOGL();

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;
  virtual void ClearData() MOZ_OVERRIDE;
  GLuint GetTexture();
  void DeleteTextureIfPresent();
  gl::GLContext* gl() const;
  void BindEGLImage(GLuint aTarget, EGLImage aImage);
  void ClearBoundEGLImage(EGLImage aImage);
protected:
  RefPtr<CompositorOGL> mCompositor;
  GLuint mTexture;
  EGLImage mBoundEGLImage;
};

inline void ApplyFilterToBoundTexture(gl::GLContext* aGL,
                                      gfx::Filter aFilter,
                                      GLuint aTarget = LOCAL_GL_TEXTURE_2D)
{
  GLenum filter =
    (aFilter == gfx::Filter::POINT ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR);

  aGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MIN_FILTER, filter);
  aGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MAG_FILTER, filter);
}

/*
 * TextureHost implementations for the OpenGL backend.
 *
 * Note that it is important to be careful about the ownership model with
 * the OpenGL backend, due to some widget limitation on Linux: before
 * the nsBaseWidget associated with our OpenGL context has been completely
 * deleted, every resource belonging to the OpenGL context MUST have been
 * released. At the moment the teardown sequence happens in the middle of
 * the nsBaseWidget's destructor, meaning that at a given moment we must be
 * able to easily find and release all the GL resources.
 * The point is: be careful about the ownership model and limit the number
 * of objects sharing references to GL resources to make the tear down
 * sequence as simple as possible.
 */

/**
 * TextureSourceOGL provides the necessary API for CompositorOGL to composite
 * a TextureSource.
 */
class TextureSourceOGL
{
public:
  TextureSourceOGL()
    : mHasCachedFilter(false)
  {}

  virtual bool IsValid() const = 0;

  virtual void BindTexture(GLenum aTextureUnit, gfx::Filter aFilter) = 0;

  virtual gfx::IntSize GetSize() const = 0;

  virtual GLenum GetTextureTarget() const { return LOCAL_GL_TEXTURE_2D; }

  virtual gfx::SurfaceFormat GetFormat() const = 0;

  virtual GLenum GetWrapMode() const = 0;

  virtual gfx::Matrix4x4 GetTextureTransform() { return gfx::Matrix4x4(); }

  virtual TextureImageTextureSourceOGL* AsTextureImageTextureSource() { return nullptr; }

  void SetFilter(gl::GLContext* aGL, gfx::Filter aFilter)
  {
    if (mHasCachedFilter &&
        mCachedFilter == aFilter) {
      return;
    }
    mHasCachedFilter = true;
    mCachedFilter = aFilter;
    ApplyFilterToBoundTexture(aGL, aFilter, GetTextureTarget());
  }

  void ClearCachedFilter() { mHasCachedFilter = false; }

private:
  gfx::Filter mCachedFilter;
  bool mHasCachedFilter;
};

/**
 * TextureHostOGL provides the necessary API for platform specific composition.
 */
class TextureHostOGL
{
public:
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17

  /**
   * Store a fence that will signal when the current buffer is no longer being read.
   * Similar to android's GLConsumer::setReleaseFence()
   */
  virtual bool SetReleaseFence(const android::sp<android::Fence>& aReleaseFence);

  /**
   * Return a releaseFence's Fence and clear a reference to the Fence.
   */
  virtual android::sp<android::Fence> GetAndResetReleaseFence();

  virtual void SetAcquireFence(const android::sp<android::Fence>& aAcquireFence);

  /**
   * Return a acquireFence's Fence and clear a reference to the Fence.
   */
  virtual android::sp<android::Fence> GetAndResetAcquireFence();

  virtual void WaitAcquireFenceSyncComplete();

protected:
  android::sp<android::Fence> mReleaseFence;

  android::sp<android::Fence> mAcquireFence;

  /**
   * Hold previous ReleaseFence to prevent Fence delivery failure via gecko IPC.
   * Fence is a kernel object and its lifetime is managed by a reference count.
   * Until the Fence is delivered to client side, need to hold Fence on host side.
   */
  android::sp<android::Fence> mPrevReleaseFence;
#endif
};

/**
 * A TextureSource backed by a TextureImage.
 *
 * Depending on the underlying TextureImage, may support texture tiling, so
 * make sure to check AsBigImageIterator() and use the texture accordingly.
 *
 * This TextureSource can be used without a TextureHost and manage it's own
 * GL texture(s).
 */
class TextureImageTextureSourceOGL : public DataTextureSource
                                   , public TextureSourceOGL
                                   , public BigImageIterator
{
public:
  TextureImageTextureSourceOGL(gl::GLContext* aGL,
                               TextureFlags aFlags = TextureFlags::DEFAULT)
    : mGL(aGL)
    , mFlags(aFlags)
    , mIterating(false)
  {}

  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) MOZ_OVERRIDE;

  void EnsureBuffer(const nsIntSize& aSize,
                            gfxContentType aContentType);

  void CopyTo(const nsIntRect& aSourceRect,
                      DataTextureSource* aDest,
                      const nsIntRect& aDestRect);

  virtual TextureImageTextureSourceOGL* AsTextureImageTextureSource() { return this; }

  // TextureSource

  virtual void DeallocateDeviceData() MOZ_OVERRIDE
  {
    mTexImage = nullptr;
    SetUpdateSerial(0);
  }

  virtual TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE { return this; }

  virtual void BindTexture(GLenum aTextureUnit, gfx::Filter aFilter) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE { return !!mTexImage; }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE
  {
    return mTexImage->GetWrapMode();
  }

  // BigImageIterator

  virtual BigImageIterator* AsBigImageIterator() MOZ_OVERRIDE { return this; }

  virtual void BeginBigImageIteration() MOZ_OVERRIDE
  {
    mTexImage->BeginBigImageIteration();
    mIterating = true;
  }

  virtual void EndBigImageIteration() MOZ_OVERRIDE
  {
    mIterating = false;
  }

  virtual nsIntRect GetTileRect() MOZ_OVERRIDE;

  virtual size_t GetTileCount() MOZ_OVERRIDE
  {
    return mTexImage->GetTileCount();
  }

  virtual bool NextTile() MOZ_OVERRIDE
  {
    return mTexImage->NextTile();
  }

protected:
  nsRefPtr<gl::TextureImage> mTexImage;
  gl::GLContext* mGL;
  TextureFlags mFlags;
  bool mIterating;
};

/**
 * A texture source meant for use with SharedTextureHostOGL.
 *
 * It does not own any GL texture, and attaches its shared handle to one of
 * the compositor's temporary textures when binding.
 *
 * The shared texture handle is owned by the TextureHost.
 */
class SharedTextureSourceOGL : public NewTextureSource
                             , public TextureSourceOGL
{
public:
  typedef gl::SharedTextureShareType SharedTextureShareType;

  SharedTextureSourceOGL(CompositorOGL* aCompositor,
                         gl::SharedTextureHandle aHandle,
                         gfx::SurfaceFormat aFormat,
                         GLenum aTarget,
                         GLenum aWrapMode,
                         SharedTextureShareType aShareType,
                         gfx::IntSize aSize);

  virtual TextureSourceOGL* AsSourceOGL() { return this; }

  virtual void BindTexture(GLenum activetex, gfx::Filter aFilter) MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual gfx::Matrix4x4 GetTextureTransform() MOZ_OVERRIDE;

  virtual GLenum GetTextureTarget() const { return mTextureTarget; }

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return mWrapMode; }

  // SharedTextureSource doesn't own any gl texture
  virtual void DeallocateDeviceData() MOZ_OVERRIDE {}

  void DetachSharedHandle();

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  gl::GLContext* gl() const;

protected:
  gfx::IntSize mSize;
  CompositorOGL* mCompositor;
  gl::SharedTextureHandle mSharedHandle;
  gfx::SurfaceFormat mFormat;
  SharedTextureShareType mShareType;
  GLenum mTextureTarget;
  GLenum mWrapMode;
};

/**
 * A TextureHost for shared GL Textures
 *
 * Most of the logic actually happens in SharedTextureSourceOGL.
 */
class SharedTextureHostOGL : public TextureHost
{
public:
  SharedTextureHostOGL(TextureFlags aFlags,
                       gl::SharedTextureShareType aShareType,
                       gl::SharedTextureHandle aSharedhandle,
                       gfx::IntSize aSize,
                       bool inverted);

  virtual ~SharedTextureHostOGL();

  // SharedTextureHostOGL doesn't own any GL texture
  virtual void DeallocateDeviceData() MOZ_OVERRIDE {}

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE
  {
    return mTextureSource;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual const char* Name() { return "SharedTextureHostOGL"; }

protected:
  gfx::IntSize mSize;
  CompositorOGL* mCompositor;
  gl::SharedTextureHandle mSharedHandle;
  gl::SharedTextureShareType mShareType;

  RefPtr<SharedTextureSourceOGL> mTextureSource;
};

/**
 * A texture source meant for use with SharedTextureHostOGL.
 *
 * It does not own any GL texture, and attaches its shared handle to one of
 * the compositor's temporary textures when binding.
 *
 * The shared texture handle is owned by the TextureHost.
 */
class GLTextureSource : public NewTextureSource
                      , public TextureSourceOGL
{
public:
  typedef gl::SharedTextureShareType SharedTextureShareType;

  GLTextureSource(CompositorOGL* aCompositor,
                  GLuint aTex,
                  gfx::SurfaceFormat aFormat,
                  GLenum aTarget,
                  gfx::IntSize aSize);

  virtual TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE { return this; }

  virtual void BindTexture(GLenum activetex, gfx::Filter aFilter) MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual GLenum GetTextureTarget() const { return mTextureTarget; }

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return LOCAL_GL_CLAMP_TO_EDGE; }

  virtual void DeallocateDeviceData() MOZ_OVERRIDE {}

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  gl::GLContext* gl() const;

protected:
  const gfx::IntSize mSize;
  CompositorOGL* mCompositor;
  const GLuint mTex;
  const gfx::SurfaceFormat mFormat;
  const GLenum mTextureTarget;
};

} // namespace
} // namespace

#endif /* MOZILLA_GFX_TEXTUREOGL_H */

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
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, IntPoint
#include "mozilla/gfx/Types.h"          // for SurfaceFormat, etc
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsISupportsImpl.h"            // for TextureImage::Release, etc
#include "nsRegionFwd.h"                // for nsIntRegion
#include "OGLShaderProgram.h"           // for ShaderProgramType, etc

namespace mozilla {
namespace gfx {
class DataSourceSurface;
} // namespace gfx

namespace gl {
class AndroidSurfaceTexture;
} // namespace gl

namespace layers {

class Compositor;
class CompositorOGL;
class TextureImageTextureSourceOGL;
class GLTextureSource;

inline void ApplySamplingFilterToBoundTexture(gl::GLContext* aGL,
                                              gfx::SamplingFilter aSamplingFilter,
                                              GLuint aTarget = LOCAL_GL_TEXTURE_2D)
{
  GLenum filter =
    (aSamplingFilter == gfx::SamplingFilter::POINT ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR);

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
    : mHasCachedSamplingFilter(false)
  {}

  virtual bool IsValid() const = 0;

  virtual void BindTexture(GLenum aTextureUnit,
                           gfx::SamplingFilter aSamplingFilter) = 0;

  virtual gfx::IntSize GetSize() const = 0;

  virtual GLenum GetTextureTarget() const { return LOCAL_GL_TEXTURE_2D; }

  virtual gfx::SurfaceFormat GetFormat() const = 0;

  virtual GLenum GetWrapMode() const = 0;

  virtual gfx::Matrix4x4 GetTextureTransform() { return gfx::Matrix4x4(); }

  virtual TextureImageTextureSourceOGL* AsTextureImageTextureSource() { return nullptr; }

  virtual GLTextureSource* AsGLTextureSource() { return nullptr; }

  void SetSamplingFilter(gl::GLContext* aGL, gfx::SamplingFilter aSamplingFilter)
  {
    if (mHasCachedSamplingFilter &&
        mCachedSamplingFilter == aSamplingFilter) {
      return;
    }
    mHasCachedSamplingFilter = true;
    mCachedSamplingFilter = aSamplingFilter;
    ApplySamplingFilterToBoundTexture(aGL, aSamplingFilter, GetTextureTarget());
  }

  void ClearCachedFilter() { mHasCachedSamplingFilter = false; }

private:
  gfx::SamplingFilter mCachedSamplingFilter;
  bool mHasCachedSamplingFilter;
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
class TextureImageTextureSourceOGL final : public DataTextureSource
                                         , public TextureSourceOGL
                                         , public BigImageIterator
{
public:
  explicit TextureImageTextureSourceOGL(CompositorOGL *aCompositor,
                                        TextureFlags aFlags = TextureFlags::DEFAULT)
    : mCompositor(aCompositor)
    , mFlags(aFlags)
    , mIterating(false)
  {}

  virtual const char* Name() const override { return "TextureImageTextureSourceOGL"; }
  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override;

  void EnsureBuffer(const gfx::IntSize& aSize,
                    gfxContentType aContentType);

  void CopyTo(const gfx::IntRect& aSourceRect,
              DataTextureSource* aDest,
              const gfx::IntRect& aDestRect);

  virtual TextureImageTextureSourceOGL* AsTextureImageTextureSource() override { return this; }

  // TextureSource

  virtual void DeallocateDeviceData() override
  {
    mTexImage = nullptr;
    SetUpdateSerial(0);
  }

  virtual TextureSourceOGL* AsSourceOGL() override { return this; }

  virtual void BindTexture(GLenum aTextureUnit,
                           gfx::SamplingFilter aSamplingFilter) override;

  virtual gfx::IntSize GetSize() const override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool IsValid() const override { return !!mTexImage; }

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual GLenum GetWrapMode() const override
  {
    return mTexImage->GetWrapMode();
  }

  // BigImageIterator

  virtual BigImageIterator* AsBigImageIterator() override { return this; }

  virtual void BeginBigImageIteration() override
  {
    mTexImage->BeginBigImageIteration();
    mIterating = true;
  }

  virtual void EndBigImageIteration() override
  {
    mIterating = false;
  }

  virtual gfx::IntRect GetTileRect() override;

  virtual size_t GetTileCount() override
  {
    return mTexImage->GetTileCount();
  }

  virtual bool NextTile() override
  {
    return mTexImage->NextTile();
  }

protected:
  RefPtr<gl::TextureImage> mTexImage;
  RefPtr<CompositorOGL> mCompositor;
  TextureFlags mFlags;
  bool mIterating;
};

/**
 * A texture source for GL textures.
 *
 * It does not own any GL texture, and attaches its shared handle to one of
 * the compositor's temporary textures when binding.
 *
 * The shared texture handle is owned by the TextureHost.
 */
class GLTextureSource : public TextureSource
                      , public TextureSourceOGL
{
public:
  GLTextureSource(CompositorOGL* aCompositor,
                  GLuint aTextureHandle,
                  GLenum aTarget,
                  gfx::IntSize aSize,
                  gfx::SurfaceFormat aFormat,
                  bool aExternallyOwned = false);

  ~GLTextureSource();

  virtual const char* Name() const override { return "GLTextureSource"; }

  virtual GLTextureSource* AsGLTextureSource() override { return this; }

  virtual TextureSourceOGL* AsSourceOGL() override { return this; }

  virtual void BindTexture(GLenum activetex,
                           gfx::SamplingFilter aSamplingFilter) override;

  virtual bool IsValid() const override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual GLenum GetTextureTarget() const override { return mTextureTarget; }

  virtual GLenum GetWrapMode() const override { return LOCAL_GL_CLAMP_TO_EDGE; }

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  void SetSize(gfx::IntSize aSize) { mSize = aSize; }

  void SetFormat(gfx::SurfaceFormat aFormat) { mFormat = aFormat; }

  GLuint GetTextureHandle() const { return mTextureHandle; }

  gl::GLContext* gl() const;

protected:
  void DeleteTextureHandle();

  RefPtr<CompositorOGL> mCompositor;
  GLuint mTextureHandle;
  GLenum mTextureTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  // If the texture is externally owned, the gl handle will not be deleted
  // in the destructor.
  bool mExternallyOwned;
};

class GLTextureHost : public TextureHost
{
public:
  GLTextureHost(TextureFlags aFlags,
                GLuint aTextureHandle,
                GLenum aTarget,
                GLsync aSync,
                gfx::IntSize aSize,
                bool aHasAlpha);

  virtual ~GLTextureHost();

  // We don't own anything.
  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override { return mCompositor; }

  virtual bool Lock() override;

  virtual void Unlock() override {}

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual const char* Name() override { return "GLTextureHost"; }

protected:
  const GLuint mTexture;
  const GLenum mTarget;
  GLsync mSync;
  const gfx::IntSize mSize;
  const bool mHasAlpha;
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<GLTextureSource> mTextureSource;
};

////////////////////////////////////////////////////////////////////////
// SurfaceTexture

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureSource : public TextureSource
                           , public TextureSourceOGL
{
public:
  SurfaceTextureSource(CompositorOGL* aCompositor,
                       mozilla::gl::AndroidSurfaceTexture* aSurfTex,
                       gfx::SurfaceFormat aFormat,
                       GLenum aTarget,
                       GLenum aWrapMode,
                       gfx::IntSize aSize);

  virtual const char* Name() const override { return "SurfaceTextureSource"; }

  virtual TextureSourceOGL* AsSourceOGL() override { return this; }

  virtual void BindTexture(GLenum activetex,
                           gfx::SamplingFilter aSamplingFilter) override;

  virtual bool IsValid() const override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual gfx::Matrix4x4 GetTextureTransform() override;

  virtual GLenum GetTextureTarget() const override { return mTextureTarget; }

  virtual GLenum GetWrapMode() const override { return mWrapMode; }

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  gl::GLContext* gl() const;

protected:
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<gl::AndroidSurfaceTexture> mSurfTex;
  const gfx::SurfaceFormat mFormat;
  const GLenum mTextureTarget;
  const GLenum mWrapMode;
  const gfx::IntSize mSize;
};

class SurfaceTextureHost : public TextureHost
{
public:
  SurfaceTextureHost(TextureFlags aFlags,
                     mozilla::gl::AndroidSurfaceTexture* aSurfTex,
                     gfx::IntSize aSize);

  virtual ~SurfaceTextureHost();

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override { return mCompositor; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual const char* Name() override { return "SurfaceTextureHost"; }

protected:
  RefPtr<gl::AndroidSurfaceTexture> mSurfTex;
  const gfx::IntSize mSize;
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<SurfaceTextureSource> mTextureSource;
};

#endif // MOZ_WIDGET_ANDROID

////////////////////////////////////////////////////////////////////////
// EGLImage

class EGLImageTextureSource : public TextureSource
                            , public TextureSourceOGL
{
public:
  EGLImageTextureSource(CompositorOGL* aCompositor,
                        EGLImage aImage,
                        gfx::SurfaceFormat aFormat,
                        GLenum aTarget,
                        GLenum aWrapMode,
                        gfx::IntSize aSize);

  virtual const char* Name() const override { return "EGLImageTextureSource"; }

  virtual TextureSourceOGL* AsSourceOGL() override { return this; }

  virtual void BindTexture(GLenum activetex,
                           gfx::SamplingFilter aSamplingFilter) override;

  virtual bool IsValid() const override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual gfx::Matrix4x4 GetTextureTransform() override;

  virtual GLenum GetTextureTarget() const override { return mTextureTarget; }

  virtual GLenum GetWrapMode() const override { return mWrapMode; }

  // We don't own anything.
  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  gl::GLContext* gl() const;

protected:
  RefPtr<CompositorOGL> mCompositor;
  const EGLImage mImage;
  const gfx::SurfaceFormat mFormat;
  const GLenum mTextureTarget;
  const GLenum mWrapMode;
  const gfx::IntSize mSize;
};

class EGLImageTextureHost : public TextureHost
{
public:
  EGLImageTextureHost(TextureFlags aFlags,
                     EGLImage aImage,
                     EGLSync aSync,
                     gfx::IntSize aSize,
                     bool hasAlpha);

  virtual ~EGLImageTextureHost();

  // We don't own anything.
  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override { return mCompositor; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual const char* Name() override { return "EGLImageTextureHost"; }

protected:
  const EGLImage mImage;
  const EGLSync mSync;
  const gfx::IntSize mSize;
  const bool mHasAlpha;
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<EGLImageTextureSource> mTextureSource;
};

CompositorOGL* AssertGLCompositor(Compositor* aCompositor);

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_TEXTUREOGL_H */

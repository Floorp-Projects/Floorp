/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint64_t
#include "CompositableHost.h"
#include "GLContextTypes.h"  // for GLContext
#include "GLDefs.h"          // for GLenum, LOCAL_GL_CLAMP_TO_EDGE, etc
#include "GLTextureImage.h"  // for TextureImage
#include "gfxTypes.h"
#include "mozilla/GfxMessageUtils.h"         // for gfxContentType
#include "mozilla/Assertions.h"              // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"              // for override
#include "mozilla/RefPtr.h"                  // for RefPtr
#include "mozilla/gfx/Matrix.h"              // for Matrix4x4
#include "mozilla/gfx/Point.h"               // for IntSize, IntPoint
#include "mozilla/gfx/Types.h"               // for SurfaceFormat, etc
#include "mozilla/layers/CompositorOGL.h"    // for CompositorOGL
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "mozilla/layers/LayersSurfaces.h"   // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"      // for TextureHost, etc
#include "mozilla/mozalloc.h"                // for operator delete, etc
#include "mozilla/webrender/RenderThread.h"
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_WARNING
#include "nsISupportsImpl.h"  // for TextureImage::Release, etc
#include "nsRegionFwd.h"      // for nsIntRegion

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidSurfaceTexture.h"
#  include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#endif

namespace mozilla {
namespace gfx {
class DataSourceSurface;
}  // namespace gfx

namespace layers {

class Compositor;
class CompositorOGL;
class AndroidHardwareBuffer;
class SurfaceDescriptorAndroidHardwareBuffer;
class TextureImageTextureSourceOGL;
class GLTextureSource;

void ApplySamplingFilterToBoundTexture(gl::GLContext* aGL,
                                       gfx::SamplingFilter aSamplingFilter,
                                       GLuint aTarget = LOCAL_GL_TEXTURE_2D);

already_AddRefed<TextureHost> CreateTextureHostOGL(
    const SurfaceDescriptor& aDesc, ISurfaceAllocator* aDeallocator,
    LayersBackend aBackend, TextureFlags aFlags);

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
class TextureSourceOGL {
 public:
  TextureSourceOGL()
      : mCachedSamplingFilter(gfx::SamplingFilter::GOOD),
        mHasCachedSamplingFilter(false) {}

  virtual bool IsValid() const = 0;

  virtual void BindTexture(GLenum aTextureUnit,
                           gfx::SamplingFilter aSamplingFilter) = 0;

  // To be overridden in textures that need this. This method will be called
  // when the compositor has used the texture to draw. This allows us to set
  // a fence with glFenceSync which we can wait on later to ensure the GPU
  // is done with the draw calls using that texture. We would like to be able
  // to simply use glFinishObjectAPPLE, but this returns earlier than
  // expected with nvidia drivers.
  virtual void MaybeFenceTexture() {}

  virtual gfx::IntSize GetSize() const = 0;

  virtual GLenum GetTextureTarget() const { return LOCAL_GL_TEXTURE_2D; }

  virtual gfx::SurfaceFormat GetFormat() const = 0;

  virtual GLenum GetWrapMode() const = 0;

  virtual gfx::Matrix4x4 GetTextureTransform() { return gfx::Matrix4x4(); }

  virtual TextureImageTextureSourceOGL* AsTextureImageTextureSource() {
    return nullptr;
  }

  virtual GLTextureSource* AsGLTextureSource() { return nullptr; }

  void SetSamplingFilter(gl::GLContext* aGL,
                         gfx::SamplingFilter aSamplingFilter) {
    if (mHasCachedSamplingFilter && mCachedSamplingFilter == aSamplingFilter) {
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
class TextureImageTextureSourceOGL final : public DataTextureSource,
                                           public TextureSourceOGL,
                                           public BigImageIterator {
 public:
  explicit TextureImageTextureSourceOGL(
      CompositorOGL* aCompositor, TextureFlags aFlags = TextureFlags::DEFAULT);

  const char* Name() const override { return "TextureImageTextureSourceOGL"; }
  // DataTextureSource

  bool Update(gfx::DataSourceSurface* aSurface,
              nsIntRegion* aDestRegion = nullptr,
              gfx::IntPoint* aSrcOffset = nullptr,
              gfx::IntPoint* aDstOffset = nullptr) override;

  void EnsureBuffer(const gfx::IntSize& aSize, gfxContentType aContentType);

  TextureImageTextureSourceOGL* AsTextureImageTextureSource() override {
    return this;
  }

  // TextureSource

  void DeallocateDeviceData() override;

  TextureSourceOGL* AsSourceOGL() override { return this; }

  void BindTexture(GLenum aTextureUnit,
                   gfx::SamplingFilter aSamplingFilter) override;

  gfx::IntSize GetSize() const override;

  gfx::SurfaceFormat GetFormat() const override;

  bool IsValid() const override { return !!mTexImage; }

  GLenum GetWrapMode() const override { return mTexImage->GetWrapMode(); }

  // BigImageIterator

  BigImageIterator* AsBigImageIterator() override { return this; }

  void BeginBigImageIteration() override {
    mTexImage->BeginBigImageIteration();
    mIterating = true;
  }

  void EndBigImageIteration() override { mIterating = false; }

  gfx::IntRect GetTileRect() override;

  size_t GetTileCount() override { return mTexImage->GetTileCount(); }

  bool NextTile() override { return mTexImage->NextTile(); }

  gl::GLContext* gl() const { return mGL; }

 protected:
  ~TextureImageTextureSourceOGL();

  RefPtr<gl::TextureImage> mTexImage;
  RefPtr<gl::GLContext> mGL;
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
class GLTextureSource : public DataTextureSource, public TextureSourceOGL {
 public:
  GLTextureSource(TextureSourceProvider* aProvider, GLuint aTextureHandle,
                  GLenum aTarget, gfx::IntSize aSize,
                  gfx::SurfaceFormat aFormat);

  GLTextureSource(gl::GLContext* aGL, GLuint aTextureHandle, GLenum aTarget,
                  gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  virtual ~GLTextureSource();

  const char* Name() const override { return "GLTextureSource"; }

  GLTextureSource* AsGLTextureSource() override { return this; }

  TextureSourceOGL* AsSourceOGL() override { return this; }

  void BindTexture(GLenum activetex,
                   gfx::SamplingFilter aSamplingFilter) override;

  bool IsValid() const override;

  gfx::IntSize GetSize() const override { return mSize; }

  gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  GLenum GetTextureTarget() const override { return mTextureTarget; }

  GLenum GetWrapMode() const override { return LOCAL_GL_CLAMP_TO_EDGE; }

  void DeallocateDeviceData() override;

  void SetSize(gfx::IntSize aSize) { mSize = aSize; }

  void SetFormat(gfx::SurfaceFormat aFormat) { mFormat = aFormat; }

  GLuint GetTextureHandle() const { return mTextureHandle; }

  gl::GLContext* gl() const { return mGL; }

  bool Update(gfx::DataSourceSurface* aSurface,
              nsIntRegion* aDestRegion = nullptr,
              gfx::IntPoint* aSrcOffset = nullptr,
              gfx::IntPoint* aDstOffset = nullptr) override {
    return false;
  }

 protected:
  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;
  RefPtr<CompositorOGL> mCompositor;
  GLuint mTextureHandle;
  GLenum mTextureTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
};

// This texture source try to wrap "aSurface" in ctor for compositor direct
// access. Since we can't know the timing for gpu buffer access, the surface
// should be alive until the ~ClientStorageTextureSource(). And if we try to
// update the surface we mapped before, we need to call Sync() to make sure
// the surface is not used by compositor.
class DirectMapTextureSource : public GLTextureSource {
 public:
  DirectMapTextureSource(gl::GLContext* aContext,
                         gfx::DataSourceSurface* aSurface);
  DirectMapTextureSource(TextureSourceProvider* aProvider,
                         gfx::DataSourceSurface* aSurface);
  ~DirectMapTextureSource();

  bool Update(gfx::DataSourceSurface* aSurface,
              nsIntRegion* aDestRegion = nullptr,
              gfx::IntPoint* aSrcOffset = nullptr,
              gfx::IntPoint* aDstOffset = nullptr) override;

  // If aBlocking is false, check if this texture is no longer being used
  // by the GPU - if aBlocking is true, this will block until the GPU is
  // done with it.
  bool Sync(bool aBlocking) override;

  void MaybeFenceTexture() override;

 private:
  bool UpdateInternal(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion, gfx::IntPoint* aSrcOffset,
                      bool aInit);

  GLsync mSync;
};

class GLTextureHost : public TextureHost {
 public:
  GLTextureHost(TextureFlags aFlags, GLuint aTextureHandle, GLenum aTarget,
                GLsync aSync, gfx::IntSize aSize, bool aHasAlpha);

  virtual ~GLTextureHost();

  // We don't own anything.
  void DeallocateDeviceData() override {}

  gfx::SurfaceFormat GetFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  gfx::IntSize GetSize() const override { return mSize; }

  const char* Name() override { return "GLTextureHost"; }

 protected:
  const GLuint mTexture;
  const GLenum mTarget;
  GLsync mSync;
  const gfx::IntSize mSize;
  const bool mHasAlpha;
  RefPtr<GLTextureSource> mTextureSource;
};

////////////////////////////////////////////////////////////////////////
// SurfaceTexture

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureSource : public TextureSource, public TextureSourceOGL {
 public:
  SurfaceTextureSource(TextureSourceProvider* aProvider,
                       java::GeckoSurfaceTexture::Ref& aSurfTex,
                       gfx::SurfaceFormat aFormat, GLenum aTarget,
                       GLenum aWrapMode, gfx::IntSize aSize,
                       bool aIgnoreTransform);

  const char* Name() const override { return "SurfaceTextureSource"; }

  TextureSourceOGL* AsSourceOGL() override { return this; }

  void BindTexture(GLenum activetex,
                   gfx::SamplingFilter aSamplingFilter) override;

  bool IsValid() const override;

  gfx::IntSize GetSize() const override { return mSize; }

  gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  gfx::Matrix4x4 GetTextureTransform() override;

  GLenum GetTextureTarget() const override { return mTextureTarget; }

  GLenum GetWrapMode() const override { return mWrapMode; }

  void DeallocateDeviceData() override;

  gl::GLContext* gl() const { return mGL; }

 protected:
  RefPtr<gl::GLContext> mGL;
  mozilla::java::GeckoSurfaceTexture::GlobalRef mSurfTex;
  const gfx::SurfaceFormat mFormat;
  const GLenum mTextureTarget;
  const GLenum mWrapMode;
  const gfx::IntSize mSize;
  const bool mIgnoreTransform;
};

class SurfaceTextureHost : public TextureHost {
 public:
  SurfaceTextureHost(TextureFlags aFlags,
                     mozilla::java::GeckoSurfaceTexture::Ref& aSurfTex,
                     gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                     bool aContinuousUpdate, bool aIgnoreTransform);

  virtual ~SurfaceTextureHost();

  void DeallocateDeviceData() override;

  gfx::SurfaceFormat GetFormat() const override;

  void NotifyNotUsed() override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  gfx::IntSize GetSize() const override { return mSize; }

  const char* Name() override { return "SurfaceTextureHost"; }

  SurfaceTextureHost* AsSurfaceTextureHost() override { return this; }

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  bool SupportsExternalCompositing(WebRenderBackend aBackend) override;

  // gecko does not need deferred deletion with WebRender
  // GPU/hardware task end could be checked by android fence.
  // SurfaceTexture uses android fence internally,
  bool NeedsDeferredDeletion() const override { return false; }

 protected:
  bool EnsureAttached();

  mozilla::java::GeckoSurfaceTexture::GlobalRef mSurfTex;
  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;
  bool mContinuousUpdate;
  const bool mIgnoreTransform;
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<SurfaceTextureSource> mTextureSource;
};

class AndroidHardwareBufferTextureHost : public TextureHost {
 public:
  static already_AddRefed<AndroidHardwareBufferTextureHost> Create(
      TextureFlags aFlags, const SurfaceDescriptorAndroidHardwareBuffer& aDesc);

  AndroidHardwareBufferTextureHost(
      TextureFlags aFlags, AndroidHardwareBuffer* aAndroidHardwareBuffer);

  virtual ~AndroidHardwareBufferTextureHost();

  void DeallocateDeviceData() override;

  gfx::SurfaceFormat GetFormat() const override;

  gfx::IntSize GetSize() const override;

  void NotifyNotUsed() override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  const char* Name() override { return "AndroidHardwareBufferTextureHost"; }

  AndroidHardwareBufferTextureHost* AsAndroidHardwareBufferTextureHost()
      override {
    return this;
  }

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  void SetAcquireFence(mozilla::ipc::FileDescriptor&& aFenceFd) override;

  void SetReleaseFence(mozilla::ipc::FileDescriptor&& aFenceFd) override;

  mozilla::ipc::FileDescriptor GetAndResetReleaseFence() override;

  AndroidHardwareBuffer* GetAndroidHardwareBuffer() const override {
    return mAndroidHardwareBuffer;
  }

  // gecko does not need deferred deletion with WebRender
  // GPU/hardware task end could be checked by android fence.
  bool NeedsDeferredDeletion() const override { return false; }

 protected:
  void DestroyEGLImage();

  RefPtr<AndroidHardwareBuffer> mAndroidHardwareBuffer;
  RefPtr<GLTextureSource> mTextureSource;
  EGLImage mEGLImage;
};

#endif  // MOZ_WIDGET_ANDROID

////////////////////////////////////////////////////////////////////////
// EGLImage

class EGLImageTextureSource : public TextureSource, public TextureSourceOGL {
 public:
  EGLImageTextureSource(TextureSourceProvider* aProvider, EGLImage aImage,
                        gfx::SurfaceFormat aFormat, GLenum aTarget,
                        GLenum aWrapMode, gfx::IntSize aSize);

  const char* Name() const override { return "EGLImageTextureSource"; }

  TextureSourceOGL* AsSourceOGL() override { return this; }

  void BindTexture(GLenum activetex,
                   gfx::SamplingFilter aSamplingFilter) override;

  bool IsValid() const override;

  gfx::IntSize GetSize() const override { return mSize; }

  gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  gfx::Matrix4x4 GetTextureTransform() override;

  GLenum GetTextureTarget() const override { return mTextureTarget; }

  GLenum GetWrapMode() const override { return mWrapMode; }

  // We don't own anything.
  void DeallocateDeviceData() override {}

  gl::GLContext* gl() const { return mGL; }

 protected:
  RefPtr<gl::GLContext> mGL;
  RefPtr<CompositorOGL> mCompositor;
  const EGLImage mImage;
  const gfx::SurfaceFormat mFormat;
  const GLenum mTextureTarget;
  const GLenum mWrapMode;
  const gfx::IntSize mSize;
};

class EGLImageTextureHost final : public TextureHost {
 public:
  EGLImageTextureHost(TextureFlags aFlags, EGLImage aImage, EGLSync aSync,
                      gfx::IntSize aSize, bool hasAlpha);

  virtual ~EGLImageTextureHost();

  // We don't own anything.
  void DeallocateDeviceData() override {}

  gfx::SurfaceFormat GetFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  gfx::IntSize GetSize() const override { return mSize; }

  const char* Name() override { return "EGLImageTextureHost"; }

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

 protected:
  const EGLImage mImage;
  const EGLSync mSync;
  const gfx::IntSize mSize;
  const bool mHasAlpha;
  RefPtr<EGLImageTextureSource> mTextureSource;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_TEXTUREOGL_H */

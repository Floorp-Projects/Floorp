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
#include "mozilla/layers/TextureHost.h"  // for DeprecatedTextureHost, etc
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

class gfxImageSurface;
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
class TextureImageDeprecatedTextureHostOGL;

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
protected:
  RefPtr<CompositorOGL> mCompositor;
  GLuint mTexture;
};

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
  virtual bool IsValid() const = 0;

  virtual void BindTexture(GLenum aTextureUnit) = 0;

  virtual gfx::IntSize GetSize() const = 0;

  virtual GLenum GetTextureTarget() const { return LOCAL_GL_TEXTURE_2D; }

  virtual gfx::SurfaceFormat GetFormat() const = 0;

  virtual GLenum GetWrapMode() const = 0;

  virtual gfx::Matrix4x4 GetTextureTransform() { return gfx::Matrix4x4(); }

  virtual TextureImageDeprecatedTextureHostOGL* AsTextureImageDeprecatedTextureHost() { return nullptr; }
};

/**
 * TextureHostOGL provides the necessary API for platform specific composition.
 */
class TextureHostOGL
{
public:
#if MOZ_WIDGET_GONK && ANDROID_VERSION >= 17

  /**
   * Store a fence that will signal when the current buffer is no longer being read.
   * Similar to android's GLConsumer::setReleaseFence()
   */
  virtual bool SetReleaseFence(const android::sp<android::Fence>& aReleaseFence);

  /**
   * Return a releaseFence's Fence and clear a reference to the Fence.
   */
  virtual android::sp<android::Fence> GetAndResetReleaseFence();
protected:
  android::sp<android::Fence> mReleaseFence;
#endif
};

/**
 * A TextureSource backed by a TextureImage.
 *
 * Depending on the underlying TextureImage, may support texture tiling, so
 * make sure to check AsTileIterator() and use the texture accordingly.
 *
 * This TextureSource can be used without a TextureHost and manage it's own
 * GL texture(s).
 */
class TextureImageTextureSourceOGL : public DataTextureSource
                                   , public TextureSourceOGL
                                   , public TileIterator
{
public:
  TextureImageTextureSourceOGL(gl::GLContext* aGL,
                               TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT)
    : mGL(aGL)
    , mFlags(aFlags)
    , mIterating(false)
  {}

  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) MOZ_OVERRIDE;

  // TextureSource

  virtual void DeallocateDeviceData() MOZ_OVERRIDE
  {
    mTexImage = nullptr;
    SetUpdateSerial(0);
  }

  virtual TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE { return this; }

  virtual void BindTexture(GLenum aTextureUnit) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE { return !!mTexImage; }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE
  {
    return mTexImage->GetWrapMode();
  }

  // TileIterator

  virtual TileIterator* AsTileIterator() MOZ_OVERRIDE { return this; }

  virtual void BeginTileIteration() MOZ_OVERRIDE
  {
    mTexImage->BeginTileIteration();
    mIterating = true;
  }

  virtual void EndTileIteration() MOZ_OVERRIDE
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

  virtual void BindTexture(GLenum activetex) MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual gfx::Matrix4x4 GetTextureTransform() MOZ_OVERRIDE;

  virtual GLenum GetTextureTarget() const { return mTextureTarget; }

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return mWrapMode; }

  // SharedTextureSource doesn't own any gl texture
  virtual void DeallocateDeviceData() {}

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
 * A texture source meant for use with StreamTextureHostOGL.
 *
 * It does not own any texture, we get texture from SurfaceStream.
 */
class StreamTextureSourceOGL : public NewTextureSource
                             , public TextureSourceOGL
{
public:
  StreamTextureSourceOGL(CompositorOGL* aCompositor,
                         gfx::SurfaceStream* aStream)
    : mCompositor(aCompositor)
    , mStream(aStream)
    , mTextureHandle(0)
    , mTextureTarget(LOCAL_GL_TEXTURE_2D)
    , mUploadTexture(0)
    , mFormat(gfx::SurfaceFormat::UNKNOWN)
  {
    MOZ_COUNT_CTOR(StreamTextureSourceOGL);
  }

  ~StreamTextureSourceOGL()
  {
    MOZ_COUNT_DTOR(StreamTextureSourceOGL);
  }

  virtual TextureSourceOGL* AsSourceOGL() { return this; }

  virtual void BindTexture(GLenum activetex) MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE { return !!gl(); }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual GLenum GetTextureTarget() const { return mTextureTarget; }

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return LOCAL_GL_CLAMP_TO_EDGE; }

  virtual void DeallocateDeviceData();

  bool RetrieveTextureFromStream();

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

protected:
  gl::GLContext* gl() const;

  CompositorOGL* mCompositor;
  gfx::SurfaceStream* mStream;
  GLuint mTextureHandle;
  GLenum mTextureTarget;
  GLuint mUploadTexture;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
};

/**
 * A TextureHost for shared SurfaceStream
 */
class StreamTextureHostOGL : public TextureHost
{
public:
  StreamTextureHostOGL(TextureFlags aFlags,
                       const SurfaceStreamDescriptor& aDesc);

  virtual ~StreamTextureHostOGL();

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

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "StreamTextureHostOGL"; }
#endif

protected:
  CompositorOGL* mCompositor;
  gfx::SurfaceStream* mStream;
  RefPtr<StreamTextureSourceOGL> mTextureSource;
};

/**
 * DeprecatedTextureHost implementation using a TextureImage as the underlying texture.
 */
class TextureImageDeprecatedTextureHostOGL : public DeprecatedTextureHost
                                           , public TextureSourceOGL
                                           , public TileIterator
{
public:
  TextureImageDeprecatedTextureHostOGL(gl::TextureImage* aTexImage = nullptr)
    : mTexture(aTexImage)
    , mGL(nullptr)
    , mIterating(false)
  {
    MOZ_COUNT_CTOR(TextureImageDeprecatedTextureHostOGL);
  }

  ~TextureImageDeprecatedTextureHostOGL();

  TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE
  {
    return this;
  }

  virtual TextureImageDeprecatedTextureHostOGL* AsTextureImageDeprecatedTextureHost() MOZ_OVERRIDE
  {
    return this;
  }

  // This is a hack that is here to not break on-main-thread ThebesLayerBuffer
  // please do not use it anywhere else, use SetCompositor instead.
  void SetGLContext(gl::GLContext* aGL)
  {
    mGL = aGL;
  }

  // DeprecatedTextureHost

  void UpdateImpl(const SurfaceDescriptor& aImage,
                  nsIntRegion* aRegion = nullptr,
                  nsIntPoint* aOffset = nullptr) MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual void EnsureBuffer(const nsIntSize& aSize, gfxContentType aType) MOZ_OVERRIDE;

  virtual void CopyTo(const nsIntRect& aSourceRect,
                      DeprecatedTextureHost *aDest,
                      const nsIntRect& aDestRect) MOZ_OVERRIDE;

  bool IsValid() const MOZ_OVERRIDE
  {
    return !!mTexture;
  }

  virtual bool Lock() MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE;

  // textureSource
  void BindTexture(GLenum aTextureUnit) MOZ_OVERRIDE
  {
    mTexture->BindTexture(aTextureUnit);
  }

  gfx::IntSize GetSize() const MOZ_OVERRIDE;

  GLenum GetWrapMode() const MOZ_OVERRIDE
  {
    return mTexture->GetWrapMode();
  }

  gl::TextureImage* GetTextureImage()
  {
    return mTexture;
  }

  void SetTextureImage(gl::TextureImage* aImage)
  {
    mTexture = aImage;
  }

  // TileIterator

  TileIterator* AsTileIterator() MOZ_OVERRIDE
  {
    return this;
  }

  void BeginTileIteration() MOZ_OVERRIDE
  {
    mTexture->BeginTileIteration();
    mIterating = true;
  }

  void EndTileIteration() MOZ_OVERRIDE
  {
    mIterating = false;
  }

  nsIntRect GetTileRect() MOZ_OVERRIDE;

  size_t GetTileCount() MOZ_OVERRIDE
  {
    return mTexture->GetTileCount();
  }

  bool NextTile() MOZ_OVERRIDE
  {
    return mTexture->NextTile();
  }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return DeprecatedTextureHost::GetFormat();
  }

  virtual const char* Name() { return "TextureImageDeprecatedTextureHostOGL"; }

protected:
  nsRefPtr<gl::TextureImage> mTexture;
  gl::GLContext* mGL;
  bool mIterating;
};

class TiledDeprecatedTextureHostOGL : public DeprecatedTextureHost
                          , public TextureSourceOGL
{
public:
  TiledDeprecatedTextureHostOGL()
    : mTextureHandle(0)
    , mGL(nullptr)
  {}
  ~TiledDeprecatedTextureHostOGL();

  virtual void SetCompositor(Compositor* aCompositor);

  // have to pass the size in here (every time) because of DrawQuad API :-(
  virtual void Update(gfxReusableSurfaceWrapper* aReusableSurface, TextureFlags aFlags, const gfx::IntSize& aSize) MOZ_OVERRIDE;
  virtual bool Lock() MOZ_OVERRIDE;
  virtual void Unlock() MOZ_OVERRIDE {}

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return DeprecatedTextureHost::GetFormat();
  }

  virtual TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE { return this; }
  virtual bool IsValid() const MOZ_OVERRIDE { return true; }
  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return LOCAL_GL_CLAMP_TO_EDGE; }
  virtual void BindTexture(GLenum aTextureUnit);
  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE
  {
    return mSize;
  }

  virtual void SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                nsIntRegion* aRegion = nullptr)
  { MOZ_ASSERT(false, "Tiles should not use this path"); }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE;

  virtual const char* Name() { return "TiledDeprecatedTextureHostOGL"; }

protected:
  void DeleteTextures();

  virtual uint64_t GetIdentifier() const MOZ_OVERRIDE {
    return static_cast<uint64_t>(mTextureHandle);
  }

private:
  GLenum GetTileType()
  {
    // Deduce the type that was assigned in GetFormatAndTileForImageFormat
    return mGLFormat == LOCAL_GL_RGB ? LOCAL_GL_UNSIGNED_SHORT_5_6_5 : LOCAL_GL_UNSIGNED_BYTE;
  }

  gfx::IntSize mSize;
  GLuint mTextureHandle;
  GLenum mGLFormat;
  nsRefPtr<gl::GLContext> mGL;
};

#ifdef MOZ_WIDGET_GONK

// For direct texturing with gralloc buffers. The corresponding DeprecatedTextureClient is DeprecatedTextureClientShmem,
// which automatically gets gralloc when it can, in which case the compositor sees that the
// SurfaceDescriptor is gralloc, and decides to use a GrallocDeprecatedTextureHostOGL to do direct texturing,
// saving the cost of a texture upload.
class GrallocDeprecatedTextureHostOGL
  : public DeprecatedTextureHost
  , public TextureSourceOGL
{
public:
  GrallocDeprecatedTextureHostOGL();

  ~GrallocDeprecatedTextureHostOGL();

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual void UpdateImpl(const SurfaceDescriptor& aImage,
                          nsIntRegion* aRegion = nullptr,
                          nsIntPoint* aOffset = nullptr) MOZ_OVERRIDE;
  virtual void SwapTexturesImpl(const SurfaceDescriptor& aImage,
                          nsIntRegion* aRegion = nullptr) MOZ_OVERRIDE;
  virtual bool Lock() MOZ_OVERRIDE;
  virtual void Unlock() MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE
  {
    return mGraphicBuffer.get() ? gfx::IntSize(mGraphicBuffer->getWidth(), mGraphicBuffer->getHeight()) : gfx::IntSize(0, 0);
  }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  GLenum GetWrapMode() const MOZ_OVERRIDE
  {
    return LOCAL_GL_CLAMP_TO_EDGE;
  }

  virtual GLenum GetTextureTarget() const MOZ_OVERRIDE
  {
    return mTextureTarget;
  }

  bool IsValid() const MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE;

  virtual const char* Name() { return "GrallocDeprecatedTextureHostOGL"; }

  void BindTexture(GLenum aTextureUnit) MOZ_OVERRIDE;

  virtual TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE
  {
    return this;
  }

  // only overridden for hacky fix in gecko 23 for bug 862324
  // see bug 865908 about fixing this.
  virtual void SetBuffer(SurfaceDescriptor* aBuffer, ISurfaceAllocator* aAllocator) MOZ_OVERRIDE;

  // used only for hacky fix in gecko 23 for bug 862324
  virtual void ForgetBuffer()
  {
    if (mBuffer) {
      // Intentionally don't destroy the actor held by mBuffer here.
      // The point is that this is only called from GrallocBufferActor::ActorDestroy
      // where we know that the actor is already being deleted.
      // See bug 862324 comment 39.
      delete mBuffer;
      mBuffer = nullptr;
    }

    mGraphicBuffer = nullptr;
    DeleteTextures();
  }

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  GLuint GetGLTexture();

private:
  gl::GLContext* gl() const;

  void DeleteTextures();

  RefPtr<CompositorOGL> mCompositor;
  android::sp<android::GraphicBuffer> mGraphicBuffer;
  GLenum mTextureTarget;
  EGLImage mEGLImage;
  //Set when the composer needs to swap RB pixels of gralloc buffer
  bool mIsRBSwapped;
};
#endif

} // namespace
} // namespace

#endif /* MOZILLA_GFX_TEXTUREOGL_H */

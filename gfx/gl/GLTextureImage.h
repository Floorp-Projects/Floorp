/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLTEXTUREIMAGE_H_
#define GLTEXTUREIMAGE_H_

#include "nsRegion.h"
#include "nsTArray.h"
#include "gfxTypes.h"
#include "GLContextTypes.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/RefPtr.h"

class gfxASurface;

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class DrawTarget;
}  // namespace gfx
}  // namespace mozilla

namespace mozilla {
namespace gl {
class GLContext;

/**
 * A TextureImage provides a mechanism to synchronize data from a
 * surface to a texture in the server.  TextureImages are associated
 * with one and only one GLContext.
 */
class TextureImage {
  NS_INLINE_DECL_REFCOUNTING(TextureImage)
 public:
  enum TextureState {
    Created,    // Texture created, but has not had glTexImage called to
                // initialize it.
    Allocated,  // Texture memory exists, but contents are invalid.
    Valid       // Texture fully ready to use.
  };

  enum Flags {
    NoFlags = 0x0,
    UseNearestFilter = 0x1,
    OriginBottomLeft = 0x2,
    DisallowBigImage = 0x4
  };

  typedef gfxContentType ContentType;
  typedef gfxImageFormat ImageFormat;

  static already_AddRefed<TextureImage> Create(
      GLContext* gl, const gfx::IntSize& aSize,
      TextureImage::ContentType aContentType, GLenum aWrapMode,
      TextureImage::Flags aFlags = TextureImage::NoFlags);

  /**
   * The Image may contain several textures for different regions (tiles).
   * These functions iterate over each sub texture image tile.
   */
  virtual void BeginBigImageIteration() {}

  virtual bool NextTile() { return false; }

  // Function prototype for a tile iteration callback. Returning false will
  // cause iteration to be interrupted (i.e. the corresponding NextTile call
  // will return false).
  typedef bool (*BigImageIterationCallback)(TextureImage* aImage,
                                            int aTileNumber,
                                            void* aCallbackData);

  // Sets a callback to be called every time NextTile is called.
  virtual void SetIterationCallback(BigImageIterationCallback aCallback,
                                    void* aCallbackData) {}

  virtual gfx::IntRect GetTileRect();

  virtual GLuint GetTextureID() = 0;

  virtual uint32_t GetTileCount() { return 1; }

  /**
   * Set this TextureImage's size, and ensure a texture has been
   * allocated.
   * After a resize, the contents are undefined.
   */
  virtual void Resize(const gfx::IntSize& aSize) = 0;

  /**
   * Mark this texture as having valid contents. Call this after modifying
   * the texture contents externally.
   */
  virtual void MarkValid() {}

  /**
   * aSurf - the source surface to update from
   * aRegion - the region in this image to update
   * aFrom - offset in the source to update from
   */
  virtual bool DirectUpdate(gfx::DataSourceSurface* aSurf,
                            const nsIntRegion& aRegion,
                            const gfx::IntPoint& aFrom = gfx::IntPoint(0,
                                                                       0)) = 0;
  bool UpdateFromDataSource(gfx::DataSourceSurface* aSurf,
                            const nsIntRegion* aDstRegion = nullptr,
                            const gfx::IntPoint* aSrcOffset = nullptr);

  virtual void BindTexture(GLenum aTextureUnit) = 0;

  /**
   * Returns the image format of the texture. Only valid after
   * DirectUpdate has been called.
   */
  virtual gfx::SurfaceFormat GetTextureFormat() { return mTextureFormat; }

  /** Can be called safely at any time. */

  /**
   * If this TextureImage has a permanent gfxASurface backing,
   * return it.  Otherwise return nullptr.
   */
  virtual already_AddRefed<gfxASurface> GetBackingSurface() { return nullptr; }

  gfx::IntSize GetSize() const;
  ContentType GetContentType() const { return mContentType; }
  GLenum GetWrapMode() const { return mWrapMode; }

  void SetSamplingFilter(gfx::SamplingFilter aSamplingFilter) {
    mSamplingFilter = aSamplingFilter;
  }

 protected:
  friend class GLContext;

  void UpdateUploadSize(size_t amount);

  /**
   * After the ctor, the TextureImage is invalid.  Implementations
   * must allocate resources successfully before returning the new
   * TextureImage from GLContext::CreateTextureImage().  That is,
   * clients must not be given partially-constructed TextureImages.
   */
  TextureImage(const gfx::IntSize& aSize, GLenum aWrapMode,
               ContentType aContentType, Flags aFlags = NoFlags);

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~TextureImage() { UpdateUploadSize(0); }

  virtual gfx::IntRect GetSrcTileRect();

  gfx::IntSize mSize;
  GLenum mWrapMode;
  ContentType mContentType;
  gfx::SurfaceFormat mTextureFormat;
  gfx::SamplingFilter mSamplingFilter;
  Flags mFlags;
  size_t mUploadSize;
};

/**
 * BasicTextureImage is the baseline TextureImage implementation ---
 * it updates its texture by allocating a scratch buffer for the
 * client to draw into, then using glTexSubImage2D() to upload the new
 * pixels.  Platforms must provide the code to create a new surface
 * into which the updated pixels will be drawn, and the code to
 * convert the update surface's pixels into an image on which we can
 * glTexSubImage2D().
 */
class BasicTextureImage : public TextureImage {
 public:
  virtual ~BasicTextureImage();

  BasicTextureImage(GLuint aTexture, const gfx::IntSize& aSize,
                    GLenum aWrapMode, ContentType aContentType,
                    GLContext* aContext,
                    TextureImage::Flags aFlags = TextureImage::NoFlags);

  void BindTexture(GLenum aTextureUnit) override;

  bool DirectUpdate(gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion,
                    const gfx::IntPoint& aFrom = gfx::IntPoint(0, 0)) override;
  GLuint GetTextureID() override { return mTexture; }

  void MarkValid() override { mTextureState = Valid; }

  void Resize(const gfx::IntSize& aSize) override;

 protected:
  GLuint mTexture;
  TextureState mTextureState;
  RefPtr<GLContext> mGLContext;
};

/**
 * A container class that complements many sub TextureImages into a big
 * TextureImage. Aims to behave just like the real thing.
 */

class TiledTextureImage final : public TextureImage {
 public:
  TiledTextureImage(
      GLContext* aGL, gfx::IntSize aSize, TextureImage::ContentType,
      TextureImage::Flags aFlags = TextureImage::NoFlags,
      TextureImage::ImageFormat aImageFormat = gfx::SurfaceFormat::UNKNOWN);
  virtual ~TiledTextureImage();
  void DumpDiv();
  void Resize(const gfx::IntSize& aSize) override;
  uint32_t GetTileCount() override;
  void BeginBigImageIteration() override;
  bool NextTile() override;
  void SetIterationCallback(BigImageIterationCallback aCallback,
                            void* aCallbackData) override;
  gfx::IntRect GetTileRect() override;
  GLuint GetTextureID() override {
    return mImages[mCurrentImage]->GetTextureID();
  }
  bool DirectUpdate(gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion,
                    const gfx::IntPoint& aFrom = gfx::IntPoint(0, 0)) override;
  void BindTexture(GLenum) override;

 protected:
  gfx::IntRect GetSrcTileRect() override;

  unsigned int mCurrentImage;
  BigImageIterationCallback mIterationCallback;
  void* mIterationCallbackData;
  nsTArray<RefPtr<TextureImage> > mImages;
  unsigned int mTileSize;
  unsigned int mRows, mColumns;
  GLContext* mGL;
  TextureState mTextureState;
  TextureImage::ImageFormat mImageFormat;
};

/**
 * Creates a TextureImage of the basic implementation, can be useful in cases
 * where we know we don't want to use platform-specific TextureImage.
 * In doubt, use GLContext::CreateTextureImage instead.
 */
already_AddRefed<TextureImage> CreateBasicTextureImage(
    GLContext* aGL, const gfx::IntSize& aSize,
    TextureImage::ContentType aContentType, GLenum aWrapMode,
    TextureImage::Flags aFlags);

/**
 * Creates a TiledTextureImage backed by platform-specific or basic
 * TextureImages. In doubt, use GLContext::CreateTextureImage instead.
 */
already_AddRefed<TextureImage> CreateTiledTextureImage(
    GLContext* aGL, const gfx::IntSize& aSize,
    TextureImage::ContentType aContentType, TextureImage::Flags aFlags,
    TextureImage::ImageFormat aImageFormat);

/**
 * Return a valid, allocated TextureImage of |aSize| with
 * |aContentType|.  If |aContentType| is COLOR, |aImageFormat| can be used
 * to hint at the preferred RGB format, however it is not necessarily
 * respected.  The TextureImage's texture is configured to use
 * |aWrapMode| (usually GL_CLAMP_TO_EDGE or GL_REPEAT) and by
 * default, GL_LINEAR filtering.  Specify
 * |aFlags=UseNearestFilter| for GL_NEAREST filtering. Specify
 * |aFlags=OriginBottomLeft| if the image is origin-bottom-left, instead of the
 * default origin-top-left. Return
 * nullptr if creating the TextureImage fails.
 *
 * The returned TextureImage may only be used with this GLContext.
 * Attempting to use the returned TextureImage after this
 * GLContext is destroyed will result in undefined (and likely
 * crashy) behavior.
 */
already_AddRefed<TextureImage> CreateTextureImage(
    GLContext* gl, const gfx::IntSize& aSize,
    TextureImage::ContentType aContentType, GLenum aWrapMode,
    TextureImage::Flags aFlags = TextureImage::NoFlags,
    TextureImage::ImageFormat aImageFormat = gfx::SurfaceFormat::UNKNOWN);

}  // namespace gl
}  // namespace mozilla

#endif /* GLTEXTUREIMAGE_H_ */

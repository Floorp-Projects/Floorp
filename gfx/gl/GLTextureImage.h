/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLTEXTUREIMAGE_H_
#define GLTEXTUREIMAGE_H_

#include "nsAutoPtr.h"
#include "nsRegion.h"
#include "gfxASurface.h"
#include "GLContextTypes.h"
#include "gfxPattern.h"

namespace mozilla {
namespace gl {
class GLContext;

/**
 * A TextureImage encapsulates a surface that can be drawn to by a
 * Thebes gfxContext and (hopefully efficiently!) synchronized to a
 * texture in the server.  TextureImages are associated with one and
 * only one GLContext.
 *
 * Implementation note: TextureImages attempt to unify two categories
 * of backends
 *
 *  (1) proxy to server-side object that can be bound to a texture;
 *      e.g. Pixmap on X11.
 *
 *  (2) efficient manager of texture memory; e.g. by having clients draw
 *      into a scratch buffer which is then uploaded with
 *      glTexSubImage2D().
 */
class TextureImage
{
    NS_INLINE_DECL_REFCOUNTING(TextureImage)
public:
    enum TextureState
    {
      Created, // Texture created, but has not had glTexImage called to initialize it.
      Allocated,  // Texture memory exists, but contents are invalid.
      Valid  // Texture fully ready to use.
    };

    enum Flags {
        NoFlags          = 0x0,
        UseNearestFilter = 0x1,
        NeedsYFlip       = 0x2,
        ForceSingleTile  = 0x4
    };

    typedef gfxASurface::gfxContentType ContentType;

    static already_AddRefed<TextureImage> Create(
                       GLContext* gl,
                       const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLenum aWrapMode,
                       TextureImage::Flags aFlags = TextureImage::NoFlags);

    virtual ~TextureImage() {}

    /**
     * Returns a gfxASurface for updating |aRegion| of the client's
     * image if successul, NULL if not.  |aRegion|'s bounds must fit
     * within Size(); its coordinate space (if any) is ignored.  If
     * the update begins successfully, the returned gfxASurface is
     * owned by this.  Otherwise, NULL is returned.
     *
     * |aRegion| is an inout param: the returned region is what the
     * client must repaint.  Category (1) regions above can
     * efficiently handle repaints to "scattered" regions, while (2)
     * can only efficiently handle repaints to rects.
     *
     * Painting the returned surface outside of |aRegion| results
     * in undefined behavior.
     *
     * BeginUpdate() calls cannot be "nested", and each successful
     * BeginUpdate() must be followed by exactly one EndUpdate() (see
     * below).  Failure to do so can leave this in a possibly
     * inconsistent state.  Unsuccessful BeginUpdate()s must not be
     * followed by EndUpdate().
     */
    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion) = 0;
    /**
     * Retrieves the region that will require updating, given a
     * region that needs to be updated. This can be used for
     * making decisions about updating before calling BeginUpdate().
     *
     * |aRegion| is an inout param.
     */
    virtual void GetUpdateRegion(nsIntRegion& aForRegion) {
    }
    /**
     * Finish the active update and synchronize with the server, if
     * necessary.
     *
     * BeginUpdate() must have been called exactly once before
     * EndUpdate().
     */
    virtual void EndUpdate() = 0;

    /**
     * The Image may contain several textures for different regions (tiles).
     * These functions iterate over each sub texture image tile.
     */
    virtual void BeginTileIteration() {
    }

    virtual bool NextTile() {
        return false;
    }

    // Function prototype for a tile iteration callback. Returning false will
    // cause iteration to be interrupted (i.e. the corresponding NextTile call
    // will return false).
    typedef bool (* TileIterationCallback)(TextureImage* aImage,
                                           int aTileNumber,
                                           void* aCallbackData);

    // Sets a callback to be called every time NextTile is called.
    virtual void SetIterationCallback(TileIterationCallback aCallback,
                                      void* aCallbackData) {
    }

    virtual nsIntRect GetTileRect() {
        return nsIntRect(nsIntPoint(0,0), mSize);
    }

    virtual GLuint GetTextureID() = 0;

    virtual uint32_t GetTileCount() {
        return 1;
    }

    /**
     * Set this TextureImage's size, and ensure a texture has been
     * allocated.  Must not be called between BeginUpdate and EndUpdate.
     * After a resize, the contents are undefined.
     *
     * If this isn't implemented by a subclass, it will just perform
     * a dummy BeginUpdate/EndUpdate pair.
     */
    virtual void Resize(const nsIntSize& aSize) {
        mSize = aSize;
        nsIntRegion r(nsIntRect(0, 0, aSize.width, aSize.height));
        BeginUpdate(r);
        EndUpdate();
    }

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
    virtual bool DirectUpdate(gfxASurface *aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0)) = 0;

    virtual void BindTexture(GLenum aTextureUnit) = 0;
    virtual void ReleaseTexture() {}

    void BindTextureAndApplyFilter(GLenum aTextureUnit) {
        BindTexture(aTextureUnit);
        ApplyFilter();
    }

    class ScopedBindTexture
    {
    public:
        ScopedBindTexture(TextureImage *aTexture, GLenum aTextureUnit);

        ~ScopedBindTexture()
        {
            if (mTexture) {
                mTexture->ReleaseTexture();
            }
        }

    protected:
        TextureImage *mTexture;
    };

    class ScopedBindTextureAndApplyFilter
        : public ScopedBindTexture
    {
    public:
        ScopedBindTextureAndApplyFilter(TextureImage *aTexture, GLenum aTextureUnit) :
          ScopedBindTexture(aTexture, aTextureUnit)
        {
            if (mTexture) {
                mTexture->ApplyFilter();
            }
        }
    };

    /**
     * Returns the shader program type that should be used to render
     * this texture. Only valid after a matching BeginUpdate/EndUpdate
     * pair have been called.
     */
    virtual ShaderProgramType GetShaderProgramType()
    {
         return mShaderType;
    }

    /** Can be called safely at any time. */

    /**
     * If this TextureImage has a permanent gfxASurface backing,
     * return it.  Otherwise return NULL.
     */
    virtual already_AddRefed<gfxASurface> GetBackingSurface()
    { return NULL; }

    const nsIntSize& GetSize() const { return mSize; }
    ContentType GetContentType() const { return mContentType; }
    virtual bool InUpdate() const = 0;
    GLenum GetWrapMode() const { return mWrapMode; }

    void SetFilter(gfxPattern::GraphicsFilter aFilter) { mFilter = aFilter; }

    /**
     * Applies this TextureImage's filter, assuming that its texture is
     * the currently bound texture.
     */
    virtual void ApplyFilter() = 0;

protected:
    friend class GLContext;

    /**
     * After the ctor, the TextureImage is invalid.  Implementations
     * must allocate resources successfully before returning the new
     * TextureImage from GLContext::CreateTextureImage().  That is,
     * clients must not be given partially-constructed TextureImages.
     */
    TextureImage(const nsIntSize& aSize,
                 GLenum aWrapMode, ContentType aContentType,
                 Flags aFlags = NoFlags)
        : mSize(aSize)
        , mWrapMode(aWrapMode)
        , mContentType(aContentType)
        , mFilter(gfxPattern::FILTER_GOOD)
        , mFlags(aFlags)
    {}

    virtual nsIntRect GetSrcTileRect() {
        return nsIntRect(nsIntPoint(0,0), mSize);
    }

    nsIntSize mSize;
    GLenum mWrapMode;
    ContentType mContentType;
    ShaderProgramType mShaderType;
    gfxPattern::GraphicsFilter mFilter;
    Flags mFlags;
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
class BasicTextureImage
    : public TextureImage
{
public:
    typedef gfxASurface::gfxImageFormat ImageFormat;
    virtual ~BasicTextureImage();

    BasicTextureImage(GLuint aTexture,
                      const nsIntSize& aSize,
                      GLenum aWrapMode,
                      ContentType aContentType,
                      GLContext* aContext,
                      TextureImage::Flags aFlags = TextureImage::NoFlags)
        : TextureImage(aSize, aWrapMode, aContentType, aFlags)
        , mTexture(aTexture)
        , mTextureState(Created)
        , mGLContext(aContext)
        , mUpdateOffset(0, 0)
    {}

    virtual void BindTexture(GLenum aTextureUnit);

    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion);
    virtual void GetUpdateRegion(nsIntRegion& aForRegion);
    virtual void EndUpdate();
    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0));
    virtual GLuint GetTextureID() { return mTexture; }
    // Returns a surface to draw into
    virtual already_AddRefed<gfxASurface>
      GetSurfaceForUpdate(const gfxIntSize& aSize, ImageFormat aFmt);

    virtual void MarkValid() { mTextureState = Valid; }

    // Call when drawing into the update surface is complete.
    // Returns true if textures should be upload with a relative
    // offset - See UploadSurfaceToTexture.
    virtual bool FinishedSurfaceUpdate();

    // Call after surface data has been uploaded to a texture.
    virtual void FinishedSurfaceUpload();

    virtual bool InUpdate() const { return !!mUpdateSurface; }

    virtual void Resize(const nsIntSize& aSize);

    virtual void ApplyFilter();
protected:

    GLuint mTexture;
    TextureState mTextureState;
    GLContext* mGLContext;
    nsRefPtr<gfxASurface> mUpdateSurface;
    nsIntRegion mUpdateRegion;

    // The offset into the update surface at which the update rect is located.
    nsIntPoint mUpdateOffset;
};

/**
 * A container class that complements many sub TextureImages into a big TextureImage.
 * Aims to behave just like the real thing.
 */

class TiledTextureImage
    : public TextureImage
{
public:
    TiledTextureImage(GLContext* aGL, nsIntSize aSize,
        TextureImage::ContentType, TextureImage::Flags aFlags = TextureImage::NoFlags);
    ~TiledTextureImage();
    void DumpDiv();
    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion);
    virtual void GetUpdateRegion(nsIntRegion& aForRegion);
    virtual void EndUpdate();
    virtual void Resize(const nsIntSize& aSize);
    virtual uint32_t GetTileCount();
    virtual void BeginTileIteration();
    virtual bool NextTile();
    virtual void SetIterationCallback(TileIterationCallback aCallback,
                                      void* aCallbackData);
    virtual nsIntRect GetTileRect();
    virtual GLuint GetTextureID() {
        return mImages[mCurrentImage]->GetTextureID();
    }
    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0));
    virtual bool InUpdate() const { return mInUpdate; }
    virtual void BindTexture(GLenum);
    virtual void ApplyFilter();

protected:
    virtual nsIntRect GetSrcTileRect();

    unsigned int mCurrentImage;
    TileIterationCallback mIterationCallback;
    void* mIterationCallbackData;
    nsTArray< nsRefPtr<TextureImage> > mImages;
    bool mInUpdate;
    nsIntSize mSize;
    unsigned int mTileSize;
    unsigned int mRows, mColumns;
    GLContext* mGL;
    // A temporary surface to faciliate cross-tile updates.
    nsRefPtr<gfxASurface> mUpdateSurface;
    // The region of update requested
    nsIntRegion mUpdateRegion;
    TextureState mTextureState;
};

} // namespace gl
} // namespace mozilla

#endif /* GLTEXTUREIMAGE_H_ */

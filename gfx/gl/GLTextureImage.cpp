/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLTextureImage.h"
#include "GLContext.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/2D.h"
#include "ScopedGLHelpers.h"
#include "GLUploadHelpers.h"

#include "TextureImageEGL.h"
#ifdef XP_MACOSX
#include "TextureImageCGL.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

already_AddRefed<TextureImage>
CreateTextureImage(GLContext* gl,
                   const gfx::IntSize& aSize,
                   TextureImage::ContentType aContentType,
                   GLenum aWrapMode,
                   TextureImage::Flags aFlags,
                   TextureImage::ImageFormat aImageFormat)
{
    switch (gl->GetContextType()) {
#ifdef XP_MACOSX
        case GLContextType::CGL:
            return CreateTextureImageCGL(gl, aSize, aContentType, aWrapMode, aFlags, aImageFormat);
#endif
        case GLContextType::EGL:
            return CreateTextureImageEGL(gl, aSize, aContentType, aWrapMode, aFlags, aImageFormat);
        default:
            return CreateBasicTextureImage(gl, aSize, aContentType, aWrapMode, aFlags, aImageFormat);
    }
}


static already_AddRefed<TextureImage>
TileGenFunc(GLContext* gl,
            const IntSize& aSize,
            TextureImage::ContentType aContentType,
            TextureImage::Flags aFlags,
            TextureImage::ImageFormat aImageFormat)
{
    switch (gl->GetContextType()) {
#ifdef XP_MACOSX
        case GLContextType::CGL:
            return TileGenFuncCGL(gl, aSize, aContentType, aFlags, aImageFormat);
#endif
        case GLContextType::EGL:
            return TileGenFuncEGL(gl, aSize, aContentType, aFlags, aImageFormat);
        default:
            return nullptr;
    }
}

already_AddRefed<TextureImage>
TextureImage::Create(GLContext* gl,
                     const gfx::IntSize& size,
                     TextureImage::ContentType contentType,
                     GLenum wrapMode,
                     TextureImage::Flags flags)
{
    return CreateTextureImage(gl, size, contentType, wrapMode, flags);
}

bool
TextureImage::UpdateFromDataSource(gfx::DataSourceSurface *aSurface,
                                   const nsIntRegion* aDestRegion,
                                   const gfx::IntPoint* aSrcPoint)
{
    nsIntRegion destRegion = aDestRegion ? *aDestRegion
                                         : IntRect(0, 0,
                                                     aSurface->GetSize().width,
                                                     aSurface->GetSize().height);
    gfx::IntPoint srcPoint = aSrcPoint ? *aSrcPoint
                                       : gfx::IntPoint(0, 0);
    return DirectUpdate(aSurface, destRegion, srcPoint);
}

gfx::IntRect TextureImage::GetTileRect() {
    return gfx::IntRect(gfx::IntPoint(0,0), mSize);
}

gfx::IntRect TextureImage::GetSrcTileRect() {
    return GetTileRect();
}

BasicTextureImage::~BasicTextureImage()
{
    GLContext *ctx = mGLContext;
    if (ctx->IsDestroyed() || !ctx->IsOwningThreadCurrent()) {
        ctx = ctx->GetSharedContext();
    }

    // If we have a context, then we need to delete the texture;
    // if we don't have a context (either real or shared),
    // then they went away when the contex was deleted, because it
    // was the only one that had access to it.
    if (ctx && ctx->MakeCurrent()) {
        ctx->fDeleteTextures(1, &mTexture);
    }
}

gfx::DrawTarget*
BasicTextureImage::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mUpdateDrawTarget, "BeginUpdate() without EndUpdate()?");

    // determine the region the client will need to repaint
    if (CanUploadSubTextures(mGLContext)) {
        GetUpdateRegion(aRegion);
    } else {
        aRegion = IntRect(IntPoint(0, 0), mSize);
    }

    mUpdateRegion = aRegion;

    IntRect rgnSize = mUpdateRegion.GetBounds();
    if (!IntRect(IntPoint(0, 0), mSize).Contains(rgnSize)) {
        NS_ERROR("update outside of image");
        return nullptr;
    }

    gfx::SurfaceFormat format =
        (GetContentType() == gfxContentType::COLOR) ?
        gfx::SurfaceFormat::B8G8R8X8 : gfx::SurfaceFormat::B8G8R8A8;
    mUpdateDrawTarget =
        GetDrawTargetForUpdate(gfx::IntSize(rgnSize.width, rgnSize.height), format);

    return mUpdateDrawTarget;
}

void
BasicTextureImage::GetUpdateRegion(nsIntRegion& aForRegion)
{
  // if the texture hasn't been initialized yet, or something important
  // changed, we need to recreate our backing surface and force the
  // client to paint everything
  if (mTextureState != Valid) {
      aForRegion = IntRect(IntPoint(0, 0), mSize);
  }
}

void
BasicTextureImage::EndUpdate()
{
    NS_ASSERTION(!!mUpdateDrawTarget, "EndUpdate() without BeginUpdate()?");

    // FIXME: this is the slow boat.  Make me fast (with GLXPixmap?).

    RefPtr<gfx::SourceSurface> updateSnapshot = mUpdateDrawTarget->Snapshot();
    RefPtr<gfx::DataSourceSurface> updateData = updateSnapshot->GetDataSurface();

    bool relative = FinishedSurfaceUpdate();

    mTextureFormat =
        UploadSurfaceToTexture(mGLContext,
                               updateData,
                               mUpdateRegion,
                               mTexture,
                               mTextureState == Created,
                               mUpdateOffset,
                               relative);
    FinishedSurfaceUpload();

    mUpdateDrawTarget = nullptr;
    mTextureState = Valid;
}

void
BasicTextureImage::BindTexture(GLenum aTextureUnit)
{
    mGLContext->fActiveTexture(aTextureUnit);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
}

already_AddRefed<gfx::DrawTarget>
BasicTextureImage::GetDrawTargetForUpdate(const gfx::IntSize& aSize, gfx::SurfaceFormat aFmt)
{
    return gfx::Factory::CreateDrawTarget(gfx::BackendType::CAIRO, aSize, aFmt);
}

bool
BasicTextureImage::FinishedSurfaceUpdate()
{
    return false;
}

void
BasicTextureImage::FinishedSurfaceUpload()
{
}

bool
BasicTextureImage::DirectUpdate(gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion, const gfx::IntPoint& aFrom /* = gfx::IntPoint(0, 0) */)
{
    IntRect bounds = aRegion.GetBounds();
    nsIntRegion region;
    if (mTextureState != Valid) {
        bounds = IntRect(0, 0, mSize.width, mSize.height);
        region = nsIntRegion(bounds);
    } else {
        region = aRegion;
    }

    mTextureFormat =
        UploadSurfaceToTexture(mGLContext,
                               aSurf,
                               region,
                               mTexture,
                               mTextureState == Created,
                               bounds.TopLeft() + IntPoint(aFrom.x, aFrom.y),
                               false);
    mTextureState = Valid;
    return true;
}

void
BasicTextureImage::Resize(const gfx::IntSize& aSize)
{
    NS_ASSERTION(!mUpdateDrawTarget, "Resize() while in update?");

    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    // This matches the logic in UploadImageDataToTexture so that
    // we avoid mixing formats.
    GLenum format;
    GLenum type;
    if (mGLContext->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
        MOZ_ASSERT(!mGLContext->IsGLES());
        format = LOCAL_GL_BGRA;
        type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
    } else {
        format = LOCAL_GL_RGBA;
        type = LOCAL_GL_UNSIGNED_BYTE;
    }

    mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                            0,
                            LOCAL_GL_RGBA,
                            aSize.width,
                            aSize.height,
                            0,
                            format,
                            type,
                            nullptr);

    mTextureState = Allocated;
    mSize = aSize;
}

gfx::IntSize TextureImage::GetSize() const {
  return mSize;
}

TextureImage::TextureImage(const gfx::IntSize& aSize,
             GLenum aWrapMode, ContentType aContentType,
             Flags aFlags, ImageFormat aImageFormat)
    : mSize(aSize)
    , mWrapMode(aWrapMode)
    , mContentType(aContentType)
    , mFilter(Filter::GOOD)
    , mFlags(aFlags)
{}

BasicTextureImage::BasicTextureImage(GLuint aTexture,
                  const gfx::IntSize& aSize,
                  GLenum aWrapMode,
                  ContentType aContentType,
                  GLContext* aContext,
                  TextureImage::Flags aFlags,
                  TextureImage::ImageFormat aImageFormat)
  : TextureImage(aSize, aWrapMode, aContentType, aFlags, aImageFormat)
  , mTexture(aTexture)
  , mTextureState(Created)
  , mGLContext(aContext)
  , mUpdateOffset(0, 0)
{}

static bool
WantsSmallTiles(GLContext* gl)
{
    // We can't use small tiles on the SGX 540, because of races in texture upload.
    if (gl->WorkAroundDriverBugs() &&
        gl->Renderer() == GLRenderer::SGX540)
        return false;

    // We should use small tiles for good performance if we can't use
    // glTexSubImage2D() for some reason.
    if (!CanUploadSubTextures(gl))
        return true;

    // Don't use small tiles otherwise. (If we implement incremental texture upload,
    // then we will want to revisit this.)
    return false;
}

TiledTextureImage::TiledTextureImage(GLContext* aGL,
                                     gfx::IntSize aSize,
                                     TextureImage::ContentType aContentType,
                                     TextureImage::Flags aFlags,
                                     TextureImage::ImageFormat aImageFormat)
    : TextureImage(aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType, aFlags)
    , mCurrentImage(0)
    , mIterationCallback(nullptr)
    , mInUpdate(false)
    , mRows(0)
    , mColumns(0)
    , mGL(aGL)
    , mTextureState(Created)
    , mImageFormat(aImageFormat)
{
    if (!(aFlags & TextureImage::DisallowBigImage) && WantsSmallTiles(mGL)) {
      mTileSize = 256;
    } else {
      mGL->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, (GLint*) &mTileSize);
    }
    if (aSize.width != 0 && aSize.height != 0) {
        Resize(aSize);
    }
}

TiledTextureImage::~TiledTextureImage()
{
}

bool
TiledTextureImage::DirectUpdate(gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion, const gfx::IntPoint& aFrom /* = gfx::IntPoint(0, 0) */)
{
    if (mSize.width == 0 || mSize.height == 0) {
        return true;
    }

    nsIntRegion region;

    if (mTextureState != Valid) {
        IntRect bounds = IntRect(0, 0, mSize.width, mSize.height);
        region = nsIntRegion(bounds);
    } else {
        region = aRegion;
    }

    bool result = true;
    int oldCurrentImage = mCurrentImage;
    BeginBigImageIteration();
    do {
        IntRect tileRect = GetSrcTileRect();
        int xPos = tileRect.x;
        int yPos = tileRect.y;

        nsIntRegion tileRegion;
        tileRegion.And(region, tileRect); // intersect with tile

        if (tileRegion.IsEmpty())
            continue;

        if (CanUploadSubTextures(mGL)) {
          tileRegion.MoveBy(-xPos, -yPos); // translate into tile local space
        } else {
          // If sub-textures are unsupported, expand to tile boundaries
          tileRect.x = tileRect.y = 0;
          tileRegion = nsIntRegion(tileRect);
        }

        result &= mImages[mCurrentImage]->
          DirectUpdate(aSurf, tileRegion, aFrom + gfx::IntPoint(xPos, yPos));

        if (mCurrentImage == mImages.Length() - 1) {
            // We know we're done, but we still need to ensure that the callback
            // gets called (e.g. to update the uploaded region).
            NextTile();
            break;
        }
        // Override a callback cancelling iteration if the texture wasn't valid.
        // We need to force the update in that situation, or we may end up
        // showing invalid/out-of-date texture data.
    } while (NextTile() || (mTextureState != Valid));
    mCurrentImage = oldCurrentImage;

    mTextureFormat = mImages[0]->GetTextureFormat();
    mTextureState = Valid;
    return result;
}

void
TiledTextureImage::GetUpdateRegion(nsIntRegion& aForRegion)
{
    if (mTextureState != Valid) {
        // if the texture hasn't been initialized yet, or something important
        // changed, we need to recreate our backing surface and force the
        // client to paint everything
        aForRegion = IntRect(IntPoint(0, 0), mSize);
        return;
    }

    nsIntRegion newRegion;

    // We need to query each texture with the region it will be drawing and
    // set aForRegion to be the combination of all of these regions
    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        IntRect imageRect = IntRect(IntPoint(xPos,yPos),
                                        mImages[i]->GetSize());

        if (aForRegion.Intersects(imageRect)) {
            // Make a copy of the region
            nsIntRegion subRegion;
            subRegion.And(aForRegion, imageRect);
            // Translate it into tile-space
            subRegion.MoveBy(-xPos, -yPos);
            // Query region
            mImages[i]->GetUpdateRegion(subRegion);
            // Translate back
            subRegion.MoveBy(xPos, yPos);
            // Add to the accumulated region
            newRegion.Or(newRegion, subRegion);
        }
    }

    aForRegion = newRegion;
}

gfx::DrawTarget*
TiledTextureImage::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mInUpdate, "nested update");
    mInUpdate = true;

    // Note, we don't call GetUpdateRegion here as if the updated region is
    // fully contained in a single tile, we get to avoid iterating through
    // the tiles again (and a little copying).
    if (mTextureState != Valid)
    {
        // if the texture hasn't been initialized yet, or something important
        // changed, we need to recreate our backing surface and force the
        // client to paint everything
        aRegion = IntRect(IntPoint(0, 0), mSize);
    }

    IntRect bounds = aRegion.GetBounds();

    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        nsIntRegion imageRegion =
          nsIntRegion(IntRect(IntPoint(xPos,yPos),
                                mImages[i]->GetSize()));

        // a single Image can handle this update request
        if (imageRegion.Contains(aRegion)) {
            // adjust for tile offset
            aRegion.MoveBy(-xPos, -yPos);
            // forward the actual call
            RefPtr<gfx::DrawTarget> drawTarget = mImages[i]->BeginUpdate(aRegion);
            // caller expects container space
            aRegion.MoveBy(xPos, yPos);
            // we don't have a temp surface
            mUpdateDrawTarget = nullptr;
            // remember which image to EndUpdate
            mCurrentImage = i;
            return drawTarget.get();
        }
    }

    // Get the real updated region, taking into account the capabilities of
    // each TextureImage tile
    GetUpdateRegion(aRegion);
    mUpdateRegion = aRegion;
    bounds = aRegion.GetBounds();

    // update covers multiple Images - create a temp surface to paint in
    gfx::SurfaceFormat format =
        (GetContentType() == gfxContentType::COLOR) ?
        gfx::SurfaceFormat::B8G8R8X8: gfx::SurfaceFormat::B8G8R8A8;
    mUpdateDrawTarget = gfx::Factory::CreateDrawTarget(gfx::BackendType::CAIRO,
                                                       bounds.Size(),
                                                       format);

    return mUpdateDrawTarget;;
}

void
TiledTextureImage::EndUpdate()
{
    NS_ASSERTION(mInUpdate, "EndUpdate not in update");
    if (!mUpdateDrawTarget) { // update was to a single TextureImage
        mImages[mCurrentImage]->EndUpdate();
        mInUpdate = false;
        mTextureState = Valid;
        mTextureFormat = mImages[mCurrentImage]->GetTextureFormat();
        return;
    }

    RefPtr<gfx::SourceSurface> updateSnapshot = mUpdateDrawTarget->Snapshot();
    RefPtr<gfx::DataSourceSurface> updateData = updateSnapshot->GetDataSurface();

    // upload tiles from temp surface
    for (unsigned i = 0; i < mImages.Length(); i++) {
        int xPos = (i % mColumns) * mTileSize;
        int yPos = (i / mColumns) * mTileSize;
        IntRect imageRect = IntRect(IntPoint(xPos,yPos), mImages[i]->GetSize());

        nsIntRegion subregion;
        subregion.And(mUpdateRegion, imageRect);
        if (subregion.IsEmpty())
            continue;
        subregion.MoveBy(-xPos, -yPos); // Tile-local space
        // copy tile from temp target
        gfx::DrawTarget* drawTarget = mImages[i]->BeginUpdate(subregion);
        MOZ_ASSERT(drawTarget->GetBackendType() == BackendType::CAIRO,
                   "updateSnapshot should not have been converted to data");
        gfxUtils::ClipToRegion(drawTarget, subregion);
        Size size(updateData->GetSize().width,
                  updateData->GetSize().height);
        drawTarget->DrawSurface(updateData,
                                Rect(Point(-xPos, -yPos), size),
                                Rect(Point(0, 0), size),
                                DrawSurfaceOptions(),
                                DrawOptions(1.0, CompositionOp::OP_SOURCE,
                                            AntialiasMode::NONE));
        drawTarget->PopClip();
        mImages[i]->EndUpdate();
    }

    mUpdateDrawTarget = nullptr;
    mInUpdate = false;
    mTextureFormat = mImages[0]->GetTextureFormat();
    mTextureState = Valid;
}

void TiledTextureImage::BeginBigImageIteration()
{
    mCurrentImage = 0;
}

bool TiledTextureImage::NextTile()
{
    bool continueIteration = true;

    if (mIterationCallback)
        continueIteration = mIterationCallback(this, mCurrentImage,
                                               mIterationCallbackData);

    if (mCurrentImage + 1 < mImages.Length()) {
        mCurrentImage++;
        return continueIteration;
    }
    return false;
}

void TiledTextureImage::SetIterationCallback(BigImageIterationCallback aCallback,
                                             void* aCallbackData)
{
    mIterationCallback = aCallback;
    mIterationCallbackData = aCallbackData;
}

gfx::IntRect TiledTextureImage::GetTileRect()
{
    if (!GetTileCount()) {
        return gfx::IntRect();
    }
    gfx::IntRect rect = mImages[mCurrentImage]->GetTileRect();
    unsigned int xPos = (mCurrentImage % mColumns) * mTileSize;
    unsigned int yPos = (mCurrentImage / mColumns) * mTileSize;
    rect.MoveBy(xPos, yPos);
    return rect;
}

gfx::IntRect TiledTextureImage::GetSrcTileRect()
{
    gfx::IntRect rect = GetTileRect();
    const bool needsYFlip = mFlags & OriginBottomLeft;
    unsigned int srcY = needsYFlip ? mSize.height - rect.height - rect.y
                                   : rect.y;
    return gfx::IntRect(rect.x, srcY, rect.width, rect.height);
}

void
TiledTextureImage::BindTexture(GLenum aTextureUnit)
{
    if (!GetTileCount()) {
        return;
    }
    mImages[mCurrentImage]->BindTexture(aTextureUnit);
}

/*
 * Resize, trying to reuse tiles. The reuse strategy is to decide on reuse per
 * column. A tile on a column is reused if it hasn't changed size, otherwise it
 * is discarded/replaced. Extra tiles on a column are pruned after iterating
 * each column, and extra rows are pruned after iteration over the entire image
 * finishes.
 */
void TiledTextureImage::Resize(const gfx::IntSize& aSize)
{
    if (mSize == aSize && mTextureState != Created) {
        return;
    }

    // calculate rows and columns, rounding up
    unsigned int columns = (aSize.width  + mTileSize - 1) / mTileSize;
    unsigned int rows = (aSize.height + mTileSize - 1) / mTileSize;

    // Iterate over old tile-store and insert/remove tiles as necessary
    int row;
    unsigned int i = 0;
    for (row = 0; row < (int)rows; row++) {
        // If we've gone beyond how many rows there were before, set mColumns to
        // zero so that we only create new tiles.
        if (row >= (int)mRows)
            mColumns = 0;

        // Similarly, if we're on the last row of old tiles and the height has
        // changed, discard all tiles in that row.
        // This will cause the pruning of columns not to work, but we don't need
        // to worry about that, as no more tiles will be reused past this point
        // anyway.
        if ((row == (int)mRows - 1) && (aSize.height != mSize.height))
            mColumns = 0;

        int col;
        for (col = 0; col < (int)columns; col++) {
            IntSize size( // use tilesize first, then the remainder
                    (col+1) * mTileSize > (unsigned int)aSize.width  ? aSize.width  % mTileSize : mTileSize,
                    (row+1) * mTileSize > (unsigned int)aSize.height ? aSize.height % mTileSize : mTileSize);

            bool replace = false;

            // Check if we can re-use old tiles.
            if (col < (int)mColumns) {
                // Reuse an existing tile. If the tile is an end-tile and the
                // width differs, replace it instead.
                if (mSize.width != aSize.width) {
                    if (col == (int)mColumns - 1) {
                        // Tile at the end of the old column, replace it with
                        // a new one.
                        replace = true;
                    } else if (col == (int)columns - 1) {
                        // Tile at the end of the new column, create a new one.
                    } else {
                        // Before the last column on both the old and new sizes,
                        // reuse existing tile.
                        i++;
                        continue;
                    }
                } else {
                    // Width hasn't changed, reuse existing tile.
                    i++;
                    continue;
                }
            }

            // Create a new tile.
            RefPtr<TextureImage> teximg =
                TileGenFunc(mGL, size, mContentType, mFlags, mImageFormat);
            if (replace)
                mImages.ReplaceElementAt(i, teximg);
            else
                mImages.InsertElementAt(i, teximg);
            i++;
        }

        // Prune any unused tiles on the end of the column.
        if (row < (int)mRows) {
            for (col = (int)mColumns - col; col > 0; col--) {
                mImages.RemoveElementAt(i);
            }
        }
    }

    // Prune any unused tiles at the end of the store.
    unsigned int length = mImages.Length();
    for (; i < length; i++)
      mImages.RemoveElementAt(mImages.Length()-1);

    // Reset tile-store properties.
    mRows = rows;
    mColumns = columns;
    mSize = aSize;
    mTextureState = Allocated;
    mCurrentImage = 0;
}

uint32_t TiledTextureImage::GetTileCount()
{
    return mImages.Length();
}

already_AddRefed<TextureImage>
CreateBasicTextureImage(GLContext* aGL,
                        const gfx::IntSize& aSize,
                        TextureImage::ContentType aContentType,
                        GLenum aWrapMode,
                        TextureImage::Flags aFlags,
                        TextureImage::ImageFormat aImageFormat)
{
    bool useNearestFilter = aFlags & TextureImage::UseNearestFilter;
    if (!aGL->MakeCurrent()) {
      return nullptr;
    }

    GLuint texture = 0;
    aGL->fGenTextures(1, &texture);

    ScopedBindTexture bind(aGL, texture);

    GLint texfilter = useNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

    RefPtr<BasicTextureImage> texImage =
        new BasicTextureImage(texture, aSize, aWrapMode, aContentType,
                              aGL, aFlags, aImageFormat);
    return texImage.forget();
}

} // namespace gl
} // namespace mozilla

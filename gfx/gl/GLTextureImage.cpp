/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
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
#include "GfxTexturesReporter.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

already_AddRefed<TextureImage> CreateTextureImage(
    GLContext* gl, const gfx::IntSize& aSize,
    TextureImage::ContentType aContentType, GLenum aWrapMode,
    TextureImage::Flags aFlags, TextureImage::ImageFormat aImageFormat) {
  GLint maxTextureSize;
  gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &maxTextureSize);
  if (aSize.width > maxTextureSize || aSize.height > maxTextureSize) {
    NS_ASSERTION(aWrapMode == LOCAL_GL_CLAMP_TO_EDGE,
                 "Can't support wrapping with tiles!");
    return CreateTiledTextureImage(gl, aSize, aContentType, aFlags,
                                   aImageFormat);
  } else {
    return CreateBasicTextureImage(gl, aSize, aContentType, aWrapMode, aFlags);
  }
}

static already_AddRefed<TextureImage> TileGenFunc(
    GLContext* gl, const IntSize& aSize, TextureImage::ContentType aContentType,
    TextureImage::Flags aFlags, TextureImage::ImageFormat aImageFormat) {
  return CreateBasicTextureImage(gl, aSize, aContentType,
                                 LOCAL_GL_CLAMP_TO_EDGE, aFlags);
}

already_AddRefed<TextureImage> TextureImage::Create(
    GLContext* gl, const gfx::IntSize& size,
    TextureImage::ContentType contentType, GLenum wrapMode,
    TextureImage::Flags flags) {
  return CreateTextureImage(gl, size, contentType, wrapMode, flags);
}

bool TextureImage::UpdateFromDataSource(gfx::DataSourceSurface* aSurface,
                                        const nsIntRegion* aDestRegion,
                                        const gfx::IntPoint* aSrcOffset,
                                        const gfx::IntPoint* aDstOffset) {
  nsIntRegion destRegion = aDestRegion
                               ? *aDestRegion
                               : IntRect(0, 0, aSurface->GetSize().width,
                                         aSurface->GetSize().height);
  gfx::IntPoint srcPoint = aSrcOffset ? *aSrcOffset : gfx::IntPoint(0, 0);
  gfx::IntPoint srcPointOut = aDstOffset ? *aDstOffset : gfx::IntPoint(0, 0);
  return DirectUpdate(aSurface, destRegion, srcPoint, srcPointOut);
}

gfx::IntRect TextureImage::GetTileRect() {
  return gfx::IntRect(gfx::IntPoint(0, 0), mSize);
}

gfx::IntRect TextureImage::GetSrcTileRect() { return GetTileRect(); }

void TextureImage::UpdateUploadSize(size_t amount) {
  if (mUploadSize > 0) {
    GfxTexturesReporter::UpdateAmount(GfxTexturesReporter::MemoryFreed,
                                      mUploadSize);
  }
  mUploadSize = amount;
  GfxTexturesReporter::UpdateAmount(GfxTexturesReporter::MemoryAllocated,
                                    mUploadSize);
}

BasicTextureImage::~BasicTextureImage() {
  GLContext* ctx = mGLContext;
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

void BasicTextureImage::BindTexture(GLenum aTextureUnit) {
  mGLContext->fActiveTexture(aTextureUnit);
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
}

bool BasicTextureImage::DirectUpdate(
    gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion,
    const gfx::IntPoint& aSrcOffset /* = gfx::IntPoint(0, 0) */,
    const gfx::IntPoint& aDstOffset /* = gfx::IntPoint(0, 0) */) {
  nsIntRegion region;
  if (mTextureState == Valid) {
    region = aRegion;
  } else {
    region = nsIntRegion(IntRect(0, 0, mSize.width, mSize.height));
  }
  bool needInit = mTextureState == Created;
  size_t uploadSize;

  mTextureFormat =
      UploadSurfaceToTexture(mGLContext, aSurf, region, mTexture, mSize,
                             &uploadSize, needInit, aSrcOffset, aDstOffset);
  if (mTextureFormat == SurfaceFormat::UNKNOWN) {
    return false;
  }

  if (uploadSize > 0) {
    UpdateUploadSize(uploadSize);
  }
  mTextureState = Valid;
  return true;
}

void BasicTextureImage::Resize(const gfx::IntSize& aSize) {
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

  mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, aSize.width,
                          aSize.height, 0, format, type, nullptr);

  mTextureState = Allocated;
  mSize = aSize;
}

gfx::IntSize TextureImage::GetSize() const { return mSize; }

TextureImage::TextureImage(const gfx::IntSize& aSize, GLenum aWrapMode,
                           ContentType aContentType, Flags aFlags)
    : mSize(aSize),
      mWrapMode(aWrapMode),
      mContentType(aContentType),
      mTextureFormat(gfx::SurfaceFormat::UNKNOWN),
      mSamplingFilter(SamplingFilter::GOOD),
      mFlags(aFlags),
      mUploadSize(0) {}

BasicTextureImage::BasicTextureImage(GLuint aTexture, const gfx::IntSize& aSize,
                                     GLenum aWrapMode, ContentType aContentType,
                                     GLContext* aContext,
                                     TextureImage::Flags aFlags)
    : TextureImage(aSize, aWrapMode, aContentType, aFlags),
      mTexture(aTexture),
      mTextureState(Created),
      mGLContext(aContext) {}

static bool WantsSmallTiles(GLContext* gl) {
  // We can't use small tiles on the SGX 540, because of races in texture
  // upload.
  if (gl->WorkAroundDriverBugs() && gl->Renderer() == GLRenderer::SGX540)
    return false;

  // We should use small tiles for good performance if we can't use
  // glTexSubImage2D() for some reason.
  if (!ShouldUploadSubTextures(gl)) return true;

  // Don't use small tiles otherwise. (If we implement incremental texture
  // upload, then we will want to revisit this.)
  return false;
}

TiledTextureImage::TiledTextureImage(GLContext* aGL, gfx::IntSize aSize,
                                     TextureImage::ContentType aContentType,
                                     TextureImage::Flags aFlags,
                                     TextureImage::ImageFormat aImageFormat)
    : TextureImage(aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType, aFlags),
      mCurrentImage(0),
      mIterationCallback(nullptr),
      mIterationCallbackData(nullptr),
      mTileSize(0),
      mRows(0),
      mColumns(0),
      mGL(aGL),
      mTextureState(Created),
      mImageFormat(aImageFormat) {
  if (!(aFlags & TextureImage::DisallowBigImage) && WantsSmallTiles(mGL)) {
    mTileSize = 256;
  } else {
    mGL->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, (GLint*)&mTileSize);
  }
  if (aSize.width != 0 && aSize.height != 0) {
    Resize(aSize);
  }
}

TiledTextureImage::~TiledTextureImage() = default;

bool TiledTextureImage::DirectUpdate(
    gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion,
    const gfx::IntPoint& aSrcOffset /* = gfx::IntPoint(0, 0) */,
    const gfx::IntPoint& aDstOffset /* = gfx::IntPoint(0, 0) */) {
  // We don't handle non-zero aDstOffset
  MOZ_RELEASE_ASSERT(aDstOffset == gfx::IntPoint());

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
    int xPos = tileRect.X();
    int yPos = tileRect.Y();

    nsIntRegion tileRegion;
    tileRegion.And(region, tileRect);  // intersect with tile

    if (tileRegion.IsEmpty()) continue;

    tileRegion.MoveBy(-xPos, -yPos);  // translate into tile local space

    result &= mImages[mCurrentImage]->DirectUpdate(
        aSurf, tileRegion, aSrcOffset + gfx::IntPoint(xPos, yPos));

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

void TiledTextureImage::BeginBigImageIteration() { mCurrentImage = 0; }

bool TiledTextureImage::NextTile() {
  bool continueIteration = true;

  if (mIterationCallback)
    continueIteration =
        mIterationCallback(this, mCurrentImage, mIterationCallbackData);

  if (mCurrentImage + 1 < mImages.Length()) {
    mCurrentImage++;
    return continueIteration;
  }
  return false;
}

void TiledTextureImage::SetIterationCallback(
    BigImageIterationCallback aCallback, void* aCallbackData) {
  mIterationCallback = aCallback;
  mIterationCallbackData = aCallbackData;
}

gfx::IntRect TiledTextureImage::GetTileRect() {
  if (!GetTileCount()) {
    return gfx::IntRect();
  }
  gfx::IntRect rect = mImages[mCurrentImage]->GetTileRect();
  unsigned int xPos = (mCurrentImage % mColumns) * mTileSize;
  unsigned int yPos = (mCurrentImage / mColumns) * mTileSize;
  rect.MoveBy(xPos, yPos);
  return rect;
}

gfx::IntRect TiledTextureImage::GetSrcTileRect() {
  gfx::IntRect rect = GetTileRect();
  const bool needsYFlip = mFlags & OriginBottomLeft;
  unsigned int srcY =
      needsYFlip ? mSize.height - rect.Height() - rect.Y() : rect.Y();
  return gfx::IntRect(rect.X(), srcY, rect.Width(), rect.Height());
}

void TiledTextureImage::BindTexture(GLenum aTextureUnit) {
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
void TiledTextureImage::Resize(const gfx::IntSize& aSize) {
  if (mSize == aSize && mTextureState != Created) {
    return;
  }

  // calculate rows and columns, rounding up
  unsigned int columns = (aSize.width + mTileSize - 1) / mTileSize;
  unsigned int rows = (aSize.height + mTileSize - 1) / mTileSize;

  // Iterate over old tile-store and insert/remove tiles as necessary
  int row;
  unsigned int i = 0;
  for (row = 0; row < (int)rows; row++) {
    // If we've gone beyond how many rows there were before, set mColumns to
    // zero so that we only create new tiles.
    if (row >= (int)mRows) mColumns = 0;

    // Similarly, if we're on the last row of old tiles and the height has
    // changed, discard all tiles in that row.
    // This will cause the pruning of columns not to work, but we don't need
    // to worry about that, as no more tiles will be reused past this point
    // anyway.
    if ((row == (int)mRows - 1) && (aSize.height != mSize.height)) mColumns = 0;

    int col;
    for (col = 0; col < (int)columns; col++) {
      IntSize size(  // use tilesize first, then the remainder
          (col + 1) * mTileSize > (unsigned int)aSize.width
              ? aSize.width % mTileSize
              : mTileSize,
          (row + 1) * mTileSize > (unsigned int)aSize.height
              ? aSize.height % mTileSize
              : mTileSize);

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
  mImages.RemoveLastElements(mImages.Length() - i);

  // Reset tile-store properties.
  mRows = rows;
  mColumns = columns;
  mSize = aSize;
  mTextureState = Allocated;
  mCurrentImage = 0;
}

uint32_t TiledTextureImage::GetTileCount() { return mImages.Length(); }

already_AddRefed<TextureImage> CreateTiledTextureImage(
    GLContext* aGL, const gfx::IntSize& aSize,
    TextureImage::ContentType aContentType, TextureImage::Flags aFlags,
    TextureImage::ImageFormat aImageFormat) {
  RefPtr<TextureImage> texImage =
      static_cast<TextureImage*>(new gl::TiledTextureImage(
          aGL, aSize, aContentType, aFlags, aImageFormat));
  return texImage.forget();
}

already_AddRefed<TextureImage> CreateBasicTextureImage(
    GLContext* aGL, const gfx::IntSize& aSize,
    TextureImage::ContentType aContentType, GLenum aWrapMode,
    TextureImage::Flags aFlags) {
  bool useNearestFilter = aFlags & TextureImage::UseNearestFilter;
  if (!aGL->MakeCurrent()) {
    return nullptr;
  }

  GLuint texture = 0;
  aGL->fGenTextures(1, &texture);

  ScopedBindTexture bind(aGL, texture);

  GLint texfilter = useNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                      texfilter);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                      texfilter);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

  RefPtr<BasicTextureImage> texImage = new BasicTextureImage(
      texture, aSize, aWrapMode, aContentType, aGL, aFlags);
  return texImage.forget();
}

}  // namespace gl
}  // namespace mozilla

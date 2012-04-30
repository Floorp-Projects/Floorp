/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersChild.h"
#include "TiledThebesLayerOGL.h"
#include "ReusableTileStoreOGL.h"
#include "BasicTiledThebesLayer.h"
#include "gfxImageSurface.h"

namespace mozilla {
namespace layers {

using mozilla::gl::GLContext;

TiledLayerBufferOGL::~TiledLayerBufferOGL()
{
  if (mRetainedTiles.Length() == 0)
    return;

  mContext->MakeCurrent();
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    if (mRetainedTiles[i] == GetPlaceholderTile())
      continue;
    mContext->fDeleteTextures(1, &mRetainedTiles[i].mTextureHandle);
  }
}

void
TiledLayerBufferOGL::ReleaseTile(TiledTexture aTile)
{
  // We've made current prior to calling TiledLayerBufferOGL::Update
  if (aTile == GetPlaceholderTile())
    return;
  mContext->fDeleteTextures(1, &aTile.mTextureHandle);
}

void
TiledLayerBufferOGL::Upload(const BasicTiledLayerBuffer* aMainMemoryTiledBuffer,
                            const nsIntRegion& aNewValidRegion,
                            const nsIntRegion& aInvalidateRegion,
                            const gfxSize& aResolution)
{
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Upload %i, %i, %i, %i\n", aInvalidateRegion.GetBounds().x, aInvalidateRegion.GetBounds().y, aInvalidateRegion.GetBounds().width, aInvalidateRegion.GetBounds().height);
  long start = PR_IntervalNow();
#endif

  mResolution = aResolution;
  mMainMemoryTiledBuffer = aMainMemoryTiledBuffer;
  mContext->MakeCurrent();
  Update(aNewValidRegion, aInvalidateRegion);
  mMainMemoryTiledBuffer = nsnull;
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    printf_stderr("Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
}

void
TiledLayerBufferOGL::GetFormatAndTileForImageFormat(gfxASurface::gfxImageFormat aFormat,
                                                    GLenum& aOutFormat,
                                                    GLenum& aOutType)
{
  if (aFormat == gfxASurface::ImageFormatRGB16_565) {
    aOutFormat = LOCAL_GL_RGB;
    aOutType = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
  } else {
    aOutFormat = LOCAL_GL_RGBA;
    aOutType = LOCAL_GL_UNSIGNED_BYTE;
  }
}

TiledTexture
TiledLayerBufferOGL::ValidateTile(TiledTexture aTile,
                                  const nsIntPoint& aTileOrigin,
                                  const nsIntRegion& aDirtyRect)
{
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  printf_stderr("Upload tile %i, %i\n", aTileOrigin.x, aTileOrigin.y);
  long start = PR_IntervalNow();
#endif
  if (aTile == GetPlaceholderTile()) {
    mContext->fGenTextures(1, &aTile.mTextureHandle);
    mContext->fBindTexture(LOCAL_GL_TEXTURE_2D, aTile.mTextureHandle);
    mContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    mContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    mContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  } else {
    mContext->fBindTexture(LOCAL_GL_TEXTURE_2D, aTile.mTextureHandle);
  }

  nsRefPtr<gfxReusableSurfaceWrapper> reusableSurface = mMainMemoryTiledBuffer->GetTile(aTileOrigin).mSurface.get();
  GLenum format, type;
  GetFormatAndTileForImageFormat(reusableSurface->Format(), format, type);

  const unsigned char* buf = reusableSurface->GetReadOnlyData();
  mContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, format,
                       GetTileLength(), GetTileLength(), 0,
                       format, type, buf);

  aTile.mFormat = format;

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 1) {
    printf_stderr("Tile Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
  return aTile;
}

TiledThebesLayerOGL::TiledThebesLayerOGL(LayerManagerOGL *aManager)
  : ShadowThebesLayer(aManager, nsnull)
  , LayerOGL(aManager)
  , mVideoMemoryTiledBuffer(aManager->gl())
{
  mImplData = static_cast<LayerOGL*>(this);
  // XXX Add a pref for reusable tile store size
  mReusableTileStore = new ReusableTileStoreOGL(aManager->gl(), 1);
}

TiledThebesLayerOGL::~TiledThebesLayerOGL()
{
  mMainMemoryTiledBuffer.ReadUnlock();
  if (mReusableTileStore)
    delete mReusableTileStore;
}

void
TiledThebesLayerOGL::PaintedTiledLayerBuffer(const BasicTiledLayerBuffer* mTiledBuffer)
{
  mMainMemoryTiledBuffer = *mTiledBuffer;
  mRegionToUpload.Or(mRegionToUpload, mMainMemoryTiledBuffer.GetLastPaintRegion());

}

void
TiledThebesLayerOGL::ProcessUploadQueue()
{
  if (mRegionToUpload.IsEmpty())
    return;

  gfxSize resolution(1, 1);
  if (mReusableTileStore) {
    // Work out render resolution by multiplying the resolution of our ancestors.
    // Only container layers can have frame metrics, so we start off with a
    // resolution of 1, 1.
    // XXX For large layer trees, it would be faster to do this once from the
    //     root node upwards and store the value on each layer.
    for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
      const FrameMetrics& metrics = parent->GetFrameMetrics();
      resolution.width *= metrics.mResolution.width;
      resolution.height *= metrics.mResolution.height;
    }

    const FrameMetrics& metrics = GetParent()->GetFrameMetrics();
    mReusableTileStore->HarvestTiles(&mVideoMemoryTiledBuffer,
                                     metrics.mContentSize,
                                     mVideoMemoryTiledBuffer.GetValidRegion(),
                                     mMainMemoryTiledBuffer.GetValidRegion(),
                                     mVideoMemoryTiledBuffer.GetResolution(),
                                     resolution);
  }

  mVideoMemoryTiledBuffer.Upload(&mMainMemoryTiledBuffer,
                                 mMainMemoryTiledBuffer.GetValidRegion(),
                                 mRegionToUpload, resolution);
  mValidRegion = mVideoMemoryTiledBuffer.GetValidRegion();

  mMainMemoryTiledBuffer.ReadUnlock();
  // Release all the tiles by replacing the tile buffer with an empty
  // tiled buffer. This will prevent us from doing a double unlock when
  // calling  ~TiledThebesLayerOGL.
  // FIXME: This wont be needed when we do progressive upload and lock
  // tile by tile.
  mMainMemoryTiledBuffer = BasicTiledLayerBuffer();
  mRegionToUpload = nsIntRegion();

}

void
TiledThebesLayerOGL::RenderTile(TiledTexture aTile,
                                const gfx3DMatrix& aTransform,
                                const nsIntPoint& aOffset,
                                nsIntRect aScreenRect,
                                nsIntRect aTextureRect,
                                nsIntSize aTextureBounds)
{
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, aTile.mTextureHandle);
    ColorTextureLayerProgram *program;
    if (aTile.mFormat == LOCAL_GL_RGB) {
      program = mOGLManager->GetRGBXLayerProgram();
    } else {
      program = mOGLManager->GetBGRALayerProgram();
    }
    program->Activate();
    program->SetTextureUnit(0);
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset);
    program->SetLayerQuadRect(aScreenRect);

    mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                aTextureRect,
                                                aTextureBounds);
}

void
TiledThebesLayerOGL::RenderLayer(int aPreviousFrameBuffer, const nsIntPoint& aOffset)
{
  gl()->MakeCurrent();
  ProcessUploadQueue();

  // Render old tiles to fill in gaps we haven't had the time to render yet.
  if (mReusableTileStore) {
    const FrameMetrics& metrics = GetParent()->GetFrameMetrics();
    mReusableTileStore->DrawTiles(this, metrics.mContentSize,
                                  mVideoMemoryTiledBuffer.GetValidRegion(),
                                  mVideoMemoryTiledBuffer.GetResolution(),
                                  GetEffectiveTransform(), aOffset);
  }

  // Render valid tiles.
  const nsIntRegion& visibleRegion = GetEffectiveVisibleRegion();
  const nsIntRect visibleRect = visibleRegion.GetBounds();
  unsigned int rowCount = 0;
  int tileX = 0;
  for (size_t x = visibleRect.x; x < visibleRect.x + visibleRect.width;) {
    rowCount++;
    uint16_t tileStartX = x % mVideoMemoryTiledBuffer.GetTileLength();
    uint16_t w = mVideoMemoryTiledBuffer.GetTileLength() - tileStartX;
    if (x + w > visibleRect.x + visibleRect.width)
      w = visibleRect.x + visibleRect.width - x;
    int tileY = 0;
    for( size_t y = visibleRect.y; y < visibleRect.y + visibleRect.height;) {
      uint16_t tileStartY = y % mVideoMemoryTiledBuffer.GetTileLength();
      uint16_t h = mVideoMemoryTiledBuffer.GetTileLength() - tileStartY;
      if (y + h > visibleRect.y + visibleRect.height)
        h = visibleRect.y + visibleRect.height - y;

      TiledTexture tileTexture = mVideoMemoryTiledBuffer.
        GetTile(nsIntPoint(mVideoMemoryTiledBuffer.RoundDownToTileEdge(x),
                           mVideoMemoryTiledBuffer.RoundDownToTileEdge(y)));
      if (tileTexture != mVideoMemoryTiledBuffer.GetPlaceholderTile()) {
        uint16_t tileSize = mVideoMemoryTiledBuffer.GetTileLength();
        RenderTile(tileTexture, GetEffectiveTransform(), aOffset, nsIntRect(x,y,w,h),
                   nsIntRect(tileStartX, tileStartY, w, h), nsIntSize(tileSize, tileSize));
      }
      tileY++;
      y += h;
    }
    tileX++;
    x += w;
  }

}

} // mozilla
} // layers

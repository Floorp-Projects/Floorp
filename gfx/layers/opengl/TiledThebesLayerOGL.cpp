/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersChild.h"
#include "TiledThebesLayerOGL.h"
#include "ReusableTileStoreOGL.h"
#include "BasicTiledThebesLayer.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace layers {

using mozilla::gl::GLContext;

TiledLayerBufferOGL::~TiledLayerBufferOGL()
{
  if (mRetainedTiles.Length() == 0)
    return;

  mContext->MakeCurrent();
  for (size_t i = 0; i < mRetainedTiles.Length(); i++) {
    ReleaseTile(mRetainedTiles[i]);
  }
}

void
TiledLayerBufferOGL::ReleaseTile(TiledTexture aTile)
{
  // We've made current prior to calling TiledLayerBufferOGL::Update
  if (aTile == GetPlaceholderTile())
    return;
  mContext->fDeleteTextures(1, &aTile.mTextureHandle);

  GLContext::UpdateTextureMemoryUsage(GLContext::MemoryFreed, aTile.mFormat, GetTileType(aTile), GetTileLength());
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

  mFrameResolution = aResolution;
  mMainMemoryTiledBuffer = aMainMemoryTiledBuffer;
  mContext->MakeCurrent();
  Update(aNewValidRegion, aInvalidateRegion);
  mMainMemoryTiledBuffer = nullptr;
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    printf_stderr("Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
}

GLenum
TiledLayerBufferOGL::GetTileType(TiledTexture aTile)
{
  // Deduce the type that was assigned in GetFormatAndTileForImageFormat
  return aTile.mFormat == LOCAL_GL_RGB ? LOCAL_GL_UNSIGNED_SHORT_5_6_5 : LOCAL_GL_UNSIGNED_BYTE;
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
    // We're re-using a texture, but the format may change. Update the memory
    // reporter with a free and alloc (below) using the old and new formats.
    GLContext::UpdateTextureMemoryUsage(GLContext::MemoryFreed, aTile.mFormat, GetTileType(aTile), GetTileLength());
  }

  nsRefPtr<gfxReusableSurfaceWrapper> reusableSurface = mMainMemoryTiledBuffer->GetTile(aTileOrigin).mSurface.get();
  GLenum format, type;
  GetFormatAndTileForImageFormat(reusableSurface->Format(), format, type);

  const unsigned char* buf = reusableSurface->GetReadOnlyData();
  mContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, format,
                       GetTileLength(), GetTileLength(), 0,
                       format, type, buf);

  GLContext::UpdateTextureMemoryUsage(GLContext::MemoryAllocated, format, type, GetTileLength());

  aTile.mFormat = format;

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 1) {
    printf_stderr("Tile Time to upload %i\n", PR_IntervalNow() - start);
  }
#endif
  return aTile;
}

TiledThebesLayerOGL::TiledThebesLayerOGL(LayerManagerOGL *aManager)
  : ShadowThebesLayer(aManager, nullptr)
  , LayerOGL(aManager)
  , mVideoMemoryTiledBuffer(aManager->gl())
  , mLowPrecisionVideoMemoryTiledBuffer(aManager->gl())
  , mReusableTileStore(nullptr)
  , mPendingUpload(false)
  , mPendingLowPrecisionUpload(false)
{
  mImplData = static_cast<LayerOGL*>(this);
}

TiledThebesLayerOGL::~TiledThebesLayerOGL()
{
  mMainMemoryTiledBuffer.ReadUnlock();
  mLowPrecisionMainMemoryTiledBuffer.ReadUnlock();
  if (mReusableTileStore)
    delete mReusableTileStore;
}

void
TiledThebesLayerOGL::MemoryPressure()
{
  if (mReusableTileStore) {
    delete mReusableTileStore;
    mReusableTileStore = new ReusableTileStoreOGL(gl(), 1);
  }
}

void
TiledThebesLayerOGL::PaintedTiledLayerBuffer(const BasicTiledLayerBuffer* mTiledBuffer)
{
  if (mTiledBuffer->IsLowPrecision()) {
    mLowPrecisionMainMemoryTiledBuffer.ReadUnlock();
    mLowPrecisionMainMemoryTiledBuffer = *mTiledBuffer;
    mLowPrecisionRegionToUpload.Or(mLowPrecisionRegionToUpload,
                                   mLowPrecisionMainMemoryTiledBuffer.GetPaintedRegion());
    mLowPrecisionMainMemoryTiledBuffer.ClearPaintedRegion();
    mPendingLowPrecisionUpload = true;
  } else {
    mMainMemoryTiledBuffer.ReadUnlock();
    mMainMemoryTiledBuffer = *mTiledBuffer;
    mRegionToUpload.Or(mRegionToUpload, mMainMemoryTiledBuffer.GetPaintedRegion());
    mMainMemoryTiledBuffer.ClearPaintedRegion();
    mPendingUpload = true;
  }

  // TODO: Remove me once Bug 747811 lands.
  delete mTiledBuffer;
}

void
TiledThebesLayerOGL::ProcessLowPrecisionUploadQueue()
{
  if (!mPendingLowPrecisionUpload)
    return;

  mLowPrecisionRegionToUpload.And(mLowPrecisionRegionToUpload,
                                  mLowPrecisionMainMemoryTiledBuffer.GetValidRegion());
  mLowPrecisionVideoMemoryTiledBuffer.SetResolution(
    mLowPrecisionMainMemoryTiledBuffer.GetResolution());
  // XXX It's assumed that the video memory tiled buffer has an up-to-date
  //     frame resolution. As it's always updated first when zooming, this
  //     should always be true.
  mLowPrecisionVideoMemoryTiledBuffer.Upload(&mLowPrecisionMainMemoryTiledBuffer,
                                 mLowPrecisionMainMemoryTiledBuffer.GetValidRegion(),
                                 mLowPrecisionRegionToUpload,
                                 mVideoMemoryTiledBuffer.GetFrameResolution());
  nsIntRegion validRegion = mLowPrecisionVideoMemoryTiledBuffer.GetValidRegion();

  mLowPrecisionMainMemoryTiledBuffer.ReadUnlock();

  mLowPrecisionMainMemoryTiledBuffer = BasicTiledLayerBuffer();
  mLowPrecisionRegionToUpload = nsIntRegion();
  mPendingLowPrecisionUpload = false;
}

void
TiledThebesLayerOGL::ProcessUploadQueue()
{
  if (!mPendingUpload)
    return;

  // We should only be retaining old tiles if we're not fixed position.
  // Fixed position layers don't/shouldn't move on the screen, so retaining
  // tiles is not useful and often results in rendering artifacts.
  if (mReusableTileStore && mIsFixedPosition) {
    delete mReusableTileStore;
    mReusableTileStore = nullptr;
  } else if (gfxPlatform::UseReusableTileStore() &&
             !mReusableTileStore && !mIsFixedPosition) {
    // XXX Add a pref for reusable tile store size
    mReusableTileStore = new ReusableTileStoreOGL(gl(), 1);
  }

  // Work out render resolution by multiplying the resolution of our ancestors.
  // Only container layers can have frame metrics, so we start off with a
  // resolution of 1, 1.
  // XXX For large layer trees, it would be faster to do this once from the
  //     root node upwards and store the value on each layer.
  gfxSize resolution(1, 1);
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    resolution.width *= metrics.mResolution.width;
    resolution.height *= metrics.mResolution.height;
  }

  if (mReusableTileStore) {
    mReusableTileStore->HarvestTiles(this,
                                     &mVideoMemoryTiledBuffer,
                                     mVideoMemoryTiledBuffer.GetValidRegion(),
                                     mMainMemoryTiledBuffer.GetValidRegion(),
                                     mVideoMemoryTiledBuffer.GetFrameResolution(),
                                     resolution);
  }

  // If we coalesce uploads while the layers' valid region is changing we will
  // end up trying to upload area outside of the valid region. (bug 756555)
  mRegionToUpload.And(mRegionToUpload, mMainMemoryTiledBuffer.GetValidRegion());

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
  mPendingUpload = false;
}

void
TiledThebesLayerOGL::RenderTile(const TiledTexture& aTile,
                                const gfx3DMatrix& aTransform,
                                const nsIntPoint& aOffset,
                                const nsIntRegion& aScreenRegion,
                                const nsIntPoint& aTextureOffset,
                                const nsIntSize& aTextureBounds,
                                Layer* aMaskLayer)
{
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, aTile.mTextureHandle);
    ShaderProgramOGL *program;
    if (aTile.mFormat == LOCAL_GL_RGB) {
      program = mOGLManager->GetProgram(gl::RGBXLayerProgramType, aMaskLayer);
    } else {
      program = mOGLManager->GetProgram(gl::BGRALayerProgramType, aMaskLayer);
    }
    program->Activate();
    program->SetTextureUnit(0);
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetLayerTransform(aTransform);
    program->SetRenderOffset(aOffset);
    program->LoadMask(aMaskLayer);

    nsIntRegionRectIterator it(aScreenRegion);
    for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
      nsIntRect textureRect(rect->x - aTextureOffset.x, rect->y - aTextureOffset.y,
                            rect->width, rect->height);
      program->SetLayerQuadRect(*rect);
      mOGLManager->BindAndDrawQuadWithTextureRect(program,
                                                  textureRect,
                                                  aTextureBounds);
    }
}

void
TiledThebesLayerOGL::RenderLayerBuffer(TiledLayerBufferOGL& aLayerBuffer,
                                       const nsIntRegion& aValidRegion,
                                       const nsIntPoint& aOffset,
                                       const nsIntRegion& aMaskRegion)
{
  Layer* maskLayer = GetMaskLayer();
  const nsIntRegion& visibleRegion = GetEffectiveVisibleRegion();
  nsIntRect visibleRect = visibleRegion.GetBounds();
  gfx3DMatrix transform = GetEffectiveTransform();

  float resolution = aLayerBuffer.GetResolution();
  gfxSize layerScale(1, 1);
  // We assume that the current frame resolution is the one used in our primary
  // layer buffer. Compensate for a changing frame resolution.
  if (aLayerBuffer.GetFrameResolution() != mVideoMemoryTiledBuffer.GetFrameResolution()) {
    const gfxSize& layerResolution = aLayerBuffer.GetFrameResolution();
    const gfxSize& localResolution = mVideoMemoryTiledBuffer.GetFrameResolution();
    layerScale.width = layerResolution.width / localResolution.width;
    layerScale.height = layerResolution.height / localResolution.height;
    visibleRect.ScaleRoundOut(layerScale.width, layerScale.height);
  }
  transform.Scale(1/(resolution * layerScale.width),
                  1/(resolution * layerScale.height), 1);

  uint32_t rowCount = 0;
  uint32_t tileX = 0;
  for (int32_t x = visibleRect.x; x < visibleRect.x + visibleRect.width;) {
    rowCount++;
    int32_t tileStartX = aLayerBuffer.GetTileStart(x);
    int32_t w = aLayerBuffer.GetScaledTileLength() - tileStartX;
    if (x + w > visibleRect.x + visibleRect.width)
      w = visibleRect.x + visibleRect.width - x;
    int tileY = 0;
    for (int32_t y = visibleRect.y; y < visibleRect.y + visibleRect.height;) {
      int32_t tileStartY = aLayerBuffer.GetTileStart(y);
      int32_t h = aLayerBuffer.GetScaledTileLength() - tileStartY;
      if (y + h > visibleRect.y + visibleRect.height)
        h = visibleRect.y + visibleRect.height - y;

      TiledTexture tileTexture = aLayerBuffer.
        GetTile(nsIntPoint(aLayerBuffer.RoundDownToTileEdge(x),
                           aLayerBuffer.RoundDownToTileEdge(y)));
      if (tileTexture != aLayerBuffer.GetPlaceholderTile()) {
        nsIntRegion tileDrawRegion;
        tileDrawRegion.And(aValidRegion,
                           nsIntRect(x * layerScale.width,
                                     y * layerScale.height,
                                     w * layerScale.width,
                                     h * layerScale.height));
        tileDrawRegion.Sub(tileDrawRegion, aMaskRegion);

        if (!tileDrawRegion.IsEmpty()) {
          tileDrawRegion.ScaleRoundOut(resolution / layerScale.width,
                                       resolution / layerScale.height);

          nsIntPoint tileOffset((x - tileStartX) * resolution,
                                (y - tileStartY) * resolution);
          uint32_t tileSize = aLayerBuffer.GetTileLength();
          RenderTile(tileTexture, transform, aOffset, tileDrawRegion,
                     tileOffset, nsIntSize(tileSize, tileSize), maskLayer);
        }
      }
      tileY++;
      y += h;
    }
    tileX++;
    x += w;
  }
}

void
TiledThebesLayerOGL::RenderLayer(int aPreviousFrameBuffer, const nsIntPoint& aOffset)
{
  gl()->MakeCurrent();
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  ProcessUploadQueue();
  ProcessLowPrecisionUploadQueue();

  // Render old tiles to fill in gaps we haven't had the time to render yet.
  if (mReusableTileStore) {
    mReusableTileStore->DrawTiles(this,
                                  mVideoMemoryTiledBuffer.GetValidRegion(),
                                  mVideoMemoryTiledBuffer.GetFrameResolution(),
                                  GetEffectiveTransform(), aOffset, GetMaskLayer());
  }

  // Render valid tiles.
  RenderLayerBuffer(mLowPrecisionVideoMemoryTiledBuffer,
                    mLowPrecisionVideoMemoryTiledBuffer.GetValidRegion(),
                    aOffset, mValidRegion);
  RenderLayerBuffer(mVideoMemoryTiledBuffer, mValidRegion, aOffset, nsIntRegion());
}

} // mozilla
} // layers

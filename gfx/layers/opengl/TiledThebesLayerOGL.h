/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDTHEBESLAYEROGL_H
#define GFX_TILEDTHEBESLAYEROGL_H

#include "mozilla/layers/ShadowLayers.h"
#include "TiledLayerBuffer.h"
#include "Layers.h"
#include "LayerManagerOGL.h"
#include "BasicTiledThebesLayer.h"
#include <algorithm>

namespace mozilla {

namespace gl {
class GLContext;
}

namespace layers {

class ReusableTileStoreOGL;

class TiledTexture {
public:
  // Constructs a placeholder TiledTexture. See the comments above
  // TiledLayerBuffer for more information on what this is used for;
  // essentially, this is a sentinel used to represent an invalid or blank
  // tile.
  //
  // Note that we assume that zero is not a valid GL texture handle here.
  TiledTexture()
    : mTextureHandle(0)
    , mFormat(0)
  {}

  // Constructs a TiledTexture from a GL texture handle and an image format.
  TiledTexture(GLuint aTextureHandle, GLenum aFormat)
    : mTextureHandle(aTextureHandle)
    , mFormat(aFormat)
  {}

  TiledTexture(const TiledTexture& o) {
    mTextureHandle = o.mTextureHandle;
    mFormat = o.mFormat;
  }
  TiledTexture& operator=(const TiledTexture& o) {
    if (this == &o) return *this;
    mTextureHandle = o.mTextureHandle;
    mFormat = o.mFormat;
    return *this;
  }
  bool operator== (const TiledTexture& o) const {
    return mTextureHandle == o.mTextureHandle;
  }
  bool operator!= (const TiledTexture& o) const {
    return mTextureHandle != o.mTextureHandle;
  }

  GLuint mTextureHandle;
  GLenum mFormat;
};

class TiledLayerBufferOGL : public TiledLayerBuffer<TiledLayerBufferOGL, TiledTexture>
{
  friend class TiledLayerBuffer<TiledLayerBufferOGL, TiledTexture>;

public:
  TiledLayerBufferOGL(gl::GLContext* aContext)
    : mContext(aContext)
  {}

  ~TiledLayerBufferOGL();

  void Upload(const BasicTiledLayerBuffer* aMainMemoryTiledBuffer,
              const nsIntRegion& aNewValidRegion,
              const nsIntRegion& aInvalidateRegion,
              const gfxSize& aResolution);

  TiledTexture GetPlaceholderTile() const { return TiledTexture(); }

  // Stores the absolute resolution of the containing frame, calculated
  // by the sum of the resolutions of all parent layers' FrameMetrics.
  const gfxSize& GetFrameResolution() { return mFrameResolution; }

protected:
  TiledTexture ValidateTile(TiledTexture aTile,
                            const nsIntPoint& aTileRect,
                            const nsIntRegion& dirtyRect);

  void ReleaseTile(TiledTexture aTile);

  void SwapTiles(TiledTexture& aTileA, TiledTexture& aTileB) {
    std::swap(aTileA, aTileB);
  }

private:
  nsRefPtr<gl::GLContext> mContext;
  const BasicTiledLayerBuffer* mMainMemoryTiledBuffer;
  gfxSize mFrameResolution;

  GLenum GetTileType(TiledTexture aTile);
  void GetFormatAndTileForImageFormat(gfxASurface::gfxImageFormat aFormat,
                                      GLenum& aOutFormat,
                                      GLenum& aOutType);
};

class TiledThebesLayerOGL : public ShadowThebesLayer,
                            public LayerOGL,
                            public TiledLayerComposer
{
public:
  TiledThebesLayerOGL(LayerManagerOGL *aManager);
  virtual ~TiledThebesLayerOGL();

  // LayerOGL impl
  void Destroy() {}
  Layer* GetLayer() { return this; }
  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources() { }

  // Shadow
  virtual TiledLayerComposer* AsTiledLayerComposer() { return this; }
  virtual void DestroyFrontBuffer() {}
  void Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion)
  {
    NS_ABORT_IF_FALSE(false, "Not supported");
  }
  void PaintedTiledLayerBuffer(const BasicTiledLayerBuffer* mTiledBuffer);
  void ProcessUploadQueue();
  void ProcessLowPrecisionUploadQueue();
  const nsIntRegion& GetValidLowPrecisionRegion() const { return mLowPrecisionVideoMemoryTiledBuffer.GetValidRegion(); }

  void MemoryPressure();

  // Renders a single given tile.
  void RenderTile(const TiledTexture& aTile,
                  const gfx3DMatrix& aTransform,
                  const nsIntPoint& aOffset,
                  const nsIntRegion& aScreenRegion,
                  const nsIntPoint& aTextureOffset,
                  const nsIntSize& aTextureBounds,
                  Layer* aMaskLayer);

private:
  void RenderLayerBuffer(TiledLayerBufferOGL& aLayerBuffer,
                         const nsIntRegion& aValidRegion,
                         const nsIntPoint& aOffset,
                         const nsIntRegion& aMaskRegion);

  nsIntRegion                  mRegionToUpload;
  nsIntRegion                  mLowPrecisionRegionToUpload;
  BasicTiledLayerBuffer        mMainMemoryTiledBuffer;
  BasicTiledLayerBuffer        mLowPrecisionMainMemoryTiledBuffer;
  TiledLayerBufferOGL          mVideoMemoryTiledBuffer;
  TiledLayerBufferOGL          mLowPrecisionVideoMemoryTiledBuffer;
  ReusableTileStoreOGL*        mReusableTileStore;
  bool                         mPendingUpload : 1;
  bool                         mPendingLowPrecisionUpload : 1;
};

} // layers
} // mozilla

#endif

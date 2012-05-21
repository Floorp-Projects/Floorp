/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACED2DTARGET_H_
#define MOZILLA_GFX_SOURCESURFACED2DTARGET_H_

#include "2D.h"
#include "HelpersD2D.h"
#include <vector>
#include <d3d10_1.h>

namespace mozilla {
namespace gfx {

class DrawTargetD2D;

class SourceSurfaceD2DTarget : public SourceSurface
{
public:
  SourceSurfaceD2DTarget(DrawTargetD2D* aDrawTarget, ID3D10Texture2D* aTexture,
                         SurfaceFormat aFormat);
  ~SourceSurfaceD2DTarget();

  virtual SurfaceType GetType() const { return SURFACE_D2D1_DRAWTARGET; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

private:
  friend class DrawTargetD2D;
  ID3D10ShaderResourceView *GetSRView();

  // This function is called by the draw target this texture belongs to when
  // it is about to be changed. The texture will be required to make a copy
  // of itself when this happens.
  void DrawTargetWillChange();

  // This will mark the surface as no longer depending on its drawtarget,
  // this may happen on destruction or copying.
  void MarkIndependent();

  ID2D1Bitmap *GetBitmap(ID2D1RenderTarget *aRT);

  RefPtr<ID3D10ShaderResourceView> mSRView;
  RefPtr<ID2D1Bitmap> mBitmap;
  // Non-null if this is a "lazy copy" of the given draw target.
  // Null if we've made a copy. The target is not kept alive, otherwise we'd
  // have leaks since it might keep us alive. If the target is destroyed, it
  // will notify us.
  DrawTargetD2D* mDrawTarget;
  mutable RefPtr<ID3D10Texture2D> mTexture;
  SurfaceFormat mFormat;
};

class DataSourceSurfaceD2DTarget : public DataSourceSurface
{
public:
  DataSourceSurfaceD2DTarget();
  ~DataSourceSurfaceD2DTarget();

  virtual SurfaceType GetType() const { return SURFACE_DATA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual unsigned char *GetData();
  virtual int32_t Stride();

private:
  friend class SourceSurfaceD2DTarget;
  void EnsureMapped();

  mutable RefPtr<ID3D10Texture2D> mTexture;
  SurfaceFormat mFormat;
  D3D10_MAPPED_TEXTURE2D mMap;
  bool mMapped;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACED2DTARGET_H_ */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACERAWDATA_H_
#define MOZILLA_GFX_SOURCESURFACERAWDATA_H_

#include "2D.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

class SourceSurfaceRawData : public DataSourceSurface
{
public:
  SourceSurfaceRawData() {}
  ~SourceSurfaceRawData() { if(mOwnData) delete [] mRawData; }

  virtual uint8_t *GetData() { return mRawData; }
  virtual int32_t Stride() { return mStride; }

  virtual SurfaceType GetType() const { return SURFACE_DATA; }
  virtual IntSize GetSize() const { return mSize; }
  virtual SurfaceFormat GetFormat() const { return mFormat; }

  bool InitWrappingData(unsigned char *aData,
                        const IntSize &aSize,
                        int32_t aStride,
                        SurfaceFormat aFormat,
                        bool aOwnData);

private:
  uint8_t *mRawData;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
  bool mOwnData;
};

class SourceSurfaceAlignedRawData : public DataSourceSurface
{
public:
  SourceSurfaceAlignedRawData() {}

  virtual uint8_t *GetData() { return mArray; }
  virtual int32_t Stride() { return mStride; }

  virtual SurfaceType GetType() const { return SURFACE_DATA; }
  virtual IntSize GetSize() const { return mSize; }
  virtual SurfaceFormat GetFormat() const { return mFormat; }

  bool Init(const IntSize &aSize,
            SurfaceFormat aFormat);

private:
  AlignedArray<uint8_t> mArray;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACERAWDATA_H_ */

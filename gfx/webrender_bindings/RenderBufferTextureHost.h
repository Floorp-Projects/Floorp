/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERBUFFERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERBUFFERTEXTUREHOST_H

#include "RenderTextureHost.h"

namespace mozilla {
namespace wr {

class RenderBufferTextureHost final : public RenderTextureHost
{
public:
  RenderBufferTextureHost(uint8_t* aBuffer,
                          const layers::BufferDescriptor& aDescriptor);

  virtual bool Lock() override;
  virtual void Unlock() override;

  virtual RenderBufferTextureHost* AsBufferTextureHost() override
  {
    return this;
  }

  class RenderBufferData
  {
  public:
    RenderBufferData(uint8_t* aData, size_t aBufferSize)
      : mData(aData)
      , mBufferSize(aBufferSize)
    {
    }
    const uint8_t* mData;
    size_t mBufferSize;
  };

  RenderBufferData GetBufferDataForRender(uint8_t aChannelIndex);

private:
  virtual ~RenderBufferTextureHost();

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface();
  uint8_t* GetBuffer() const
  {
    return mBuffer;
  }

  uint8_t* mBuffer;
  layers::BufferDescriptor mDescriptor;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  RefPtr<gfx::DataSourceSurface> mSurface;
  gfx::DataSourceSurface::MappedSurface mMap;
  bool mLocked;
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERBUFFERTEXTUREHOST_H

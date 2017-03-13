/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERTEXTUREHOST_H

#include "nsISupportsImpl.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace wr {

class RenderTextureHost
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RenderTextureHost)

  RenderTextureHost(uint8_t* aBuffer, const layers::BufferDescriptor& aDescriptor);

  bool Lock();

  void Unlock();

  gfx::IntSize GetSize() const { return mSize; }

  gfx::SurfaceFormat GetFormat() const { return mFormat; }

  uint8_t* GetDataForRender() const;
  size_t GetBufferSizeForRender() const;

protected:
  ~RenderTextureHost();
  already_AddRefed<gfx::DataSourceSurface> GetAsSurface();
  uint8_t* GetBuffer() const { return mBuffer; }

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

#endif // MOZILLA_GFX_RENDERTEXTUREHOST_H

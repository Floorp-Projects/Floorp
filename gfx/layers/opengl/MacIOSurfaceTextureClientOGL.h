/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H

#include "mozilla/layers/TextureClientOGL.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

class MacIOSurfaceTextureClientOGL : public TextureClient
{
public:
  explicit MacIOSurfaceTextureClientOGL(ISurfaceAllocator* aAllcator,
                                        TextureFlags aFlags);

  virtual ~MacIOSurfaceTextureClientOGL();

  // Creates a TextureClient and init width.
  static already_AddRefed<MacIOSurfaceTextureClientOGL>
  Create(ISurfaceAllocator* aAllocator,
         TextureFlags aFlags,
         MacIOSurface* aSurface);

  virtual bool Lock(OpenMode aMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override;

  virtual bool IsAllocated() const override { return !!mSurface; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const override;

  virtual bool HasInternalBuffer() const override { return false; }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  // This TextureClient should not be used in a context where we use CreateSimilar
  // (ex. component alpha) because the underlying texture data is always created by
  // an external producer.
  virtual already_AddRefed<TextureClient>
  CreateSimilar(TextureFlags, TextureAllocationFlags) const override { return nullptr; }

protected:
  RefPtr<MacIOSurface> mSurface;
  bool mIsLocked;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H

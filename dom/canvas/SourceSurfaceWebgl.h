/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACEWEBGL_H_
#define MOZILLA_GFX_SOURCESURFACEWEBGL_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/WeakPtr.h"

namespace mozilla::gfx {

class DrawTargetWebgl;
class SharedContextWebgl;
class TextureHandle;

// SourceSurfaceWebgl holds WebGL resources that can be used to efficiently
// copy snapshot data between multiple DrawTargetWebgls. It also takes care
// of copy-on-write behavior when the owner target is modified or destructs.
class SourceSurfaceWebgl : public DataSourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceWebgl, override)

  explicit SourceSurfaceWebgl(DrawTargetWebgl* aDT);
  SourceSurfaceWebgl(const RefPtr<TextureHandle>& aHandle,
                     const RefPtr<SharedContextWebgl>& aSharedContext);
  virtual ~SourceSurfaceWebgl();

  SurfaceType GetType() const override { return SurfaceType::WEBGL; }
  IntSize GetSize() const override { return mSize; }
  SurfaceFormat GetFormat() const override { return mFormat; }

  uint8_t* GetData() override;
  int32_t Stride() override;

  bool Map(MapType aType, MappedSurface* aMappedSurface) override;
  void Unmap() override;

  bool HasReadData() const { return !!mData; }

  already_AddRefed<SourceSurface> ExtractSubrect(const IntRect& aRect) override;

 private:
  friend class DrawTargetWebgl;
  friend class SharedContextWebgl;

  bool EnsureData();

  void DrawTargetWillChange(bool aNeedHandle);

  void GiveTexture(RefPtr<TextureHandle> aHandle);

  void OnUnlinkTexture(SharedContextWebgl* aContext);

  DrawTargetWebgl* GetTarget() const { return mDT.get(); }

  SurfaceFormat mFormat = SurfaceFormat::UNKNOWN;
  IntSize mSize;
  // Any data that has been read back from the WebGL context for mapping.
  RefPtr<DataSourceSurface> mData;
  // The draw target that currently owns the texture for this surface.
  WeakPtr<DrawTargetWebgl> mDT;
  // The actual shared context that any WebGL resources belong to.
  WeakPtr<SharedContextWebgl> mSharedContext;
  // If this snapshot has been copied into a cached texture handle.
  RefPtr<TextureHandle> mHandle;
};

}  // namespace mozilla::gfx

#endif /* MOZILLA_GFX_SOURCESURFACEWEBGL_H_ */

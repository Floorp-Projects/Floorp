/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_RecyclingSourceSurface_h
#define mozilla_image_RecyclingSourceSurface_h

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace image {

class imgFrame;

/**
 * This surface subclass will prevent the underlying surface from being recycled
 * as long as it is still alive. We will create this surface to wrap imgFrame's
 * mLockedSurface, if we are accessing it on a path that will keep the surface
 * alive for an indeterminate period of time (e.g. imgFrame::GetSourceSurface,
 * imgFrame::Draw with a recording or capture DrawTarget).
 */
class RecyclingSourceSurface final : public gfx::DataSourceSurface {
 public:
  RecyclingSourceSurface(imgFrame* aParent, gfx::DataSourceSurface* aSurface);

  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(RecyclingSourceSurface, override);

  uint8_t* GetData() override { return mSurface->GetData(); }
  int32_t Stride() override { return mSurface->Stride(); }
  gfx::SurfaceType GetType() const override { return mType; }
  gfx::IntSize GetSize() const override { return mSurface->GetSize(); }
  gfx::SurfaceFormat GetFormat() const override {
    return mSurface->GetFormat();
  }

  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf, size_t& aHeapSizeOut,
                              size_t& aNonHeapSizeOut, size_t& aExtHandlesOut,
                              uint64_t& aExtIdOut) const override {}

  bool OnHeap() const override { return mSurface->OnHeap(); }
  bool Map(MapType aType, MappedSurface* aMappedSurface) override {
    return mSurface->Map(aType, aMappedSurface);
  }
  void Unmap() override { mSurface->Unmap(); }

  gfx::DataSourceSurface* GetChildSurface() const { return mSurface; }

 protected:
  void GuaranteePersistance() override {}

  ~RecyclingSourceSurface() override;

  RefPtr<imgFrame> mParent;
  RefPtr<DataSourceSurface> mSurface;
  gfx::SurfaceType mType;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_RecyclingSourceSurface_h

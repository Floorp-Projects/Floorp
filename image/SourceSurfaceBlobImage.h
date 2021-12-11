/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGE_SOURCESURFACEBLOBIMAGE_H_
#define MOZILLA_IMAGE_SOURCESURFACEBLOBIMAGE_H_

#include "mozilla/Maybe.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "ImageRegion.h"

#include <vector>

namespace mozilla {
namespace image {

class BlobImageKeyData final {
 public:
  BlobImageKeyData(layers::WebRenderLayerManager* aManager,
                   const wr::BlobImageKey& aBlobKey,
                   std::vector<RefPtr<gfx::ScaledFont>>&& aScaledFonts,
                   std::vector<RefPtr<gfx::SourceSurface>>&& aExternalSurfaces)
      : mManager(aManager),
        mBlobKey(aBlobKey),
        mScaledFonts(std::move(aScaledFonts)),
        mExternalSurfaces(std::move(aExternalSurfaces)),
        mDirty(false) {}

  BlobImageKeyData(BlobImageKeyData&& aOther) noexcept
      : mManager(std::move(aOther.mManager)),
        mBlobKey(aOther.mBlobKey),
        mScaledFonts(std::move(aOther.mScaledFonts)),
        mExternalSurfaces(std::move(aOther.mExternalSurfaces)),
        mDirty(aOther.mDirty) {}

  BlobImageKeyData& operator=(BlobImageKeyData&& aOther) noexcept {
    mManager = std::move(aOther.mManager);
    mBlobKey = aOther.mBlobKey;
    mScaledFonts = std::move(aOther.mScaledFonts);
    mExternalSurfaces = std::move(aOther.mExternalSurfaces);
    mDirty = aOther.mDirty;
    return *this;
  }

  BlobImageKeyData(const BlobImageKeyData&) = delete;
  BlobImageKeyData& operator=(const BlobImageKeyData&) = delete;

  RefPtr<layers::WebRenderLayerManager> mManager;
  wr::BlobImageKey mBlobKey;
  std::vector<RefPtr<gfx::ScaledFont>> mScaledFonts;
  std::vector<RefPtr<gfx::SourceSurface>> mExternalSurfaces;
  bool mDirty;
};

}  // namespace image
}  // namespace mozilla

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::image::BlobImageKeyData);

namespace mozilla {
namespace wr {
class IpcResourceUpdateQueue;
}  // namespace wr

namespace image {
class SVGDocumentWrapper;

/**
 * This class is used to wrap blob recordings stored in ImageContainers, used
 * by SVG images. Unlike rasterized images stored in SourceSurfaceSharedData,
 * each SourceSurfaceBlobImage can only be used by one WebRenderLayerManager
 * because the recording is tied to a particular instance.
 */
class SourceSurfaceBlobImage final : public gfx::SourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceBlobImage, override)

  SourceSurfaceBlobImage(SVGDocumentWrapper* aSVGDocumentWrapper,
                         const Maybe<SVGImageContext>& aSVGContext,
                         const Maybe<ImageIntRegion>& aRegion,
                         const gfx::IntSize& aSize, uint32_t aWhichFrame,
                         uint32_t aImageFlags);

  Maybe<wr::BlobImageKey> UpdateKey(layers::WebRenderLayerManager* aManager,
                                    wr::IpcResourceUpdateQueue& aResources);

  void MarkDirty();

  gfx::SurfaceType GetType() const override {
    return gfx::SurfaceType::BLOB_IMAGE;
  }
  gfx::IntSize GetSize() const override { return mSize; }
  gfx::SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::OS_RGBA;
  }
  already_AddRefed<gfx::DataSourceSurface> GetDataSurface() override {
    return nullptr;
  }

  void SizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                           SizeOfInfo& aInfo) const override {
    aInfo.AddType(gfx::SurfaceType::BLOB_IMAGE);
    aInfo.mHeapBytes += mKeys.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  ~SourceSurfaceBlobImage() override;

  Maybe<BlobImageKeyData> RecordDrawing(layers::WebRenderLayerManager* aManager,
                                        wr::IpcResourceUpdateQueue& aResources,
                                        Maybe<wr::BlobImageKey> aBlobKey);

  static void DestroyKeys(const AutoTArray<BlobImageKeyData, 1>& aKeys);

  AutoTArray<BlobImageKeyData, 1> mKeys;

  RefPtr<image::SVGDocumentWrapper> mSVGDocumentWrapper;
  Maybe<SVGImageContext> mSVGContext;
  Maybe<ImageIntRegion> mRegion;
  gfx::IntSize mSize;
  uint32_t mWhichFrame;
  uint32_t mImageFlags;
};

}  // namespace image
}  // namespace mozilla

#endif /* MOZILLA_IMAGE_SOURCESURFACEBLOBIMAGE_H_ */

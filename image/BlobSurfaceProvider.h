/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGE_BLOBSURFACEPROVIDER_H_
#define MOZILLA_IMAGE_BLOBSURFACEPROVIDER_H_

#include "mozilla/Maybe.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "ImageRegion.h"
#include "ISurfaceProvider.h"

#include <vector>

namespace mozilla {
namespace image {

class BlobImageKeyData final {
 public:
  BlobImageKeyData(
      layers::WebRenderLayerManager* aManager, const wr::BlobImageKey& aBlobKey,
      std::vector<RefPtr<gfx::ScaledFont>>&& aScaledFonts,
      gfx::DrawEventRecorderPrivate::ExternalSurfacesHolder&& aExternalSurfaces)
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
  gfx::DrawEventRecorderPrivate::ExternalSurfacesHolder mExternalSurfaces;
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
 * An ISurfaceProvider that manages blob recordings of SVG images. Unlike the
 * rasterized ISurfaceProviders, it only provides a recording which may be
 * replayed in the compositor process by WebRender. It may be invalidated
 * directly in order to reuse the resource ids and underlying buffers when the
 * SVG image has changed (e.g. it is animated).
 */
class BlobSurfaceProvider final : public ISurfaceProvider {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlobSurfaceProvider, override)

  BlobSurfaceProvider(ImageKey aImageKey, const SurfaceKey& aSurfaceKey,
                      SVGDocumentWrapper* aSVGDocumentWrapper,
                      uint32_t aImageFlags);

  bool IsFinished() const override { return true; }

  size_t LogicalSizeInBytes() const override {
    const gfx::IntSize& size = GetSurfaceKey().Size();
    return size.width * size.height * sizeof(uint32_t);
  }

  nsresult UpdateKey(layers::RenderRootStateManager* aManager,
                     wr::IpcResourceUpdateQueue& aResources,
                     wr::ImageKey& aKey) override;

  void InvalidateRecording() override;

  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              const AddSizeOfCb& aCallback) override {
    AddSizeOfCbData metadata;
    metadata.mFinished = true;
    metadata.mHeapBytes += mKeys.ShallowSizeOfExcludingThis(aMallocSizeOf);

    gfx::SourceSurface::SizeOfInfo info;
    info.AddType(gfx::SurfaceType::BLOB_IMAGE);
    metadata.Accumulate(info);

    aCallback(metadata);
  }

 protected:
  DrawableFrameRef DrawableRef(size_t aFrame) override {
    MOZ_ASSERT_UNREACHABLE("BlobSurfaceProvider::DrawableRef not supported!");
    return DrawableFrameRef();
  }
  bool IsLocked() const override { return true; }
  void SetLocked(bool) override {}

 private:
  ~BlobSurfaceProvider() override;

  Maybe<BlobImageKeyData> RecordDrawing(layers::WebRenderLayerManager* aManager,
                                        wr::IpcResourceUpdateQueue& aResources,
                                        Maybe<wr::BlobImageKey> aBlobKey);

  static void DestroyKeys(const AutoTArray<BlobImageKeyData, 1>& aKeys);

  AutoTArray<BlobImageKeyData, 1> mKeys;

  RefPtr<image::SVGDocumentWrapper> mSVGDocumentWrapper;
  uint32_t mImageFlags;
};

}  // namespace image
}  // namespace mozilla

#endif /* MOZILLA_IMAGE_BLOBSURFACEPROVIDER_H_ */

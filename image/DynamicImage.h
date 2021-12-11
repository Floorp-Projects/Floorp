/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_DynamicImage_h
#define mozilla_image_DynamicImage_h

#include "mozilla/MemoryReporting.h"
#include "gfxDrawable.h"
#include "Image.h"

namespace mozilla {
namespace image {

/**
 * An Image that is dynamically created. The content of the image is provided by
 * a gfxDrawable. It's anticipated that most uses of DynamicImage will be
 * ephemeral.
 */
class DynamicImage : public Image {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER

  explicit DynamicImage(gfxDrawable* aDrawable) : mDrawable(aDrawable) {
    MOZ_ASSERT(aDrawable, "Must have a gfxDrawable to wrap");
  }

  // Inherited methods from Image.
  nsresult GetNativeSizes(nsTArray<gfx::IntSize>& aNativeSizes) const override;
  size_t GetNativeSizesLength() const override;
  virtual already_AddRefed<ProgressTracker> GetProgressTracker() override;
  virtual size_t SizeOfSourceWithComputedFallback(
      SizeOfState& aState) const override;
  virtual void CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                     MallocSizeOf aMallocSizeOf) const override;

  virtual void IncrementAnimationConsumers() override;
  virtual void DecrementAnimationConsumers() override;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() override;
#endif

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) override;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext, nsresult aStatus,
                                       bool aLastPart) override;

  virtual void OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) override;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) override;
  virtual uint64_t InnerWindowID() const override;

  virtual bool HasError() override;
  virtual void SetHasError() override;

  nsIURI* GetURI() const override;

 private:
  virtual ~DynamicImage() {}

  RefPtr<gfxDrawable> mDrawable;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_DynamicImage_h

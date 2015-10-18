/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageWrapper_h
#define mozilla_image_ImageWrapper_h

#include "mozilla/MemoryReporting.h"
#include "Image.h"

namespace mozilla {
namespace image {

/**
 * Abstract superclass for Images that wrap other Images.
 */
class ImageWrapper : public Image
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER

  // Inherited methods from Image.
  virtual already_AddRefed<ProgressTracker> GetProgressTracker() override;

  virtual size_t
    SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const override;
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
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) override;

  virtual void OnSurfaceDiscarded() override;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) override;
  virtual uint64_t InnerWindowID() const override;

  virtual bool HasError() override;
  virtual void SetHasError() override;

  virtual ImageURL* GetURI() override;

protected:
  explicit ImageWrapper(Image* aInnerImage)
    : mInnerImage(aInnerImage)
  {
    MOZ_ASSERT(aInnerImage, "Need an image to wrap");
  }

  virtual ~ImageWrapper() { }

  /**
   * Returns a weak reference to the inner image wrapped by this ImageWrapper.
   */
  Image* InnerImage() const { return mInnerImage.get(); }

  void SetInnerImage(Image* aInnerImage)
  {
    MOZ_ASSERT(aInnerImage, "Need an image to wrap");
    mInnerImage = aInnerImage;
  }

private:
  RefPtr<Image> mInnerImage;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ImageWrapper_h

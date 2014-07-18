/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_IMAGEWRAPPER_H_
#define MOZILLA_IMAGELIB_IMAGEWRAPPER_H_

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
  virtual nsresult Init(const char* aMimeType, uint32_t aFlags) MOZ_OVERRIDE;

  virtual already_AddRefed<imgStatusTracker> GetStatusTracker() MOZ_OVERRIDE;
  virtual nsIntRect FrameRect(uint32_t aWhichFrame) MOZ_OVERRIDE;

  virtual uint32_t SizeOfData() MOZ_OVERRIDE;
  virtual size_t HeapSizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE;
  virtual size_t NonHeapSizeOfDecoded() const MOZ_OVERRIDE;
  virtual size_t OutOfProcessSizeOfDecoded() const MOZ_OVERRIDE;

  virtual size_t HeapSizeOfVectorImageDocument(nsACString* aDocURL = nullptr) const MOZ_OVERRIDE {
    return mInnerImage->HeapSizeOfVectorImageDocument(aDocURL);
  }

  virtual void IncrementAnimationConsumers() MOZ_OVERRIDE;
  virtual void DecrementAnimationConsumers() MOZ_OVERRIDE;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() MOZ_OVERRIDE;
#endif

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) MOZ_OVERRIDE;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) MOZ_OVERRIDE;
  virtual nsresult OnNewSourceData() MOZ_OVERRIDE;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) MOZ_OVERRIDE;
  virtual uint64_t InnerWindowID() const MOZ_OVERRIDE;

  virtual bool HasError() MOZ_OVERRIDE;
  virtual void SetHasError() MOZ_OVERRIDE;

  virtual ImageURL* GetURI() MOZ_OVERRIDE;

protected:
  ImageWrapper(Image* aInnerImage)
    : mInnerImage(aInnerImage)
  {
    NS_ABORT_IF_FALSE(aInnerImage, "Cannot wrap a null image");
  }

  virtual ~ImageWrapper() { }

  /**
   * Returns a weak reference to the inner image wrapped by this ImageWrapper.
   */
  Image* InnerImage() { return mInnerImage.get(); }

private:
  nsRefPtr<Image> mInnerImage;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_IMAGEWRAPPER_H_

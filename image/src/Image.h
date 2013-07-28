/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_IMAGE_H_
#define MOZILLA_IMAGELIB_IMAGE_H_

#include "mozilla/MemoryReporting.h"
#include "imgIContainer.h"
#include "imgStatusTracker.h"
#include "nsIURI.h"
#include "nsIRequest.h"
#include "nsIInputStream.h"

namespace mozilla {
namespace image {

class Image : public imgIContainer
{
public:
  // Mimetype translation
  enum eDecoderType {
    eDecoderType_png     = 0,
    eDecoderType_gif     = 1,
    eDecoderType_jpeg    = 2,
    eDecoderType_bmp     = 3,
    eDecoderType_ico     = 4,
    eDecoderType_icon    = 5,
    eDecoderType_unknown = 6
  };
  static eDecoderType GetDecoderType(const char *aMimeType);

  /**
   * Flags for Image initialization.
   *
   * Meanings:
   *
   * INIT_FLAG_NONE: Lack of flags
   *
   * INIT_FLAG_DISCARDABLE: The container should be discardable
   *
   * INIT_FLAG_DECODE_ON_DRAW: The container should decode on draw rather than
   * decoding on load.
   *
   * INIT_FLAG_MULTIPART: The container will be used to display a stream of
   * images in a multipart channel. If this flag is set, INIT_FLAG_DISCARDABLE
   * and INIT_FLAG_DECODE_ON_DRAW must not be set.
   */
  static const uint32_t INIT_FLAG_NONE           = 0x0;
  static const uint32_t INIT_FLAG_DISCARDABLE    = 0x1;
  static const uint32_t INIT_FLAG_DECODE_ON_DRAW = 0x2;
  static const uint32_t INIT_FLAG_MULTIPART      = 0x4;

  /**
   * Creates a new image container.
   *
   * @param aMimeType The mimetype of the image.
   * @param aFlags Initialization flags of the INIT_FLAG_* variety.
   */
  virtual nsresult Init(const char* aMimeType,
                        uint32_t aFlags) = 0;

  virtual imgStatusTracker& GetStatusTracker() = 0;

  /**
   * The rectangle defining the location and size of the given frame.
   */
  virtual nsIntRect FrameRect(uint32_t aWhichFrame) = 0;

  /**
   * The size, in bytes, occupied by the significant data portions of the image.
   * This includes both compressed source data and decoded frames.
   */
  virtual uint32_t SizeOfData() = 0;

  /**
   * The components that make up SizeOfData().
   */
  virtual size_t HeapSizeOfSourceWithComputedFallback(mozilla::MallocSizeOf aMallocSizeOf) const = 0;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(mozilla::MallocSizeOf aMallocSizeOf) const = 0;
  virtual size_t NonHeapSizeOfDecoded() const = 0;
  virtual size_t OutOfProcessSizeOfDecoded() const = 0;

  virtual void IncrementAnimationConsumers() = 0;
  virtual void DecrementAnimationConsumers() = 0;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() = 0;
#endif

  /**
   * Called from OnDataAvailable when the stream associated with the image has
   * received new image data. The arguments are the same as OnDataAvailable's,
   * but by separating this functionality into a different method we don't
   * interfere with subclasses which wish to implement nsIStreamListener.
   *
   * Images should not do anything that could send out notifications until they
   * have received their first OnImageDataAvailable notification; in
   * particular, this means that instantiating decoders should be deferred
   * until OnImageDataAvailable is called.
   */
  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) = 0;

  /**
   * Called from OnStopRequest when the image's underlying request completes.
   *
   * @param aRequest  The completed request.
   * @param aContext  Context from Necko's OnStopRequest.
   * @param aStatus   A success or failure code.
   * @param aLastPart Whether this is the final part of the underlying request.
   */
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aStatus,
                                       bool aLastPart) = 0;

  /**
   * Called for multipart images to allow for any necessary reinitialization
   * when there's a new part to add.
   */
  virtual nsresult OnNewSourceData() = 0;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) = 0;
  virtual uint64_t InnerWindowID() const = 0;

  virtual bool HasError() = 0;
  virtual void SetHasError() = 0;

  virtual nsIURI* GetURI() = 0;
};

class ImageResource : public Image
{
public:
  virtual imgStatusTracker& GetStatusTracker() MOZ_OVERRIDE { return *mStatusTracker; }
  virtual uint32_t SizeOfData() MOZ_OVERRIDE;

  virtual void IncrementAnimationConsumers() MOZ_OVERRIDE;
  virtual void DecrementAnimationConsumers() MOZ_OVERRIDE;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() MOZ_OVERRIDE { return mAnimationConsumers; }
#endif

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) MOZ_OVERRIDE {
    mInnerWindowId = aInnerWindowId;
  }
  virtual uint64_t InnerWindowID() const MOZ_OVERRIDE { return mInnerWindowId; }

  virtual bool HasError() MOZ_OVERRIDE    { return mError; }
  virtual void SetHasError() MOZ_OVERRIDE { mError = true; }

  /*
   * Returns a non-AddRefed pointer to the URI associated with this image.
   */
  virtual nsIURI* GetURI() MOZ_OVERRIDE { return mURI; }

protected:
  ImageResource(imgStatusTracker* aStatusTracker, nsIURI* aURI);

  // Shared functionality for implementors of imgIContainer. Every
  // implementation of attribute animationMode should forward here.
  nsresult GetAnimationModeInternal(uint16_t *aAnimationMode);
  nsresult SetAnimationModeInternal(uint16_t aAnimationMode);

  /**
   * Decides whether animation should or should not be happening,
   * and makes sure the right thing is being done.
   */
  virtual void EvaluateAnimation();

  /**
   * Extended by child classes, if they have additional
   * conditions for being able to animate.
   */
  virtual bool ShouldAnimate() {
    return mAnimationConsumers > 0 && mAnimationMode != kDontAnimMode;
  }

  virtual nsresult StartAnimation() = 0;
  virtual nsresult StopAnimation() = 0;

  // Member data shared by all implementations of this abstract class
  nsRefPtr<imgStatusTracker>  mStatusTracker;
  nsCOMPtr<nsIURI>            mURI;
  uint64_t                    mInnerWindowId;
  uint32_t                    mAnimationConsumers;
  uint16_t                    mAnimationMode;   // Enum values in imgIContainer
  bool                        mInitialized:1;   // Have we been initalized?
  bool                        mAnimating:1;     // Are we currently animating?
  bool                        mError:1;         // Error handling
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_IMAGE_H_

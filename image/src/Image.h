/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_src_Image_h
#define mozilla_image_src_Image_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "gfx2DGlue.h"                // for gfxMemoryLocation
#include "imgIContainer.h"
#include "ProgressTracker.h"
#include "ImageURL.h"
#include "nsStringFwd.h"

class nsIRequest;
class nsIInputStream;

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
  static eDecoderType GetDecoderType(const char* aMimeType);

  /**
   * Flags for Image initialization.
   *
   * Meanings:
   *
   * INIT_FLAG_NONE: Lack of flags
   *
   * INIT_FLAG_DISCARDABLE: The container should be discardable
   *
   * INIT_FLAG_DECODE_ONLY_ON_DRAW: The container should decode on draw rather
   * than possibly being speculatively decoded earlier.
   *
   * INIT_FLAG_TRANSIENT: The container is likely to exist for only a short time
   * before being destroyed. (For example, containers for
   * multipart/x-mixed-replace image parts fall into this category.) If this
   * flag is set, INIT_FLAG_DISCARDABLE and INIT_FLAG_DECODE_ONLY_ON_DRAW must
   * not be set.
   *
   * INIT_FLAG_DOWNSCALE_DURING_DECODE: The container should attempt to
   * downscale images during decoding instead of decoding them to their
   * intrinsic size.
   */
  static const uint32_t INIT_FLAG_NONE                     = 0x0;
  static const uint32_t INIT_FLAG_DISCARDABLE              = 0x1;
  static const uint32_t INIT_FLAG_DECODE_ONLY_ON_DRAW      = 0x2;
  static const uint32_t INIT_FLAG_TRANSIENT                = 0x4;
  static const uint32_t INIT_FLAG_DOWNSCALE_DURING_DECODE  = 0x8;

  /**
   * Creates a new image container.
   *
   * @param aMimeType The mimetype of the image.
   * @param aFlags Initialization flags of the INIT_FLAG_* variety.
   */
  virtual nsresult Init(const char* aMimeType,
                        uint32_t aFlags) = 0;

  virtual already_AddRefed<ProgressTracker> GetProgressTracker() = 0;
  virtual void SetProgressTracker(ProgressTracker* aProgressTracker) {}

  /**
   * The size, in bytes, occupied by the compressed source data of the image.
   * If MallocSizeOf does not work on this platform, uses a fallback approach to
   * ensure that something reasonable is always returned.
   */
  virtual size_t SizeOfSourceWithComputedFallback(
    MallocSizeOf aMallocSizeOf) const = 0;

  /**
   * The size, in bytes, occupied by the image's decoded data.
   */
  virtual size_t SizeOfDecoded(gfxMemoryLocation aLocation,
                               MallocSizeOf aMallocSizeOf) const = 0;

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
   * Called when the SurfaceCache discards a persistent surface belonging to
   * this image.
   */
  virtual void OnSurfaceDiscarded() = 0;

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) = 0;
  virtual uint64_t InnerWindowID() const = 0;

  virtual bool HasError() = 0;
  virtual void SetHasError() = 0;

  virtual ImageURL* GetURI() = 0;
};

class ImageResource : public Image
{
public:
  already_AddRefed<ProgressTracker> GetProgressTracker() override
  {
    nsRefPtr<ProgressTracker> progressTracker = mProgressTracker;
    MOZ_ASSERT(progressTracker);
    return progressTracker.forget();
  }

  void SetProgressTracker(
                       ProgressTracker* aProgressTracker) override final
  {
    MOZ_ASSERT(aProgressTracker);
    MOZ_ASSERT(!mProgressTracker);
    mProgressTracker = aProgressTracker;
  }

  virtual void IncrementAnimationConsumers() override;
  virtual void DecrementAnimationConsumers() override;
#ifdef DEBUG
  virtual uint32_t GetAnimationConsumers() override
  {
    return mAnimationConsumers;
  }
#endif

  virtual void OnSurfaceDiscarded() override { }

  virtual void SetInnerWindowID(uint64_t aInnerWindowId) override
  {
    mInnerWindowId = aInnerWindowId;
  }
  virtual uint64_t InnerWindowID() const override { return mInnerWindowId; }

  virtual bool HasError() override    { return mError; }
  virtual void SetHasError() override { mError = true; }

  /*
   * Returns a non-AddRefed pointer to the URI associated with this image.
   * Illegal to use off-main-thread.
   */
  virtual ImageURL* GetURI() override { return mURI.get(); }

protected:
  explicit ImageResource(ImageURL* aURI);

  // Shared functionality for implementors of imgIContainer. Every
  // implementation of attribute animationMode should forward here.
  nsresult GetAnimationModeInternal(uint16_t* aAnimationMode);
  nsresult SetAnimationModeInternal(uint16_t aAnimationMode);

  /**
   * Helper for RequestRefresh.
   *
   * If we've had a "recent" refresh (i.e. if this image is being used in
   * multiple documents & some other document *just* called RequestRefresh() on
   * this image with a timestamp close to aTime), this method returns true.
   *
   * Otherwise, this method updates mLastRefreshTime to aTime & returns false.
   */
  bool HadRecentRefresh(const TimeStamp& aTime);

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
  nsRefPtr<ProgressTracker>     mProgressTracker;
  nsRefPtr<ImageURL>            mURI;
  TimeStamp                     mLastRefreshTime;
  uint64_t                      mInnerWindowId;
  uint32_t                      mAnimationConsumers;
  uint16_t                      mAnimationMode; // Enum values in imgIContainer
  bool                          mInitialized:1; // Have we been initalized?
  bool                          mAnimating:1;   // Are we currently animating?
  bool                          mError:1;       // Error handling
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_Image_h

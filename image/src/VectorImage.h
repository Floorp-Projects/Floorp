/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_src_VectorImage_h
#define mozilla_image_src_VectorImage_h

#include "Image.h"
#include "nsIStreamListener.h"
#include "mozilla/MemoryReporting.h"

class nsIRequest;
class gfxDrawable;

namespace mozilla {
namespace layers {
class LayerManager;
class ImageContainer;
}
namespace image {

struct SVGDrawingParameters;
class  SVGDocumentWrapper;
class  SVGRootRenderingObserver;
class  SVGLoadEventListener;
class  SVGParseCompleteListener;

class VectorImage final : public ImageResource,
                          public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_IMGICONTAINER

  // (no public constructor - use ImageFactory)

  // Methods inherited from Image
  nsresult Init(const char* aMimeType,
                uint32_t aFlags) override;

  virtual size_t SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf)
    const override;
  virtual size_t SizeOfDecoded(gfxMemoryLocation aLocation,
                               MallocSizeOf aMallocSizeOf) const override;

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) override;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aResult,
                                       bool aLastPart) override;

  void OnSurfaceDiscarded() override;

  /**
   * Callback for SVGRootRenderingObserver.
   *
   * This just sets a dirty flag that we check in VectorImage::RequestRefresh,
   * which is called under the ticks of the refresh driver of any observing
   * documents that we may have. Only then (after all animations in this image
   * have been updated) do we send out "frame changed" notifications,
   */
  void InvalidateObserversOnNextRefreshDriverTick();

  // Callback for SVGParseCompleteListener.
  void OnSVGDocumentParsed();

  // Callbacks for SVGLoadEventListener.
  void OnSVGDocumentLoaded();
  void OnSVGDocumentError();

protected:
  explicit VectorImage(ProgressTracker* aProgressTracker = nullptr,
                       ImageURL* aURI = nullptr);
  virtual ~VectorImage();

  virtual nsresult StartAnimation() override;
  virtual nsresult StopAnimation() override;
  virtual bool     ShouldAnimate() override;

  void CreateSurfaceAndShow(const SVGDrawingParameters& aParams);
  void Show(gfxDrawable* aDrawable, const SVGDrawingParameters& aParams);

private:
  /**
   * In catastrophic circumstances like a GPU driver crash, we may lose our
   * surfaces even if they're locked. RecoverFromLossOfSurfaces discards all
   * existing surfaces, allowing us to recover.
   */
  void RecoverFromLossOfSurfaces();

  void CancelAllListeners();
  void SendInvalidationNotifications();

  nsRefPtr<SVGDocumentWrapper>       mSVGDocumentWrapper;
  nsRefPtr<SVGRootRenderingObserver> mRenderingObserver;
  nsRefPtr<SVGLoadEventListener>     mLoadEventListener;
  nsRefPtr<SVGParseCompleteListener> mParseCompleteListener;

  /// Count of locks on this image (roughly correlated to visible instances).
  uint32_t mLockCount;

  bool           mIsInitialized;          // Have we been initialized?
  bool           mDiscardable;            // Are we discardable?
  bool           mIsFullyLoaded;          // Has the SVG document finished
                                          // loading?
  bool           mIsDrawing;              // Are we currently drawing?
  bool           mHaveAnimations;         // Is our SVG content SMIL-animated?
                                          // (Only set after mIsFullyLoaded.)
  bool           mHasPendingInvalidation; // Invalidate observers next refresh
                                          // driver tick.

  // Initializes ProgressTracker and resets it on RasterImage destruction.
  nsAutoPtr<ProgressTrackerInit> mProgressTrackerInit;

  friend class ImageFactory;
};

inline NS_IMETHODIMP VectorImage::GetAnimationMode(uint16_t* aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

inline NS_IMETHODIMP VectorImage::SetAnimationMode(uint16_t aAnimationMode) {
  return SetAnimationModeInternal(aAnimationMode);
}

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_VectorImage_h

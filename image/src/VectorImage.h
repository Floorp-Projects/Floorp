/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imagelib_VectorImage_h_
#define mozilla_imagelib_VectorImage_h_

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

class VectorImage : public ImageResource,
                    public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_IMGICONTAINER

  // (no public constructor - use ImageFactory)
  virtual ~VectorImage();

  // Methods inherited from Image
  nsresult Init(const char* aMimeType,
                uint32_t aFlags);
  virtual nsIntRect FrameRect(uint32_t aWhichFrame) MOZ_OVERRIDE;

  virtual size_t HeapSizeOfSourceWithComputedFallback(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t NonHeapSizeOfDecoded() const;
  virtual size_t OutOfProcessSizeOfDecoded() const;

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) MOZ_OVERRIDE;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext,
                                       nsresult aResult,
                                       bool aLastPart) MOZ_OVERRIDE;
  virtual nsresult OnNewSourceData() MOZ_OVERRIDE;

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
  VectorImage(imgStatusTracker* aStatusTracker = nullptr,
              ImageURL* aURI = nullptr);

  virtual nsresult StartAnimation();
  virtual nsresult StopAnimation();
  virtual bool     ShouldAnimate();

  void CreateDrawableAndShow(const SVGDrawingParameters& aParams);
  void Show(gfxDrawable* aDrawable, const SVGDrawingParameters& aParams);

private:
  void CancelAllListeners();

  nsRefPtr<SVGDocumentWrapper>       mSVGDocumentWrapper;
  nsRefPtr<SVGRootRenderingObserver> mRenderingObserver;
  nsRefPtr<SVGLoadEventListener>     mLoadEventListener;
  nsRefPtr<SVGParseCompleteListener> mParseCompleteListener;

  bool           mIsInitialized;          // Have we been initalized?
  bool           mIsFullyLoaded;          // Has the SVG document finished loading?
  bool           mIsDrawing;              // Are we currently drawing?
  bool           mHaveAnimations;         // Is our SVG content SMIL-animated?
                                          // (Only set after mIsFullyLoaded.)
  bool           mHasPendingInvalidation; // Invalidate observers next refresh
                                          // driver tick.

  // Initializes imgStatusTracker and resets it on RasterImage destruction.
  nsAutoPtr<imgStatusTrackerInit> mStatusTrackerInit;

  friend class ImageFactory;
};

inline NS_IMETHODIMP VectorImage::GetAnimationMode(uint16_t *aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

inline NS_IMETHODIMP VectorImage::SetAnimationMode(uint16_t aAnimationMode) {
  return SetAnimationModeInternal(aAnimationMode);
}

} // namespace image
} // namespace mozilla

#endif // mozilla_imagelib_VectorImage_h_

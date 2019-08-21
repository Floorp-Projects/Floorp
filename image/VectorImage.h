/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_VectorImage_h
#define mozilla_image_VectorImage_h

#include "Image.h"
#include "nsIStreamListener.h"
#include "mozilla/MemoryReporting.h"

class nsIRequest;
class gfxDrawable;

namespace mozilla {
namespace image {

struct SVGDrawingParameters;
class SVGDocumentWrapper;
class SVGRootRenderingObserver;
class SVGLoadEventListener;
class SVGParseCompleteListener;

class VectorImage final : public ImageResource, public nsIStreamListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_IMGICONTAINER

  // (no public constructor - use ImageFactory)

  // Methods inherited from Image
  nsresult GetNativeSizes(nsTArray<gfx::IntSize>& aNativeSizes) const override;
  size_t GetNativeSizesLength() const override;
  virtual size_t SizeOfSourceWithComputedFallback(
      SizeOfState& aState) const override;
  virtual void CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                     MallocSizeOf aMallocSizeOf) const override;

  virtual nsresult OnImageDataAvailable(nsIRequest* aRequest,
                                        nsISupports* aContext,
                                        nsIInputStream* aInStr,
                                        uint64_t aSourceOffset,
                                        uint32_t aCount) override;
  virtual nsresult OnImageDataComplete(nsIRequest* aRequest,
                                       nsISupports* aContext, nsresult aResult,
                                       bool aLastPart) override;

  virtual void OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) override;

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
  explicit VectorImage(nsIURI* aURI = nullptr);
  virtual ~VectorImage();

  virtual nsresult StartAnimation() override;
  virtual nsresult StopAnimation() override;
  virtual bool ShouldAnimate() override;

 private:
  Tuple<ImgDrawResult, IntSize, RefPtr<SourceSurface>> GetFrameInternal(
      const IntSize& aSize, const Maybe<SVGImageContext>& aSVGContext,
      uint32_t aWhichFrame, uint32_t aFlags) override;

  Tuple<ImgDrawResult, IntSize> GetImageContainerSize(
      layers::LayerManager* aManager, const IntSize& aSize,
      uint32_t aFlags) override;

  /**
   * Attempt to find a matching cached surface in the SurfaceCache. Returns the
   * cached surface, if found, and the size to rasterize at, if applicable.
   * If we cannot rasterize, it will be the requested size to draw at (aSize).
   */
  Tuple<RefPtr<SourceSurface>, IntSize> LookupCachedSurface(
      const IntSize& aSize, const Maybe<SVGImageContext>& aSVGContext,
      uint32_t aFlags);

  bool MaybeRestrictSVGContext(Maybe<SVGImageContext>& aNewSVGContext,
                               const Maybe<SVGImageContext>& aSVGContext,
                               uint32_t aFlags);

  /// Create a gfxDrawable which callbacks into the SVG document.
  already_AddRefed<gfxDrawable> CreateSVGDrawable(
      const SVGDrawingParameters& aParams);

  /// Rasterize the SVG into a surface. aWillCache will be set to whether or
  /// not the new surface was put into the cache.
  already_AddRefed<SourceSurface> CreateSurface(
      const SVGDrawingParameters& aParams, gfxDrawable* aSVGDrawable,
      bool& aWillCache);

  /// Send a frame complete notification if appropriate. Must be called only
  /// after all drawing has been completed.
  void SendFrameComplete(bool aDidCache, uint32_t aFlags);

  void Show(gfxDrawable* aDrawable, const SVGDrawingParameters& aParams);

  nsresult Init(const char* aMimeType, uint32_t aFlags);

  /**
   * In catastrophic circumstances like a GPU driver crash, we may lose our
   * surfaces even if they're locked. RecoverFromLossOfSurfaces discards all
   * existing surfaces, allowing us to recover.
   */
  void RecoverFromLossOfSurfaces();

  void CancelAllListeners();
  void SendInvalidationNotifications();

  RefPtr<SVGDocumentWrapper> mSVGDocumentWrapper;
  RefPtr<SVGRootRenderingObserver> mRenderingObserver;
  RefPtr<SVGLoadEventListener> mLoadEventListener;
  RefPtr<SVGParseCompleteListener> mParseCompleteListener;

  /// Count of locks on this image (roughly correlated to visible instances).
  uint32_t mLockCount;

  // Stored result from the Necko load of the image, which we save in
  // OnImageDataComplete if the underlying SVG document isn't loaded. If we save
  // this, we actually notify this progress (and clear this value) in
  // OnSVGDocumentLoaded or OnSVGDocumentError.
  Maybe<Progress> mLoadProgress;

  bool mIsInitialized;           // Have we been initialized?
  bool mDiscardable;             // Are we discardable?
  bool mIsFullyLoaded;           // Has the SVG document finished
                                 // loading?
  bool mIsDrawing;               // Are we currently drawing?
  bool mHaveAnimations;          // Is our SVG content SMIL-animated?
                                 // (Only set after mIsFullyLoaded.)
  bool mHasPendingInvalidation;  // Invalidate observers next refresh
                                 // driver tick.

  friend class ImageFactory;
};

inline NS_IMETHODIMP VectorImage::GetAnimationMode(uint16_t* aAnimationMode) {
  return GetAnimationModeInternal(aAnimationMode);
}

inline NS_IMETHODIMP VectorImage::SetAnimationMode(uint16_t aAnimationMode) {
  return SetAnimationModeInternal(aAnimationMode);
}

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_VectorImage_h

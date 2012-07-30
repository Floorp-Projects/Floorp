/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VectorImage.h"

#include "imgIDecoderObserver.h"
#include "SVGDocumentWrapper.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIStreamListener.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsSVGUtils.h"  // for nsSVGUtils::ConvertToSurfaceSize
#include "nsSVGEffects.h" // for nsSVGRenderingObserver
#include "gfxDrawable.h"
#include "gfxUtils.h"
#include "nsSVGSVGElement.h"

namespace mozilla {

using namespace dom;

namespace image {

// Helper-class: SVGRootRenderingObserver
class SVGRootRenderingObserver : public nsSVGRenderingObserver {
public:
  SVGRootRenderingObserver(SVGDocumentWrapper* aDocWrapper,
                           VectorImage*        aVectorImage)
    : nsSVGRenderingObserver(),
      mDocWrapper(aDocWrapper),
      mVectorImage(aVectorImage)
  {
    StartListening();
    Element* elem = GetTarget();
    if (elem) {
      nsSVGEffects::AddRenderingObserver(elem, this);
      mInObserverList = true;
    }
#ifdef DEBUG
    else {
      NS_ABORT_IF_FALSE(!mInObserverList,
                        "Have no target, so we can't be in "
                        "target's observer list...");
    }
#endif
  }

  virtual ~SVGRootRenderingObserver()
  {
    StopListening();
  }

protected:
  virtual Element* GetTarget()
  {
    return mDocWrapper->GetRootSVGElem();
  }

  virtual void DoUpdate()
  {
    Element* elem = GetTarget();
    if (!elem)
      return;

    if (!mDocWrapper->ShouldIgnoreInvalidation()) {
      nsIFrame* frame = elem->GetPrimaryFrame();
      if (!frame || frame->PresContext()->PresShell()->IsDestroying()) {
        // We're being destroyed. Bail out.
        return;
      }

      mVectorImage->InvalidateObserver();
    }

    // Our caller might've removed us from rendering-observer list.
    // Add ourselves back!
    if (!mInObserverList) {
      nsSVGEffects::AddRenderingObserver(elem, this);
      mInObserverList = true;
    }
  }

  // Private data
  nsRefPtr<SVGDocumentWrapper> mDocWrapper;
  VectorImage* mVectorImage;   // Raw pointer because it owns me.
};

// Helper-class: SVGDrawingCallback
class SVGDrawingCallback : public gfxDrawingCallback {
public:
  SVGDrawingCallback(SVGDocumentWrapper* aSVGDocumentWrapper,
                     const nsIntRect& aViewport,
                     PRUint32 aImageFlags) :
    mSVGDocumentWrapper(aSVGDocumentWrapper),
    mViewport(aViewport),
    mImageFlags(aImageFlags)
  {}
  virtual bool operator()(gfxContext* aContext,
                            const gfxRect& aFillRect,
                            const gfxPattern::GraphicsFilter& aFilter,
                            const gfxMatrix& aTransform);
private:
  nsRefPtr<SVGDocumentWrapper> mSVGDocumentWrapper;
  const nsIntRect mViewport;
  PRUint32        mImageFlags;
};

// Based loosely on nsSVGIntegrationUtils' PaintFrameCallback::operator()
bool
SVGDrawingCallback::operator()(gfxContext* aContext,
                               const gfxRect& aFillRect,
                               const gfxPattern::GraphicsFilter& aFilter,
                               const gfxMatrix& aTransform)
{
  NS_ABORT_IF_FALSE(mSVGDocumentWrapper, "need an SVGDocumentWrapper");

  // Get (& sanity-check) the helper-doc's presShell
  nsCOMPtr<nsIPresShell> presShell;
  if (NS_FAILED(mSVGDocumentWrapper->GetPresShell(getter_AddRefs(presShell)))) {
    NS_WARNING("Unable to draw -- presShell lookup failed");
    return false;
  }
  NS_ABORT_IF_FALSE(presShell, "GetPresShell succeeded but returned null");

  gfxContextAutoSaveRestore contextRestorer(aContext);

  // Clip to aFillRect so that we don't paint outside.
  aContext->NewPath();
  aContext->Rectangle(aFillRect);
  aContext->Clip();

  gfxContextMatrixAutoSaveRestore contextMatrixRestorer(aContext);
  aContext->Multiply(gfxMatrix(aTransform).Invert());

  nsPresContext* presContext = presShell->GetPresContext();
  NS_ABORT_IF_FALSE(presContext, "pres shell w/out pres context");

  nsRect svgRect(presContext->DevPixelsToAppUnits(mViewport.x),
                 presContext->DevPixelsToAppUnits(mViewport.y),
                 presContext->DevPixelsToAppUnits(mViewport.width),
                 presContext->DevPixelsToAppUnits(mViewport.height));

  PRUint32 renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  if (!(mImageFlags & imgIContainer::FLAG_SYNC_DECODE)) {
    renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
  }

  presShell->RenderDocument(svgRect, renderDocFlags,
                            NS_RGBA(0, 0, 0, 0), // transparent
                            aContext);

  return true;
}

// Implement VectorImage's nsISupports-inherited methods
NS_IMPL_ISUPPORTS3(VectorImage,
                   imgIContainer,
                   nsIStreamListener,
                   nsIRequestObserver)

//------------------------------------------------------------------------------
// Constructor / Destructor

VectorImage::VectorImage(imgStatusTracker* aStatusTracker) :
  Image(aStatusTracker), // invoke superclass's constructor
  mRestrictedRegion(0, 0, 0, 0),
  mLastRenderedSize(0, 0),
  mIsInitialized(false),
  mIsFullyLoaded(false),
  mIsDrawing(false),
  mHaveAnimations(false),
  mHaveRestrictedRegion(false)
{
}

VectorImage::~VectorImage()
{
}

//------------------------------------------------------------------------------
// Methods inherited from Image.h

nsresult
VectorImage::Init(imgIDecoderObserver* aObserver,
                  const char* aMimeType,
                  const char* aURIString,
                  PRUint32 aFlags)
{
  // We don't support re-initialization
  if (mIsInitialized)
    return NS_ERROR_ILLEGAL_VALUE;

  NS_ABORT_IF_FALSE(!mIsFullyLoaded && !mHaveAnimations &&
                    !mHaveRestrictedRegion && !mError,
                    "Flags unexpectedly set before initialization");

  mObserver = do_GetWeakReference(aObserver);
  NS_ABORT_IF_FALSE(!strcmp(aMimeType, SVG_MIMETYPE), "Unexpected mimetype");

  mIsInitialized = true;

  return NS_OK;
}

void
VectorImage::GetCurrentFrameRect(nsIntRect& aRect)
{
  aRect = nsIntRect::GetMaxSizedIntRect();
}

size_t
VectorImage::HeapSizeOfSourceWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const
{
  // We're not storing the source data -- we just feed that directly to
  // our helper SVG document as we receive it, for it to parse.
  // So 0 is an appropriate return value here.
  return 0;
}

size_t
VectorImage::HeapSizeOfDecodedWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const
{
  // XXXdholbert TODO: return num bytes used by helper SVG doc. (bug 590790)
  return 0;
}

size_t
VectorImage::NonHeapSizeOfDecoded() const
{
  return 0;
}

size_t
VectorImage::OutOfProcessSizeOfDecoded() const
{
  return 0;
}

nsresult
VectorImage::StartAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(ShouldAnimate(), "Should not animate!");

  mSVGDocumentWrapper->StartAnimation();
  return NS_OK;
}

nsresult
VectorImage::StopAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(mIsFullyLoaded && mHaveAnimations,
                    "Should not have been animating!");

  mSVGDocumentWrapper->StopAnimation();
  return NS_OK;
}

bool
VectorImage::ShouldAnimate()
{
  return Image::ShouldAnimate() && mIsFullyLoaded && mHaveAnimations;
}

//------------------------------------------------------------------------------
// imgIContainer methods

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP
VectorImage::GetWidth(PRInt32* aWidth)
{
  if (mError || !mIsFullyLoaded) {
    *aWidth = 0;
    return NS_ERROR_FAILURE;
  }

  if (!mSVGDocumentWrapper->GetWidthOrHeight(SVGDocumentWrapper::eWidth,
                                             *aWidth)) {
    *aWidth = 0;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

//******************************************************************************
/* [notxpcom] void requestRefresh ([const] in TimeStamp aTime); */
NS_IMETHODIMP_(void)
VectorImage::RequestRefresh(const mozilla::TimeStamp& aTime)
{
  // TODO: Implement for b666446.
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP
VectorImage::GetHeight(PRInt32* aHeight)
{
  if (mError || !mIsFullyLoaded) {
    *aHeight = 0;
    return NS_ERROR_FAILURE;
  }

  if (!mSVGDocumentWrapper->GetWidthOrHeight(SVGDocumentWrapper::eHeight,
                                             *aHeight)) {
    *aHeight = 0;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

//******************************************************************************
/* readonly attribute unsigned short type; */
NS_IMETHODIMP
VectorImage::GetType(PRUint16* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = GetType();
  return NS_OK;
}

//******************************************************************************
/* [noscript, notxpcom] PRUint16 GetType(); */
NS_IMETHODIMP_(PRUint16)
VectorImage::GetType()
{
  return imgIContainer::TYPE_VECTOR;
}

//******************************************************************************
/* readonly attribute boolean animated; */
NS_IMETHODIMP
VectorImage::GetAnimated(bool* aAnimated)
{
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  *aAnimated = mSVGDocumentWrapper->IsAnimated();
  return NS_OK;
}

//******************************************************************************
/* readonly attribute boolean currentFrameIsOpaque; */
NS_IMETHODIMP
VectorImage::GetCurrentFrameIsOpaque(bool* aIsOpaque)
{
  NS_ENSURE_ARG_POINTER(aIsOpaque);
  *aIsOpaque = false;   // In general, SVG content is not opaque.
  return NS_OK;
}

//******************************************************************************
/* [noscript] gfxASurface getFrame(in PRUint32 aWhichFrame,
 *                                 in PRUint32 aFlags; */
NS_IMETHODIMP
VectorImage::GetFrame(PRUint32 aWhichFrame,
                      PRUint32 aFlags,
                      gfxASurface** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsRefPtr<gfxImageSurface> surface;
  nsresult rv = CopyFrame(aWhichFrame, aFlags, getter_AddRefs(surface));
  if (NS_SUCCEEDED(rv)) {
    *_retval = surface.forget().get();
  }
  return rv;
}

//******************************************************************************
/* [noscript] gfxImageSurface copyFrame(in PRUint32 aWhichFrame,
 *                                      in PRUint32 aFlags); */
NS_IMETHODIMP
VectorImage::CopyFrame(PRUint32 aWhichFrame,
                       PRUint32 aFlags,
                       gfxImageSurface** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  // XXXdholbert NOTE: Currently assuming FRAME_CURRENT for simplicity.
  // Could handle FRAME_FIRST by saving helper-doc current time, seeking
  // to time 0, rendering, and then seeking to saved time.
  if (aWhichFrame > FRAME_MAX_VALUE)
    return NS_ERROR_INVALID_ARG;

  if (mError)
    return NS_ERROR_FAILURE;

  // Look up height & width
  // ----------------------
  nsIntSize imageIntSize;
  if (!mSVGDocumentWrapper->GetWidthOrHeight(SVGDocumentWrapper::eWidth,
                                             imageIntSize.width) ||
      !mSVGDocumentWrapper->GetWidthOrHeight(SVGDocumentWrapper::eHeight,
                                             imageIntSize.height)) {
    // We'll get here if our SVG doc has a percent-valued width or height.
    return NS_ERROR_FAILURE;
  }

  // Create a surface that we'll ultimately return
  // ---------------------------------------------
  // Make our surface the size of what will ultimately be drawn to it.
  // (either the full image size, or the restricted region)
  gfxIntSize surfaceSize;
  if (mHaveRestrictedRegion) {
    surfaceSize.width = mRestrictedRegion.width;
    surfaceSize.height = mRestrictedRegion.height;
  } else {
    surfaceSize.width = imageIntSize.width;
    surfaceSize.height = imageIntSize.height;
  }

  nsRefPtr<gfxImageSurface> surface =
    new gfxImageSurface(surfaceSize, gfxASurface::ImageFormatARGB32);
  nsRefPtr<gfxContext> context = new gfxContext(surface);

  // Draw to our surface!
  // --------------------
  nsresult rv = Draw(context, gfxPattern::FILTER_NEAREST, gfxMatrix(),
                     gfxRect(gfxPoint(0,0), gfxIntSize(imageIntSize.width,
                                                       imageIntSize.height)),
                     nsIntRect(nsIntPoint(0,0), imageIntSize),
                     imageIntSize, aFlags);
  if (NS_SUCCEEDED(rv)) {
    *_retval = surface.forget().get();
  }

  return rv;
}

//******************************************************************************
/* [noscript] imgIContainer extractFrame(PRUint32 aWhichFrame,
 *                                       [const] in nsIntRect aRegion,
 *                                       in PRUint32 aFlags); */
NS_IMETHODIMP
VectorImage::ExtractFrame(PRUint32 aWhichFrame,
                          const nsIntRect& aRegion,
                          PRUint32 aFlags,
                          imgIContainer** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  // XXXdholbert NOTE: This method assumes FRAME_CURRENT (not FRAME_FIRST)
  // right now, because mozilla doesn't actually contain any clients of this
  // method that use FRAME_FIRST.  If it's needed, we *could* handle
  // FRAME_FIRST by saving the helper-doc's current SMIL time, seeking it to
  // time 0, rendering to a RasterImage, and then restoring our saved time.
  if (aWhichFrame != FRAME_CURRENT) {
    NS_WARNING("VectorImage::ExtractFrame with something other than "
               "FRAME_CURRENT isn't supported yet. Assuming FRAME_CURRENT.");
  }

  // XXXdholbert This method also doesn't actually freeze animation in the
  // returned imgIContainer, because it shares our helper-document. To
  // get a true snapshot, we need to clone the document - see bug 590792.

  // Make a new container with same SVG document.
  nsRefPtr<VectorImage> extractedImg = new VectorImage();
  extractedImg->mSVGDocumentWrapper = mSVGDocumentWrapper;
  extractedImg->mAnimationMode = kDontAnimMode;

  extractedImg->mRestrictedRegion.x = aRegion.x;
  extractedImg->mRestrictedRegion.y = aRegion.y;

  // (disallow negative width/height on our restricted region)
  extractedImg->mRestrictedRegion.width  = NS_MAX(aRegion.width,  0);
  extractedImg->mRestrictedRegion.height = NS_MAX(aRegion.height, 0);

  extractedImg->mIsInitialized = true;
  extractedImg->mIsFullyLoaded = true;
  extractedImg->mHaveRestrictedRegion = true;

  *_retval = extractedImg.forget().get();
  return NS_OK;
}


//******************************************************************************
/* [noscript] void draw(in gfxContext aContext,
 *                      in gfxGraphicsFilter aFilter,
 *                      [const] in gfxMatrix aUserSpaceToImageSpace,
 *                      [const] in gfxRect aFill,
 *                      [const] in nsIntRect aSubimage,
 *                      [const] in nsIntSize aViewportSize,
 *                      in PRUint32 aFlags); */
NS_IMETHODIMP
VectorImage::Draw(gfxContext* aContext,
                  gfxPattern::GraphicsFilter aFilter,
                  const gfxMatrix& aUserSpaceToImageSpace,
                  const gfxRect& aFill,
                  const nsIntRect& aSubimage,
                  const nsIntSize& aViewportSize,
                  PRUint32 aFlags)
{
  NS_ENSURE_ARG_POINTER(aContext);
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  if (mIsDrawing) {
    NS_WARNING("Refusing to make re-entrant call to VectorImage::Draw");
    return NS_ERROR_FAILURE;
  }
  mIsDrawing = true;

  if (aViewportSize != mLastRenderedSize) {
    mSVGDocumentWrapper->UpdateViewportBounds(aViewportSize);
    mLastRenderedSize = aViewportSize;
  }
  mSVGDocumentWrapper->FlushImageTransformInvalidation();

  nsIntSize imageSize = mHaveRestrictedRegion ?
    mRestrictedRegion.Size() : aViewportSize;

  // XXXdholbert Do we need to convert image size from
  // CSS pixels to dev pixels here? (is gfxCallbackDrawable's 2nd arg in dev
  // pixels?)
  gfxIntSize imageSizeGfx(imageSize.width, imageSize.height);

  // Based on imgFrame::Draw
  gfxRect sourceRect = aUserSpaceToImageSpace.Transform(aFill);
  gfxRect imageRect(0, 0, imageSize.width, imageSize.height);
  gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);


  nsRefPtr<gfxDrawingCallback> cb =
    new SVGDrawingCallback(mSVGDocumentWrapper,
                           mHaveRestrictedRegion ?
                           mRestrictedRegion :
                           nsIntRect(nsIntPoint(0, 0), aViewportSize),
                           aFlags);

  nsRefPtr<gfxDrawable> drawable = new gfxCallbackDrawable(cb, imageSizeGfx);

  gfxUtils::DrawPixelSnapped(aContext, drawable,
                             aUserSpaceToImageSpace,
                             subimage, sourceRect, imageRect, aFill,
                             gfxASurface::ImageFormatARGB32, aFilter);

  mIsDrawing = false;
  return NS_OK;
}

//******************************************************************************
/* [notxpcom] nsIFrame GetRootLayoutFrame() */
nsIFrame*
VectorImage::GetRootLayoutFrame()
{
  if (mError)
    return nullptr;

  return mSVGDocumentWrapper->GetRootLayoutFrame();
}

//******************************************************************************
/* void requestDecode() */
NS_IMETHODIMP
VectorImage::RequestDecode()
{
  // Nothing to do for SVG images
  return NS_OK;
}

//******************************************************************************
/* void lockImage() */
NS_IMETHODIMP
VectorImage::LockImage()
{
  // This method is for image-discarding, which only applies to RasterImages.
  return NS_OK;
}

//******************************************************************************
/* void unlockImage() */
NS_IMETHODIMP
VectorImage::UnlockImage()
{
  // This method is for image-discarding, which only applies to RasterImages.
  return NS_OK;
}

//******************************************************************************
/* void requestDiscard() */
NS_IMETHODIMP
VectorImage::RequestDiscard()
{
  // This method is for image-discarding, which only applies to RasterImages.
  return NS_OK;
}

//******************************************************************************
/* void resetAnimation (); */
NS_IMETHODIMP
VectorImage::ResetAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  if (!mIsFullyLoaded || !mHaveAnimations) {
    return NS_OK; // There are no animations to be reset.
  }

  mSVGDocumentWrapper->ResetAnimation();

  return NS_OK;
}

//------------------------------------------------------------------------------
// nsIRequestObserver methods

//******************************************************************************
/* void onStartRequest(in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
VectorImage::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt)
{
  NS_ABORT_IF_FALSE(!mSVGDocumentWrapper,
                    "Repeated call to OnStartRequest -- can this happen?");

  mSVGDocumentWrapper = new SVGDocumentWrapper();
  nsresult rv = mSVGDocumentWrapper->OnStartRequest(aRequest, aCtxt);
  if (NS_FAILED(rv)) {
    mSVGDocumentWrapper = nullptr;
    mError = true;
  }

  return rv;
}

//******************************************************************************
/* void onStopRequest(in nsIRequest request, in nsISupports ctxt,
                      in nsresult status); */
NS_IMETHODIMP
VectorImage::OnStopRequest(nsIRequest* aRequest, nsISupports* aCtxt,
                           nsresult aStatus)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(!mIsFullyLoaded && !mHaveAnimations,
                    "these flags shouldn't get set until OnStopRequest. "
                    "Duplicate calls to OnStopRequest?");

  nsresult rv = mSVGDocumentWrapper->OnStopRequest(aRequest, aCtxt, aStatus);
  if (!mSVGDocumentWrapper->ParsedSuccessfully()) {
    // XXXdholbert Need to do something more here -- right now, this just
    // makes us draw the "object" icon, rather than the (jagged) "broken image"
    // icon.  See bug 594505.
    mError = true;
    return rv;
  }

  mIsFullyLoaded = true;
  mHaveAnimations = mSVGDocumentWrapper->IsAnimated();

  // Start listening to our image for rendering updates
  mRenderingObserver = new SVGRootRenderingObserver(mSVGDocumentWrapper, this);

  // Tell *our* observers that we're done loading
  nsCOMPtr<imgIDecoderObserver> observer = do_QueryReferent(mObserver);
  if (observer) {
    // NOTE: This signals that width/height are available.
    observer->OnStartContainer(nullptr, this);

    observer->FrameChanged(nullptr, this, &nsIntRect::GetMaxSizedIntRect());
    observer->OnStopFrame(nullptr, 0);
    observer->OnStopDecode(nullptr, NS_OK, nullptr);
  }
  EvaluateAnimation();

  return rv;
}

//------------------------------------------------------------------------------
// nsIStreamListener method

//******************************************************************************
/* void onDataAvailable(in nsIRequest request, in nsISupports ctxt,
                        in nsIInputStream inStr, in unsigned long sourceOffset,
                        in unsigned long count); */
NS_IMETHODIMP
VectorImage::OnDataAvailable(nsIRequest* aRequest, nsISupports* aCtxt,
                             nsIInputStream* aInStr, PRUint32 aSourceOffset,
                             PRUint32 aCount)
{
  if (mError)
    return NS_ERROR_FAILURE;

  return mSVGDocumentWrapper->OnDataAvailable(aRequest, aCtxt, aInStr,
                                              aSourceOffset, aCount);
}

// --------------------------
// Invalidation helper method

void
VectorImage::InvalidateObserver()
{
  if (!mObserver)
    return;

  nsCOMPtr<imgIContainerObserver> containerObs(do_QueryReferent(mObserver));
  if (containerObs) {
    containerObs->FrameChanged(nullptr, this, &nsIntRect::GetMaxSizedIntRect());
  }

  nsCOMPtr<imgIDecoderObserver> decoderObs(do_QueryReferent(mObserver));
  if (decoderObs) {
    decoderObs->OnStopFrame(nullptr, imgIContainer::FRAME_CURRENT);
  }
}

} // namespace image
} // namespace mozilla

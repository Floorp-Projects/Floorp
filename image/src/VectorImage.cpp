/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VectorImage.h"

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxDrawable.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "imgFrame.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsIDOMEvent.h"
#include "nsIPresShell.h"
#include "nsIStreamListener.h"
#include "nsMimeTypes.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsStubDocumentObserver.h"
#include "nsSVGEffects.h" // for nsSVGRenderingObserver
#include "nsWindowMemoryReporter.h"
#include "ImageRegion.h"
#include "Orientation.h"
#include "SVGDocumentWrapper.h"
#include "nsIDOMEventListener.h"
#include "SurfaceCache.h"

// undef the GetCurrentTime macro defined in WinBase.h from the MS Platform SDK
#undef GetCurrentTime

namespace mozilla {

using namespace dom;
using namespace gfx;
using namespace layers;

namespace image {

// Helper-class: SVGRootRenderingObserver
class SVGRootRenderingObserver MOZ_FINAL : public nsSVGRenderingObserver {
public:
  SVGRootRenderingObserver(SVGDocumentWrapper* aDocWrapper,
                           VectorImage*        aVectorImage)
    : nsSVGRenderingObserver()
    , mDocWrapper(aDocWrapper)
    , mVectorImage(aVectorImage)
    , mHonoringInvalidations(true)
  {
    MOZ_ASSERT(mDocWrapper, "Need a non-null SVG document wrapper");
    MOZ_ASSERT(mVectorImage, "Need a non-null VectorImage");

    StartListening();
    Element* elem = GetTarget();
    MOZ_ASSERT(elem, "no root SVG node for us to observe");

    nsSVGEffects::AddRenderingObserver(elem, this);
    mInObserverList = true;
  }

  virtual ~SVGRootRenderingObserver()
  {
    StopListening();
  }

  void ResumeHonoringInvalidations()
  {
    mHonoringInvalidations = true;
  }

protected:
  virtual Element* GetTarget() MOZ_OVERRIDE
  {
    return mDocWrapper->GetRootSVGElem();
  }

  virtual void DoUpdate() MOZ_OVERRIDE
  {
    Element* elem = GetTarget();
    MOZ_ASSERT(elem, "missing root SVG node");

    if (mHonoringInvalidations && !mDocWrapper->ShouldIgnoreInvalidation()) {
      nsIFrame* frame = elem->GetPrimaryFrame();
      if (!frame || frame->PresContext()->PresShell()->IsDestroying()) {
        // We're being destroyed. Bail out.
        return;
      }

      // Ignore further invalidations until we draw.
      mHonoringInvalidations = false;

      mVectorImage->InvalidateObserversOnNextRefreshDriverTick();
    }

    // Our caller might've removed us from rendering-observer list.
    // Add ourselves back!
    if (!mInObserverList) {
      nsSVGEffects::AddRenderingObserver(elem, this);
      mInObserverList = true;
    } 
  }

  // Private data
  const nsRefPtr<SVGDocumentWrapper> mDocWrapper;
  VectorImage* const mVectorImage;   // Raw pointer because it owns me.
  bool mHonoringInvalidations;
};

class SVGParseCompleteListener MOZ_FINAL : public nsStubDocumentObserver {
public:
  NS_DECL_ISUPPORTS

  SVGParseCompleteListener(nsIDocument* aDocument,
                           VectorImage* aImage)
    : mDocument(aDocument)
    , mImage(aImage)
  {
    MOZ_ASSERT(mDocument, "Need an SVG document");
    MOZ_ASSERT(mImage, "Need an image");

    mDocument->AddObserver(this);
  }

private:
  ~SVGParseCompleteListener()
  {
    if (mDocument) {
      // The document must have been destroyed before we got our event.
      // Otherwise this can't happen, since documents hold strong references to
      // their observers.
      Cancel();
    }
  }

public:
  void EndLoad(nsIDocument* aDocument) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aDocument == mDocument, "Got EndLoad for wrong document?");

    // OnSVGDocumentParsed will release our owner's reference to us, so ensure
    // we stick around long enough to complete our work.
    nsRefPtr<SVGParseCompleteListener> kungFuDeathGroup(this);

    mImage->OnSVGDocumentParsed();
  }

  void Cancel()
  {
    MOZ_ASSERT(mDocument, "Duplicate call to Cancel");
    if (mDocument) {
      mDocument->RemoveObserver(this);
      mDocument = nullptr;
    }
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
  VectorImage* const mImage; // Raw pointer to owner.
};

NS_IMPL_ISUPPORTS(SVGParseCompleteListener, nsIDocumentObserver)

class SVGLoadEventListener MOZ_FINAL : public nsIDOMEventListener {
public:
  NS_DECL_ISUPPORTS

  SVGLoadEventListener(nsIDocument* aDocument,
                       VectorImage* aImage)
    : mDocument(aDocument)
    , mImage(aImage)
  {
    MOZ_ASSERT(mDocument, "Need an SVG document");
    MOZ_ASSERT(mImage, "Need an image");

    mDocument->AddEventListener(NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"), this, true, false);
    mDocument->AddEventListener(NS_LITERAL_STRING("SVGAbort"), this, true, false);
    mDocument->AddEventListener(NS_LITERAL_STRING("SVGError"), this, true, false);
  }

private:
  ~SVGLoadEventListener()
  {
    if (mDocument) {
      // The document must have been destroyed before we got our event.
      // Otherwise this can't happen, since documents hold strong references to
      // their observers.
      Cancel();
    }
  }

public:
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mDocument, "Need an SVG document. Received multiple events?");

    // OnSVGDocumentLoaded/OnSVGDocumentError will release our owner's reference
    // to us, so ensure we stick around long enough to complete our work.
    nsRefPtr<SVGLoadEventListener> kungFuDeathGroup(this);

    nsAutoString eventType;
    aEvent->GetType(eventType);
    MOZ_ASSERT(eventType.EqualsLiteral("MozSVGAsImageDocumentLoad")  ||
               eventType.EqualsLiteral("SVGAbort")                   ||
               eventType.EqualsLiteral("SVGError"),
               "Received unexpected event");

    if (eventType.EqualsLiteral("MozSVGAsImageDocumentLoad")) {
      mImage->OnSVGDocumentLoaded();
    } else {
      mImage->OnSVGDocumentError();
    }

    return NS_OK;
  }

  void Cancel()
  {
    MOZ_ASSERT(mDocument, "Duplicate call to Cancel");
    if (mDocument) {
      mDocument->RemoveEventListener(NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"), this, true);
      mDocument->RemoveEventListener(NS_LITERAL_STRING("SVGAbort"), this, true);
      mDocument->RemoveEventListener(NS_LITERAL_STRING("SVGError"), this, true);
      mDocument = nullptr;
    }
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
  VectorImage* const mImage; // Raw pointer to owner.
};

NS_IMPL_ISUPPORTS(SVGLoadEventListener, nsIDOMEventListener)

// Helper-class: SVGDrawingCallback
class SVGDrawingCallback : public gfxDrawingCallback {
public:
  SVGDrawingCallback(SVGDocumentWrapper* aSVGDocumentWrapper,
                     const nsIntRect& aViewport,
                     const IntSize& aSize,
                     uint32_t aImageFlags)
    : mSVGDocumentWrapper(aSVGDocumentWrapper)
    , mViewport(aViewport)
    , mSize(aSize)
    , mImageFlags(aImageFlags)
  {}
  virtual bool operator()(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          const GraphicsFilter& aFilter,
                          const gfxMatrix& aTransform);
private:
  nsRefPtr<SVGDocumentWrapper> mSVGDocumentWrapper;
  const nsIntRect              mViewport;
  const IntSize                mSize;
  uint32_t                     mImageFlags;
};

// Based loosely on nsSVGIntegrationUtils' PaintFrameCallback::operator()
bool
SVGDrawingCallback::operator()(gfxContext* aContext,
                               const gfxRect& aFillRect,
                               const GraphicsFilter& aFilter,
                               const gfxMatrix& aTransform)
{
  MOZ_ASSERT(mSVGDocumentWrapper, "need an SVGDocumentWrapper");

  // Get (& sanity-check) the helper-doc's presShell
  nsCOMPtr<nsIPresShell> presShell;
  if (NS_FAILED(mSVGDocumentWrapper->GetPresShell(getter_AddRefs(presShell)))) {
    NS_WARNING("Unable to draw -- presShell lookup failed");
    return false;
  }
  MOZ_ASSERT(presShell, "GetPresShell succeeded but returned null");

  gfxContextAutoSaveRestore contextRestorer(aContext);

  // Clip to aFillRect so that we don't paint outside.
  aContext->NewPath();
  aContext->Rectangle(aFillRect);
  aContext->Clip();

  gfxMatrix matrix = aTransform;
  if (!matrix.Invert()) {
    return false;
  }
  aContext->SetMatrix(
    aContext->CurrentMatrix().PreMultiply(matrix).
                              Scale(double(mSize.width) / mViewport.width,
                                    double(mSize.height) / mViewport.height));

  nsPresContext* presContext = presShell->GetPresContext();
  MOZ_ASSERT(presContext, "pres shell w/out pres context");

  nsRect svgRect(presContext->DevPixelsToAppUnits(mViewport.x),
                 presContext->DevPixelsToAppUnits(mViewport.y),
                 presContext->DevPixelsToAppUnits(mViewport.width),
                 presContext->DevPixelsToAppUnits(mViewport.height));

  uint32_t renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  if (!(mImageFlags & imgIContainer::FLAG_SYNC_DECODE)) {
    renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
  }

  presShell->RenderDocument(svgRect, renderDocFlags,
                            NS_RGBA(0, 0, 0, 0), // transparent
                            aContext);

  return true;
}

// Implement VectorImage's nsISupports-inherited methods
NS_IMPL_ISUPPORTS(VectorImage,
                  imgIContainer,
                  nsIStreamListener,
                  nsIRequestObserver)

//------------------------------------------------------------------------------
// Constructor / Destructor

VectorImage::VectorImage(ProgressTracker* aProgressTracker,
                         ImageURL* aURI /* = nullptr */) :
  ImageResource(aURI), // invoke superclass's constructor
  mIsInitialized(false),
  mIsFullyLoaded(false),
  mIsDrawing(false),
  mHaveAnimations(false),
  mHasPendingInvalidation(false)
{
  mProgressTrackerInit = new ProgressTrackerInit(this, aProgressTracker);
}

VectorImage::~VectorImage()
{
  CancelAllListeners();
  SurfaceCache::RemoveImage(ImageKey(this));
}

//------------------------------------------------------------------------------
// Methods inherited from Image.h

nsresult
VectorImage::Init(const char* aMimeType,
                  uint32_t aFlags)
{
  // We don't support re-initialization
  if (mIsInitialized)
    return NS_ERROR_ILLEGAL_VALUE;

  MOZ_ASSERT(!mIsFullyLoaded && !mHaveAnimations && !mError,
             "Flags unexpectedly set before initialization");
  MOZ_ASSERT(!strcmp(aMimeType, IMAGE_SVG_XML), "Unexpected mimetype");

  mIsInitialized = true;
  return NS_OK;
}

size_t
VectorImage::SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const
{
  nsIDocument* doc = mSVGDocumentWrapper->GetDocument();
  if (!doc) {
    return 0; // No document, so no memory used for the document.
  }

  nsWindowSizes windowSizes(aMallocSizeOf);
  doc->DocAddSizeOfIncludingThis(&windowSizes);

  if (windowSizes.getTotalSize() == 0) {
    // MallocSizeOf fails on this platform. Because we also use this method for
    // determining the size of cache entries, we need to return something
    // reasonable here. Unfortunately, there's no way to estimate the document's
    // size accurately, so we just use a constant value of 100KB, which will
    // generally underestimate the true size.
    return 100 * 1024;
  }

  return windowSizes.getTotalSize();
}

size_t
VectorImage::SizeOfDecoded(gfxMemoryLocation aLocation,
                           MallocSizeOf aMallocSizeOf) const
{
  return SurfaceCache::SizeOfSurfaces(ImageKey(this), aLocation, aMallocSizeOf);
}

nsresult
VectorImage::OnImageDataComplete(nsIRequest* aRequest,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 bool aLastPart)
{
  // Call our internal OnStopRequest method, which only talks to our embedded
  // SVG document. This won't have any effect on our ProgressTracker.
  nsresult finalStatus = OnStopRequest(aRequest, aContext, aStatus);

  // Give precedence to Necko failure codes.
  if (NS_FAILED(aStatus))
    finalStatus = aStatus;

  // Actually fire OnStopRequest.
  if (mProgressTracker) {
    mProgressTracker->SyncNotifyProgress(LoadCompleteProgress(aLastPart,
                                                              mError,
                                                              finalStatus));
  }
  return finalStatus;
}

nsresult
VectorImage::OnImageDataAvailable(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsIInputStream* aInStr,
                                  uint64_t aSourceOffset,
                                  uint32_t aCount)
{
  return OnDataAvailable(aRequest, aContext, aInStr, aSourceOffset, aCount);
}

nsresult
VectorImage::StartAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  MOZ_ASSERT(ShouldAnimate(), "Should not animate!");

  mSVGDocumentWrapper->StartAnimation();
  return NS_OK;
}

nsresult
VectorImage::StopAnimation()
{
  nsresult rv = NS_OK;
  if (mError) {
    rv = NS_ERROR_FAILURE;
  } else {
    MOZ_ASSERT(mIsFullyLoaded && mHaveAnimations,
               "Should not have been animating!");

    mSVGDocumentWrapper->StopAnimation();
  }

  mAnimating = false;
  return rv;
}

bool
VectorImage::ShouldAnimate()
{
  return ImageResource::ShouldAnimate() && mIsFullyLoaded && mHaveAnimations;
}

NS_IMETHODIMP_(void)
VectorImage::SetAnimationStartTime(const TimeStamp& aTime)
{
  // We don't care about animation start time.
}

//------------------------------------------------------------------------------
// imgIContainer methods

//******************************************************************************
/* readonly attribute int32_t width; */
NS_IMETHODIMP
VectorImage::GetWidth(int32_t* aWidth)
{
  if (mError || !mIsFullyLoaded) {
    *aWidth = -1;
  } else {
    SVGSVGElement* rootElem = mSVGDocumentWrapper->GetRootSVGElem();
    MOZ_ASSERT(rootElem, "Should have a root SVG elem, since we finished "
                         "loading without errors");
    *aWidth = rootElem->GetIntrinsicWidth();
  }
  return *aWidth >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

//******************************************************************************
/* [notxpcom] void requestRefresh ([const] in TimeStamp aTime); */
NS_IMETHODIMP_(void)
VectorImage::RequestRefresh(const TimeStamp& aTime)
{
  if (HadRecentRefresh(aTime)) {
    return;
  }

  EvaluateAnimation();

  mSVGDocumentWrapper->TickRefreshDriver();

  if (mHasPendingInvalidation) {
    SendInvalidationNotifications();
    mHasPendingInvalidation = false;
  }
}

void
VectorImage::SendInvalidationNotifications()
{
  // Animated images don't send out invalidation notifications as soon as
  // they're generated. Instead, InvalidateObserversOnNextRefreshDriverTick
  // records that there are pending invalidations and then returns immediately.
  // The notifications are actually sent from RequestRefresh(). We send these
  // notifications there to ensure that there is actually a document observing
  // us. Otherwise, the notifications are just wasted effort.
  //
  // Non-animated images call this method directly from
  // InvalidateObserversOnNextRefreshDriverTick, because RequestRefresh is never
  // called for them. Ordinarily this isn't needed, since we send out
  // invalidation notifications in OnSVGDocumentLoaded, but in rare cases the
  // SVG document may not be 100% ready to render at that time. In those cases
  // we would miss the subsequent invalidations if we didn't send out the
  // notifications directly in |InvalidateObservers...|.

  if (mProgressTracker) {
    SurfaceCache::RemoveImage(ImageKey(this));
    mProgressTracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE,
                                         nsIntRect::GetMaxSizedIntRect());
  }
}

NS_IMETHODIMP_(nsIntRect)
VectorImage::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  return aRect;
}

//******************************************************************************
/* readonly attribute int32_t height; */
NS_IMETHODIMP
VectorImage::GetHeight(int32_t* aHeight)
{
  if (mError || !mIsFullyLoaded) {
    *aHeight = -1;
  } else {
    SVGSVGElement* rootElem = mSVGDocumentWrapper->GetRootSVGElem();
    MOZ_ASSERT(rootElem, "Should have a root SVG elem, since we finished "
                         "loading without errors");
    *aHeight = rootElem->GetIntrinsicHeight();
  }
  return *aHeight >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

//******************************************************************************
/* [noscript] readonly attribute nsSize intrinsicSize; */
NS_IMETHODIMP
VectorImage::GetIntrinsicSize(nsSize* aSize)
{
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  nsIFrame* rootFrame = mSVGDocumentWrapper->GetRootLayoutFrame();
  if (!rootFrame)
    return NS_ERROR_FAILURE;

  *aSize = nsSize(-1, -1);
  IntrinsicSize rfSize = rootFrame->GetIntrinsicSize();
  if (rfSize.width.GetUnit() == eStyleUnit_Coord)
    aSize->width = rfSize.width.GetCoordValue();
  if (rfSize.height.GetUnit() == eStyleUnit_Coord)
    aSize->height = rfSize.height.GetCoordValue();

  return NS_OK;
}

//******************************************************************************
/* [noscript] readonly attribute nsSize intrinsicRatio; */
NS_IMETHODIMP
VectorImage::GetIntrinsicRatio(nsSize* aRatio)
{
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  nsIFrame* rootFrame = mSVGDocumentWrapper->GetRootLayoutFrame();
  if (!rootFrame)
    return NS_ERROR_FAILURE;

  *aRatio = rootFrame->GetIntrinsicRatio();
  return NS_OK;
}

NS_IMETHODIMP_(Orientation)
VectorImage::GetOrientation()
{
  return Orientation();
}

//******************************************************************************
/* readonly attribute unsigned short type; */
NS_IMETHODIMP
VectorImage::GetType(uint16_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = GetType();
  return NS_OK;
}

//******************************************************************************
/* [noscript, notxpcom] uint16_t GetType(); */
NS_IMETHODIMP_(uint16_t)
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
/* [notxpcom] int32_t getFirstFrameDelay (); */
int32_t
VectorImage::GetFirstFrameDelay()
{
  if (mError)
    return -1;

  if (!mSVGDocumentWrapper->IsAnimated())
    return -1;

  // We don't really have a frame delay, so just pretend that we constantly
  // need updates.
  return 0;
}

NS_IMETHODIMP_(bool)
VectorImage::IsOpaque()
{
  return false; // In general, SVG content is not opaque.
}

//******************************************************************************
/* [noscript] SourceSurface getFrame(in uint32_t aWhichFrame,
 *                                   in uint32_t aFlags; */
NS_IMETHODIMP_(TemporaryRef<SourceSurface>)
VectorImage::GetFrame(uint32_t aWhichFrame,
                      uint32_t aFlags)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE);

  if (aWhichFrame > FRAME_MAX_VALUE)
    return nullptr;

  if (mError || !mIsFullyLoaded)
    return nullptr;

  // Look up height & width
  // ----------------------
  SVGSVGElement* svgElem = mSVGDocumentWrapper->GetRootSVGElem();
  MOZ_ASSERT(svgElem, "Should have a root SVG elem, since we finished "
                      "loading without errors");
  nsIntSize imageIntSize(svgElem->GetIntrinsicWidth(),
                         svgElem->GetIntrinsicHeight());
  
  if (imageIntSize.IsEmpty()) {
    // We'll get here if our SVG doc has a percent-valued or negative width or
    // height.
    return nullptr;
  }

  // Make our surface the size of what will ultimately be drawn to it.
  // (either the full image size, or the restricted region)
  RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->
    CreateOffscreenContentDrawTarget(IntSize(imageIntSize.width,
                                             imageIntSize.height),
                                     SurfaceFormat::B8G8R8A8);
  if (!dt) {
    NS_ERROR("Could not create a DrawTarget");
    return nullptr;
  }

  nsRefPtr<gfxContext> context = new gfxContext(dt);

  auto result = Draw(context, imageIntSize,
                     ImageRegion::Create(imageIntSize),
                     aWhichFrame, GraphicsFilter::FILTER_NEAREST,
                     Nothing(), aFlags);

  return result == DrawResult::SUCCESS ? dt->Snapshot() : nullptr;
}

//******************************************************************************
/* [noscript] ImageContainer getImageContainer(); */
NS_IMETHODIMP
VectorImage::GetImageContainer(LayerManager* aManager,
                               layers::ImageContainer** _retval)
{
  *_retval = nullptr;
  return NS_OK;
}

struct SVGDrawingParameters
{
  SVGDrawingParameters(gfxContext* aContext,
                       const nsIntSize& aSize,
                       const ImageRegion& aRegion,
                       GraphicsFilter aFilter,
                       const Maybe<SVGImageContext>& aSVGContext,
                       float aAnimationTime,
                       uint32_t aFlags)
    : context(aContext)
    , size(aSize.width, aSize.height)
    , imageRect(0, 0, aSize.width, aSize.height)
    , region(aRegion)
    , filter(aFilter)
    , svgContext(aSVGContext)
    , viewportSize(aSVGContext ? aSVGContext->GetViewportSize() : aSize)
    , animationTime(aAnimationTime)
    , flags(aFlags)
    , opacity(aSVGContext ? aSVGContext->GetGlobalOpacity() : 1.0)
  { }

  gfxContext*                   context;
  IntSize                       size;
  IntRect                       imageRect;
  ImageRegion                   region;
  GraphicsFilter                filter;
  const Maybe<SVGImageContext>& svgContext;
  nsIntSize                     viewportSize;
  float                         animationTime;
  uint32_t                      flags;
  gfxFloat                      opacity;
};

//******************************************************************************
/* [noscript] void draw(in gfxContext aContext,
 *                      [const] in nsIntSize aSize,
 *                      [const] in ImageRegion aRegion,
 *                      in uint32_t aWhichFrame,
 *                      in gfxGraphicsFilter aFilter,
 *                      [const] in MaybeSVGImageContext aSVGContext,
 *                      in uint32_t aFlags); */
NS_IMETHODIMP_(DrawResult)
VectorImage::Draw(gfxContext* aContext,
                  const nsIntSize& aSize,
                  const ImageRegion& aRegion,
                  uint32_t aWhichFrame,
                  GraphicsFilter aFilter,
                  const Maybe<SVGImageContext>& aSVGContext,
                  uint32_t aFlags)
{
  if (aWhichFrame > FRAME_MAX_VALUE) {
    return DrawResult::BAD_ARGS;
  }

  if (!aContext) {
    return DrawResult::BAD_ARGS;
  }

  if (mError) {
    return DrawResult::BAD_IMAGE;
  }

  if (!mIsFullyLoaded) {
    return DrawResult::NOT_READY;
  }

  if (mIsDrawing) {
    NS_WARNING("Refusing to make re-entrant call to VectorImage::Draw");
    return DrawResult::TEMPORARY_ERROR;
  }

  if (mAnimationConsumers == 0 && mProgressTracker) {
    mProgressTracker->OnUnlockedDraw();
  }

  AutoRestore<bool> autoRestoreIsDrawing(mIsDrawing);
  mIsDrawing = true;

  float animTime = (aWhichFrame == FRAME_FIRST) ? 0.0f
                                                : mSVGDocumentWrapper->GetCurrentTime();
  AutoSVGRenderingState autoSVGState(aSVGContext, animTime,
                                     mSVGDocumentWrapper->GetRootSVGElem());

  // Pack up the drawing parameters.
  SVGDrawingParameters params(aContext, aSize, aRegion, aFilter,
                              aSVGContext, animTime, aFlags);

  if (aFlags & FLAG_BYPASS_SURFACE_CACHE) {
    CreateSurfaceAndShow(params);
    return DrawResult::SUCCESS;
  }

  DrawableFrameRef frameRef =
    SurfaceCache::Lookup(ImageKey(this),
                         VectorSurfaceKey(params.size,
                                          params.svgContext,
                                          params.animationTime));

  // Draw.
  if (frameRef) {
    RefPtr<SourceSurface> surface = frameRef->GetSurface();
    if (surface) {
      nsRefPtr<gfxDrawable> svgDrawable =
        new gfxSurfaceDrawable(surface, ThebesIntSize(frameRef->GetSize()));
      Show(svgDrawable, params);
      return DrawResult::SUCCESS;
    }

    // We lost our surface due to some catastrophic event.
    RecoverFromLossOfSurfaces();
  }

  CreateSurfaceAndShow(params);

  return DrawResult::SUCCESS;
}

void
VectorImage::CreateSurfaceAndShow(const SVGDrawingParameters& aParams)
{
  mSVGDocumentWrapper->UpdateViewportBounds(aParams.viewportSize);
  mSVGDocumentWrapper->FlushImageTransformInvalidation();

  nsRefPtr<gfxDrawingCallback> cb =
    new SVGDrawingCallback(mSVGDocumentWrapper,
                           nsIntRect(nsIntPoint(0, 0), aParams.viewportSize),
                           aParams.size,
                           aParams.flags);

  nsRefPtr<gfxDrawable> svgDrawable =
    new gfxCallbackDrawable(cb, ThebesIntSize(aParams.size));

  bool bypassCache = bool(aParams.flags & FLAG_BYPASS_SURFACE_CACHE) ||
                     // Refuse to cache animated images:
                     // XXX(seth): We may remove this restriction in bug 922893.
                     mHaveAnimations ||
                     // The image is too big to fit in the cache:
                     !SurfaceCache::CanHold(aParams.size);
  if (bypassCache)
    return Show(svgDrawable, aParams);

  // Try to create an imgFrame, initializing the surface it contains by drawing
  // our gfxDrawable into it. (We use FILTER_NEAREST since we never scale here.)
  nsRefPtr<imgFrame> frame = new imgFrame;
  nsresult rv =
    frame->InitWithDrawable(svgDrawable, ThebesIntSize(aParams.size),
                            SurfaceFormat::B8G8R8A8,
                            GraphicsFilter::FILTER_NEAREST, aParams.flags);

  // If we couldn't create the frame, it was probably because it would end
  // up way too big. Generally it also wouldn't fit in the cache, but the prefs
  // could be set such that the cache isn't the limiting factor.
  if (NS_FAILED(rv))
    return Show(svgDrawable, aParams);

  // Take a strong reference to the frame's surface and make sure it hasn't
  // already been purged by the operating system.
  RefPtr<SourceSurface> surface = frame->GetSurface();
  if (!surface)
    return Show(svgDrawable, aParams);

  // Attempt to cache the frame.
  SurfaceCache::Insert(frame, ImageKey(this),
                       VectorSurfaceKey(aParams.size,
                                        aParams.svgContext,
                                        aParams.animationTime),
                       Lifetime::Transient);

  // Draw.
  nsRefPtr<gfxDrawable> drawable =
    new gfxSurfaceDrawable(surface, ThebesIntSize(aParams.size));
  Show(drawable, aParams);
}


void
VectorImage::Show(gfxDrawable* aDrawable, const SVGDrawingParameters& aParams)
{
  MOZ_ASSERT(aDrawable, "Should have a gfxDrawable by now");
  gfxUtils::DrawPixelSnapped(aParams.context, aDrawable,
                             ThebesIntSize(aParams.size),
                             aParams.region,
                             SurfaceFormat::B8G8R8A8,
                             aParams.filter, aParams.flags, aParams.opacity);

  MOZ_ASSERT(mRenderingObserver, "Should have a rendering observer by now");
  mRenderingObserver->ResumeHonoringInvalidations();
}

void
VectorImage::RecoverFromLossOfSurfaces()
{
  NS_WARNING("An imgFrame became invalid. Attempting to recover...");

  // Discard all existing frames, since they're probably all now invalid.
  SurfaceCache::RemoveImage(ImageKey(this));
}

//******************************************************************************
/* void requestDecode() */
NS_IMETHODIMP
VectorImage::RequestDecode()
{
  // Nothing to do for SVG images
  return NS_OK;
}

NS_IMETHODIMP
VectorImage::StartDecoding()
{
  // Nothing to do for SVG images
  return NS_OK;
}

NS_IMETHODIMP
VectorImage::RequestDecodeForSize(const nsIntSize& aSize, uint32_t aFlags)
{
  // Nothing to do for SVG images, though in theory we could rasterize to the
  // provided size ahead of time if we supported off-main-thread SVG
  // rasterization...
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
  SurfaceCache::RemoveImage(ImageKey(this));
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

NS_IMETHODIMP_(float)
VectorImage::GetFrameIndex(uint32_t aWhichFrame)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE, "Invalid argument");
  return aWhichFrame == FRAME_FIRST
         ? 0.0f
         : mSVGDocumentWrapper->GetCurrentTime();
}

//------------------------------------------------------------------------------
// nsIRequestObserver methods

//******************************************************************************
/* void onStartRequest(in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
VectorImage::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt)
{
  MOZ_ASSERT(!mSVGDocumentWrapper,
             "Repeated call to OnStartRequest -- can this happen?");

  mSVGDocumentWrapper = new SVGDocumentWrapper();
  nsresult rv = mSVGDocumentWrapper->OnStartRequest(aRequest, aCtxt);
  if (NS_FAILED(rv)) {
    mSVGDocumentWrapper = nullptr;
    mError = true;
    return rv;
  }

  // Sending StartDecode will block page load until the document's ready.  (We
  // unblock it by sending StopDecode in OnSVGDocumentLoaded or
  // OnSVGDocumentError.)
  if (mProgressTracker) {
    mProgressTracker->SyncNotifyProgress(FLAG_DECODE_STARTED |
                                         FLAG_ONLOAD_BLOCKED);
  }

  // Create a listener to wait until the SVG document is fully loaded, which
  // will signal that this image is ready to render. Certain error conditions
  // will prevent us from ever getting this notification, so we also create a
  // listener that waits for parsing to complete and cancels the
  // SVGLoadEventListener if needed. The listeners are automatically attached
  // to the document by their constructors.
  nsIDocument* document = mSVGDocumentWrapper->GetDocument();
  mLoadEventListener = new SVGLoadEventListener(document, this);
  mParseCompleteListener = new SVGParseCompleteListener(document, this);

  return NS_OK;
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

  return mSVGDocumentWrapper->OnStopRequest(aRequest, aCtxt, aStatus);
}

void
VectorImage::OnSVGDocumentParsed()
{
  MOZ_ASSERT(mParseCompleteListener, "Should have the parse complete listener");
  MOZ_ASSERT(mLoadEventListener, "Should have the load event listener");

  if (!mSVGDocumentWrapper->GetRootSVGElem()) {
    // This is an invalid SVG document. It may have failed to parse, or it may
    // be missing the <svg> root element, or the <svg> root element may not
    // declare the correct namespace. In any of these cases, we'll never be
    // notified that the SVG finished loading, so we need to treat this as an error.
    OnSVGDocumentError();
  }
}

void
VectorImage::CancelAllListeners()
{
  if (mParseCompleteListener) {
    mParseCompleteListener->Cancel();
    mParseCompleteListener = nullptr;
  }
  if (mLoadEventListener) {
    mLoadEventListener->Cancel();
    mLoadEventListener = nullptr;
  }
}

void
VectorImage::OnSVGDocumentLoaded()
{
  MOZ_ASSERT(mSVGDocumentWrapper->GetRootSVGElem(),
             "Should have parsed successfully");
  MOZ_ASSERT(!mIsFullyLoaded && !mHaveAnimations,
             "These flags shouldn't get set until OnSVGDocumentLoaded. "
             "Duplicate calls to OnSVGDocumentLoaded?");

  CancelAllListeners();

  // XXX Flushing is wasteful if embedding frame hasn't had initial reflow.
  mSVGDocumentWrapper->FlushLayout();

  mIsFullyLoaded = true;
  mHaveAnimations = mSVGDocumentWrapper->IsAnimated();

  // Start listening to our image for rendering updates.
  mRenderingObserver = new SVGRootRenderingObserver(mSVGDocumentWrapper, this);

  // Tell *our* observers that we're done loading.
  if (mProgressTracker) {
    mProgressTracker->SyncNotifyProgress(FLAG_SIZE_AVAILABLE |
                                         FLAG_HAS_TRANSPARENCY |
                                         FLAG_FRAME_COMPLETE |
                                         FLAG_DECODE_COMPLETE |
                                         FLAG_ONLOAD_UNBLOCKED,
                                         nsIntRect::GetMaxSizedIntRect());
  }

  EvaluateAnimation();
}

void
VectorImage::OnSVGDocumentError()
{
  CancelAllListeners();

  // XXXdholbert Need to do something more for the parsing failed case -- right
  // now, this just makes us draw the "object" icon, rather than the (jagged)
  // "broken image" icon.  See bug 594505.
  mError = true;

  if (mProgressTracker) {
    // Unblock page load.
    mProgressTracker->SyncNotifyProgress(FLAG_DECODE_COMPLETE |
                                         FLAG_ONLOAD_UNBLOCKED |
                                         FLAG_HAS_ERROR);
  }
}

//------------------------------------------------------------------------------
// nsIStreamListener method

//******************************************************************************
/* void onDataAvailable(in nsIRequest request, in nsISupports ctxt,
                        in nsIInputStream inStr, in unsigned long sourceOffset,
                        in unsigned long count); */
NS_IMETHODIMP
VectorImage::OnDataAvailable(nsIRequest* aRequest, nsISupports* aCtxt,
                             nsIInputStream* aInStr, uint64_t aSourceOffset,
                             uint32_t aCount)
{
  if (mError)
    return NS_ERROR_FAILURE;

  return mSVGDocumentWrapper->OnDataAvailable(aRequest, aCtxt, aInStr,
                                              aSourceOffset, aCount);
}

// --------------------------
// Invalidation helper method

void
VectorImage::InvalidateObserversOnNextRefreshDriverTick()
{
  if (mHaveAnimations) {
    mHasPendingInvalidation = true;
  } else {
    SendInvalidationNotifications();
  }
}

nsIntSize
VectorImage::OptimalImageSizeForDest(const gfxSize& aDest, uint32_t aWhichFrame, GraphicsFilter aFilter, uint32_t aFlags)
{
  MOZ_ASSERT(aDest.width >= 0 || ceil(aDest.width) <= INT32_MAX ||
             aDest.height >= 0 || ceil(aDest.height) <= INT32_MAX,
             "Unexpected destination size");

  // We can rescale SVGs freely, so just return the provided destination size.
  return nsIntSize(ceil(aDest.width), ceil(aDest.height));
}

already_AddRefed<imgIContainer>
VectorImage::Unwrap()
{
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
}

} // namespace image
} // namespace mozilla

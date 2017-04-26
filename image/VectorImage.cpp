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
#include "ISurfaceProvider.h"
#include "LookupResult.h"
#include "Orientation.h"
#include "SVGDocumentWrapper.h"
#include "nsIDOMEventListener.h"
#include "SurfaceCache.h"
#include "nsDocument.h"

// undef the GetCurrentTime macro defined in WinBase.h from the MS Platform SDK
#undef GetCurrentTime

namespace mozilla {

using namespace dom;
using namespace gfx;
using namespace layers;

namespace image {

// Helper-class: SVGRootRenderingObserver
class SVGRootRenderingObserver final : public nsSVGRenderingObserver {
public:
  NS_DECL_ISUPPORTS

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


  void ResumeHonoringInvalidations()
  {
    mHonoringInvalidations = true;
  }

protected:
  virtual ~SVGRootRenderingObserver()
  {
    StopListening();
  }

  virtual Element* GetTarget() override
  {
    return mDocWrapper->GetRootSVGElem();
  }

  virtual void DoUpdate() override
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
  const RefPtr<SVGDocumentWrapper> mDocWrapper;
  VectorImage* const mVectorImage;   // Raw pointer because it owns me.
  bool mHonoringInvalidations;
};

NS_IMPL_ISUPPORTS(SVGRootRenderingObserver, nsIMutationObserver)

class SVGParseCompleteListener final : public nsStubDocumentObserver {
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
  void EndLoad(nsIDocument* aDocument) override
  {
    MOZ_ASSERT(aDocument == mDocument, "Got EndLoad for wrong document?");

    // OnSVGDocumentParsed will release our owner's reference to us, so ensure
    // we stick around long enough to complete our work.
    RefPtr<SVGParseCompleteListener> kungFuDeathGrip(this);

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

class SVGLoadEventListener final : public nsIDOMEventListener {
public:
  NS_DECL_ISUPPORTS

  SVGLoadEventListener(nsIDocument* aDocument,
                       VectorImage* aImage)
    : mDocument(aDocument)
    , mImage(aImage)
  {
    MOZ_ASSERT(mDocument, "Need an SVG document");
    MOZ_ASSERT(mImage, "Need an image");

    mDocument->AddEventListener(NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"),
                                this, true, false);
    mDocument->AddEventListener(NS_LITERAL_STRING("SVGAbort"), this, true,
                                false);
    mDocument->AddEventListener(NS_LITERAL_STRING("SVGError"), this, true,
                                false);
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
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override
  {
    MOZ_ASSERT(mDocument, "Need an SVG document. Received multiple events?");

    // OnSVGDocumentLoaded/OnSVGDocumentError will release our owner's reference
    // to us, so ensure we stick around long enough to complete our work.
    RefPtr<SVGLoadEventListener> kungFuDeathGrip(this);

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
      mDocument
        ->RemoveEventListener(NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"),
                              this, true);
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
                     const IntSize& aViewportSize,
                     const IntSize& aSize,
                     uint32_t aImageFlags)
    : mSVGDocumentWrapper(aSVGDocumentWrapper)
    , mViewportSize(aViewportSize)
    , mSize(aSize)
    , mImageFlags(aImageFlags)
  { }
  virtual bool operator()(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          const SamplingFilter aSamplingFilter,
                          const gfxMatrix& aTransform);
private:
  RefPtr<SVGDocumentWrapper> mSVGDocumentWrapper;
  const IntSize                mViewportSize;
  const IntSize                mSize;
  uint32_t                     mImageFlags;
};

// Based loosely on nsSVGIntegrationUtils' PaintFrameCallback::operator()
bool
SVGDrawingCallback::operator()(gfxContext* aContext,
                               const gfxRect& aFillRect,
                               const SamplingFilter aSamplingFilter,
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
                              Scale(double(mSize.width) / mViewportSize.width,
                                    double(mSize.height) / mViewportSize.height));

  nsPresContext* presContext = presShell->GetPresContext();
  MOZ_ASSERT(presContext, "pres shell w/out pres context");

  nsRect svgRect(0, 0,
                 presContext->DevPixelsToAppUnits(mViewportSize.width),
                 presContext->DevPixelsToAppUnits(mViewportSize.height));

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

VectorImage::VectorImage(ImageURL* aURI /* = nullptr */) :
  ImageResource(aURI), // invoke superclass's constructor
  mLockCount(0),
  mIsInitialized(false),
  mIsFullyLoaded(false),
  mIsDrawing(false),
  mHaveAnimations(false),
  mHasPendingInvalidation(false)
{ }

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
  if (mIsInitialized) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  MOZ_ASSERT(!mIsFullyLoaded && !mHaveAnimations && !mError,
             "Flags unexpectedly set before initialization");
  MOZ_ASSERT(!strcmp(aMimeType, IMAGE_SVG_XML), "Unexpected mimetype");

  mDiscardable = !!(aFlags & INIT_FLAG_DISCARDABLE);

  // Lock this image's surfaces in the SurfaceCache if we're not discardable.
  if (!mDiscardable) {
    mLockCount++;
    SurfaceCache::LockImage(ImageKey(this));
  }

  mIsInitialized = true;
  return NS_OK;
}

size_t
VectorImage::SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const
{
  if (!mSVGDocumentWrapper) {
    return 0; // No document, so no memory used for the document.
  }

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

void
VectorImage::CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                   MallocSizeOf aMallocSizeOf) const
{
  SurfaceCache::CollectSizeOfSurfaces(ImageKey(this), aCounters, aMallocSizeOf);
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
  if (NS_FAILED(aStatus)) {
    finalStatus = aStatus;
  }
  
  Progress loadProgress = LoadCompleteProgress(aLastPart, mError, finalStatus);

  if (mIsFullyLoaded || mError) {
    // Our document is loaded, so we're ready to notify now.
    mProgressTracker->SyncNotifyProgress(loadProgress);
  } else {
    // Record our progress so far; we'll actually send the notifications in
    // OnSVGDocumentLoaded or OnSVGDocumentError.
    mLoadProgress = Some(loadProgress);
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
  if (mError) {
    return NS_ERROR_FAILURE;
  }

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
NS_IMETHODIMP
VectorImage::GetWidth(int32_t* aWidth)
{
  if (mError || !mIsFullyLoaded) {
    // XXXdholbert Technically we should leave outparam untouched when we
    // fail. But since many callers don't check for failure, we set it to 0 on
    // failure, for sane/predictable results.
    *aWidth = 0;
    return NS_ERROR_FAILURE;
  }

  SVGSVGElement* rootElem = mSVGDocumentWrapper->GetRootSVGElem();
  MOZ_ASSERT(rootElem, "Should have a root SVG elem, since we finished "
             "loading without errors");
  int32_t rootElemWidth = rootElem->GetIntrinsicWidth();
  if (rootElemWidth < 0) {
    *aWidth = 0;
    return NS_ERROR_FAILURE;
  }
  *aWidth = rootElemWidth;
  return NS_OK;
}

//******************************************************************************
nsresult
VectorImage::GetNativeSizes(nsTArray<IntSize>& aNativeSizes) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
NS_IMETHODIMP_(void)
VectorImage::RequestRefresh(const TimeStamp& aTime)
{
  if (HadRecentRefresh(aTime)) {
    return;
  }

  PendingAnimationTracker* tracker =
    mSVGDocumentWrapper->GetDocument()->GetPendingAnimationTracker();
  if (tracker && ShouldAnimate()) {
    tracker->TriggerPendingAnimationsOnNextTick(aTime);
  }

  EvaluateAnimation();

  mSVGDocumentWrapper->TickRefreshDriver();

  if (mHasPendingInvalidation) {
    mHasPendingInvalidation = false;
    SendInvalidationNotifications();
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
                                         GetMaxSizedIntRect());
  }
}

NS_IMETHODIMP_(IntRect)
VectorImage::GetImageSpaceInvalidationRect(const IntRect& aRect)
{
  return aRect;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetHeight(int32_t* aHeight)
{
  if (mError || !mIsFullyLoaded) {
    // XXXdholbert Technically we should leave outparam untouched when we
    // fail. But since many callers don't check for failure, we set it to 0 on
    // failure, for sane/predictable results.
    *aHeight = 0;
    return NS_ERROR_FAILURE;
  }

  SVGSVGElement* rootElem = mSVGDocumentWrapper->GetRootSVGElem();
  MOZ_ASSERT(rootElem, "Should have a root SVG elem, since we finished "
             "loading without errors");
  int32_t rootElemHeight = rootElem->GetIntrinsicHeight();
  if (rootElemHeight < 0) {
    *aHeight = 0;
    return NS_ERROR_FAILURE;
  }
  *aHeight = rootElemHeight;
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetIntrinsicSize(nsSize* aSize)
{
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* rootFrame = mSVGDocumentWrapper->GetRootLayoutFrame();
  if (!rootFrame) {
    return NS_ERROR_FAILURE;
  }

  *aSize = nsSize(-1, -1);
  IntrinsicSize rfSize = rootFrame->GetIntrinsicSize();
  if (rfSize.width.GetUnit() == eStyleUnit_Coord) {
    aSize->width = rfSize.width.GetCoordValue();
  }
  if (rfSize.height.GetUnit() == eStyleUnit_Coord) {
    aSize->height = rfSize.height.GetCoordValue();
  }

  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetIntrinsicRatio(nsSize* aRatio)
{
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* rootFrame = mSVGDocumentWrapper->GetRootLayoutFrame();
  if (!rootFrame) {
    return NS_ERROR_FAILURE;
  }

  *aRatio = rootFrame->GetIntrinsicRatio();
  return NS_OK;
}

NS_IMETHODIMP_(Orientation)
VectorImage::GetOrientation()
{
  return Orientation();
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetType(uint16_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  *aType = imgIContainer::TYPE_VECTOR;
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetAnimated(bool* aAnimated)
{
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  *aAnimated = mSVGDocumentWrapper->IsAnimated();
  return NS_OK;
}

//******************************************************************************
int32_t
VectorImage::GetFirstFrameDelay()
{
  if (mError) {
    return -1;
  }

  if (!mSVGDocumentWrapper->IsAnimated()) {
    return -1;
  }

  // We don't really have a frame delay, so just pretend that we constantly
  // need updates.
  return 0;
}

NS_IMETHODIMP_(bool)
VectorImage::WillDrawOpaqueNow()
{
  return false; // In general, SVG content is not opaque.
}

//******************************************************************************
NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
VectorImage::GetFrame(uint32_t aWhichFrame, uint32_t aFlags)
{
  if (mError) {
    return nullptr;
  }

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

  return GetFrameAtSize(imageIntSize, aWhichFrame, aFlags);
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
VectorImage::GetFrameAtSize(const IntSize& aSize,
                            uint32_t aWhichFrame,
                            uint32_t aFlags)
{
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE);

  if (aSize.IsEmpty()) {
    return nullptr;
  }

  if (aWhichFrame > FRAME_MAX_VALUE) {
    return nullptr;
  }

  if (mError || !mIsFullyLoaded) {
    return nullptr;
  }

  // Make our surface the size of what will ultimately be drawn to it.
  // (either the full image size, or the restricted region)
  RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->
    CreateOffscreenContentDrawTarget(aSize, SurfaceFormat::B8G8R8A8);
  if (!dt || !dt->IsValid()) {
    NS_ERROR("Could not create a DrawTarget");
    return nullptr;
  }

  RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
  MOZ_ASSERT(context); // already checked the draw target above

  auto result = Draw(context, aSize, ImageRegion::Create(aSize),
                     aWhichFrame, SamplingFilter::POINT, Nothing(), aFlags,
                     1.0);

  return result == DrawResult::SUCCESS ? dt->Snapshot() : nullptr;
}

NS_IMETHODIMP_(bool)
VectorImage::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  return false;
}

//******************************************************************************
NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
VectorImage::GetImageContainer(LayerManager* aManager, uint32_t aFlags)
{
  return nullptr;
}

struct SVGDrawingParameters
{
  SVGDrawingParameters(gfxContext* aContext,
                       const nsIntSize& aSize,
                       const ImageRegion& aRegion,
                       SamplingFilter aSamplingFilter,
                       const Maybe<SVGImageContext>& aSVGContext,
                       float aAnimationTime,
                       uint32_t aFlags,
                       float aOpacity)
    : context(aContext)
    , size(aSize.width, aSize.height)
    , region(aRegion)
    , samplingFilter(aSamplingFilter)
    , svgContext(aSVGContext)
    , viewportSize(aSize)
    , animationTime(aAnimationTime)
    , flags(aFlags)
    , opacity(aOpacity)
  {
    if (aSVGContext) {
      auto sz = aSVGContext->GetViewportSize();
      if (sz) {
        viewportSize = nsIntSize(sz->width, sz->height); // XXX losing unit
      }
    }
  }

  gfxContext*                   context;
  IntSize                       size;
  ImageRegion                   region;
  SamplingFilter                samplingFilter;
  const Maybe<SVGImageContext>& svgContext;
  nsIntSize                     viewportSize;
  float                         animationTime;
  uint32_t                      flags;
  gfxFloat                      opacity;
};

//******************************************************************************
NS_IMETHODIMP_(DrawResult)
VectorImage::Draw(gfxContext* aContext,
                  const nsIntSize& aSize,
                  const ImageRegion& aRegion,
                  uint32_t aWhichFrame,
                  SamplingFilter aSamplingFilter,
                  const Maybe<SVGImageContext>& aSVGContext,
                  uint32_t aFlags,
                  float aOpacity)
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

  if (mAnimationConsumers == 0) {
    SendOnUnlockedDraw(aFlags);
  }

  MOZ_ASSERT(!(aFlags & FLAG_FORCE_PRESERVEASPECTRATIO_NONE) ||
             (aSVGContext && aSVGContext->GetViewportSize()),
             "Viewport size is required when using "
             "FLAG_FORCE_PRESERVEASPECTRATIO_NONE");

  Maybe<SVGImageContext> newSVGContext;
  if ((aFlags & FLAG_FORCE_PRESERVEASPECTRATIO_NONE) && aSVGContext) {
    // Create an SVGImageContext with the appropriate 'preserveAspectRatio'
    // value so that LookupCachedSurface() below uses the appropriate key:
    MOZ_ASSERT(!aSVGContext->GetPreserveAspectRatio(),
               "FLAG_FORCE_PRESERVEASPECTRATIO_NONE is not expected if a "
               "preserveAspectRatio override is supplied");
    Maybe<SVGPreserveAspectRatio> aspectRatio =
      Some(SVGPreserveAspectRatio(SVG_PRESERVEASPECTRATIO_NONE,
                                  SVG_MEETORSLICE_UNKNOWN));
    newSVGContext = aSVGContext; // copy
    newSVGContext->SetPreserveAspectRatio(aspectRatio);
  }

  float animTime = (aWhichFrame == FRAME_FIRST)
                     ? 0.0f : mSVGDocumentWrapper->GetCurrentTime();

  SVGDrawingParameters params(aContext, aSize, aRegion, aSamplingFilter,
                              newSVGContext ? newSVGContext : aSVGContext,
                              animTime, aFlags, aOpacity);

  // If we have an prerasterized version of this image that matches the
  // drawing parameters, use that.
  RefPtr<gfxDrawable> svgDrawable = LookupCachedSurface(params);
  if (svgDrawable) {
    Show(svgDrawable, params);
    return DrawResult::SUCCESS;
  }

  // else, we need to paint the image:

  if (mIsDrawing) {
    NS_WARNING("Refusing to make re-entrant call to VectorImage::Draw");
    return DrawResult::TEMPORARY_ERROR;
  }
  AutoRestore<bool> autoRestoreIsDrawing(mIsDrawing);
  mIsDrawing = true;

  // Apply any 'preserveAspectRatio' override (if specified) to the root
  // element:
  AutoPreserveAspectRatioOverride autoPAR(newSVGContext ? newSVGContext : aSVGContext,
                                          mSVGDocumentWrapper->GetRootSVGElem());

  // Set the animation time:
  AutoSVGTimeSetRestore autoSVGTime(mSVGDocumentWrapper->GetRootSVGElem(),
                                    animTime);

  // Set context paint (if specified) on the document:
  Maybe<AutoSetRestoreSVGContextPaint> autoContextPaint;
  if (aSVGContext &&
      aSVGContext->GetContextPaint()) {
    autoContextPaint.emplace(aSVGContext->GetContextPaint(),
                             mSVGDocumentWrapper->GetDocument());
  }

  // We didn't get a hit in the surface cache, so we'll need to rerasterize.
  CreateSurfaceAndShow(params, aContext->GetDrawTarget()->GetBackendType());
  return DrawResult::SUCCESS;
}

already_AddRefed<gfxDrawable>
VectorImage::LookupCachedSurface(const SVGDrawingParameters& aParams)
{
  // If we're not allowed to use a cached surface, don't attempt a lookup.
  if (aParams.flags & FLAG_BYPASS_SURFACE_CACHE) {
    return nullptr;
  }

  // We don't do any caching if we have animation, so don't bother with a lookup
  // in this case either.
  if (mHaveAnimations) {
    return nullptr;
  }

  LookupResult result =
    SurfaceCache::Lookup(ImageKey(this),
                         VectorSurfaceKey(aParams.size, aParams.svgContext));
  if (!result) {
    return nullptr;  // No matching surface, or the OS freed the volatile buffer.
  }

  RefPtr<SourceSurface> sourceSurface = result.Surface()->GetSourceSurface();
  if (!sourceSurface) {
    // Something went wrong. (Probably a GPU driver crash or device reset.)
    // Attempt to recover.
    RecoverFromLossOfSurfaces();
    return nullptr;
  }

  RefPtr<gfxDrawable> svgDrawable =
    new gfxSurfaceDrawable(sourceSurface, result.Surface()->GetSize());
  return svgDrawable.forget();
}

void
VectorImage::CreateSurfaceAndShow(const SVGDrawingParameters& aParams, BackendType aBackend)
{
  mSVGDocumentWrapper->UpdateViewportBounds(aParams.viewportSize);
  mSVGDocumentWrapper->FlushImageTransformInvalidation();

  RefPtr<gfxDrawingCallback> cb =
    new SVGDrawingCallback(mSVGDocumentWrapper,
                           aParams.viewportSize,
                           aParams.size,
                           aParams.flags);

  RefPtr<gfxDrawable> svgDrawable =
    new gfxCallbackDrawable(cb, aParams.size);

  bool bypassCache = bool(aParams.flags & FLAG_BYPASS_SURFACE_CACHE) ||
                     // Refuse to cache animated images:
                     // XXX(seth): We may remove this restriction in bug 922893.
                     mHaveAnimations ||
                     // The image is too big to fit in the cache:
                     !SurfaceCache::CanHold(aParams.size);
  if (bypassCache) {
    return Show(svgDrawable, aParams);
  }

  // We're about to rerasterize, which may mean that some of the previous
  // surfaces we've rasterized aren't useful anymore. We can allow them to
  // expire from the cache by unlocking them here, and then sending out an
  // invalidation. If this image is locked, any surfaces that are still useful
  // will become locked again when Draw touches them, and the remainder will
  // eventually expire.
  SurfaceCache::UnlockEntries(ImageKey(this));

  // Try to create an imgFrame, initializing the surface it contains by drawing
  // our gfxDrawable into it. (We use FILTER_NEAREST since we never scale here.)
  NotNull<RefPtr<imgFrame>> frame = WrapNotNull(new imgFrame);
  nsresult rv =
    frame->InitWithDrawable(svgDrawable, aParams.size,
                            SurfaceFormat::B8G8R8A8,
                            SamplingFilter::POINT, aParams.flags,
                            aBackend);

  // If we couldn't create the frame, it was probably because it would end
  // up way too big. Generally it also wouldn't fit in the cache, but the prefs
  // could be set such that the cache isn't the limiting factor.
  if (NS_FAILED(rv)) {
    return Show(svgDrawable, aParams);
  }

  // Take a strong reference to the frame's surface and make sure it hasn't
  // already been purged by the operating system.
  RefPtr<SourceSurface> surface = frame->GetSourceSurface();
  if (!surface) {
    return Show(svgDrawable, aParams);
  }

  // Attempt to cache the frame.
  SurfaceKey surfaceKey = VectorSurfaceKey(aParams.size, aParams.svgContext);
  NotNull<RefPtr<ISurfaceProvider>> provider =
    WrapNotNull(new SimpleSurfaceProvider(ImageKey(this), surfaceKey, frame));
  SurfaceCache::Insert(provider);

  // Draw.
  RefPtr<gfxDrawable> drawable =
    new gfxSurfaceDrawable(surface, aParams.size);
  Show(drawable, aParams);

  // Send out an invalidation so that surfaces that are still in use get
  // re-locked. See the discussion of the UnlockSurfaces call above.
  if (!(aParams.flags & FLAG_ASYNC_NOTIFY)) {
    mProgressTracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE,
                                         GetMaxSizedIntRect());
  } else {
    NotNull<RefPtr<VectorImage>> image = WrapNotNull(this);
    NS_DispatchToMainThread(NS_NewRunnableFunction(
                              "ProgressTracker::SyncNotifyProgress",
                              [=]() -> void {
      RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
      if (tracker) {
        tracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE,
                                    GetMaxSizedIntRect());
      }
    }));
  }
}


void
VectorImage::Show(gfxDrawable* aDrawable, const SVGDrawingParameters& aParams)
{
  MOZ_ASSERT(aDrawable, "Should have a gfxDrawable by now");
  gfxUtils::DrawPixelSnapped(aParams.context, aDrawable,
                             aParams.size,
                             aParams.region,
                             SurfaceFormat::B8G8R8A8,
                             aParams.samplingFilter,
                             aParams.flags, aParams.opacity);

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

NS_IMETHODIMP
VectorImage::StartDecoding(uint32_t aFlags)
{
  // Nothing to do for SVG images
  return NS_OK;
}

bool
VectorImage::StartDecodingWithResult(uint32_t aFlags)
{
  // SVG images are ready to draw when they are loaded
  return mIsFullyLoaded;
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

NS_IMETHODIMP
VectorImage::LockImage()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return NS_ERROR_FAILURE;
  }

  mLockCount++;

  if (mLockCount == 1) {
    // Lock this image's surfaces in the SurfaceCache.
    SurfaceCache::LockImage(ImageKey(this));
  }

  return NS_OK;
}

//******************************************************************************

NS_IMETHODIMP
VectorImage::UnlockImage()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mError) {
    return NS_ERROR_FAILURE;
  }

  if (mLockCount == 0) {
    MOZ_ASSERT_UNREACHABLE("Calling UnlockImage with a zero lock count");
    return NS_ERROR_ABORT;
  }

  mLockCount--;

  if (mLockCount == 0) {
    // Unlock this image's surfaces in the SurfaceCache.
    SurfaceCache::UnlockImage(ImageKey(this));
  }

  return NS_OK;
}

//******************************************************************************

NS_IMETHODIMP
VectorImage::RequestDiscard()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mDiscardable && mLockCount == 0) {
    SurfaceCache::RemoveImage(ImageKey(this));
    mProgressTracker->OnDiscard();
  }

  return NS_OK;
}

void
VectorImage::OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey)
{
  MOZ_ASSERT(mProgressTracker);

  NS_DispatchToMainThread(NewRunnableMethod(mProgressTracker, &ProgressTracker::OnDiscard));
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::ResetAnimation()
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

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

  // ProgressTracker::SyncNotifyProgress may release us, so ensure we
  // stick around long enough to complete our work.
  RefPtr<VectorImage> kungFuDeathGrip(this);

  // Block page load until the document's ready.  (We unblock it in
  // OnSVGDocumentLoaded or OnSVGDocumentError.)
  if (mProgressTracker) {
    mProgressTracker->SyncNotifyProgress(FLAG_ONLOAD_BLOCKED);
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
NS_IMETHODIMP
VectorImage::OnStopRequest(nsIRequest* aRequest, nsISupports* aCtxt,
                           nsresult aStatus)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

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
    // notified that the SVG finished loading, so we need to treat this as an
    // error.
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

  // ProgressTracker::SyncNotifyProgress may release us, so ensure we
  // stick around long enough to complete our work.
  RefPtr<VectorImage> kungFuDeathGrip(this);

  // Tell *our* observers that we're done loading.
  if (mProgressTracker) {
    Progress progress = FLAG_SIZE_AVAILABLE |
                        FLAG_HAS_TRANSPARENCY |
                        FLAG_FRAME_COMPLETE |
                        FLAG_DECODE_COMPLETE |
                        FLAG_ONLOAD_UNBLOCKED;

    if (mHaveAnimations) {
      progress |= FLAG_IS_ANIMATED;
    }

    // Merge in any saved progress from OnImageDataComplete.
    if (mLoadProgress) {
      progress |= *mLoadProgress;
      mLoadProgress = Nothing();
    }

    mProgressTracker->SyncNotifyProgress(progress, GetMaxSizedIntRect());
  }

  EvaluateAnimation();
}

void
VectorImage::OnSVGDocumentError()
{
  CancelAllListeners();

  mError = true;

  if (mProgressTracker) {
    // Notify observers about the error and unblock page load.
    Progress progress = FLAG_ONLOAD_UNBLOCKED | FLAG_HAS_ERROR;

    // Merge in any saved progress from OnImageDataComplete.
    if (mLoadProgress) {
      progress |= *mLoadProgress;
      mLoadProgress = Nothing();
    }

    mProgressTracker->SyncNotifyProgress(progress);
  }
}

//------------------------------------------------------------------------------
// nsIStreamListener method

//******************************************************************************
NS_IMETHODIMP
VectorImage::OnDataAvailable(nsIRequest* aRequest, nsISupports* aCtxt,
                             nsIInputStream* aInStr, uint64_t aSourceOffset,
                             uint32_t aCount)
{
  if (mError) {
    return NS_ERROR_FAILURE;
  }

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

void
VectorImage::PropagateUseCounters(nsIDocument* aParentDocument)
{
  nsIDocument* doc = mSVGDocumentWrapper->GetDocument();
  if (doc) {
    doc->PropagateUseCounters(aParentDocument);
  }
}

void
VectorImage::ReportUseCounters()
{
  nsIDocument* doc = mSVGDocumentWrapper->GetDocument();
  if (doc) {
    static_cast<nsDocument*>(doc)->ReportUseCounters();
  }
}

nsIntSize
VectorImage::OptimalImageSizeForDest(const gfxSize& aDest,
                                     uint32_t aWhichFrame,
                                     SamplingFilter aSamplingFilter,
                                     uint32_t aFlags)
{
  MOZ_ASSERT(aDest.width >= 0 || ceil(aDest.width) <= INT32_MAX ||
             aDest.height >= 0 || ceil(aDest.height) <= INT32_MAX,
             "Unexpected destination size");

  // We can rescale SVGs freely, so just return the provided destination size.
  return nsIntSize::Ceil(aDest.width, aDest.height);
}

already_AddRefed<imgIContainer>
VectorImage::Unwrap()
{
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
}

} // namespace image
} // namespace mozilla

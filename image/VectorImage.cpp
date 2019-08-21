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
#include "mozilla/dom/Event.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGDocument.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/PresShell.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"
#include "nsIStreamListener.h"
#include "nsMimeTypes.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsStubDocumentObserver.h"
#include "SVGObserverUtils.h"  // for SVGRenderingObserver
#include "nsWindowSizes.h"
#include "ImageRegion.h"
#include "ISurfaceProvider.h"
#include "LookupResult.h"
#include "Orientation.h"
#include "SVGDocumentWrapper.h"
#include "SVGDrawingParameters.h"
#include "nsIDOMEventListener.h"
#include "SurfaceCache.h"
#include "mozilla/dom/Document.h"

namespace mozilla {

using namespace dom;
using namespace dom::SVGPreserveAspectRatio_Binding;
using namespace gfx;
using namespace layers;

namespace image {

// Helper-class: SVGRootRenderingObserver
class SVGRootRenderingObserver final : public SVGRenderingObserver {
 public:
  NS_DECL_ISUPPORTS

  SVGRootRenderingObserver(SVGDocumentWrapper* aDocWrapper,
                           VectorImage* aVectorImage)
      : SVGRenderingObserver(),
        mDocWrapper(aDocWrapper),
        mVectorImage(aVectorImage),
        mHonoringInvalidations(true) {
    MOZ_ASSERT(mDocWrapper, "Need a non-null SVG document wrapper");
    MOZ_ASSERT(mVectorImage, "Need a non-null VectorImage");

    StartObserving();
    Element* elem = GetReferencedElementWithoutObserving();
    MOZ_ASSERT(elem, "no root SVG node for us to observe");

    SVGObserverUtils::AddRenderingObserver(elem, this);
    mInObserverSet = true;
  }

  void ResumeHonoringInvalidations() { mHonoringInvalidations = true; }

 protected:
  virtual ~SVGRootRenderingObserver() {
    // This needs to call our GetReferencedElementWithoutObserving override,
    // so must be called here rather than in our base class's dtor.
    StopObserving();
  }

  Element* GetReferencedElementWithoutObserving() final {
    return mDocWrapper->GetRootSVGElem();
  }

  virtual void OnRenderingChange() override {
    Element* elem = GetReferencedElementWithoutObserving();
    MOZ_ASSERT(elem, "missing root SVG node");

    if (mHonoringInvalidations && !mDocWrapper->ShouldIgnoreInvalidation()) {
      nsIFrame* frame = elem->GetPrimaryFrame();
      if (!frame || frame->PresShell()->IsDestroying()) {
        // We're being destroyed. Bail out.
        return;
      }

      // Ignore further invalidations until we draw.
      mHonoringInvalidations = false;

      mVectorImage->InvalidateObserversOnNextRefreshDriverTick();
    }

    // Our caller might've removed us from rendering-observer list.
    // Add ourselves back!
    if (!mInObserverSet) {
      SVGObserverUtils::AddRenderingObserver(elem, this);
      mInObserverSet = true;
    }
  }

  // Private data
  const RefPtr<SVGDocumentWrapper> mDocWrapper;
  VectorImage* const mVectorImage;  // Raw pointer because it owns me.
  bool mHonoringInvalidations;
};

NS_IMPL_ISUPPORTS(SVGRootRenderingObserver, nsIMutationObserver)

class SVGParseCompleteListener final : public nsStubDocumentObserver {
 public:
  NS_DECL_ISUPPORTS

  SVGParseCompleteListener(SVGDocument* aDocument, VectorImage* aImage)
      : mDocument(aDocument), mImage(aImage) {
    MOZ_ASSERT(mDocument, "Need an SVG document");
    MOZ_ASSERT(mImage, "Need an image");

    mDocument->AddObserver(this);
  }

 private:
  ~SVGParseCompleteListener() {
    if (mDocument) {
      // The document must have been destroyed before we got our event.
      // Otherwise this can't happen, since documents hold strong references to
      // their observers.
      Cancel();
    }
  }

 public:
  void EndLoad(Document* aDocument) override {
    MOZ_ASSERT(aDocument == mDocument, "Got EndLoad for wrong document?");

    // OnSVGDocumentParsed will release our owner's reference to us, so ensure
    // we stick around long enough to complete our work.
    RefPtr<SVGParseCompleteListener> kungFuDeathGrip(this);

    mImage->OnSVGDocumentParsed();
  }

  void Cancel() {
    MOZ_ASSERT(mDocument, "Duplicate call to Cancel");
    if (mDocument) {
      mDocument->RemoveObserver(this);
      mDocument = nullptr;
    }
  }

 private:
  RefPtr<SVGDocument> mDocument;
  VectorImage* const mImage;  // Raw pointer to owner.
};

NS_IMPL_ISUPPORTS(SVGParseCompleteListener, nsIDocumentObserver)

class SVGLoadEventListener final : public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS

  SVGLoadEventListener(Document* aDocument, VectorImage* aImage)
      : mDocument(aDocument), mImage(aImage) {
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
  ~SVGLoadEventListener() {
    if (mDocument) {
      // The document must have been destroyed before we got our event.
      // Otherwise this can't happen, since documents hold strong references to
      // their observers.
      Cancel();
    }
  }

 public:
  NS_IMETHOD HandleEvent(Event* aEvent) override {
    MOZ_ASSERT(mDocument, "Need an SVG document. Received multiple events?");

    // OnSVGDocumentLoaded/OnSVGDocumentError will release our owner's reference
    // to us, so ensure we stick around long enough to complete our work.
    RefPtr<SVGLoadEventListener> kungFuDeathGrip(this);

    nsAutoString eventType;
    aEvent->GetType(eventType);
    MOZ_ASSERT(eventType.EqualsLiteral("MozSVGAsImageDocumentLoad") ||
                   eventType.EqualsLiteral("SVGAbort") ||
                   eventType.EqualsLiteral("SVGError"),
               "Received unexpected event");

    if (eventType.EqualsLiteral("MozSVGAsImageDocumentLoad")) {
      mImage->OnSVGDocumentLoaded();
    } else {
      mImage->OnSVGDocumentError();
    }

    return NS_OK;
  }

  void Cancel() {
    MOZ_ASSERT(mDocument, "Duplicate call to Cancel");
    if (mDocument) {
      mDocument->RemoveEventListener(
          NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"), this, true);
      mDocument->RemoveEventListener(NS_LITERAL_STRING("SVGAbort"), this, true);
      mDocument->RemoveEventListener(NS_LITERAL_STRING("SVGError"), this, true);
      mDocument = nullptr;
    }
  }

 private:
  nsCOMPtr<Document> mDocument;
  VectorImage* const mImage;  // Raw pointer to owner.
};

NS_IMPL_ISUPPORTS(SVGLoadEventListener, nsIDOMEventListener)

// Helper-class: SVGDrawingCallback
class SVGDrawingCallback : public gfxDrawingCallback {
 public:
  SVGDrawingCallback(SVGDocumentWrapper* aSVGDocumentWrapper,
                     const IntSize& aViewportSize, const IntSize& aSize,
                     uint32_t aImageFlags)
      : mSVGDocumentWrapper(aSVGDocumentWrapper),
        mViewportSize(aViewportSize),
        mSize(aSize),
        mImageFlags(aImageFlags) {}
  virtual bool operator()(gfxContext* aContext, const gfxRect& aFillRect,
                          const SamplingFilter aSamplingFilter,
                          const gfxMatrix& aTransform) override;

 private:
  RefPtr<SVGDocumentWrapper> mSVGDocumentWrapper;
  const IntSize mViewportSize;
  const IntSize mSize;
  uint32_t mImageFlags;
};

// Based loosely on nsSVGIntegrationUtils' PaintFrameCallback::operator()
bool SVGDrawingCallback::operator()(gfxContext* aContext,
                                    const gfxRect& aFillRect,
                                    const SamplingFilter aSamplingFilter,
                                    const gfxMatrix& aTransform) {
  MOZ_ASSERT(mSVGDocumentWrapper, "need an SVGDocumentWrapper");

  // Get (& sanity-check) the helper-doc's presShell
  RefPtr<PresShell> presShell = mSVGDocumentWrapper->GetPresShell();
  MOZ_ASSERT(presShell, "GetPresShell returned null for an SVG image?");

#ifdef MOZ_GECKO_PROFILER
  Document* doc = presShell->GetDocument();
  nsIURI* uri = doc ? doc->GetDocumentURI() : nullptr;
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "SVG Image drawing", GRAPHICS,
      nsPrintfCString("%dx%d %s", mSize.width, mSize.height,
                      uri ? uri->GetSpecOrDefault().get() : "N/A"));
#endif

  gfxContextAutoSaveRestore contextRestorer(aContext);

  // Clip to aFillRect so that we don't paint outside.
  aContext->NewPath();
  aContext->Rectangle(aFillRect);
  aContext->Clip();

  gfxMatrix matrix = aTransform;
  if (!matrix.Invert()) {
    return false;
  }
  aContext->SetMatrixDouble(
      aContext->CurrentMatrixDouble().PreMultiply(matrix).PreScale(
          double(mSize.width) / mViewportSize.width,
          double(mSize.height) / mViewportSize.height));

  nsPresContext* presContext = presShell->GetPresContext();
  MOZ_ASSERT(presContext, "pres shell w/out pres context");

  nsRect svgRect(0, 0, presContext->DevPixelsToAppUnits(mViewportSize.width),
                 presContext->DevPixelsToAppUnits(mViewportSize.height));

  RenderDocumentFlags renderDocFlags =
      RenderDocumentFlags::IgnoreViewportScrolling;
  if (!(mImageFlags & imgIContainer::FLAG_SYNC_DECODE)) {
    renderDocFlags |= RenderDocumentFlags::AsyncDecodeImages;
  }

  presShell->RenderDocument(svgRect, renderDocFlags,
                            NS_RGBA(0, 0, 0, 0),  // transparent
                            aContext);

  return true;
}

class MOZ_STACK_CLASS AutoRestoreSVGState final {
 public:
  AutoRestoreSVGState(const SVGDrawingParameters& aParams,
                      SVGDocumentWrapper* aSVGDocumentWrapper, bool& aIsDrawing,
                      bool aContextPaint)
      : mIsDrawing(aIsDrawing)
        // Apply any 'preserveAspectRatio' override (if specified) to the root
        // element:
        ,
        mPAR(aParams.svgContext, aSVGDocumentWrapper->GetRootSVGElem())
        // Set the animation time:
        ,
        mTime(aSVGDocumentWrapper->GetRootSVGElem(), aParams.animationTime) {
    MOZ_ASSERT(!aIsDrawing);
    MOZ_ASSERT(aSVGDocumentWrapper->GetDocument());

    aIsDrawing = true;

    // Set context paint (if specified) on the document:
    if (aContextPaint) {
      MOZ_ASSERT(aParams.svgContext->GetContextPaint());
      mContextPaint.emplace(*aParams.svgContext->GetContextPaint(),
                            *aSVGDocumentWrapper->GetDocument());
    }
  }

 private:
  AutoRestore<bool> mIsDrawing;
  AutoPreserveAspectRatioOverride mPAR;
  AutoSVGTimeSetRestore mTime;
  Maybe<AutoSetRestoreSVGContextPaint> mContextPaint;
};

// Implement VectorImage's nsISupports-inherited methods
NS_IMPL_ISUPPORTS(VectorImage, imgIContainer, nsIStreamListener,
                  nsIRequestObserver)

//------------------------------------------------------------------------------
// Constructor / Destructor

VectorImage::VectorImage(nsIURI* aURI /* = nullptr */)
    : ImageResource(aURI),  // invoke superclass's constructor
      mLockCount(0),
      mIsInitialized(false),
      mDiscardable(false),
      mIsFullyLoaded(false),
      mIsDrawing(false),
      mHaveAnimations(false),
      mHasPendingInvalidation(false) {}

VectorImage::~VectorImage() {
  CancelAllListeners();
  SurfaceCache::RemoveImage(ImageKey(this));
}

//------------------------------------------------------------------------------
// Methods inherited from Image.h

nsresult VectorImage::Init(const char* aMimeType, uint32_t aFlags) {
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

size_t VectorImage::SizeOfSourceWithComputedFallback(
    SizeOfState& aState) const {
  if (!mSVGDocumentWrapper) {
    return 0;  // No document, so no memory used for the document.
  }

  SVGDocument* doc = mSVGDocumentWrapper->GetDocument();
  if (!doc) {
    return 0;  // No document, so no memory used for the document.
  }

  nsWindowSizes windowSizes(aState);
  doc->DocAddSizeOfIncludingThis(windowSizes);

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

void VectorImage::CollectSizeOfSurfaces(
    nsTArray<SurfaceMemoryCounter>& aCounters,
    MallocSizeOf aMallocSizeOf) const {
  SurfaceCache::CollectSizeOfSurfaces(ImageKey(this), aCounters, aMallocSizeOf);
}

nsresult VectorImage::OnImageDataComplete(nsIRequest* aRequest,
                                          nsISupports* aContext,
                                          nsresult aStatus, bool aLastPart) {
  // Call our internal OnStopRequest method, which only talks to our embedded
  // SVG document. This won't have any effect on our ProgressTracker.
  nsresult finalStatus = OnStopRequest(aRequest, aStatus);

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

nsresult VectorImage::OnImageDataAvailable(nsIRequest* aRequest,
                                           nsISupports* aContext,
                                           nsIInputStream* aInStr,
                                           uint64_t aSourceOffset,
                                           uint32_t aCount) {
  return OnDataAvailable(aRequest, aInStr, aSourceOffset, aCount);
}

nsresult VectorImage::StartAnimation() {
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(ShouldAnimate(), "Should not animate!");

  mSVGDocumentWrapper->StartAnimation();
  return NS_OK;
}

nsresult VectorImage::StopAnimation() {
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

bool VectorImage::ShouldAnimate() {
  return ImageResource::ShouldAnimate() && mIsFullyLoaded && mHaveAnimations;
}

NS_IMETHODIMP_(void)
VectorImage::SetAnimationStartTime(const TimeStamp& aTime) {
  // We don't care about animation start time.
}

//------------------------------------------------------------------------------
// imgIContainer methods

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetWidth(int32_t* aWidth) {
  if (mError || !mIsFullyLoaded) {
    // XXXdholbert Technically we should leave outparam untouched when we
    // fail. But since many callers don't check for failure, we set it to 0 on
    // failure, for sane/predictable results.
    *aWidth = 0;
    return NS_ERROR_FAILURE;
  }

  SVGSVGElement* rootElem = mSVGDocumentWrapper->GetRootSVGElem();
  MOZ_ASSERT(rootElem,
             "Should have a root SVG elem, since we finished "
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
nsresult VectorImage::GetNativeSizes(nsTArray<IntSize>& aNativeSizes) const {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
size_t VectorImage::GetNativeSizesLength() const { return 0; }

//******************************************************************************
NS_IMETHODIMP_(void)
VectorImage::RequestRefresh(const TimeStamp& aTime) {
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
    SendInvalidationNotifications();
  }
}

void VectorImage::SendInvalidationNotifications() {
  // Animated images don't send out invalidation notifications as soon as
  // they're generated. Instead, InvalidateObserversOnNextRefreshDriverTick
  // records that there are pending invalidations and then returns immediately.
  // The notifications are actually sent from RequestRefresh(). We send these
  // notifications there to ensure that there is actually a document observing
  // us. Otherwise, the notifications are just wasted effort.
  //
  // Non-animated images post an event to call this method from
  // InvalidateObserversOnNextRefreshDriverTick, because RequestRefresh is never
  // called for them. Ordinarily this isn't needed, since we send out
  // invalidation notifications in OnSVGDocumentLoaded, but in rare cases the
  // SVG document may not be 100% ready to render at that time. In those cases
  // we would miss the subsequent invalidations if we didn't send out the
  // notifications indirectly in |InvalidateObservers...|.

  MOZ_ASSERT(mHasPendingInvalidation);
  mHasPendingInvalidation = false;
  SurfaceCache::RemoveImage(ImageKey(this));

  if (UpdateImageContainer(Nothing())) {
    // If we have image containers, that means we probably won't get a Draw call
    // from the owner since they are using the container. We must assume all
    // invalidations need to be handled.
    MOZ_ASSERT(mRenderingObserver, "Should have a rendering observer by now");
    mRenderingObserver->ResumeHonoringInvalidations();
  }

  if (mProgressTracker) {
    mProgressTracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE,
                                         GetMaxSizedIntRect());
  }
}

NS_IMETHODIMP_(IntRect)
VectorImage::GetImageSpaceInvalidationRect(const IntRect& aRect) {
  return aRect;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetHeight(int32_t* aHeight) {
  if (mError || !mIsFullyLoaded) {
    // XXXdholbert Technically we should leave outparam untouched when we
    // fail. But since many callers don't check for failure, we set it to 0 on
    // failure, for sane/predictable results.
    *aHeight = 0;
    return NS_ERROR_FAILURE;
  }

  SVGSVGElement* rootElem = mSVGDocumentWrapper->GetRootSVGElem();
  MOZ_ASSERT(rootElem,
             "Should have a root SVG elem, since we finished "
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
VectorImage::GetIntrinsicSize(nsSize* aSize) {
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* rootFrame = mSVGDocumentWrapper->GetRootLayoutFrame();
  if (!rootFrame) {
    return NS_ERROR_FAILURE;
  }

  *aSize = nsSize(-1, -1);
  IntrinsicSize rfSize = rootFrame->GetIntrinsicSize();
  if (rfSize.width) {
    aSize->width = *rfSize.width;
  }
  if (rfSize.height) {
    aSize->height = *rfSize.height;
  }
  return NS_OK;
}

//******************************************************************************
Maybe<AspectRatio> VectorImage::GetIntrinsicRatio() {
  if (mError || !mIsFullyLoaded) {
    return Nothing();
  }

  nsIFrame* rootFrame = mSVGDocumentWrapper->GetRootLayoutFrame();
  if (!rootFrame) {
    return Nothing();
  }

  return Some(rootFrame->GetIntrinsicRatio());
}

NS_IMETHODIMP_(Orientation)
VectorImage::GetOrientation() { return Orientation(); }

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetType(uint16_t* aType) {
  NS_ENSURE_ARG_POINTER(aType);

  *aType = imgIContainer::TYPE_VECTOR;
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetProducerId(uint32_t* aId) {
  NS_ENSURE_ARG_POINTER(aId);

  *aId = ImageResource::GetImageProducerId();
  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::GetAnimated(bool* aAnimated) {
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  *aAnimated = mSVGDocumentWrapper->IsAnimated();
  return NS_OK;
}

//******************************************************************************
int32_t VectorImage::GetFirstFrameDelay() {
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
VectorImage::WillDrawOpaqueNow() {
  return false;  // In general, SVG content is not opaque.
}

//******************************************************************************
NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
VectorImage::GetFrame(uint32_t aWhichFrame, uint32_t aFlags) {
  if (mError) {
    return nullptr;
  }

  // Look up height & width
  // ----------------------
  SVGSVGElement* svgElem = mSVGDocumentWrapper->GetRootSVGElem();
  MOZ_ASSERT(svgElem,
             "Should have a root SVG elem, since we finished "
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
VectorImage::GetFrameAtSize(const IntSize& aSize, uint32_t aWhichFrame,
                            uint32_t aFlags) {
#ifdef DEBUG
  NotifyDrawingObservers();
#endif

  auto result = GetFrameInternal(aSize, Nothing(), aWhichFrame, aFlags);
  return Get<2>(result).forget();
}

Tuple<ImgDrawResult, IntSize, RefPtr<SourceSurface>>
VectorImage::GetFrameInternal(const IntSize& aSize,
                              const Maybe<SVGImageContext>& aSVGContext,
                              uint32_t aWhichFrame, uint32_t aFlags) {
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE);

  if (aSize.IsEmpty() || aWhichFrame > FRAME_MAX_VALUE) {
    return MakeTuple(ImgDrawResult::BAD_ARGS, aSize, RefPtr<SourceSurface>());
  }

  if (mError) {
    return MakeTuple(ImgDrawResult::BAD_IMAGE, aSize, RefPtr<SourceSurface>());
  }

  if (!mIsFullyLoaded) {
    return MakeTuple(ImgDrawResult::NOT_READY, aSize, RefPtr<SourceSurface>());
  }

  RefPtr<SourceSurface> sourceSurface;
  IntSize decodeSize;
  Tie(sourceSurface, decodeSize) =
      LookupCachedSurface(aSize, aSVGContext, aFlags);
  if (sourceSurface) {
    return MakeTuple(ImgDrawResult::SUCCESS, decodeSize,
                     std::move(sourceSurface));
  }

  if (mIsDrawing) {
    NS_WARNING("Refusing to make re-entrant call to VectorImage::Draw");
    return MakeTuple(ImgDrawResult::TEMPORARY_ERROR, decodeSize,
                     RefPtr<SourceSurface>());
  }

  // By using a null gfxContext, we ensure that we will always attempt to
  // create a surface, even if we aren't capable of caching it (e.g. due to our
  // flags, having an animation, etc). Otherwise CreateSurface will assume that
  // the caller is capable of drawing directly to its own draw target if we
  // cannot cache.
  SVGDrawingParameters params(
      nullptr, decodeSize, aSize, ImageRegion::Create(decodeSize),
      SamplingFilter::POINT, aSVGContext,
      mSVGDocumentWrapper->GetCurrentTimeAsFloat(), aFlags, 1.0);

  bool didCache;  // Was the surface put into the cache?
  bool contextPaint = aSVGContext && aSVGContext->GetContextPaint();

  AutoRestoreSVGState autoRestore(params, mSVGDocumentWrapper, mIsDrawing,
                                  contextPaint);

  RefPtr<gfxDrawable> svgDrawable = CreateSVGDrawable(params);
  RefPtr<SourceSurface> surface = CreateSurface(params, svgDrawable, didCache);
  if (!surface) {
    MOZ_ASSERT(!didCache);
    return MakeTuple(ImgDrawResult::TEMPORARY_ERROR, decodeSize,
                     RefPtr<SourceSurface>());
  }

  SendFrameComplete(didCache, params.flags);
  return MakeTuple(ImgDrawResult::SUCCESS, decodeSize, std::move(surface));
}

//******************************************************************************
Tuple<ImgDrawResult, IntSize> VectorImage::GetImageContainerSize(
    LayerManager* aManager, const IntSize& aSize, uint32_t aFlags) {
  if (mError) {
    return MakeTuple(ImgDrawResult::BAD_IMAGE, IntSize(0, 0));
  }

  if (!mIsFullyLoaded) {
    return MakeTuple(ImgDrawResult::NOT_READY, IntSize(0, 0));
  }

  if (mHaveAnimations ||
      aManager->GetBackendType() != LayersBackend::LAYERS_WR) {
    return MakeTuple(ImgDrawResult::NOT_SUPPORTED, IntSize(0, 0));
  }

  // We don't need to check if the size is too big since we only support
  // WebRender backends.
  if (aSize.IsEmpty()) {
    return MakeTuple(ImgDrawResult::BAD_ARGS, IntSize(0, 0));
  }

  return MakeTuple(ImgDrawResult::SUCCESS, aSize);
}

NS_IMETHODIMP_(bool)
VectorImage::IsImageContainerAvailable(LayerManager* aManager,
                                       uint32_t aFlags) {
  if (mError || !mIsFullyLoaded || mHaveAnimations ||
      aManager->GetBackendType() != LayersBackend::LAYERS_WR) {
    return false;
  }

  return true;
}

//******************************************************************************
NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
VectorImage::GetImageContainer(LayerManager* aManager, uint32_t aFlags) {
  MOZ_ASSERT(aManager->GetBackendType() != LayersBackend::LAYERS_WR,
             "WebRender should always use GetImageContainerAvailableAtSize!");
  return nullptr;
}

//******************************************************************************
NS_IMETHODIMP_(bool)
VectorImage::IsImageContainerAvailableAtSize(LayerManager* aManager,
                                             const IntSize& aSize,
                                             uint32_t aFlags) {
  // Since we only support image containers with WebRender, and it can handle
  // textures larger than the hw max texture size, we don't need to check aSize.
  return !aSize.IsEmpty() && IsImageContainerAvailable(aManager, aFlags);
}

//******************************************************************************
NS_IMETHODIMP_(ImgDrawResult)
VectorImage::GetImageContainerAtSize(layers::LayerManager* aManager,
                                     const gfx::IntSize& aSize,
                                     const Maybe<SVGImageContext>& aSVGContext,
                                     uint32_t aFlags,
                                     layers::ImageContainer** aOutContainer) {
  Maybe<SVGImageContext> newSVGContext;
  MaybeRestrictSVGContext(newSVGContext, aSVGContext, aFlags);

  // The aspect ratio flag was already handled as part of the SVG context
  // restriction above.
  uint32_t flags = aFlags & ~(FLAG_FORCE_PRESERVEASPECTRATIO_NONE);
  return GetImageContainerImpl(aManager, aSize,
                               newSVGContext ? newSVGContext : aSVGContext,
                               flags, aOutContainer);
}

bool VectorImage::MaybeRestrictSVGContext(
    Maybe<SVGImageContext>& aNewSVGContext,
    const Maybe<SVGImageContext>& aSVGContext, uint32_t aFlags) {
  bool overridePAR =
      (aFlags & FLAG_FORCE_PRESERVEASPECTRATIO_NONE) && aSVGContext;

  bool haveContextPaint = aSVGContext && aSVGContext->GetContextPaint();
  bool blockContextPaint = false;
  if (haveContextPaint) {
    blockContextPaint = !SVGContextPaint::IsAllowedForImageFromURI(mURI);
  }

  if (overridePAR || blockContextPaint) {
    // The key that we create for the image surface cache must match the way
    // that the image will be painted, so we need to initialize a new matching
    // SVGImageContext here in order to generate the correct key.

    aNewSVGContext = aSVGContext;  // copy

    if (overridePAR) {
      // The SVGImageContext must take account of the preserveAspectRatio
      // overide:
      MOZ_ASSERT(!aSVGContext->GetPreserveAspectRatio(),
                 "FLAG_FORCE_PRESERVEASPECTRATIO_NONE is not expected if a "
                 "preserveAspectRatio override is supplied");
      Maybe<SVGPreserveAspectRatio> aspectRatio = Some(SVGPreserveAspectRatio(
          SVG_PRESERVEASPECTRATIO_NONE, SVG_MEETORSLICE_UNKNOWN));
      aNewSVGContext->SetPreserveAspectRatio(aspectRatio);
    }

    if (blockContextPaint) {
      // The SVGImageContext must not include context paint if the image is
      // not allowed to use it:
      aNewSVGContext->ClearContextPaint();
    }
  }

  return haveContextPaint && !blockContextPaint;
}

//******************************************************************************
NS_IMETHODIMP_(ImgDrawResult)
VectorImage::Draw(gfxContext* aContext, const nsIntSize& aSize,
                  const ImageRegion& aRegion, uint32_t aWhichFrame,
                  SamplingFilter aSamplingFilter,
                  const Maybe<SVGImageContext>& aSVGContext, uint32_t aFlags,
                  float aOpacity) {
  if (aWhichFrame > FRAME_MAX_VALUE) {
    return ImgDrawResult::BAD_ARGS;
  }

  if (!aContext) {
    return ImgDrawResult::BAD_ARGS;
  }

  if (mError) {
    return ImgDrawResult::BAD_IMAGE;
  }

  if (!mIsFullyLoaded) {
    return ImgDrawResult::NOT_READY;
  }

  if (mAnimationConsumers == 0) {
    SendOnUnlockedDraw(aFlags);
  }

  // We should bypass the cache when:
  // - We are using a DrawTargetRecording because we prefer the drawing commands
  //   in general to the rasterized surface. This allows blob images to avoid
  //   rasterized SVGs with WebRender.
  // - The size exceeds what we are willing to cache as a rasterized surface.
  //   We don't do this for WebRender because the performance of the fallback
  //   path is quite bad and upscaling the SVG from the clamped size is better
  //   than bringing the browser to a crawl.
  if (aContext->GetDrawTarget()->GetBackendType() == BackendType::RECORDING ||
      (!gfxVars::UseWebRender() &&
       aSize != SurfaceCache::ClampVectorSize(aSize))) {
    aFlags |= FLAG_BYPASS_SURFACE_CACHE;
  }

  MOZ_ASSERT(!(aFlags & FLAG_FORCE_PRESERVEASPECTRATIO_NONE) ||
                 (aSVGContext && aSVGContext->GetViewportSize()),
             "Viewport size is required when using "
             "FLAG_FORCE_PRESERVEASPECTRATIO_NONE");

  float animTime = (aWhichFrame == FRAME_FIRST)
                       ? 0.0f
                       : mSVGDocumentWrapper->GetCurrentTimeAsFloat();

  Maybe<SVGImageContext> newSVGContext;
  bool contextPaint =
      MaybeRestrictSVGContext(newSVGContext, aSVGContext, aFlags);

  SVGDrawingParameters params(aContext, aSize, aSize, aRegion, aSamplingFilter,
                              newSVGContext ? newSVGContext : aSVGContext,
                              animTime, aFlags, aOpacity);

  // If we have an prerasterized version of this image that matches the
  // drawing parameters, use that.
  RefPtr<SourceSurface> sourceSurface;
  Tie(sourceSurface, params.size) =
      LookupCachedSurface(aSize, params.svgContext, aFlags);
  if (sourceSurface) {
    RefPtr<gfxDrawable> drawable =
        new gfxSurfaceDrawable(sourceSurface, params.size);
    Show(drawable, params);
    return ImgDrawResult::SUCCESS;
  }

  // else, we need to paint the image:

  if (mIsDrawing) {
    NS_WARNING("Refusing to make re-entrant call to VectorImage::Draw");
    return ImgDrawResult::TEMPORARY_ERROR;
  }

  AutoRestoreSVGState autoRestore(params, mSVGDocumentWrapper, mIsDrawing,
                                  contextPaint);

  bool didCache;  // Was the surface put into the cache?
  RefPtr<gfxDrawable> svgDrawable = CreateSVGDrawable(params);
  sourceSurface = CreateSurface(params, svgDrawable, didCache);
  if (!sourceSurface) {
    MOZ_ASSERT(!didCache);
    Show(svgDrawable, params);
    return ImgDrawResult::SUCCESS;
  }

  RefPtr<gfxDrawable> drawable =
      new gfxSurfaceDrawable(sourceSurface, params.size);
  Show(drawable, params);
  SendFrameComplete(didCache, params.flags);
  return ImgDrawResult::SUCCESS;
}

already_AddRefed<gfxDrawable> VectorImage::CreateSVGDrawable(
    const SVGDrawingParameters& aParams) {
  RefPtr<gfxDrawingCallback> cb = new SVGDrawingCallback(
      mSVGDocumentWrapper, aParams.viewportSize, aParams.size, aParams.flags);

  RefPtr<gfxDrawable> svgDrawable = new gfxCallbackDrawable(cb, aParams.size);
  return svgDrawable.forget();
}

Tuple<RefPtr<SourceSurface>, IntSize> VectorImage::LookupCachedSurface(
    const IntSize& aSize, const Maybe<SVGImageContext>& aSVGContext,
    uint32_t aFlags) {
  // If we're not allowed to use a cached surface, don't attempt a lookup.
  if (aFlags & FLAG_BYPASS_SURFACE_CACHE) {
    return MakeTuple(RefPtr<SourceSurface>(), aSize);
  }

  // We don't do any caching if we have animation, so don't bother with a lookup
  // in this case either.
  if (mHaveAnimations) {
    return MakeTuple(RefPtr<SourceSurface>(), aSize);
  }

  LookupResult result(MatchType::NOT_FOUND);
  SurfaceKey surfaceKey = VectorSurfaceKey(aSize, aSVGContext);
  if ((aFlags & FLAG_SYNC_DECODE) || !(aFlags & FLAG_HIGH_QUALITY_SCALING)) {
    result = SurfaceCache::Lookup(ImageKey(this), surfaceKey,
                                  /* aMarkUsed = */ true);
  } else {
    result = SurfaceCache::LookupBestMatch(ImageKey(this), surfaceKey,
                                           /* aMarkUsed = */ true);
  }

  IntSize rasterSize =
      result.SuggestedSize().IsEmpty() ? aSize : result.SuggestedSize();
  MOZ_ASSERT(result.Type() != MatchType::SUBSTITUTE_BECAUSE_PENDING);
  if (!result || result.Type() == MatchType::SUBSTITUTE_BECAUSE_NOT_FOUND) {
    // No matching surface, or the OS freed the volatile buffer.
    return MakeTuple(RefPtr<SourceSurface>(), rasterSize);
  }

  RefPtr<SourceSurface> sourceSurface = result.Surface()->GetSourceSurface();
  if (!sourceSurface) {
    // Something went wrong. (Probably a GPU driver crash or device reset.)
    // Attempt to recover.
    RecoverFromLossOfSurfaces();
    return MakeTuple(RefPtr<SourceSurface>(), rasterSize);
  }

  return MakeTuple(std::move(sourceSurface), rasterSize);
}

already_AddRefed<SourceSurface> VectorImage::CreateSurface(
    const SVGDrawingParameters& aParams, gfxDrawable* aSVGDrawable,
    bool& aWillCache) {
  MOZ_ASSERT(mIsDrawing);

  mSVGDocumentWrapper->UpdateViewportBounds(aParams.viewportSize);
  mSVGDocumentWrapper->FlushImageTransformInvalidation();

  // Determine whether or not we should put the surface to be created into
  // the cache. If we fail, we need to reset this to false to let the caller
  // know nothing was put in the cache.
  aWillCache = !(aParams.flags & FLAG_BYPASS_SURFACE_CACHE) &&
               // Refuse to cache animated images:
               // XXX(seth): We may remove this restriction in bug 922893.
               !mHaveAnimations &&
               // The image is too big to fit in the cache:
               SurfaceCache::CanHold(aParams.size);

  // If we weren't given a context, then we know we just want the rasterized
  // surface. We will create the frame below but only insert it into the cache
  // if we actually need to.
  if (!aWillCache && aParams.context) {
    return nullptr;
  }

  // We're about to rerasterize, which may mean that some of the previous
  // surfaces we've rasterized aren't useful anymore. We can allow them to
  // expire from the cache by unlocking them here, and then sending out an
  // invalidation. If this image is locked, any surfaces that are still useful
  // will become locked again when Draw touches them, and the remainder will
  // eventually expire.
  if (aWillCache) {
    SurfaceCache::UnlockEntries(ImageKey(this));
  }

  // If there is no context, the default backend is fine.
  BackendType backend =
      aParams.context ? aParams.context->GetDrawTarget()->GetBackendType()
                      : gfxPlatform::GetPlatform()->GetDefaultContentBackend();

  // Try to create an imgFrame, initializing the surface it contains by drawing
  // our gfxDrawable into it. (We use FILTER_NEAREST since we never scale here.)
  auto frame = MakeNotNull<RefPtr<imgFrame>>();
  nsresult rv = frame->InitWithDrawable(
      aSVGDrawable, aParams.size, SurfaceFormat::B8G8R8A8,
      SamplingFilter::POINT, aParams.flags, backend);

  // If we couldn't create the frame, it was probably because it would end
  // up way too big. Generally it also wouldn't fit in the cache, but the prefs
  // could be set such that the cache isn't the limiting factor.
  if (NS_FAILED(rv)) {
    aWillCache = false;
    return nullptr;
  }

  // Take a strong reference to the frame's surface and make sure it hasn't
  // already been purged by the operating system.
  RefPtr<SourceSurface> surface = frame->GetSourceSurface();
  if (!surface) {
    aWillCache = false;
    return nullptr;
  }

  // We created the frame, but only because we had no context to draw to
  // directly. All the caller wants is the surface in this case.
  if (!aWillCache) {
    return surface.forget();
  }

  // Attempt to cache the frame.
  SurfaceKey surfaceKey = VectorSurfaceKey(aParams.size, aParams.svgContext);
  NotNull<RefPtr<ISurfaceProvider>> provider =
      MakeNotNull<SimpleSurfaceProvider*>(ImageKey(this), surfaceKey, frame);

  if (SurfaceCache::Insert(provider) == InsertOutcome::SUCCESS &&
      aParams.size != aParams.drawSize) {
    // We created a new surface that wasn't the size we requested, which means
    // we entered factor-of-2 mode. We should purge any surfaces we no longer
    // need rather than waiting for the cache to expire them.
    SurfaceCache::PruneImage(ImageKey(this));
  }

  return surface.forget();
}

void VectorImage::SendFrameComplete(bool aDidCache, uint32_t aFlags) {
  // If the cache was not updated, we have nothing to do.
  if (!aDidCache) {
    return;
  }

  // Send out an invalidation so that surfaces that are still in use get
  // re-locked. See the discussion of the UnlockSurfaces call above.
  if (!(aFlags & FLAG_ASYNC_NOTIFY)) {
    mProgressTracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE,
                                         GetMaxSizedIntRect());
  } else {
    NotNull<RefPtr<VectorImage>> image = WrapNotNull(this);
    NS_DispatchToMainThread(CreateMediumHighRunnable(NS_NewRunnableFunction(
        "ProgressTracker::SyncNotifyProgress", [=]() -> void {
          RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
          if (tracker) {
            tracker->SyncNotifyProgress(FLAG_FRAME_COMPLETE,
                                        GetMaxSizedIntRect());
          }
        })));
  }
}

void VectorImage::Show(gfxDrawable* aDrawable,
                       const SVGDrawingParameters& aParams) {
  // The surface size may differ from the size at which we wish to draw. As
  // such, we may need to adjust the context/region to take this into account.
  gfxContextMatrixAutoSaveRestore saveMatrix(aParams.context);
  ImageRegion region(aParams.region);
  if (aParams.drawSize != aParams.size) {
    gfx::Size scale(double(aParams.drawSize.width) / aParams.size.width,
                    double(aParams.drawSize.height) / aParams.size.height);
    aParams.context->Multiply(gfxMatrix::Scaling(scale.width, scale.height));
    region.Scale(1.0 / scale.width, 1.0 / scale.height);
  }

  MOZ_ASSERT(aDrawable, "Should have a gfxDrawable by now");
  gfxUtils::DrawPixelSnapped(aParams.context, aDrawable,
                             SizeDouble(aParams.size), region,
                             SurfaceFormat::B8G8R8A8, aParams.samplingFilter,
                             aParams.flags, aParams.opacity, false);

#ifdef DEBUG
  NotifyDrawingObservers();
#endif

  MOZ_ASSERT(mRenderingObserver, "Should have a rendering observer by now");
  mRenderingObserver->ResumeHonoringInvalidations();
}

void VectorImage::RecoverFromLossOfSurfaces() {
  NS_WARNING("An imgFrame became invalid. Attempting to recover...");

  // Discard all existing frames, since they're probably all now invalid.
  SurfaceCache::RemoveImage(ImageKey(this));
}

NS_IMETHODIMP
VectorImage::StartDecoding(uint32_t aFlags, uint32_t aWhichFrame) {
  // Nothing to do for SVG images
  return NS_OK;
}

bool VectorImage::StartDecodingWithResult(uint32_t aFlags,
                                          uint32_t aWhichFrame) {
  // SVG images are ready to draw when they are loaded
  return mIsFullyLoaded;
}

bool VectorImage::RequestDecodeWithResult(uint32_t aFlags,
                                          uint32_t aWhichFrame) {
  // SVG images are ready to draw when they are loaded
  return mIsFullyLoaded;
}

NS_IMETHODIMP
VectorImage::RequestDecodeForSize(const nsIntSize& aSize, uint32_t aFlags,
                                  uint32_t aWhichFrame) {
  // Nothing to do for SVG images, though in theory we could rasterize to the
  // provided size ahead of time if we supported off-main-thread SVG
  // rasterization...
  return NS_OK;
}

//******************************************************************************

NS_IMETHODIMP
VectorImage::LockImage() {
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
VectorImage::UnlockImage() {
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
VectorImage::RequestDiscard() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mDiscardable && mLockCount == 0) {
    SurfaceCache::RemoveImage(ImageKey(this));
    mProgressTracker->OnDiscard();
  }

  if (Document* doc = mSVGDocumentWrapper->GetDocument()) {
    doc->ReportUseCounters();
  }

  return NS_OK;
}

void VectorImage::OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey) {
  MOZ_ASSERT(mProgressTracker);

  NS_DispatchToMainThread(NewRunnableMethod("ProgressTracker::OnDiscard",
                                            mProgressTracker,
                                            &ProgressTracker::OnDiscard));
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::ResetAnimation() {
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  if (!mIsFullyLoaded || !mHaveAnimations) {
    return NS_OK;  // There are no animations to be reset.
  }

  mSVGDocumentWrapper->ResetAnimation();

  return NS_OK;
}

NS_IMETHODIMP_(float)
VectorImage::GetFrameIndex(uint32_t aWhichFrame) {
  MOZ_ASSERT(aWhichFrame <= FRAME_MAX_VALUE, "Invalid argument");
  return aWhichFrame == FRAME_FIRST
             ? 0.0f
             : mSVGDocumentWrapper->GetCurrentTimeAsFloat();
}

//------------------------------------------------------------------------------
// nsIRequestObserver methods

//******************************************************************************
NS_IMETHODIMP
VectorImage::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(!mSVGDocumentWrapper,
             "Repeated call to OnStartRequest -- can this happen?");

  mSVGDocumentWrapper = new SVGDocumentWrapper();
  nsresult rv = mSVGDocumentWrapper->OnStartRequest(aRequest);
  if (NS_FAILED(rv)) {
    mSVGDocumentWrapper = nullptr;
    mError = true;
    return rv;
  }

  // Create a listener to wait until the SVG document is fully loaded, which
  // will signal that this image is ready to render. Certain error conditions
  // will prevent us from ever getting this notification, so we also create a
  // listener that waits for parsing to complete and cancels the
  // SVGLoadEventListener if needed. The listeners are automatically attached
  // to the document by their constructors.
  SVGDocument* document = mSVGDocumentWrapper->GetDocument();
  mLoadEventListener = new SVGLoadEventListener(document, this);
  mParseCompleteListener = new SVGParseCompleteListener(document, this);

  return NS_OK;
}

//******************************************************************************
NS_IMETHODIMP
VectorImage::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  return mSVGDocumentWrapper->OnStopRequest(aRequest, aStatus);
}

void VectorImage::OnSVGDocumentParsed() {
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

void VectorImage::CancelAllListeners() {
  if (mParseCompleteListener) {
    mParseCompleteListener->Cancel();
    mParseCompleteListener = nullptr;
  }
  if (mLoadEventListener) {
    mLoadEventListener->Cancel();
    mLoadEventListener = nullptr;
  }
}

void VectorImage::OnSVGDocumentLoaded() {
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
    Progress progress = FLAG_SIZE_AVAILABLE | FLAG_HAS_TRANSPARENCY |
                        FLAG_FRAME_COMPLETE | FLAG_DECODE_COMPLETE;

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

void VectorImage::OnSVGDocumentError() {
  CancelAllListeners();

  mError = true;

  if (mProgressTracker) {
    // Notify observers about the error and unblock page load.
    Progress progress = FLAG_HAS_ERROR;

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
VectorImage::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInStr,
                             uint64_t aSourceOffset, uint32_t aCount) {
  if (mError) {
    return NS_ERROR_FAILURE;
  }

  return mSVGDocumentWrapper->OnDataAvailable(aRequest, aInStr, aSourceOffset,
                                              aCount);
}

// --------------------------
// Invalidation helper method

void VectorImage::InvalidateObserversOnNextRefreshDriverTick() {
  if (mHasPendingInvalidation) {
    return;
  }

  mHasPendingInvalidation = true;

  // Animated images can wait for the refresh tick.
  if (mHaveAnimations) {
    return;
  }

  // Non-animated images won't get the refresh tick, so we should just send an
  // invalidation outside the current execution context. We need to defer
  // because the layout tree is in the middle of invalidation, and the tree
  // state needs to be consistent. Specifically only some of the frames have
  // had the NS_FRAME_DESCENDANT_NEEDS_PAINT and/or NS_FRAME_NEEDS_PAINT bits
  // set by InvalidateFrameInternal in layout/generic/nsFrame.cpp. These bits
  // get cleared when we repaint the SVG into a surface by
  // nsIFrame::ClearInvalidationStateBits in nsDisplayList::PaintRoot.
  nsCOMPtr<nsIEventTarget> eventTarget;
  if (mProgressTracker) {
    eventTarget = mProgressTracker->GetEventTarget();
  } else {
    eventTarget = do_GetMainThread();
  }

  RefPtr<VectorImage> self(this);
  nsCOMPtr<nsIRunnable> ev(NS_NewRunnableFunction(
      "VectorImage::SendInvalidationNotifications",
      [=]() -> void { self->SendInvalidationNotifications(); }));
  eventTarget->Dispatch(CreateMediumHighRunnable(ev.forget()),
                        NS_DISPATCH_NORMAL);
}

void VectorImage::PropagateUseCounters(Document* aParentDocument) {
  Document* doc = mSVGDocumentWrapper->GetDocument();
  if (doc) {
    doc->PropagateUseCounters(aParentDocument);
  }
}

nsIntSize VectorImage::OptimalImageSizeForDest(const gfxSize& aDest,
                                               uint32_t aWhichFrame,
                                               SamplingFilter aSamplingFilter,
                                               uint32_t aFlags) {
  MOZ_ASSERT(aDest.width >= 0 || ceil(aDest.width) <= INT32_MAX ||
                 aDest.height >= 0 || ceil(aDest.height) <= INT32_MAX,
             "Unexpected destination size");

  // We can rescale SVGs freely, so just return the provided destination size.
  return nsIntSize::Ceil(aDest.width, aDest.height);
}

already_AddRefed<imgIContainer> VectorImage::Unwrap() {
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
}

}  // namespace image
}  // namespace mozilla

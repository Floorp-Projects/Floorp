/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicImage.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/RefPtr.h"
#include "ImageRegion.h"
#include "Orientation.h"
#include "SVGImageContext.h"

#include "mozilla/MemoryReporting.h"

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::layers::LayerManager;
using mozilla::layers::ImageContainer;

namespace mozilla {
namespace image {

// Inherited methods from Image.

already_AddRefed<ProgressTracker>
DynamicImage::GetProgressTracker()
{
  return nullptr;
}

size_t
DynamicImage::SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const
{
  return 0;
}

void
DynamicImage::CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                    MallocSizeOf aMallocSizeOf) const
{
  // We can't report anything useful because gfxDrawable doesn't expose this
  // information.
}

void
DynamicImage::IncrementAnimationConsumers()
{ }

void
DynamicImage::DecrementAnimationConsumers()
{ }

#ifdef DEBUG
uint32_t
DynamicImage::GetAnimationConsumers()
{
  return 0;
}
#endif

nsresult
DynamicImage::OnImageDataAvailable(nsIRequest* aRequest,
                                   nsISupports* aContext,
                                   nsIInputStream* aInStr,
                                   uint64_t aSourceOffset,
                                   uint32_t aCount)
{
  return NS_OK;
}

nsresult
DynamicImage::OnImageDataComplete(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsresult aStatus,
                                  bool aLastPart)
{
  return NS_OK;
}

void
DynamicImage::OnSurfaceDiscarded(const SurfaceKey& aSurfaceKey)
{ }

void
DynamicImage::SetInnerWindowID(uint64_t aInnerWindowId)
{ }

uint64_t
DynamicImage::InnerWindowID() const
{
  return 0;
}

bool
DynamicImage::HasError()
{
  return !mDrawable;
}

void
DynamicImage::SetHasError()
{ }

ImageURL*
DynamicImage::GetURI()
{
  return nullptr;
}

// Methods inherited from XPCOM interfaces.

NS_IMPL_ISUPPORTS(DynamicImage, imgIContainer)

NS_IMETHODIMP
DynamicImage::GetWidth(int32_t* aWidth)
{
  *aWidth = mDrawable->Size().width;
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::GetHeight(int32_t* aHeight)
{
  *aHeight = mDrawable->Size().height;
  return NS_OK;
}

nsresult
DynamicImage::GetNativeSizes(nsTArray<IntSize>& aNativeSizes) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DynamicImage::GetIntrinsicSize(nsSize* aSize)
{
  IntSize intSize(mDrawable->Size());
  *aSize = nsSize(intSize.width, intSize.height);
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::GetIntrinsicRatio(nsSize* aSize)
{
  IntSize intSize(mDrawable->Size());
  *aSize = nsSize(intSize.width, intSize.height);
  return NS_OK;
}

NS_IMETHODIMP_(Orientation)
DynamicImage::GetOrientation()
{
  return Orientation();
}

NS_IMETHODIMP
DynamicImage::GetType(uint16_t* aType)
{
  *aType = imgIContainer::TYPE_RASTER;
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::GetAnimated(bool* aAnimated)
{
  *aAnimated = false;
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
DynamicImage::GetFrame(uint32_t aWhichFrame,
                       uint32_t aFlags)
{
  IntSize size(mDrawable->Size());
  return GetFrameAtSize(IntSize(size.width, size.height),
                        aWhichFrame,
                        aFlags);
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
DynamicImage::GetFrameAtSize(const IntSize& aSize,
                             uint32_t aWhichFrame,
                             uint32_t aFlags)
{
  RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->
    CreateOffscreenContentDrawTarget(aSize, SurfaceFormat::B8G8R8A8);
  if (!dt || !dt->IsValid()) {
    gfxWarning() <<
      "DynamicImage::GetFrame failed in CreateOffscreenContentDrawTarget";
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
DynamicImage::WillDrawOpaqueNow()
{
  return false;
}

NS_IMETHODIMP_(bool)
DynamicImage::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  return false;
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
DynamicImage::GetImageContainer(LayerManager* aManager, uint32_t aFlags)
{
  return nullptr;
}

NS_IMETHODIMP_(DrawResult)
DynamicImage::Draw(gfxContext* aContext,
                   const nsIntSize& aSize,
                   const ImageRegion& aRegion,
                   uint32_t aWhichFrame,
                   SamplingFilter aSamplingFilter,
                   const Maybe<SVGImageContext>& aSVGContext,
                   uint32_t aFlags,
                   float aOpacity)
{
  MOZ_ASSERT(!aSize.IsEmpty(), "Unexpected empty size");

  IntSize drawableSize(mDrawable->Size());

  if (aSize == drawableSize) {
    gfxUtils::DrawPixelSnapped(aContext, mDrawable, SizeDouble(drawableSize), aRegion,
                               SurfaceFormat::B8G8R8A8, aSamplingFilter,
                               aOpacity);
    return DrawResult::SUCCESS;
  }

  gfxSize scale(double(aSize.width) / drawableSize.width,
                double(aSize.height) / drawableSize.height);

  ImageRegion region(aRegion);
  region.Scale(1.0 / scale.width, 1.0 / scale.height);

  gfxContextMatrixAutoSaveRestore saveMatrix(aContext);
  aContext->Multiply(gfxMatrix::Scaling(scale.width, scale.height));

  gfxUtils::DrawPixelSnapped(aContext, mDrawable, SizeDouble(drawableSize), region,
                             SurfaceFormat::B8G8R8A8, aSamplingFilter,
                             aOpacity);
  return DrawResult::SUCCESS;
}

NS_IMETHODIMP
DynamicImage::StartDecoding(uint32_t aFlags)
{
  return NS_OK;
}

bool
DynamicImage::StartDecodingWithResult(uint32_t aFlags)
{
  return true;
}

NS_IMETHODIMP
DynamicImage::RequestDecodeForSize(const nsIntSize& aSize, uint32_t aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::LockImage()
{
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::UnlockImage()
{
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::RequestDiscard()
{
  return NS_OK;
}

NS_IMETHODIMP_(void)
DynamicImage::RequestRefresh(const mozilla::TimeStamp& aTime)
{ }

NS_IMETHODIMP
DynamicImage::GetAnimationMode(uint16_t* aAnimationMode)
{
  *aAnimationMode = kNormalAnimMode;
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::SetAnimationMode(uint16_t aAnimationMode)
{
  return NS_OK;
}

NS_IMETHODIMP
DynamicImage::ResetAnimation()
{
  return NS_OK;
}

NS_IMETHODIMP_(float)
DynamicImage::GetFrameIndex(uint32_t aWhichFrame)
{
  return 0;
}

NS_IMETHODIMP_(int32_t)
DynamicImage::GetFirstFrameDelay()
{
  return 0;
}

NS_IMETHODIMP_(void)
DynamicImage::SetAnimationStartTime(const mozilla::TimeStamp& aTime)
{ }

nsIntSize
DynamicImage::OptimalImageSizeForDest(const gfxSize& aDest,
                                      uint32_t aWhichFrame,
                                      SamplingFilter aSamplingFilter,
                                      uint32_t aFlags)
{
  IntSize size(mDrawable->Size());
  return nsIntSize(size.width, size.height);
}

NS_IMETHODIMP_(nsIntRect)
DynamicImage::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  return aRect;
}

already_AddRefed<imgIContainer>
DynamicImage::Unwrap()
{
  nsCOMPtr<imgIContainer> self(this);
  return self.forget();
}

void
DynamicImage::PropagateUseCounters(nsIDocument*)
{
  // No use counters.
}

} // namespace image
} // namespace mozilla

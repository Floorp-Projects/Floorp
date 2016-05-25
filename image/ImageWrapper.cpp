/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageWrapper.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "Orientation.h"

#include "mozilla/MemoryReporting.h"

namespace mozilla {

using gfx::DataSourceSurface;
using gfx::SamplingFilter;
using gfx::SourceSurface;
using layers::LayerManager;
using layers::ImageContainer;

namespace image {

// Inherited methods from Image.

already_AddRefed<ProgressTracker>
ImageWrapper::GetProgressTracker()
{
  return mInnerImage->GetProgressTracker();
}

size_t
ImageWrapper::SizeOfSourceWithComputedFallback(MallocSizeOf aMallocSizeOf) const
{
  return mInnerImage->SizeOfSourceWithComputedFallback(aMallocSizeOf);
}

void
ImageWrapper::CollectSizeOfSurfaces(nsTArray<SurfaceMemoryCounter>& aCounters,
                                    MallocSizeOf aMallocSizeOf) const
{
  mInnerImage->CollectSizeOfSurfaces(aCounters, aMallocSizeOf);
}

void
ImageWrapper::IncrementAnimationConsumers()
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only to encourage serialization "
                                "with DecrementAnimationConsumers");
  mInnerImage->IncrementAnimationConsumers();
}

void
ImageWrapper::DecrementAnimationConsumers()
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only to encourage serialization "
                                "with IncrementAnimationConsumers");
  mInnerImage->DecrementAnimationConsumers();
}

#ifdef DEBUG
uint32_t
ImageWrapper::GetAnimationConsumers()
{
  return mInnerImage->GetAnimationConsumers();
}
#endif

nsresult
ImageWrapper::OnImageDataAvailable(nsIRequest* aRequest,
                                   nsISupports* aContext,
                                   nsIInputStream* aInStr,
                                   uint64_t aSourceOffset,
                                   uint32_t aCount)
{
  return mInnerImage->OnImageDataAvailable(aRequest, aContext, aInStr,
                                           aSourceOffset, aCount);
}

nsresult
ImageWrapper::OnImageDataComplete(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsresult aStatus,
                                  bool aLastPart)
{
  return mInnerImage->OnImageDataComplete(aRequest, aContext, aStatus,
                                          aLastPart);
}

void
ImageWrapper::OnSurfaceDiscarded()
{
  return mInnerImage->OnSurfaceDiscarded();
}

void
ImageWrapper::SetInnerWindowID(uint64_t aInnerWindowId)
{
  mInnerImage->SetInnerWindowID(aInnerWindowId);
}

uint64_t
ImageWrapper::InnerWindowID() const
{
  return mInnerImage->InnerWindowID();
}

bool
ImageWrapper::HasError()
{
  return mInnerImage->HasError();
}

void
ImageWrapper::SetHasError()
{
  mInnerImage->SetHasError();
}

ImageURL*
ImageWrapper::GetURI()
{
  return mInnerImage->GetURI();
}

// Methods inherited from XPCOM interfaces.

NS_IMPL_ISUPPORTS(ImageWrapper, imgIContainer)

NS_IMETHODIMP
ImageWrapper::GetWidth(int32_t* aWidth)
{
  return mInnerImage->GetWidth(aWidth);
}

NS_IMETHODIMP
ImageWrapper::GetHeight(int32_t* aHeight)
{
  return mInnerImage->GetHeight(aHeight);
}

NS_IMETHODIMP
ImageWrapper::GetIntrinsicSize(nsSize* aSize)
{
  return mInnerImage->GetIntrinsicSize(aSize);
}

NS_IMETHODIMP
ImageWrapper::GetIntrinsicRatio(nsSize* aSize)
{
  return mInnerImage->GetIntrinsicRatio(aSize);
}

NS_IMETHODIMP_(Orientation)
ImageWrapper::GetOrientation()
{
  return mInnerImage->GetOrientation();
}

NS_IMETHODIMP
ImageWrapper::GetType(uint16_t* aType)
{
  return mInnerImage->GetType(aType);
}

NS_IMETHODIMP
ImageWrapper::GetAnimated(bool* aAnimated)
{
  return mInnerImage->GetAnimated(aAnimated);
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
ImageWrapper::GetFrame(uint32_t aWhichFrame,
                       uint32_t aFlags)
{
  return mInnerImage->GetFrame(aWhichFrame, aFlags);
}

NS_IMETHODIMP_(already_AddRefed<SourceSurface>)
ImageWrapper::GetFrameAtSize(const IntSize& aSize,
                             uint32_t aWhichFrame,
                             uint32_t aFlags)
{
  return mInnerImage->GetFrameAtSize(aSize, aWhichFrame, aFlags);
}

NS_IMETHODIMP_(bool)
ImageWrapper::IsOpaque()
{
  return mInnerImage->IsOpaque();
}

NS_IMETHODIMP_(bool)
ImageWrapper::IsImageContainerAvailable(LayerManager* aManager, uint32_t aFlags)
{
  return mInnerImage->IsImageContainerAvailable(aManager, aFlags);
}

NS_IMETHODIMP_(already_AddRefed<ImageContainer>)
ImageWrapper::GetImageContainer(LayerManager* aManager, uint32_t aFlags)
{
  return mInnerImage->GetImageContainer(aManager, aFlags);
}

NS_IMETHODIMP_(DrawResult)
ImageWrapper::Draw(gfxContext* aContext,
                   const nsIntSize& aSize,
                   const ImageRegion& aRegion,
                   uint32_t aWhichFrame,
                   SamplingFilter aSamplingFilter,
                   const Maybe<SVGImageContext>& aSVGContext,
                   uint32_t aFlags)
{
  return mInnerImage->Draw(aContext, aSize, aRegion, aWhichFrame,
                           aSamplingFilter, aSVGContext, aFlags);
}

NS_IMETHODIMP
ImageWrapper::StartDecoding()
{
  return mInnerImage->StartDecoding();
}

NS_IMETHODIMP
ImageWrapper::RequestDecodeForSize(const nsIntSize& aSize, uint32_t aFlags)
{
  return mInnerImage->RequestDecodeForSize(aSize, aFlags);
}

NS_IMETHODIMP
ImageWrapper::LockImage()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread to encourage serialization with UnlockImage");
  return mInnerImage->LockImage();
}

NS_IMETHODIMP
ImageWrapper::UnlockImage()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Main thread to encourage serialization with LockImage");
  return mInnerImage->UnlockImage();
}

NS_IMETHODIMP
ImageWrapper::RequestDiscard()
{
  return mInnerImage->RequestDiscard();
}

NS_IMETHODIMP_(void)
ImageWrapper::RequestRefresh(const TimeStamp& aTime)
{
  return mInnerImage->RequestRefresh(aTime);
}

NS_IMETHODIMP
ImageWrapper::GetAnimationMode(uint16_t* aAnimationMode)
{
  return mInnerImage->GetAnimationMode(aAnimationMode);
}

NS_IMETHODIMP
ImageWrapper::SetAnimationMode(uint16_t aAnimationMode)
{
  return mInnerImage->SetAnimationMode(aAnimationMode);
}

NS_IMETHODIMP
ImageWrapper::ResetAnimation()
{
  return mInnerImage->ResetAnimation();
}

NS_IMETHODIMP_(float)
ImageWrapper::GetFrameIndex(uint32_t aWhichFrame)
{
  return mInnerImage->GetFrameIndex(aWhichFrame);
}

NS_IMETHODIMP_(int32_t)
ImageWrapper::GetFirstFrameDelay()
{
  return mInnerImage->GetFirstFrameDelay();
}

NS_IMETHODIMP_(void)
ImageWrapper::SetAnimationStartTime(const TimeStamp& aTime)
{
  mInnerImage->SetAnimationStartTime(aTime);
}

void
ImageWrapper::PropagateUseCounters(nsIDocument* aParentDocument)
{
  mInnerImage->PropagateUseCounters(aParentDocument);
}

nsIntSize
ImageWrapper::OptimalImageSizeForDest(const gfxSize& aDest,
                                      uint32_t aWhichFrame,
                                      SamplingFilter aSamplingFilter,
                                      uint32_t aFlags)
{
  return mInnerImage->OptimalImageSizeForDest(aDest, aWhichFrame,
                                              aSamplingFilter, aFlags);
}

NS_IMETHODIMP_(nsIntRect)
ImageWrapper::GetImageSpaceInvalidationRect(const nsIntRect& aRect)
{
  return mInnerImage->GetImageSpaceInvalidationRect(aRect);
}

already_AddRefed<imgIContainer>
ImageWrapper::Unwrap()
{
  return mInnerImage->Unwrap();
}

} // namespace image
} // namespace mozilla

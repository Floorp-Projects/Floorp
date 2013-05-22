/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageWrapper.h"

using mozilla::layers::LayerManager;
using mozilla::layers::ImageContainer;

namespace mozilla {
namespace image {

// Inherited methods from Image.

nsresult
ImageWrapper::Init(const char* aMimeType, uint32_t aFlags)
{
  return mInnerImage->Init(aMimeType, aFlags);
}

imgStatusTracker&
ImageWrapper::GetStatusTracker()
{
  return mInnerImage->GetStatusTracker();
}

nsIntRect
ImageWrapper::FrameRect(uint32_t aWhichFrame)
{
  return mInnerImage->FrameRect(aWhichFrame);
}

uint32_t
ImageWrapper::SizeOfData()
{
  return mInnerImage->SizeOfData();
}

size_t
ImageWrapper::HeapSizeOfSourceWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const
{
  return mInnerImage->HeapSizeOfSourceWithComputedFallback(aMallocSizeOf);
}

size_t
ImageWrapper::HeapSizeOfDecodedWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const
{
  return mInnerImage->HeapSizeOfDecodedWithComputedFallback(aMallocSizeOf);
}

size_t
ImageWrapper::NonHeapSizeOfDecoded() const
{
  return mInnerImage->NonHeapSizeOfDecoded();
}

size_t
ImageWrapper::OutOfProcessSizeOfDecoded() const
{
  return mInnerImage->OutOfProcessSizeOfDecoded();
}

void
ImageWrapper::IncrementAnimationConsumers()
{
  mInnerImage->IncrementAnimationConsumers();
}

void
ImageWrapper::DecrementAnimationConsumers()
{
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
  return mInnerImage->OnImageDataComplete(aRequest, aContext, aStatus, aLastPart);
}

nsresult
ImageWrapper::OnNewSourceData()
{
  return mInnerImage->OnNewSourceData();
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

nsIURI*
ImageWrapper::GetURI()
{
  return mInnerImage->GetURI();
}

// Methods inherited from XPCOM interfaces.

NS_IMPL_ISUPPORTS1(ImageWrapper, imgIContainer)

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

NS_IMETHODIMP
ImageWrapper::GetType(uint16_t* aType)
{
  return mInnerImage->GetType(aType);
}

NS_IMETHODIMP_(uint16_t)
ImageWrapper::GetType()
{
  return mInnerImage->GetType();
}

NS_IMETHODIMP
ImageWrapper::GetAnimated(bool* aAnimated)
{
  return mInnerImage->GetAnimated(aAnimated);
}

NS_IMETHODIMP
ImageWrapper::GetFrame(uint32_t aWhichFrame,
                       uint32_t aFlags,
                       gfxASurface** _retval)
{
  return mInnerImage->GetFrame(aWhichFrame, aFlags, _retval);
}

NS_IMETHODIMP_(bool)
ImageWrapper::FrameIsOpaque(uint32_t aWhichFrame)
{
  return mInnerImage->FrameIsOpaque(aWhichFrame);
}

NS_IMETHODIMP
ImageWrapper::GetImageContainer(LayerManager* aManager, ImageContainer** _retval)
{
  return mInnerImage->GetImageContainer(aManager, _retval);
}

NS_IMETHODIMP
ImageWrapper::Draw(gfxContext* aContext,
                   gfxPattern::GraphicsFilter aFilter,
                   const gfxMatrix& aUserSpaceToImageSpace,
                   const gfxRect& aFill,
                   const nsIntRect& aSubimage,
                   const nsIntSize& aViewportSize,
                   const SVGImageContext* aSVGContext,
                   uint32_t aWhichFrame,
                   uint32_t aFlags)
{
  return mInnerImage->Draw(aContext, aFilter, aUserSpaceToImageSpace, aFill,
                           aSubimage, aViewportSize, aSVGContext, aWhichFrame,
                           aFlags);
}

NS_IMETHODIMP
ImageWrapper::RequestDecode()
{
  return mInnerImage->RequestDecode();
}

NS_IMETHODIMP
ImageWrapper::StartDecoding()
{
  return mInnerImage->StartDecoding();
}

bool
ImageWrapper::IsDecoded()
{
  return mInnerImage->IsDecoded();
}

NS_IMETHODIMP
ImageWrapper::LockImage()
{
  return mInnerImage->LockImage();
}

NS_IMETHODIMP
ImageWrapper::UnlockImage()
{
  return mInnerImage->UnlockImage();
}

NS_IMETHODIMP
ImageWrapper::RequestDiscard()
{
  return mInnerImage->RequestDiscard();
}

NS_IMETHODIMP_(void)
ImageWrapper::RequestRefresh(const mozilla::TimeStamp& aTime)
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
ImageWrapper::SetAnimationStartTime(const mozilla::TimeStamp& aTime)
{
  mInnerImage->SetAnimationStartTime(aTime);
}

} // namespace image
} // namespace mozilla

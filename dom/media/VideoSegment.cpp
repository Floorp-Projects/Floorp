/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoSegment.h"

#include "gfx2DGlue.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

using namespace layers;

VideoFrame::VideoFrame(already_AddRefed<Image>& aImage,
                       const gfx::IntSize& aIntrinsicSize)
  : mImage(aImage), mIntrinsicSize(aIntrinsicSize), mForceBlack(false),
    mPrincipalHandle(PRINCIPAL_HANDLE_NONE)
{}

VideoFrame::VideoFrame()
  : mIntrinsicSize(0, 0), mForceBlack(false), mPrincipalHandle(PRINCIPAL_HANDLE_NONE)
{}

VideoFrame::~VideoFrame()
{}

void
VideoFrame::SetNull() {
  mImage = nullptr;
  mIntrinsicSize = gfx::IntSize(0, 0);
  mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
}

void
VideoFrame::TakeFrom(VideoFrame* aFrame)
{
  mImage = aFrame->mImage.forget();
  mIntrinsicSize = aFrame->mIntrinsicSize;
  mForceBlack = aFrame->GetForceBlack();
  mPrincipalHandle = aFrame->mPrincipalHandle;
}

/* static */ already_AddRefed<Image>
VideoFrame::CreateBlackImage(const gfx::IntSize& aSize)
{
  RefPtr<ImageContainer> container =
    LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS);
  RefPtr<PlanarYCbCrImage> image = container->CreatePlanarYCbCrImage();
  if (!image) {
    return nullptr;
  }

  int len = ((aSize.width * aSize.height) * 3 / 2);

  // Generate a black image.
  auto frame = MakeUnique<uint8_t[]>(len);
  int y = aSize.width * aSize.height;
  // Fill Y plane.
  memset(frame.get(), 0x10, y);
  // Fill Cb/Cr planes.
  memset(frame.get() + y, 0x80, (len - y));

  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  layers::PlanarYCbCrData data;
  data.mYChannel = frame.get();
  data.mYSize = gfx::IntSize(aSize.width, aSize.height);
  data.mYStride = (int32_t) (aSize.width * lumaBpp / 8.0);
  data.mCbCrStride = (int32_t) (aSize.width * chromaBpp / 8.0);
  data.mCbChannel = frame.get() + aSize.height * data.mYStride;
  data.mCrChannel = data.mCbChannel + aSize.height * data.mCbCrStride / 2;
  data.mCbCrSize = gfx::IntSize(aSize.width / 2, aSize.height / 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfx::IntSize(aSize.width, aSize.height);
  data.mStereoMode = StereoMode::MONO;

  // Copies data, so we can free data.
  if (!image->CopyData(data)) {
    return nullptr;
  }

  return image.forget();
}

VideoChunk::VideoChunk()
{}

VideoChunk::~VideoChunk()
{}

void
VideoSegment::AppendFrame(already_AddRefed<Image>&& aImage,
                          StreamTime aDuration,
                          const IntSize& aIntrinsicSize,
                          const PrincipalHandle& aPrincipalHandle,
                          bool aForceBlack,
                          TimeStamp aTimeStamp)
{
  VideoChunk* chunk = AppendChunk(aDuration);
  chunk->mTimeStamp = aTimeStamp;
  VideoFrame frame(aImage, aIntrinsicSize);
  frame.SetForceBlack(aForceBlack);
  frame.SetPrincipalHandle(aPrincipalHandle);
  chunk->mFrame.TakeFrom(&frame);
}

VideoSegment::VideoSegment()
  : MediaSegmentBase<VideoSegment, VideoChunk>(VIDEO)
{}

VideoSegment::~VideoSegment()
{}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoSegment.h"

#include "gfx2DGlue.h"
#include "ImageContainer.h"
#include "Layers.h"

namespace mozilla {

using namespace layers;

VideoFrame::VideoFrame(already_AddRefed<Image>& aImage,
                       const gfxIntSize& aIntrinsicSize)
  : mImage(aImage), mIntrinsicSize(aIntrinsicSize), mForceBlack(false)
{}

VideoFrame::VideoFrame()
  : mIntrinsicSize(0, 0), mForceBlack(false)
{}

VideoFrame::~VideoFrame()
{}

void
VideoFrame::SetNull() {
  mImage = nullptr;
  mIntrinsicSize = gfxIntSize(0, 0);
}

void
VideoFrame::TakeFrom(VideoFrame* aFrame)
{
  mImage = aFrame->mImage.forget();
  mIntrinsicSize = aFrame->mIntrinsicSize;
  mForceBlack = aFrame->GetForceBlack();
}

#if !defined(MOZILLA_XPCOMRT_API)
/* static */ already_AddRefed<Image>
VideoFrame::CreateBlackImage(const gfxIntSize& aSize)
{
  nsRefPtr<ImageContainer> container;
  nsRefPtr<Image> image;
  container = LayerManager::CreateImageContainer();
  image = container->CreateImage(ImageFormat::PLANAR_YCBCR);
  if (!image) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  int len = ((aSize.width * aSize.height) * 3 / 2);
  PlanarYCbCrImage* planar = static_cast<PlanarYCbCrImage*>(image.get());

  // Generate a black image.
  ScopedDeletePtr<uint8_t> frame(new uint8_t[len]);
  int y = aSize.width * aSize.height;
  // Fill Y plane.
  memset(frame.rwget(), 0x10, y);
  // Fill Cb/Cr planes.
  memset(frame.rwget() + y, 0x80, (len - y));

  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  layers::PlanarYCbCrData data;
  data.mYChannel = frame.rwget();
  data.mYSize = gfx::IntSize(aSize.width, aSize.height);
  data.mYStride = (int32_t) (aSize.width * lumaBpp / 8.0);
  data.mCbCrStride = (int32_t) (aSize.width * chromaBpp / 8.0);
  data.mCbChannel = frame.rwget() + aSize.height * data.mYStride;
  data.mCrChannel = data.mCbChannel + aSize.height * data.mCbCrStride / 2;
  data.mCbCrSize = gfx::IntSize(aSize.width / 2, aSize.height / 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfx::IntSize(aSize.width, aSize.height);
  data.mStereoMode = StereoMode::MONO;

  // SetData copies data, so we can free data.
  planar->SetData(data);

  return image.forget();
}
#endif // !defined(MOZILLA_XPCOMRT_API)

VideoChunk::VideoChunk()
{}

VideoChunk::~VideoChunk()
{}

void
VideoSegment::AppendFrame(already_AddRefed<Image>&& aImage,
                          StreamTime aDuration,
                          const IntSize& aIntrinsicSize,
                          bool aForceBlack)
{
  VideoChunk* chunk = AppendChunk(aDuration);
  VideoFrame frame(aImage, aIntrinsicSize);
  frame.SetForceBlack(aForceBlack);
  chunk->mFrame.TakeFrom(&frame);
}

VideoSegment::VideoSegment()
  : MediaSegmentBase<VideoSegment, VideoChunk>(VIDEO)
{}

VideoSegment::~VideoSegment()
{}

}

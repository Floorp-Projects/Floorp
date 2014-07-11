/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoi420FrameImpl.h"
#include "mozilla/gmp/GMPTypes.h"

namespace mozilla {
namespace gmp {

GMPVideoi420FrameImpl::GMPVideoi420FrameImpl(GMPVideoHostImpl* aHost)
: mYPlane(aHost),
  mUPlane(aHost),
  mVPlane(aHost),
  mWidth(0),
  mHeight(0),
  mTimestamp(0),
  mRenderTime_ms(0)
{
  MOZ_ASSERT(aHost);
}

GMPVideoi420FrameImpl::GMPVideoi420FrameImpl(const GMPVideoi420FrameData& aFrameData,
                                             GMPVideoHostImpl* aHost)
: mYPlane(aFrameData.mYPlane(), aHost),
  mUPlane(aFrameData.mUPlane(), aHost),
  mVPlane(aFrameData.mVPlane(), aHost),
  mWidth(aFrameData.mWidth()),
  mHeight(aFrameData.mHeight()),
  mTimestamp(aFrameData.mTimestamp()),
  mRenderTime_ms(aFrameData.mRenderTime_ms())
{
  MOZ_ASSERT(aHost);
}

GMPVideoi420FrameImpl::~GMPVideoi420FrameImpl()
{
}

bool
GMPVideoi420FrameImpl::InitFrameData(GMPVideoi420FrameData& aFrameData)
{
  mYPlane.InitPlaneData(aFrameData.mYPlane());
  mUPlane.InitPlaneData(aFrameData.mUPlane());
  mVPlane.InitPlaneData(aFrameData.mVPlane());
  aFrameData.mWidth() = mWidth;
  aFrameData.mHeight() = mHeight;
  aFrameData.mTimestamp() = mTimestamp;
  aFrameData.mRenderTime_ms() = mRenderTime_ms;
  return true;
}

GMPVideoFrameFormat
GMPVideoi420FrameImpl::GetFrameFormat()
{
  return kGMPI420VideoFrame;
}

void
GMPVideoi420FrameImpl::Destroy()
{
  delete this;
}

bool
GMPVideoi420FrameImpl::CheckDimensions(int32_t aWidth, int32_t aHeight,
                                       int32_t aStride_y, int32_t aStride_u, int32_t aStride_v)
{
  int32_t half_width = (aWidth + 1) / 2;
  if (aWidth < 1 || aHeight < 1 || aStride_y < aWidth ||
                                   aStride_u < half_width ||
                                   aStride_v < half_width) {
    return false;
  }
  return true;
}

const GMPPlaneImpl*
GMPVideoi420FrameImpl::GetPlane(GMPPlaneType aType) const {
  switch (aType) {
    case kGMPYPlane:
      return &mYPlane;
    case kGMPUPlane:
      return &mUPlane;
    case kGMPVPlane:
      return &mVPlane;
    default:
      MOZ_CRASH("Unknown plane type!");
  }
  return nullptr;
}

GMPPlaneImpl*
GMPVideoi420FrameImpl::GetPlane(GMPPlaneType aType) {
  switch (aType) {
    case kGMPYPlane :
      return &mYPlane;
    case kGMPUPlane :
      return &mUPlane;
    case kGMPVPlane :
      return &mVPlane;
    default:
      MOZ_CRASH("Unknown plane type!");
  }
  return nullptr;
}

GMPVideoErr
GMPVideoi420FrameImpl::CreateEmptyFrame(int32_t aWidth, int32_t aHeight,
                                        int32_t aStride_y, int32_t aStride_u, int32_t aStride_v)
{
  if (!CheckDimensions(aWidth, aHeight, aStride_y, aStride_u, aStride_v)) {
    return GMPVideoGenericErr;
  }

  int32_t size_y = aStride_y * aHeight;
  int32_t half_height = (aHeight + 1) / 2;
  int32_t size_u = aStride_u * half_height;
  int32_t size_v = aStride_v * half_height;

  GMPVideoErr err = mYPlane.CreateEmptyPlane(size_y, aStride_y, size_y);
  if (err != GMPVideoNoErr) {
    return err;
  }
  err = mUPlane.CreateEmptyPlane(size_u, aStride_u, size_u);
  if (err != GMPVideoNoErr) {
    return err;
  }
  err = mVPlane.CreateEmptyPlane(size_v, aStride_v, size_v);
  if (err != GMPVideoNoErr) {
    return err;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mTimestamp = 0;
  mRenderTime_ms = 0;

  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoi420FrameImpl::CreateFrame(int32_t aSize_y, const uint8_t* aBuffer_y,
                                   int32_t aSize_u, const uint8_t* aBuffer_u,
                                   int32_t aSize_v, const uint8_t* aBuffer_v,
                                   int32_t aWidth, int32_t aHeight,
                                   int32_t aStride_y, int32_t aStride_u, int32_t aStride_v)
{
  MOZ_ASSERT(aBuffer_y);
  MOZ_ASSERT(aBuffer_u);
  MOZ_ASSERT(aBuffer_v);

  if (aSize_y < 1 || aSize_u < 1 || aSize_v < 1) {
    return GMPVideoGenericErr;
  }

  if (!CheckDimensions(aWidth, aHeight, aStride_y, aStride_u, aStride_v)) {
    return GMPVideoGenericErr;
  }

  GMPVideoErr err = mYPlane.Copy(aSize_y, aStride_y, aBuffer_y);
  if (err != GMPVideoNoErr) {
    return err;
  }
  err = mUPlane.Copy(aSize_u, aStride_u, aBuffer_u);
  if (err != GMPVideoNoErr) {
    return err;
  }
  err = mVPlane.Copy(aSize_v, aStride_v, aBuffer_v);
  if (err != GMPVideoNoErr) {
    return err;
  }

  mWidth = aWidth;
  mHeight = aHeight;

  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoi420FrameImpl::CopyFrame(const GMPVideoi420Frame& aFrame)
{
  auto& f = static_cast<const GMPVideoi420FrameImpl&>(aFrame);

  GMPVideoErr err = mYPlane.Copy(f.mYPlane);
  if (err != GMPVideoNoErr) {
    return err;
  }

  err = mUPlane.Copy(f.mUPlane);
  if (err != GMPVideoNoErr) {
    return err;
  }

  err = mVPlane.Copy(f.mVPlane);
  if (err != GMPVideoNoErr) {
    return err;
  }

  mWidth = f.mWidth;
  mHeight = f.mHeight;
  mTimestamp = f.mTimestamp;
  mRenderTime_ms = f.mRenderTime_ms;

  return GMPVideoNoErr;
}

void
GMPVideoi420FrameImpl::SwapFrame(GMPVideoi420Frame* aFrame)
{
  auto f = static_cast<GMPVideoi420FrameImpl*>(aFrame);
  mYPlane.Swap(f->mYPlane);
  mUPlane.Swap(f->mUPlane);
  mVPlane.Swap(f->mVPlane);
  std::swap(mWidth, f->mWidth);
  std::swap(mHeight, f->mHeight);
  std::swap(mTimestamp, f->mTimestamp);
  std::swap(mRenderTime_ms, f->mRenderTime_ms);
}

uint8_t*
GMPVideoi420FrameImpl::Buffer(GMPPlaneType aType)
{
  GMPPlane* p = GetPlane(aType);
  if (p) {
    return p->Buffer();
  }
  return nullptr;
}

const uint8_t*
GMPVideoi420FrameImpl::Buffer(GMPPlaneType aType) const
{
 const GMPPlane* p = GetPlane(aType);
  if (p) {
    return p->Buffer();
  }
  return nullptr;
}

int32_t
GMPVideoi420FrameImpl::AllocatedSize(GMPPlaneType aType) const
{
  const GMPPlane* p = GetPlane(aType);
  if (p) {
    return p->AllocatedSize();
  }
  return -1;
}

int32_t
GMPVideoi420FrameImpl::Stride(GMPPlaneType aType) const
{
  const GMPPlane* p = GetPlane(aType);
  if (p) {
    return p->Stride();
  }
  return -1;
}

GMPVideoErr
GMPVideoi420FrameImpl::SetWidth(int32_t aWidth)
{
  if (!CheckDimensions(aWidth, mHeight,
                       mYPlane.Stride(), mUPlane.Stride(),
                       mVPlane.Stride())) {
    return GMPVideoGenericErr;
  }
  mWidth = aWidth;
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoi420FrameImpl::SetHeight(int32_t aHeight)
{
  if (!CheckDimensions(mWidth, aHeight,
                       mYPlane.Stride(), mUPlane.Stride(),
                       mVPlane.Stride())) {
    return GMPVideoGenericErr;
  }
  mHeight = aHeight;
  return GMPVideoNoErr;
}

int32_t
GMPVideoi420FrameImpl::Width() const
{
  return mWidth;
}

int32_t
GMPVideoi420FrameImpl::Height() const
{
  return mHeight;
}

void
GMPVideoi420FrameImpl::SetTimestamp(uint32_t aTimestamp)
{
  mTimestamp = aTimestamp;
}

uint32_t
GMPVideoi420FrameImpl::Timestamp() const
{
  return mTimestamp;
}

void
GMPVideoi420FrameImpl::SetRenderTime_ms(int64_t aRenderTime_ms)
{
  mRenderTime_ms = aRenderTime_ms;
}

int64_t
GMPVideoi420FrameImpl::RenderTime_ms() const
{
  return mRenderTime_ms;
}

bool
GMPVideoi420FrameImpl::IsZeroSize() const
{
  return (mYPlane.IsZeroSize() && mUPlane.IsZeroSize() && mVPlane.IsZeroSize());
}

void
GMPVideoi420FrameImpl::ResetSize()
{
  mYPlane.ResetSize();
  mUPlane.ResetSize();
  mVPlane.ResetSize();
}

} // namespace gmp
} // namespace mozilla

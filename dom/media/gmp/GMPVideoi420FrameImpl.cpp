/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoi420FrameImpl.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/CheckedInt.h"

namespace mozilla {
namespace gmp {

GMPVideoi420FrameImpl::GMPVideoi420FrameImpl(GMPVideoHostImpl* aHost)
: mYPlane(aHost),
  mUPlane(aHost),
  mVPlane(aHost),
  mWidth(0),
  mHeight(0),
  mTimestamp(0ll),
  mDuration(0ll)
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
  mDuration(aFrameData.mDuration())
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
  aFrameData.mDuration() = mDuration;
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

/* static */ bool
GMPVideoi420FrameImpl::CheckFrameData(const GMPVideoi420FrameData& aFrameData)
{
  // We may be passed the "wrong" shmem (one smaller than the actual size).
  // This implies a bug or serious error on the child size.  Ignore this frame if so.
  // Note: Size() greater than expected is also an error, but with no negative consequences
  int32_t half_width = (aFrameData.mWidth() + 1) / 2;
  if ((aFrameData.mYPlane().mStride() <= 0) || (aFrameData.mYPlane().mSize() <= 0) ||
      (aFrameData.mUPlane().mStride() <= 0) || (aFrameData.mUPlane().mSize() <= 0) ||
      (aFrameData.mVPlane().mStride() <= 0) || (aFrameData.mVPlane().mSize() <= 0) ||
      (aFrameData.mYPlane().mSize() > (int32_t) aFrameData.mYPlane().mBuffer().Size<uint8_t>()) ||
      (aFrameData.mUPlane().mSize() > (int32_t) aFrameData.mUPlane().mBuffer().Size<uint8_t>()) ||
      (aFrameData.mVPlane().mSize() > (int32_t) aFrameData.mVPlane().mBuffer().Size<uint8_t>()) ||
      (aFrameData.mYPlane().mStride() < aFrameData.mWidth()) ||
      (aFrameData.mUPlane().mStride() < half_width) ||
      (aFrameData.mVPlane().mStride() < half_width) ||
      (aFrameData.mYPlane().mSize() < aFrameData.mYPlane().mStride() * aFrameData.mHeight()) ||
      (aFrameData.mUPlane().mSize() < aFrameData.mUPlane().mStride() * ((aFrameData.mHeight()+1)/2)) ||
      (aFrameData.mVPlane().mSize() < aFrameData.mVPlane().mStride() * ((aFrameData.mHeight()+1)/2)))
  {
    return false;
  }
  return true;
}

bool
GMPVideoi420FrameImpl::CheckDimensions(int32_t aWidth, int32_t aHeight,
                                       int32_t aStride_y, int32_t aStride_u, int32_t aStride_v)
{
  int32_t half_width = (aWidth + 1) / 2;
  if (aWidth < 1 || aHeight < 1 ||
      aStride_y < aWidth || aStride_u < half_width || aStride_v < half_width ||
      !(CheckedInt<int32_t>(aHeight) * aStride_y
        + ((CheckedInt<int32_t>(aHeight) + 1) / 2)
          * (CheckedInt<int32_t>(aStride_u) + aStride_v)).isValid()) {
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

GMPErr
GMPVideoi420FrameImpl::CreateEmptyFrame(int32_t aWidth, int32_t aHeight,
                                        int32_t aStride_y, int32_t aStride_u, int32_t aStride_v)
{
  if (!CheckDimensions(aWidth, aHeight, aStride_y, aStride_u, aStride_v)) {
    return GMPGenericErr;
  }

  int32_t size_y = aStride_y * aHeight;
  int32_t half_height = (aHeight + 1) / 2;
  int32_t size_u = aStride_u * half_height;
  int32_t size_v = aStride_v * half_height;

  GMPErr err = mYPlane.CreateEmptyPlane(size_y, aStride_y, size_y);
  if (err != GMPNoErr) {
    return err;
  }
  err = mUPlane.CreateEmptyPlane(size_u, aStride_u, size_u);
  if (err != GMPNoErr) {
    return err;
  }
  err = mVPlane.CreateEmptyPlane(size_v, aStride_v, size_v);
  if (err != GMPNoErr) {
    return err;
  }

  mWidth = aWidth;
  mHeight = aHeight;
  mTimestamp = 0ll;
  mDuration = 0ll;

  return GMPNoErr;
}

GMPErr
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
    return GMPGenericErr;
  }

  if (!CheckDimensions(aWidth, aHeight, aStride_y, aStride_u, aStride_v)) {
    return GMPGenericErr;
  }

  GMPErr err = mYPlane.Copy(aSize_y, aStride_y, aBuffer_y);
  if (err != GMPNoErr) {
    return err;
  }
  err = mUPlane.Copy(aSize_u, aStride_u, aBuffer_u);
  if (err != GMPNoErr) {
    return err;
  }
  err = mVPlane.Copy(aSize_v, aStride_v, aBuffer_v);
  if (err != GMPNoErr) {
    return err;
  }

  mWidth = aWidth;
  mHeight = aHeight;

  return GMPNoErr;
}

GMPErr
GMPVideoi420FrameImpl::CopyFrame(const GMPVideoi420Frame& aFrame)
{
  auto& f = static_cast<const GMPVideoi420FrameImpl&>(aFrame);

  GMPErr err = mYPlane.Copy(f.mYPlane);
  if (err != GMPNoErr) {
    return err;
  }

  err = mUPlane.Copy(f.mUPlane);
  if (err != GMPNoErr) {
    return err;
  }

  err = mVPlane.Copy(f.mVPlane);
  if (err != GMPNoErr) {
    return err;
  }

  mWidth = f.mWidth;
  mHeight = f.mHeight;
  mTimestamp = f.mTimestamp;
  mDuration = f.mDuration;

  return GMPNoErr;
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
  std::swap(mDuration, f->mDuration);
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

GMPErr
GMPVideoi420FrameImpl::SetWidth(int32_t aWidth)
{
  if (!CheckDimensions(aWidth, mHeight,
                       mYPlane.Stride(), mUPlane.Stride(),
                       mVPlane.Stride())) {
    return GMPGenericErr;
  }
  mWidth = aWidth;
  return GMPNoErr;
}

GMPErr
GMPVideoi420FrameImpl::SetHeight(int32_t aHeight)
{
  if (!CheckDimensions(mWidth, aHeight,
                       mYPlane.Stride(), mUPlane.Stride(),
                       mVPlane.Stride())) {
    return GMPGenericErr;
  }
  mHeight = aHeight;
  return GMPNoErr;
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
GMPVideoi420FrameImpl::SetTimestamp(uint64_t aTimestamp)
{
  mTimestamp = aTimestamp;
}

uint64_t
GMPVideoi420FrameImpl::Timestamp() const
{
  return mTimestamp;
}

void
GMPVideoi420FrameImpl::SetDuration(uint64_t aDuration)
{
  mDuration = aDuration;
}

uint64_t
GMPVideoi420FrameImpl::Duration() const
{
  return mDuration;
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoi420FrameImpl_h_
#define GMPVideoi420FrameImpl_h_

#include "gmp-video-frame-i420.h"
#include "mozilla/ipc/Shmem.h"
#include "GMPVideoPlaneImpl.h"
#include "mozilla/Maybe.h"

namespace mozilla::gmp {

class GMPVideoi420FrameData;

class GMPVideoi420FrameImpl : public GMPVideoi420Frame {
  friend struct IPC::ParamTraits<mozilla::gmp::GMPVideoi420FrameImpl>;

 public:
  explicit GMPVideoi420FrameImpl(GMPVideoHostImpl* aHost);
  GMPVideoi420FrameImpl(const GMPVideoi420FrameData& aFrameData,
                        GMPVideoHostImpl* aHost);
  virtual ~GMPVideoi420FrameImpl();

  static bool CheckFrameData(const GMPVideoi420FrameData& aFrameData);

  bool InitFrameData(GMPVideoi420FrameData& aFrameData);
  const GMPPlaneImpl* GetPlane(GMPPlaneType aType) const;
  GMPPlaneImpl* GetPlane(GMPPlaneType aType);

  // GMPVideoFrame
  GMPVideoFrameFormat GetFrameFormat() override;
  void Destroy() override;

  // GMPVideoi420Frame
  GMPErr CreateEmptyFrame(int32_t aWidth, int32_t aHeight, int32_t aStride_y,
                          int32_t aStride_u, int32_t aStride_v) override;
  GMPErr CreateFrame(int32_t aSize_y, const uint8_t* aBuffer_y, int32_t aSize_u,
                     const uint8_t* aBuffer_u, int32_t aSize_v,
                     const uint8_t* aBuffer_v, int32_t aWidth, int32_t aHeight,
                     int32_t aStride_y, int32_t aStride_u,
                     int32_t aStride_v) override;
  GMPErr CopyFrame(const GMPVideoi420Frame& aFrame) override;
  void SwapFrame(GMPVideoi420Frame* aFrame) override;
  uint8_t* Buffer(GMPPlaneType aType) override;
  const uint8_t* Buffer(GMPPlaneType aType) const override;
  int32_t AllocatedSize(GMPPlaneType aType) const override;
  int32_t Stride(GMPPlaneType aType) const override;
  GMPErr SetWidth(int32_t aWidth) override;
  GMPErr SetHeight(int32_t aHeight) override;
  int32_t Width() const override;
  int32_t Height() const override;
  void SetTimestamp(uint64_t aTimestamp) override;
  uint64_t Timestamp() const override;
  void SetUpdatedTimestamp(uint64_t aTimestamp) override;
  uint64_t UpdatedTimestamp() const override;
  void SetDuration(uint64_t aDuration) override;
  uint64_t Duration() const override;
  bool IsZeroSize() const override;
  void ResetSize() override;

 private:
  bool CheckDimensions(int32_t aWidth, int32_t aHeight, int32_t aStride_y,
                       int32_t aStride_u, int32_t aStride_v, int32_t aSize_y,
                       int32_t aSize_u, int32_t aSize_v);
  bool CheckDimensions(int32_t aWidth, int32_t aHeight, int32_t aStride_y,
                       int32_t aStride_u, int32_t aStride_v);

  GMPPlaneImpl mYPlane;
  GMPPlaneImpl mUPlane;
  GMPPlaneImpl mVPlane;
  int32_t mWidth;
  int32_t mHeight;
  uint64_t mTimestamp;
  Maybe<uint64_t> mUpdatedTimestamp;
  uint64_t mDuration;
};

}  // namespace mozilla::gmp

#endif  // GMPVideoi420FrameImpl_h_

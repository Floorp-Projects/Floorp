/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidevineVideoFrame_h_
#define WidevineVideoFrame_h_

#include "stddef.h"
#include "content_decryption_module.h"
#include "mozilla/Attributes.h"
#include <vector>

namespace mozilla {

class WidevineVideoFrame : public cdm::VideoFrame {
 public:
  WidevineVideoFrame();
  WidevineVideoFrame(WidevineVideoFrame&& other);
  ~WidevineVideoFrame();

  void SetFormat(cdm::VideoFormat aFormat) override;
  cdm::VideoFormat Format() const override;

  void SetSize(cdm::Size aSize) override;
  cdm::Size Size() const override;

  void SetFrameBuffer(cdm::Buffer* aFrameBuffer) override;
  cdm::Buffer* FrameBuffer() override;

  void SetPlaneOffset(cdm::VideoPlane aPlane, uint32_t aOffset) override;
  uint32_t PlaneOffset(cdm::VideoPlane aPlane) override;

  void SetStride(cdm::VideoPlane aPlane, uint32_t aStride) override;
  uint32_t Stride(cdm::VideoPlane aPlane) override;

  void SetTimestamp(int64_t aTimestamp) override;
  int64_t Timestamp() const override;

  [[nodiscard]] bool InitToBlack(int32_t aWidth, int32_t aHeight,
                                 int64_t aTimeStamp);

 protected:
  cdm::VideoFormat mFormat;
  cdm::Size mSize;
  cdm::Buffer* mBuffer;
  uint32_t mPlaneOffsets[cdm::VideoPlane::kMaxPlanes];
  uint32_t mPlaneStrides[cdm::VideoPlane::kMaxPlanes];
  int64_t mTimestamp;
};

}  // namespace mozilla

#endif

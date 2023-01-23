/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataEncoder.h"

namespace mozilla {

// Prepend SPS/PPS to keyframe in realtime mode, which expects AnnexB bitstream.
// Convert to AVCC otherwise.
template <>
bool WMFMediaDataEncoder<MediaDataEncoder::H264Config>::WriteFrameData(
    RefPtr<MediaRawData>& aDest, LockBuffer& aSrc, bool aIsKeyframe) {
  size_t prependLength = 0;
  RefPtr<MediaByteBuffer> avccHeader;
  if (aIsKeyframe && mConfigData) {
    if (mConfig.mUsage == Usage::Realtime) {
      prependLength = mConfigData->Length();
    } else {
      avccHeader = mConfigData;
    }
  }

  UniquePtr<MediaRawDataWriter> writer(aDest->CreateWriter());
  if (!writer->SetSize(prependLength + aSrc.Length())) {
    WMF_ENC_LOGE("fail to allocate output buffer");
    return false;
  }

  if (prependLength > 0) {
    PodCopy(writer->Data(), mConfigData->Elements(), prependLength);
  }
  PodCopy(writer->Data() + prependLength, aSrc.Data(), aSrc.Length());

  if (mConfig.mUsage != Usage::Realtime &&
      !AnnexB::ConvertSampleToAVCC(aDest, avccHeader)) {
    WMF_ENC_LOGE("fail to convert annex-b sample to AVCC");
    return false;
  }

  return true;
}

}  // namespace mozilla

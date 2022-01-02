/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DummyMediaDataDecoder.h"
#include "AnnexB.h"
#include "H264.h"
#include "MP4Decoder.h"

namespace mozilla {

DummyDataCreator::~DummyDataCreator() = default;

DummyMediaDataDecoder::DummyMediaDataDecoder(
    UniquePtr<DummyDataCreator>&& aCreator, const nsACString& aDescription,
    const CreateDecoderParams& aParams)
    : mCreator(std::move(aCreator)),
      mIsH264(MP4Decoder::IsH264(aParams.mConfig.mMimeType)),
      mMaxRefFrames(mIsH264 ? H264::HasSPS(aParams.VideoConfig().mExtraData)
                                  ? H264::ComputeMaxRefFrames(
                                        aParams.VideoConfig().mExtraData)
                                  : 16
                            : 0),
      mType(aParams.mConfig.GetType()),
      mDescription(aDescription) {}

RefPtr<MediaDataDecoder::InitPromise> DummyMediaDataDecoder::Init() {
  return InitPromise::CreateAndResolve(mType, __func__);
}

RefPtr<ShutdownPromise> DummyMediaDataDecoder::Shutdown() {
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> DummyMediaDataDecoder::Decode(
    MediaRawData* aSample) {
  RefPtr<MediaData> data = mCreator->Create(aSample);

  if (!data) {
    return DecodePromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // Frames come out in DTS order but we need to output them in PTS order.
  mReorderQueue.Push(std::move(data));

  if (mReorderQueue.Length() > mMaxRefFrames) {
    return DecodePromise::CreateAndResolve(DecodedData{mReorderQueue.Pop()},
                                           __func__);
  }
  return DecodePromise::CreateAndResolve(DecodedData(), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> DummyMediaDataDecoder::Drain() {
  DecodedData samples;
  while (!mReorderQueue.IsEmpty()) {
    samples.AppendElement(mReorderQueue.Pop());
  }
  return DecodePromise::CreateAndResolve(std::move(samples), __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> DummyMediaDataDecoder::Flush() {
  mReorderQueue.Clear();
  return FlushPromise::CreateAndResolve(true, __func__);
}

nsCString DummyMediaDataDecoder::GetDescriptionName() const {
  return "blank media data decoder"_ns;
}

MediaDataDecoder::ConversionRequired DummyMediaDataDecoder::NeedsConversion()
    const {
  return mIsH264 ? ConversionRequired::kNeedAVCC
                 : ConversionRequired::kNeedNone;
}

}  // namespace mozilla

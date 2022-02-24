/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFMediaDataEncoder_h_
#define WMFMediaDataEncoder_h_

#include "MFTEncoder.h"
#include "PlatformEncoderModule.h"
#include "TimeUnits.h"

namespace mozilla {

bool CanCreateWMFEncoder(MediaDataEncoder::CodecType aCodec);

template <typename ConfigType>
class WMFMediaDataEncoder final : public MediaDataEncoder {
 public:
  WMFMediaDataEncoder(const ConfigType& aConfig, RefPtr<TaskQueue> aTaskQueue);

  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(Rate aBitsPerSec) override;

  nsCString GetDescriptionName() const override;

 private:
  // Automatically lock/unlock IMFMediaBuffer.
  class LockBuffer final {
   public:
    explicit LockBuffer(RefPtr<IMFMediaBuffer>& aBuffer) : mBuffer(aBuffer) {
      mResult = mBuffer->Lock(&mBytes, &mCapacity, &mLength);
    }

    ~LockBuffer() {
      if (SUCCEEDED(mResult)) {
        mBuffer->Unlock();
      }
    }

    BYTE* Data() { return mBytes; }
    DWORD Capacity() { return mCapacity; }
    DWORD Length() { return mLength; }
    HRESULT Result() { return mResult; }

   private:
    RefPtr<IMFMediaBuffer> mBuffer;
    BYTE* mBytes;
    DWORD mCapacity;
    DWORD mLength;
    HRESULT mResult;
  };

  RefPtr<InitPromise> ProcessInit();
  HRESULT InitMFTEncoder(RefPtr<MFTEncoder>& aEncoder);

  RefPtr<EncodePromise> ProcessEncode(RefPtr<const VideoData>&& aSample);
  already_AddRefed<IMFSample> ConvertToNV12InputSample(
      RefPtr<const VideoData>&& aData);
  RefPtr<EncodePromise> ProcessOutputSamples(
      nsTArray<RefPtr<IMFSample>>& aSamples);
  already_AddRefed<MediaRawData> IMFSampleToMediaData(
      RefPtr<IMFSample>& aSample);
  bool WriteFrameData(RefPtr<MediaRawData>& aDest, LockBuffer& aSrc,
                      bool aIsKeyframe);

  void AssertOnTaskQueue() { MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn()); }

  const ConfigType mConfig;
  const RefPtr<TaskQueue> mTaskQueue;
  RefPtr<MFTEncoder> mEncoder;
  // SPS/PPS NALUs for realtime usage, avcC otherwise.
  RefPtr<MediaByteBuffer> mConfigData;
};

}  // namespace mozilla

#endif

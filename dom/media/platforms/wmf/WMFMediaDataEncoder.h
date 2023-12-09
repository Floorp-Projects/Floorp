/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFMediaDataEncoder_h_
#define WMFMediaDataEncoder_h_

#include "ImageContainer.h"
#include "MFTEncoder.h"
#include "PlatformEncoderModule.h"
#include "TimeUnits.h"
#include "WMFDataEncoderUtils.h"
#include "WMFUtils.h"

namespace mozilla {

class WMFMediaDataEncoder final : public MediaDataEncoder {
 public:
  WMFMediaDataEncoder(const EncoderConfig& aConfig,
                      const RefPtr<TaskQueue>& aTaskQueue)
      : mConfig(aConfig),
        mTaskQueue(aTaskQueue),
        mHardwareNotAllowed(aConfig.mHardwarePreference ==
                                HardwarePreference::RequireSoftware ||
                            aConfig.mHardwarePreference ==
                                HardwarePreference::None) {
    MOZ_ASSERT(mTaskQueue);
  }

  RefPtr<InitPromise> Init() override {
    return InvokeAsync(mTaskQueue, this, __func__,
                       &WMFMediaDataEncoder::ProcessInit);
  }
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override {
    MOZ_ASSERT(aSample);

    RefPtr<const VideoData> sample(aSample->As<const VideoData>());

    return InvokeAsync<RefPtr<const VideoData>>(
        mTaskQueue, this, __func__, &WMFMediaDataEncoder::ProcessEncode,
        std::move(sample));
  }
  RefPtr<EncodePromise> Drain() override {
    return InvokeAsync(
        mTaskQueue, __func__, [self = RefPtr<WMFMediaDataEncoder>(this)]() {
          nsTArray<RefPtr<IMFSample>> outputs;
          return SUCCEEDED(self->mEncoder->Drain(outputs))
                     ? self->ProcessOutputSamples(outputs)
                     : EncodePromise::CreateAndReject(
                           NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
        });
  }
  RefPtr<ShutdownPromise> Shutdown() override {
    return InvokeAsync(
        mTaskQueue, __func__, [self = RefPtr<WMFMediaDataEncoder>(this)]() {
          if (self->mEncoder) {
            self->mEncoder->Destroy();
            self->mEncoder = nullptr;
          }
          return ShutdownPromise::CreateAndResolve(true, __func__);
        });
  }
  RefPtr<GenericPromise> SetBitrate(uint32_t aBitsPerSec) override {
    return InvokeAsync(
        mTaskQueue, __func__,
        [self = RefPtr<WMFMediaDataEncoder>(this), aBitsPerSec]() {
          MOZ_ASSERT(self->mEncoder);
          return SUCCEEDED(self->mEncoder->SetBitrate(aBitsPerSec))
                     ? GenericPromise::CreateAndResolve(true, __func__)
                     : GenericPromise::CreateAndReject(
                           NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR, __func__);
        });
  }

  RefPtr<ReconfigurationPromise> Reconfigure(
      const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges)
      override {
    // General reconfiguration interface not implemented right now
    return MediaDataEncoder::ReconfigurationPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  };

  nsCString GetDescriptionName() const override {
    return MFTEncoder::GetFriendlyName(CodecToSubtype(mConfig.mCodec));
  }

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

  RefPtr<InitPromise> ProcessInit() {
    AssertOnTaskQueue();

    MOZ_ASSERT(!mEncoder,
               "Should not initialize encoder again without shutting down");

    if (!wmf::MediaFoundationInitializer::HasInitialized()) {
      return InitPromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("Can't create the MFT encoder.")),
          __func__);
    }

    RefPtr<MFTEncoder> encoder = new MFTEncoder(mHardwareNotAllowed);
    HRESULT hr;
    mscom::EnsureMTA([&]() { hr = InitMFTEncoder(encoder); });

    if (FAILED(hr)) {
      WMF_ENC_LOGE("init MFTEncoder: error = 0x%lX", hr);
      return InitPromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("Can't create the MFT encoder.")),
          __func__);
    }

    mEncoder = std::move(encoder);
    FillConfigData();
    return InitPromise::CreateAndResolve(TrackInfo::TrackType::kVideoTrack,
                                         __func__);
  }

  HRESULT InitMFTEncoder(RefPtr<MFTEncoder>& aEncoder) {
    HRESULT hr = aEncoder->Create(CodecToSubtype(mConfig.mCodec));
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    hr = SetMediaTypes(aEncoder, mConfig);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    hr = aEncoder->SetModes(mConfig.mBitrate);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    return S_OK;
  }

  void FillConfigData() {
    nsTArray<UINT8> header;
    NS_ENSURE_TRUE_VOID(SUCCEEDED(mEncoder->GetMPEGSequenceHeader(header)));

    mConfigData =
        header.Length() > 0
            ? ParseH264Parameters(header, mConfig.mUsage == Usage::Realtime)
            : nullptr;
  }

  RefPtr<EncodePromise> ProcessEncode(RefPtr<const VideoData>&& aSample) {
    AssertOnTaskQueue();
    MOZ_ASSERT(mEncoder);
    MOZ_ASSERT(aSample);

    RefPtr<IMFSample> nv12 = ConvertToNV12InputSample(std::move(aSample));
    if (!nv12 || FAILED(mEncoder->PushInput(std::move(nv12)))) {
      WMF_ENC_LOGE("failed to process input sample");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("Failed to process input.")),
          __func__);
    }

    nsTArray<RefPtr<IMFSample>> outputs;
    HRESULT hr = mEncoder->TakeOutput(outputs);
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      FillConfigData();
    } else if (FAILED(hr)) {
      WMF_ENC_LOGE("failed to process output");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("Failed to process output.")),
          __func__);
    }

    return ProcessOutputSamples(outputs);
  }

  already_AddRefed<IMFSample> ConvertToNV12InputSample(
      RefPtr<const VideoData>&& aData) {
    AssertOnTaskQueue();
    MOZ_ASSERT(mEncoder);

    const layers::PlanarYCbCrImage* image = aData->mImage->AsPlanarYCbCrImage();
    MOZ_ASSERT(image);
    const layers::PlanarYCbCrData* yuv = image->GetData();
    auto ySize = yuv->YDataSize();
    auto cbcrSize = yuv->CbCrDataSize();
    size_t yLength = yuv->mYStride * ySize.height;
    size_t length = yLength + (yuv->mCbCrStride * cbcrSize.height * 2);

    RefPtr<IMFSample> input;
    HRESULT hr = mEncoder->CreateInputSample(&input, length);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    RefPtr<IMFMediaBuffer> buffer;
    hr = input->GetBufferByIndex(0, getter_AddRefs(buffer));
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    hr = buffer->SetCurrentLength(length);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    LockBuffer lockBuffer(buffer);
    NS_ENSURE_TRUE(SUCCEEDED(lockBuffer.Result()), nullptr);

    bool ok = libyuv::I420ToNV12(
                  yuv->mYChannel, yuv->mYStride, yuv->mCbChannel,
                  yuv->mCbCrStride, yuv->mCrChannel, yuv->mCbCrStride,
                  lockBuffer.Data(), yuv->mYStride, lockBuffer.Data() + yLength,
                  yuv->mCbCrStride * 2, ySize.width, ySize.height) == 0;
    NS_ENSURE_TRUE(ok, nullptr);

    hr = input->SetSampleTime(UsecsToHNs(aData->mTime.ToMicroseconds()));
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    hr =
        input->SetSampleDuration(UsecsToHNs(aData->mDuration.ToMicroseconds()));
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    return input.forget();
  }

  RefPtr<EncodePromise> ProcessOutputSamples(
      nsTArray<RefPtr<IMFSample>>& aSamples) {
    EncodedData frames;
    for (auto sample : aSamples) {
      RefPtr<MediaRawData> frame = IMFSampleToMediaData(sample);
      if (frame) {
        frames.AppendElement(std::move(frame));
      } else {
        WMF_ENC_LOGE("failed to convert output frame");
      }
    }
    aSamples.Clear();
    return EncodePromise::CreateAndResolve(std::move(frames), __func__);
  }

  already_AddRefed<MediaRawData> IMFSampleToMediaData(
      RefPtr<IMFSample>& aSample) {
    AssertOnTaskQueue();
    MOZ_ASSERT(aSample);

    RefPtr<IMFMediaBuffer> buffer;
    HRESULT hr = aSample->GetBufferByIndex(0, getter_AddRefs(buffer));
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    LockBuffer lockBuffer(buffer);
    NS_ENSURE_TRUE(SUCCEEDED(lockBuffer.Result()), nullptr);

    LONGLONG time = 0;
    hr = aSample->GetSampleTime(&time);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    LONGLONG duration = 0;
    hr = aSample->GetSampleDuration(&duration);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    bool isKeyframe =
        MFGetAttributeUINT32(aSample, MFSampleExtension_CleanPoint, false);

    auto frame = MakeRefPtr<MediaRawData>();
    if (!WriteFrameData(frame, lockBuffer, isKeyframe)) {
      return nullptr;
    }

    frame->mTime = media::TimeUnit::FromMicroseconds(HNsToUsecs(time));
    frame->mDuration = media::TimeUnit::FromMicroseconds(HNsToUsecs(duration));
    frame->mKeyframe = isKeyframe;

    return frame.forget();
  }

  bool WriteFrameData(RefPtr<MediaRawData>& aDest, LockBuffer& aSrc,
                      bool aIsKeyframe) {
    if (mConfig.mCodec == CodecType::H264) {
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
    UniquePtr<MediaRawDataWriter> writer(aDest->CreateWriter());
    if (!writer->SetSize(aSrc.Length())) {
      WMF_ENC_LOGE("fail to allocate output buffer");
      return false;
    }

    PodCopy(writer->Data(), aSrc.Data(), aSrc.Length());
    return true;
  }

  void AssertOnTaskQueue() { MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn()); }

  EncoderConfig mConfig;
  const RefPtr<TaskQueue> mTaskQueue;
  const bool mHardwareNotAllowed;
  RefPtr<MFTEncoder> mEncoder;
  // SPS/PPS NALUs for realtime usage, avcC otherwise.
  RefPtr<MediaByteBuffer> mConfigData;
};

}  // namespace mozilla

#endif

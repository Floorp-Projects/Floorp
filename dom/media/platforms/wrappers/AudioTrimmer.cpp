/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioTrimmer.h"

#define LOG(arg, ...)                                                  \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, \
            ##__VA_ARGS__)

#define LOGV(arg, ...)                                                   \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Verbose, "::%s: " arg, __func__, \
            ##__VA_ARGS__)

namespace mozilla {

using media::TimeInterval;
using media::TimeUnit;

RefPtr<MediaDataDecoder::InitPromise> AudioTrimmer::Init() {
  mThread = GetCurrentSerialEventTarget();
  return mDecoder->Init();
}

RefPtr<MediaDataDecoder::DecodePromise> AudioTrimmer::Decode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread(),
             "We're not on the thread we were first initialized on");
  RefPtr<MediaRawData> sample = aSample;
  PrepareTrimmers(sample);
  RefPtr<AudioTrimmer> self = this;
  RefPtr<DecodePromise> p = mDecoder->Decode(sample)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self, sample](DecodePromise::ResolveOrRejectValue&& aValue) {
        return self->HandleDecodedResult(std::move(aValue), sample);
      });
  return p;
}

RefPtr<MediaDataDecoder::FlushPromise> AudioTrimmer::Flush() {
  MOZ_ASSERT(mThread->IsOnCurrentThread(),
             "We're not on the thread we were first initialized on");
  RefPtr<FlushPromise> p = mDecoder->Flush();
  mTrimmers.Clear();
  return p;
}

RefPtr<MediaDataDecoder::DecodePromise> AudioTrimmer::Drain() {
  MOZ_ASSERT(mThread->IsOnCurrentThread(),
             "We're not on the thread we were first initialized on");
  LOG("Draining");
  RefPtr<DecodePromise> p = mDecoder->Drain()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}](DecodePromise::ResolveOrRejectValue&& aValue) {
        return self->HandleDecodedResult(std::move(aValue), nullptr);
      });
  return p;
}

RefPtr<ShutdownPromise> AudioTrimmer::Shutdown() {
  // mThread may not be set if Init hasn't been called first.
  MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());
  return mDecoder->Shutdown();
}

nsCString AudioTrimmer::GetDescriptionName() const {
  return mDecoder->GetDescriptionName();
}

nsCString AudioTrimmer::GetProcessName() const {
  return mDecoder->GetProcessName();
}

nsCString AudioTrimmer::GetCodecName() const {
  return mDecoder->GetCodecName();
}

bool AudioTrimmer::IsHardwareAccelerated(nsACString& aFailureReason) const {
  return mDecoder->IsHardwareAccelerated(aFailureReason);
}

void AudioTrimmer::SetSeekThreshold(const media::TimeUnit& aTime) {
  mDecoder->SetSeekThreshold(aTime);
}

bool AudioTrimmer::SupportDecoderRecycling() const {
  return mDecoder->SupportDecoderRecycling();
}

MediaDataDecoder::ConversionRequired AudioTrimmer::NeedsConversion() const {
  return mDecoder->NeedsConversion();
}

RefPtr<MediaDataDecoder::DecodePromise> AudioTrimmer::HandleDecodedResult(
    DecodePromise::ResolveOrRejectValue&& aValue, MediaRawData* aRaw) {
  MOZ_ASSERT(mThread->IsOnCurrentThread(),
             "We're not on the thread we were first initialized on");
  if (aValue.IsReject()) {
    return DecodePromise::CreateAndReject(std::move(aValue.RejectValue()),
                                          __func__);
  }
  TimeUnit rawStart = aRaw ? aRaw->mTime : TimeUnit::Zero();
  TimeUnit rawEnd = aRaw ? aRaw->GetEndTime() : TimeUnit::Zero();
  MediaDataDecoder::DecodedData results = std::move(aValue.ResolveValue());
  if (results.IsEmpty()) {
    // No samples returned, we assume this is due to the latency of the
    // decoder and that the related decoded sample will be returned during
    // the next call to Decode().
    LOGV("No sample returned for sample[%s, %s]", rawStart.ToString().get(),
         rawEnd.ToString().get());
  }
  for (uint32_t i = 0; i < results.Length();) {
    const RefPtr<MediaData>& data = results[i];
    MOZ_ASSERT(data->mType == MediaData::Type::AUDIO_DATA);
    TimeInterval sampleInterval(data->mTime, data->GetEndTime());
    if (mTrimmers.IsEmpty()) {
      // mTrimmers being empty can only occurs if the decoder returned more
      // frames than we pushed in. We can't handle this case, abort trimming.
      LOG("sample[%s, %s] (decoded[%s, %s] no trimming information)",
          rawStart.ToString().get(), rawEnd.ToString().get(),
          sampleInterval.mStart.ToString().get(),
          sampleInterval.mEnd.ToString().get());
      i++;
      continue;
    }

    Maybe<TimeInterval> trimmer = mTrimmers[0];
    mTrimmers.RemoveElementAt(0);
    if (!trimmer) {
      // Those frames didn't need trimming.
      LOGV("sample[%s, %s] (decoded[%s, %s] no trimming needed",
           rawStart.ToString().get(), rawEnd.ToString().get(),
           sampleInterval.mStart.ToString().get(),
           sampleInterval.mEnd.ToString().get());
      i++;
      continue;
    }
    if (!trimmer->Intersects(sampleInterval)) {
      LOGV(
          "sample[%s, %s] (decoded[%s, %s] would be empty after trimming, "
          "dropping it",
          rawStart.ToString().get(), rawEnd.ToString().get(),
          sampleInterval.mStart.ToString().get(),
          sampleInterval.mEnd.ToString().get());
      results.RemoveElementAt(i);
      continue;
    }
    LOGV("Trimming sample[%s,%s] to [%s,%s] (raw was:[%s, %s])",
         sampleInterval.mStart.ToString().get(),
         sampleInterval.mEnd.ToString().get(), trimmer->mStart.ToString().get(),
         trimmer->mEnd.ToString().get(), rawStart.ToString().get(),
         rawEnd.ToString().get());

    TimeInterval trim({std::max(trimmer->mStart, sampleInterval.mStart),
                       std::min(trimmer->mEnd, sampleInterval.mEnd)});
    AudioData* sample = static_cast<AudioData*>(data.get());
    bool ok = sample->SetTrimWindow(trim);
    NS_ASSERTION(ok, "Trimming of audio sample failed");
    Unused << ok;
    if (sample->Frames() == 0) {
      LOGV("sample[%s, %s] is empty after trimming, dropping it",
           rawStart.ToString().get(), rawEnd.ToString().get());
      results.RemoveElementAt(i);
      continue;
    }
    i++;
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> AudioTrimmer::DecodeBatch(
    nsTArray<RefPtr<MediaRawData>>&& aSamples) {
  MOZ_ASSERT(mThread->IsOnCurrentThread(),
             "We're not on the thread we were first initialized on");
  LOGV("DecodeBatch");

  for (auto&& sample : aSamples) {
    PrepareTrimmers(sample);
  }
  RefPtr<DecodePromise> p =
      mDecoder->DecodeBatch(std::move(aSamples))
          ->Then(GetCurrentSerialEventTarget(), __func__,
                 [self = RefPtr{this}](
                     DecodePromise::ResolveOrRejectValue&& aValue) {
                   // If the decoder returned less samples than what we fed it.
                   // We can assume that this is due to the decoder encoding
                   // delay and that all decoded frames have been shifted by n =
                   // compressedSamples.Length() - decodedSamples.Length() and
                   // that the first n compressed samples returned nothing.
                   return self->HandleDecodedResult(std::move(aValue), nullptr);
                 });
  return p;
}

void AudioTrimmer::PrepareTrimmers(MediaRawData* aRaw) {
  // A compress sample indicates that it needs to be trimmed after decoding by
  // having its mOriginalPresentationWindow member set; in which case
  // mOriginalPresentationWindow contains the original time and duration of
  // the frame set by the demuxer and mTime and mDuration set to what it
  // should be after trimming.
  if (aRaw->mOriginalPresentationWindow) {
    LOGV("sample[%s, %s] has trimming info ([%s, %s]",
         aRaw->mOriginalPresentationWindow->mStart.ToString().get(),
         aRaw->mOriginalPresentationWindow->mEnd.ToString().get(),
         aRaw->mTime.ToString().get(), aRaw->GetEndTime().ToString().get());
    mTrimmers.AppendElement(
        Some(TimeInterval(aRaw->mTime, aRaw->GetEndTime())));
    aRaw->mTime = aRaw->mOriginalPresentationWindow->mStart;
    aRaw->mDuration = aRaw->mOriginalPresentationWindow->Length();
  } else {
    LOGV("sample[%s,%s] no trimming information", aRaw->mTime.ToString().get(),
         aRaw->GetEndTime().ToString().get());
    mTrimmers.AppendElement(Nothing());
  }
}

}  // namespace mozilla

#undef LOG

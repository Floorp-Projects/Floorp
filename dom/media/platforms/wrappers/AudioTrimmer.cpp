/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioTrimmer.h"

#define LOG(arg, ...)                                                  \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, \
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
  // A compress sample indicates that it needs to be trimmed after decoding by
  // having its mOriginalPresentationWindow member set; in which case
  // mOriginalPresentationWindow contains the original time and duration of the
  // frame set by the demuxer and mTime and mDuration set to what it should be
  // after trimming.
  RefPtr<MediaRawData> sample = aSample;
  if (sample->mOriginalPresentationWindow) {
    LOG("sample[%" PRId64 ",%" PRId64 "] has trimming info ([%" PRId64
        ",%" PRId64 "]",
        sample->mOriginalPresentationWindow->mStart.ToMicroseconds(),
        sample->mOriginalPresentationWindow->mEnd.ToMicroseconds(),
        sample->mTime.ToMicroseconds(), sample->GetEndTime().ToMicroseconds());
    mTrimmers.AppendElement(
        Some(TimeInterval(sample->mTime, sample->GetEndTime())));
    sample->mTime = sample->mOriginalPresentationWindow->mStart;
    sample->mDuration = sample->mOriginalPresentationWindow->Length();
  } else {
    LOG("sample[%" PRId64 ",%" PRId64 "] no trimming information",
        sample->mTime.ToMicroseconds(), sample->GetEndTime().ToMicroseconds());
    mTrimmers.AppendElement(Nothing());
  }
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
        auto p = self->HandleDecodedResult(std::move(aValue), nullptr);
        return p;
      });
  return p;
}

RefPtr<ShutdownPromise> AudioTrimmer::Shutdown() {
  MOZ_ASSERT(mThread->IsOnCurrentThread(),
             "We're not on the thread we were first initialized on");
  return mDecoder->Shutdown();
}

nsCString AudioTrimmer::GetDescriptionName() const {
  return mDecoder->GetDescriptionName();
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
  int64_t rawStart = aRaw ? aRaw->mTime.ToMicroseconds() : 0;
  int64_t rawEnd = aRaw ? aRaw->GetEndTime().ToMicroseconds() : 0;
  MediaDataDecoder::DecodedData results = std::move(aValue.ResolveValue());
  if (results.IsEmpty()) {
    // No samples returned, we assume this is due to the latency of the
    // decoder and that the related decoded sample will be returned during
    // the next call to Decode().
    LOG("No sample returned for sample[%" PRId64 ",%" PRId64 "]", rawStart,
        rawEnd);
  }
  for (uint32_t i = 0; i < results.Length();) {
    const RefPtr<MediaData>& data = results[i];
    MOZ_ASSERT(data->mType == MediaData::Type::AUDIO_DATA);
    TimeInterval sampleInterval(data->mTime, data->GetEndTime());
    if (mTrimmers.IsEmpty()) {
      // mTrimmers being empty can only occurs if the decoder returned more
      // frames than we pushed in. We can't handle this case, abort trimming.
      LOG("sample[%" PRId64 ",%" PRId64 "] (decoded[%" PRId64 ",%" PRId64
          "] no trimming information",
          rawStart, rawEnd, sampleInterval.mStart.ToMicroseconds(),
          sampleInterval.mEnd.ToMicroseconds());
      i++;
      continue;
    }

    Maybe<TimeInterval> trimmer = mTrimmers[0];
    mTrimmers.RemoveElementAt(0);
    if (!trimmer) {
      // Those frames didn't need trimming.
      LOG("sample[%" PRId64 ",%" PRId64 "] (decoded[%" PRId64 ",%" PRId64
          "] no trimming needed",
          rawStart, rawEnd, sampleInterval.mStart.ToMicroseconds(),
          sampleInterval.mEnd.ToMicroseconds());
      i++;
      continue;
    }
    if (!trimmer->Intersects(sampleInterval)) {
      LOG("sample[%" PRId64 ",%" PRId64 "] (decoded[%" PRId64 ",%" PRId64
          "] would be empty after trimming, dropping it",
          rawStart, rawEnd, sampleInterval.mStart.ToMicroseconds(),
          sampleInterval.mEnd.ToMicroseconds());
      results.RemoveElementAt(i);
      continue;
    }
    LOG("Trimming sample[%" PRId64 ",%" PRId64 "] to [%" PRId64 ",%" PRId64
        "] (raw "
        "was:[%" PRId64 ",%" PRId64 "])",
        sampleInterval.mStart.ToMicroseconds(),
        sampleInterval.mEnd.ToMicroseconds(), trimmer->mStart.ToMicroseconds(),
        trimmer->mEnd.ToMicroseconds(), rawStart, rawEnd);

    TimeInterval trim({std::max(trimmer->mStart, sampleInterval.mStart),
                       std::min(trimmer->mEnd, sampleInterval.mEnd)});
    AudioData* sample = static_cast<AudioData*>(data.get());
    bool ok = sample->SetTrimWindow(trim);
    NS_ASSERTION(ok, "Trimming of audio sample failed");
    Unused << ok;
    if (sample->Frames() == 0) {
      LOG("sample[%" PRId64 ",%" PRId64
          "] is empty after trimming, dropping it",
          rawStart, rawEnd);
      results.RemoveElementAt(i);
      continue;
    }
    i++;
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

}  // namespace mozilla

#undef LOG

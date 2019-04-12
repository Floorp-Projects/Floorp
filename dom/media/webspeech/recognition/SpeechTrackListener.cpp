/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechTrackListener.h"

#include "SpeechRecognition.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

SpeechTrackListener::SpeechTrackListener(SpeechRecognition* aRecognition)
    : mRecognition(aRecognition),
      mRemovedPromise(
          mRemovedHolder.Ensure("SpeechTrackListener::mRemovedPromise")) {
  MOZ_ASSERT(NS_IsMainThread());
  mRemovedPromise->Then(GetCurrentThreadSerialEventTarget(), __func__,
                        [self = RefPtr<SpeechTrackListener>(this), this] {
                          mRecognition = nullptr;
                        });
}

void SpeechTrackListener::NotifyQueuedChanges(
    MediaStreamGraph* aGraph, StreamTime aTrackOffset,
    const MediaSegment& aQueuedMedia) {
  AudioSegment* audio = const_cast<AudioSegment*>(
      static_cast<const AudioSegment*>(&aQueuedMedia));

  AudioSegment::ChunkIterator iterator(*audio);
  while (!iterator.IsEnded()) {
    // Skip over-large chunks so we don't crash!
    if (iterator->GetDuration() > INT_MAX) {
      continue;
    }
    int duration = int(iterator->GetDuration());

    if (iterator->IsNull()) {
      nsTArray<int16_t> nullData;
      PodZero(nullData.AppendElements(duration), duration);
      ConvertAndDispatchAudioChunk(duration, iterator->mVolume,
                                   nullData.Elements(), aGraph->GraphRate());
    } else {
      AudioSampleFormat format = iterator->mBufferFormat;

      MOZ_ASSERT(format == AUDIO_FORMAT_S16 || format == AUDIO_FORMAT_FLOAT32);

      if (format == AUDIO_FORMAT_S16) {
        ConvertAndDispatchAudioChunk(
            duration, iterator->mVolume,
            static_cast<const int16_t*>(iterator->mChannelData[0]),
            aGraph->GraphRate());
      } else if (format == AUDIO_FORMAT_FLOAT32) {
        ConvertAndDispatchAudioChunk(
            duration, iterator->mVolume,
            static_cast<const float*>(iterator->mChannelData[0]),
            aGraph->GraphRate());
      }
    }

    iterator.Next();
  }
}

template <typename SampleFormatType>
void SpeechTrackListener::ConvertAndDispatchAudioChunk(int aDuration,
                                                       float aVolume,
                                                       SampleFormatType* aData,
                                                       TrackRate aTrackRate) {
  RefPtr<SharedBuffer> samples(SharedBuffer::Create(aDuration * 1 *  // channel
                                                    sizeof(int16_t)));

  int16_t* to = static_cast<int16_t*>(samples->Data());
  ConvertAudioSamplesWithScale(aData, to, aDuration, aVolume);

  mRecognition->FeedAudioData(samples.forget(), aDuration, this, aTrackRate);
}

void SpeechTrackListener::NotifyEnded() {
  // TODO dispatch SpeechEnd event so services can be informed
}

void SpeechTrackListener::NotifyRemoved() {
  mRemovedHolder.ResolveIfExists(true, __func__);
}

}  // namespace dom
}  // namespace mozilla

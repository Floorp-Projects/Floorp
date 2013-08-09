/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "MediaStreamGraph.h"
#include "AudioSegment.h"

namespace mozilla {

class AudioSegment;

namespace dom {

class SpeechRecognition;

class SpeechStreamListener : public MediaStreamListener
{
public:
  SpeechStreamListener(SpeechRecognition* aRecognition);
  ~SpeechStreamListener();

  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                TrackRate aTrackRate,
                                TrackTicks aTrackOffset,
                                uint32_t aTrackEvents,
                                const MediaSegment& aQueuedMedia) MOZ_OVERRIDE;

  void NotifyFinished(MediaStreamGraph* aGraph) MOZ_OVERRIDE;

private:
  template<typename SampleFormatType>
  void ConvertAndDispatchAudioChunk(int aDuration, float aVolume, SampleFormatType* aData);
  nsRefPtr<SpeechRecognition> mRecognition;
};

} // namespace dom
} // namespace mozilla

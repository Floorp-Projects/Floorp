/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechStreamListener_h
#define mozilla_dom_SpeechStreamListener_h

#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "AudioSegment.h"
#include "mozilla/MozPromise.h"

namespace mozilla {

class AudioSegment;

namespace dom {

class SpeechRecognition;

class SpeechTrackListener : public MediaTrackListener {
 public:
  explicit SpeechTrackListener(SpeechRecognition* aRecognition);
  ~SpeechTrackListener() = default;

  void NotifyQueuedChanges(MediaTrackGraph* aGraph, TrackTime aTrackOffset,
                           const MediaSegment& aQueuedMedia) override;

  void NotifyEnded(MediaTrackGraph* aGraph) override;

  void NotifyRemoved(MediaTrackGraph* aGraph) override;

 private:
  template <typename SampleFormatType>
  void ConvertAndDispatchAudioChunk(int aDuration, float aVolume,
                                    SampleFormatType* aData,
                                    TrackRate aTrackRate);
  nsMainThreadPtrHandle<SpeechRecognition> mRecognition;
  MozPromiseHolder<GenericNonExclusivePromise> mRemovedHolder;

 public:
  const RefPtr<GenericNonExclusivePromise> mRemovedPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODESTREAM_H_
#define MOZILLA_AUDIONODESTREAM_H_

#include "MediaStreamGraph.h"
#include "AudioChannelFormat.h"
#include "AudioNodeEngine.h"
#include "mozilla/dom/AudioParam.h"

#ifdef PR_LOGGING
#define LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {

namespace dom {
struct ThreeDPoint;
}

class ThreadSharedFloatArrayBufferList;

/**
 * An AudioNodeStream produces one audio track with ID AUDIO_TRACK.
 * The start time of the AudioTrack is aligned to the start time of the
 * AudioContext's destination node stream, plus some multiple of BLOCK_SIZE
 * samples.
 *
 * An AudioNodeStream has an AudioNodeEngine plugged into it that does the
 * actual audio processing. AudioNodeStream contains the glue code that
 * integrates audio processing with the MediaStreamGraph.
 */
class AudioNodeStream : public ProcessedMediaStream {
public:
  enum { AUDIO_TRACK = 1 };

  /**
   * Transfers ownership of aEngine to the new AudioNodeStream.
   */
  AudioNodeStream(AudioNodeEngine* aEngine,
                  MediaStreamGraph::AudioNodeStreamKind aKind)
    : ProcessedMediaStream(nullptr),
      mEngine(aEngine),
      mKind(aKind)
  {
    // AudioNodes are always producing data
    mHasCurrentData = true;
    MOZ_COUNT_CTOR(AudioNodeStream);
  }
  ~AudioNodeStream();

  // Control API
  /**
   * Sets a parameter that's a time relative to some stream's played time.
   * This time is converted to a time relative to this stream when it's set.
   */
  void SetStreamTimeParameter(uint32_t aIndex, MediaStream* aRelativeToStream,
                              double aStreamTime);
  void SetDoubleParameter(uint32_t aIndex, double aValue);
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue);
  void SetTimelineParameter(uint32_t aIndex, const dom::AudioParamTimeline& aValue);
  void SetThreeDPointParameter(uint32_t aIndex, const dom::ThreeDPoint& aValue);
  void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer);

  virtual AudioNodeStream* AsAudioNodeStream() { return this; }

  // Graph thread only
  void SetStreamTimeParameterImpl(uint32_t aIndex, MediaStream* aRelativeToStream,
                                  double aStreamTime);
  virtual void ProduceOutput(GraphTime aFrom, GraphTime aTo);
  TrackTicks GetCurrentPosition();
  bool AllInputsFinished() const;

  // Any thread
  AudioNodeEngine* Engine() { return mEngine; }

protected:
  void FinishOutput();

  StreamBuffer::Track* EnsureTrack();
  AudioChunk* ObtainInputBlock(AudioChunk* aTmpChunk);

  // The engine that will generate output for this node.
  nsAutoPtr<AudioNodeEngine> mEngine;
  // The last block produced by this node.
  AudioChunk mLastChunk;
  // Whether this is an internal or external stream
  MediaStreamGraph::AudioNodeStreamKind mKind;
};

}

#endif /* MOZILLA_AUDIONODESTREAM_H_ */

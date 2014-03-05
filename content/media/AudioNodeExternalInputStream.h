/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIONODEEXTERNALINPUTSTREAM_H_
#define MOZILLA_AUDIONODEEXTERNALINPUTSTREAM_H_

#include "MediaStreamGraph.h"
#include "AudioNodeStream.h"

// Forward declaration for mResamplerMap
typedef struct SpeexResamplerState_ SpeexResamplerState;

namespace mozilla {

/**
 * This is a MediaStream implementation that acts for a Web Audio node but
 * unlike other AudioNodeStreams, supports any kind of MediaStream as an
 * input --- handling any number of audio tracks, resampling them from whatever
 * sample rate they're using, and handling blocking of the input MediaStream.
 */
class AudioNodeExternalInputStream : public AudioNodeStream {
public:
  AudioNodeExternalInputStream(AudioNodeEngine* aEngine, TrackRate aSampleRate);
  ~AudioNodeExternalInputStream();

  virtual void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) MOZ_OVERRIDE;

private:
  // For storing pointers and data about input tracks, like the last TrackTick which
  // was read, and the associated speex resampler.
  struct TrackMapEntry {
    ~TrackMapEntry();

    /**
     * Resamples data from all chunks in aIterator and following, using mResampler,
     * adding the results to mResampledData.
     */
    void ResampleInputData(AudioSegment* aSegment);
    /**
     * Resamples a set of channel buffers using mResampler, adding the results
     * to mResampledData.
     */
    void ResampleChannels(const nsTArray<const void*>& aBuffers,
                          uint32_t aInputDuration,
                          AudioSampleFormat aFormat,
                          float aVolume);

    // mEndOfConsumedInputTicks is the end of the input ticks that we've consumed.
    // 0 if we haven't consumed any yet.
    TrackTicks mEndOfConsumedInputTicks;
    // mEndOfLastInputIntervalInInputStream is the timestamp for the end of the
    // previous interval which was unblocked for both the input and output
    // stream, in the input stream's timeline, or -1 if there wasn't one.
    StreamTime mEndOfLastInputIntervalInInputStream;
    // mEndOfLastInputIntervalInOutputStream is the timestamp for the end of the
    // previous interval which was unblocked for both the input and output
    // stream, in the output stream's timeline, or -1 if there wasn't one.
    StreamTime mEndOfLastInputIntervalInOutputStream;
    /**
     * Number of samples passed to the resampler so far.
     */
    TrackTicks mSamplesPassedToResampler;
    /**
     * Resampler being applied to this track.
     */
    SpeexResamplerState* mResampler;
    /**
     * The track data that has been resampled to the rate of the
     * AudioNodeExternalInputStream. All data in these chunks is in floats (or null),
     * and has the number of channels given in mResamplerChannelCount.
     * mResampledData starts at zero in the stream's output track (so generally
     * it will consist of null data followed by actual data).
     */
    AudioSegment mResampledData;
    /**
     * Number of channels used to create mResampler.
     */
    uint32_t mResamplerChannelCount;
    /**
     * The ID for the track of the input stream this entry is for.
     */
    TrackID mTrackID;
  };

  nsTArray<TrackMapEntry> mTrackMap;
  // Amount of track data produced so far. A multiple of WEBAUDIO_BLOCK_SIZE.
  TrackTicks mCurrentOutputPosition;

  /**
   * Creates a TrackMapEntry for the track, if needed. Returns the index
   * of the TrackMapEntry or NoIndex if no entry is needed yet.
   */
  uint32_t GetTrackMapEntry(const StreamBuffer::Track& aTrack,
                            GraphTime aFrom);
};

}

#endif /* MOZILLA_AUDIONODESTREAM_H_ */

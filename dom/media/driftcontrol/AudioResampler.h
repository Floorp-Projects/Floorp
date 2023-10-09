/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_AUDIORESAMPLER_H_
#define DOM_MEDIA_DRIFTCONTROL_AUDIORESAMPLER_H_

#include "AudioChunkList.h"
#include "AudioSegment.h"
#include "DynamicResampler.h"
#include "TimeUnits.h"

namespace mozilla {

/**
 * Audio Resampler is a resampler able to change the output rate and channels
 * count on the fly. The API is simple and it is based in AudioSegment in order
 * to be used MTG. Memory allocations, for input and output buffers, will happen
 * in the constructor, when channel count changes and if the amount of input
 * data outgrows the input buffer. The memory is recycled in order to avoid
 * reallocations. It also supports prebuffering of silence. It consists of
 * DynamicResampler and AudioChunkList so please read their documentation if you
 * are interested in more details.
 *
 * The output buffer is preallocated  and returned in the form of AudioSegment.
 * The intention is to be used directly in a MediaTrack. Since an AudioChunk
 * must no be "shared" in order to be written, the AudioSegment returned by
 * resampler method must be cleaned up in order to be able for the `AudioChunk`s
 * that it consists of to be reused. For `MediaTrack::mSegment` this happens
 * every ~50ms (look at MediaTrack::AdvanceTimeVaryingValuesToCurrentTime). Thus
 * memory capacity of 100ms has been preallocated for internal input and output
 * buffering. Note that the amount of memory used for input buffering may
 * increase if needed.
 */
class AudioResampler final {
 public:
  AudioResampler(uint32_t aInRate, uint32_t aOutRate,
                 media::TimeUnit aPreBufferDuration,
                 const PrincipalHandle& aPrincipalHandle);

  /**
   * Append input data into the resampler internal buffer. Copy/move of the
   * memory is taking place. Also, the channel count will change according to
   * the channel count of the chunks.
   */
  void AppendInput(const AudioSegment& aInSegment);
  /**
   * Get the number of frames that the internal input buffer can hold.
   */
  uint32_t InputCapacityFrames() const;
  /**
   * Get the number of frames that can be read from the internal input buffer
   * before it becomes empty.
   */
  uint32_t InputReadableFrames() const;

  /*
   * Reguest `aOutFrames` of audio in the output sample rate. The internal
   * buffered input is used. If the input buffer does not have enough data to
   * reach `aOutFrames` frames, the input buffer is padded with enough silence
   * to allow the requested frames to be resampled and returned, and the
   * pre-buffer is reset so that the next call will be treated as the first.
   *
   * On first call, prepends the internal buffer with silence so that after
   * resampling aOutFrames frames of data, the internal buffer holds input
   * data as close as possible to the configured pre-buffer size.
   */
  AudioSegment Resample(uint32_t aOutFrames, bool* aHasUnderrun);

  /*
   * Updates the output rate that will be used by the resampler.
   */
  void UpdateOutRate(uint32_t aOutRate) {
    Update(aOutRate, mResampler.GetChannels());
  }

  /**
   * Set the duration that should be used for pre-buffering.
   */
  void SetPreBufferDuration(media::TimeUnit aPreBufferDuration) {
    mResampler.SetPreBufferDuration(aPreBufferDuration);
  }

 private:
  void UpdateChannels(uint32_t aChannels) {
    Update(mResampler.GetOutRate(), aChannels);
  }
  void Update(uint32_t aOutRate, uint32_t aChannels);

 private:
  DynamicResampler mResampler;
  AudioChunkList mOutputChunks;
  bool mIsSampleFormatSet = false;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_DRIFTCONTROL_AUDIORESAMPLER_H_

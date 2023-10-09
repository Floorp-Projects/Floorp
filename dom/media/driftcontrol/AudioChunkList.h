/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_AUDIOCHUNKLIST_H_
#define DOM_MEDIA_DRIFTCONTROL_AUDIOCHUNKLIST_H_

#include "AudioSegment.h"
#include "TimeUnits.h"

namespace mozilla {

/**
 * AudioChunkList provides a way to have preallocated audio buffers in
 * AudioSegment. The idea is that the amount of  AudioChunks is created in
 * advance. Each AudioChunk is able to hold a specific amount of audio
 * (capacity). The total capacity of AudioChunkList is specified by the number
 * of AudioChunks. The important aspect of the AudioChunkList is that
 * preallocates everything and reuse the same chunks similar to a ring buffer.
 *
 * Why the whole AudioChunk is preallocated and not some raw memory buffer? This
 * is due to the limitations of MediaTrackGraph. The way that MTG works depends
 * on `AudioSegment`s to convey the actual audio data. An AudioSegment consists
 * of AudioChunks. The AudioChunk is built in a way, that owns and allocates the
 * audio buffers. Thus, since the use of AudioSegment is mandatory if the audio
 * data was in a different form, the only way to use it from the audio thread
 * would be to create the AudioChunk there. That would result in a copy
 * operation (not very important) and most of all an allocation of the audio
 * buffer in the audio thread. This happens in many places inside MTG it's a bad
 * practice, though, and it has been avoided due to the AudioChunkList.
 *
 * After construction the sample format must be set, when it is available. It
 * can be set in the audio thread. Before setting the sample format is not
 * possible to use any method of AudioChunkList.
 *
 * Every AudioChunk in the AudioChunkList is preallocated with a capacity of 128
 * frames of float audio. Nevertheless, the sample format is not available at
 * that point. Thus if the sample format is set to short, the capacity of each
 * chunk changes to 256 number of frames, and the total duration becomes twice
 * big. There are methods to get the chunk capacity and total capacity in frames
 * and must always be used.
 *
 * Two things to note. First, when the channel count changes everything is
 * recreated which means reallocations. Second, the total capacity might differs
 * from the requested total capacity for two reasons. First, if the sample
 * format is set to short and second because the number of chunks in the list
 * divides exactly the final total capacity. The corresponding method must
 * always be used to query the total capacity.
 */
class AudioChunkList {
 public:
  /**
   * Constructor, the final total duration might be different from the requested
   * `aTotalDuration`. Memory allocation takes place.
   */
  AudioChunkList(uint32_t aTotalDuration, uint32_t aChannels,
                 const PrincipalHandle& aPrincipalHandle);
  AudioChunkList(const AudioChunkList&) = delete;
  AudioChunkList(AudioChunkList&&) = delete;
  ~AudioChunkList() = default;

  /**
   * Set sample format. It must be done before any other method being used.
   */
  void SetSampleFormat(AudioSampleFormat aFormat);
  /**
   * Get the next available AudioChunk. The duration of the chunk will be zero
   * and the volume 1.0. However, the buffers will be there ready to be written.
   * Please note, that a reference of the preallocated chunk is returned. Thus
   * it _must not be consumed_ directly. If the chunk needs to be consumed it
   * must be copied to a temporary chunk first. For example:
   * ```
   *   AudioChunk& chunk = audioChunklist.GetNext();
   *   // Set up the chunk
   *   AudioChunk tmp = chunk;
   *   audioSegment.AppendAndConsumeChunk(std::move(tmp));
   * ```
   * This way no memory allocation or copy, takes place.
   */
  AudioChunk& GetNext();

  /**
   * Get the capacity of each individual AudioChunk in the list.
   */
  uint32_t ChunkCapacity() const {
    MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16 ||
               mSampleFormat == AUDIO_FORMAT_FLOAT32);
    return mChunkCapacity;
  }
  /**
   * Get the total capacity of AudioChunkList.
   */
  uint32_t TotalCapacity() const {
    MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16 ||
               mSampleFormat == AUDIO_FORMAT_FLOAT32);
    return CheckedInt<uint32_t>(mChunkCapacity * mChunks.Length()).value();
  }

  /**
   * Update the channel count of the AudioChunkList. Memory allocation is
   * taking place.
   */
  void Update(uint32_t aChannels);

 private:
  void IncrementIndex() {
    ++mIndex;
    mIndex = CheckedInt<uint32_t>(mIndex % mChunks.Length()).value();
  }
  void CreateChunks(uint32_t aNumOfChunks, uint32_t aChannels);
  void UpdateToMonoOrStereo(uint32_t aChannels);

 private:
  const PrincipalHandle mPrincipalHandle;
  nsTArray<AudioChunk> mChunks;
  uint32_t mIndex = 0;
  uint32_t mChunkCapacity = WEBAUDIO_BLOCK_SIZE;
  AudioSampleFormat mSampleFormat = AUDIO_FORMAT_SILENCE;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_DRIFTCONTROL_AUDIOCHUNKLIST_H_

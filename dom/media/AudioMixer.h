/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOMIXER_H_
#define MOZILLA_AUDIOMIXER_H_

#include "AudioSampleFormat.h"
#include "nsTArray.h"
#include "mozilla/PodOperations.h"
#include "mozilla/LinkedList.h"
#include "AudioStream.h"

namespace mozilla {

struct MixerCallbackReceiver {
  virtual void MixerCallback(AudioDataValue* aMixedBuffer,
                             AudioSampleFormat aFormat,
                             uint32_t aChannels,
                             uint32_t aFrames,
                             uint32_t aSampleRate) = 0;
};
/**
 * This class mixes multiple streams of audio together to output a single audio
 * stream.
 *
 * AudioMixer::Mix is to be called repeatedly with buffers that have the same
 * length, sample rate, sample format and channel count. This class works with
 * interleaved and plannar buffers, but the buffer mixed must be of the same
 * type during a mixing cycle.
 *
 * When all the tracks have been mixed, calling FinishMixing will call back with
 * a buffer containing the mixed audio data.
 *
 * This class is not thread safe.
 */
class AudioMixer
{
public:
  AudioMixer()
    : mFrames(0),
      mChannels(0),
      mSampleRate(0)
  { }

  ~AudioMixer()
  {
    MixerCallback* cb;
    while ((cb = mCallbacks.popFirst())) {
      delete cb;
    }
  }

  void StartMixing()
  {
    mSampleRate = mChannels = mFrames = 0;
  }

  /* Get the data from the mixer. This is supposed to be called when all the
   * tracks have been mixed in. The caller should not hold onto the data. */
  void FinishMixing() {
    MOZ_ASSERT(mChannels && mFrames && mSampleRate, "Mix not called for this cycle?");
    for (MixerCallback* cb = mCallbacks.getFirst();
         cb != nullptr; cb = cb->getNext()) {
      MixerCallbackReceiver* receiver = cb->mReceiver;
      receiver->MixerCallback(mMixedAudio.Elements(),
                              AudioSampleTypeToFormat<AudioDataValue>::Format,
                              mChannels,
                              mFrames,
                              mSampleRate);
    }
    PodZero(mMixedAudio.Elements(), mMixedAudio.Length());
    mSampleRate = mChannels = mFrames = 0;
  }

  /* Add a buffer to the mix. The buffer can be null if there's nothing to mix
   * but the callback is still needed. */
  void Mix(AudioDataValue* aSamples,
           uint32_t aChannels,
           uint32_t aFrames,
           uint32_t aSampleRate) {
    if (!mFrames && !mChannels) {
      mFrames = aFrames;
      mChannels = aChannels;
      mSampleRate = aSampleRate;
      EnsureCapacityAndSilence();
    }

    MOZ_ASSERT(aFrames == mFrames);
    MOZ_ASSERT(aChannels == mChannels);
    MOZ_ASSERT(aSampleRate == mSampleRate);

    if (!aSamples) {
      return;
    }

    for (uint32_t i = 0; i < aFrames * aChannels; i++) {
      mMixedAudio[i] += aSamples[i];
    }
  }

  void AddCallback(MixerCallbackReceiver* aReceiver) {
    mCallbacks.insertBack(new MixerCallback(aReceiver));
  }

  bool FindCallback(MixerCallbackReceiver* aReceiver) {
    for (MixerCallback* cb = mCallbacks.getFirst();
         cb != nullptr; cb = cb->getNext()) {
      if (cb->mReceiver == aReceiver) {
        return true;
      }
    }
    return false;
  }

  bool RemoveCallback(MixerCallbackReceiver* aReceiver) {
    for (MixerCallback* cb = mCallbacks.getFirst();
         cb != nullptr; cb = cb->getNext()) {
      if (cb->mReceiver == aReceiver) {
        cb->remove();
        delete cb;
        return true;
      }
    }
    return false;
  }
private:
  void EnsureCapacityAndSilence() {
    if (mFrames * mChannels > mMixedAudio.Length()) {
      mMixedAudio.SetLength(mFrames* mChannels);
    }
    PodZero(mMixedAudio.Elements(), mMixedAudio.Length());
  }

  class MixerCallback : public LinkedListElement<MixerCallback>
  {
  public:
    explicit MixerCallback(MixerCallbackReceiver* aReceiver)
      : mReceiver(aReceiver)
    { }
    MixerCallbackReceiver* mReceiver;
  };

  /* Function that is called when the mixing is done. */
  LinkedList<MixerCallback> mCallbacks;
  /* Number of frames for this mixing block. */
  uint32_t mFrames;
  /* Number of channels for this mixing block. */
  uint32_t mChannels;
  /* Sample rate the of the mixed data. */
  uint32_t mSampleRate;
  /* Buffer containing the mixed audio data. */
  nsTArray<AudioDataValue> mMixedAudio;
};

} // namespace mozilla

#endif // MOZILLA_AUDIOMIXER_H_

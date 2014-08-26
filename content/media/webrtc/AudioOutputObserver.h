/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIOOUTPUTOBSERVER_H_
#define AUDIOOUTPUTOBSERVER_H_

#include "mozilla/StaticPtr.h"
#include "AudioMixer.h"

namespace webrtc {
class SingleRwFifo;
}

namespace mozilla {

typedef struct FarEndAudioChunk_ {
  uint16_t mSamples;
  bool mOverrun;
  int16_t mData[1]; // variable-length
} FarEndAudioChunk;

// XXX Really a singleton currently
class AudioOutputObserver : public MixerCallbackReceiver
{
public:
  AudioOutputObserver();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioOutputObserver);

  void MixerCallback(AudioDataValue* aMixedBuffer,
                     AudioSampleFormat aFormat,
                     uint32_t aChannels,
                     uint32_t aFrames,
                     uint32_t aSampleRate) MOZ_OVERRIDE;

  void Clear();
  void InsertFarEnd(const AudioDataValue *aBuffer, uint32_t aFrames, bool aOverran,
                    int aFreq, int aChannels, AudioSampleFormat aFormat);
  uint32_t PlayoutFrequency() { return mPlayoutFreq; }
  uint32_t PlayoutChannels() { return mPlayoutChannels; }

  FarEndAudioChunk *Pop();
  uint32_t Size();

private:
  virtual ~AudioOutputObserver();
  uint32_t mPlayoutFreq;
  uint32_t mPlayoutChannels;

  nsAutoPtr<webrtc::SingleRwFifo> mPlayoutFifo;
  uint32_t mChunkSize;

  // chunking to 10ms support
  FarEndAudioChunk *mSaved; // can't be nsAutoPtr since we need to use moz_free()
  uint32_t mSamplesSaved;
};

extern StaticRefPtr<AudioOutputObserver> gFarendObserver;

}

#endif

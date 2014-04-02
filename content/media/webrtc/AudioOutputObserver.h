/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIOOUTPUTOBSERVER_H_
#define AUDIOOUTPUTOBSERVER_H_

#include "mozilla/StaticPtr.h"

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
class AudioOutputObserver // : public MSGOutputObserver
{
public:
  AudioOutputObserver();
  virtual ~AudioOutputObserver();

  void Clear();
  void InsertFarEnd(const AudioDataValue *aBuffer, uint32_t aSamples, bool aOverran,
                    int aFreq, int aChannels, AudioSampleFormat aFormat);
  uint32_t PlayoutFrequency() { return mPlayoutFreq; }
  uint32_t PlayoutChannels() { return mPlayoutChannels; }

  FarEndAudioChunk *Pop();
  uint32_t Size();

private:
  uint32_t mPlayoutFreq;
  uint32_t mPlayoutChannels;

  nsAutoPtr<webrtc::SingleRwFifo> mPlayoutFifo;
  uint32_t mChunkSize;

  // chunking to 10ms support
  nsAutoPtr<FarEndAudioChunk> mSaved;
  uint32_t mSamplesSaved;
};

// XXX until there's a registration API in MSG
extern StaticAutoPtr<AudioOutputObserver> gFarendObserver;

}

#endif

/*
* Copyright 2013, Mozilla Foundation and contributors
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef GMP_AUDIO_DECODE_h_
#define GMP_AUDIO_DECODE_h_

#include "gmp-errors.h"
#include "gmp-audio-samples.h"
#include "gmp-audio-codec.h"
#include <stdint.h>

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPAudioDecoderCallback
{
public:
  virtual ~GMPAudioDecoderCallback() {}

  virtual void Decoded(GMPAudioSamples* aDecodedSamples) = 0;

  virtual void InputDataExhausted() = 0;

  virtual void DrainComplete() = 0;

  virtual void ResetComplete() = 0;
};

// ALL METHODS MUST BE CALLED ON THE MAIN THREAD
class GMPAudioDecoder
{
public:
  virtual ~GMPAudioDecoder() {}

  // aCallback: Subclass should retain reference to it until DecodingComplete
  //            is called. Do not attempt to delete it, host retains ownership.
  // TODO: Pass AudioHost so decoder can create GMPAudioEncodedFrame objects?
  virtual GMPErr InitDecode(const GMPAudioCodec& aCodecSettings,
                            GMPAudioDecoderCallback* aCallback) = 0;

  // Decode encoded audio frames (as a part of an audio stream). The decoded
  // frames must be returned to the user through the decode complete callback.
  virtual GMPErr Decode(GMPAudioSamples* aEncodedSamples) = 0;

  // Reset decoder state and prepare for a new call to Decode(...).
  // Flushes the decoder pipeline.
  // The decoder should enqueue a task to run ResetComplete() on the main
  // thread once the reset has finished.
  virtual GMPErr Reset() = 0;

  // Output decoded frames for any data in the pipeline, regardless of ordering.
  // All remaining decoded frames should be immediately returned via callback.
  // The decoder should enqueue a task to run DrainComplete() on the main
  // thread once the reset has finished.
  virtual GMPErr Drain() = 0;

  // May free decoder memory.
  virtual void DecodingComplete() = 0;
};

#endif // GMP_VIDEO_DECODE_h_

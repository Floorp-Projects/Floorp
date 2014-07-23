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

#ifndef GMP_AUDIO_FRAME_h_
#define GMP_AUDIO_FRAME_h_

#include <stdint.h>
#include "gmp-errors.h"
#include "gmp-decryption.h"

enum GMPAudioFormat
{
  kGMPAudioEncodedSamples, // Raw compressed data, i.e. an AAC/Vorbis packet.
  kGMPAudioIS16Samples, // Interleaved int16_t PCM samples.
  kGMPAudioSamplesFormatInvalid // Should always be last.
};

class GMPAudioSamples {
public:
  // The format of the buffer.
  virtual GMPAudioFormat GetFormat() = 0;
  virtual void Destroy() = 0;

  // MAIN THREAD ONLY
  // Buffer size must be exactly what's required to contain all samples in
  // the buffer; every byte is assumed to be part of a sample.
  virtual GMPErr SetBufferSize(uint32_t aSize) = 0;

  // Size of the buffer in bytes.
  virtual uint32_t Size() = 0;

  // Timestamps are in microseconds, and are the playback start time of the
  // first sample in the buffer.
  virtual void SetTimeStamp(uint64_t aTimeStamp) = 0;
  virtual uint64_t TimeStamp() = 0;
  virtual const uint8_t* Buffer() const = 0;
  virtual uint8_t*       Buffer() = 0;

  // Get metadata describing how this frame is encrypted, or nullptr if the
  // buffer is not encrypted.
  virtual const GMPEncryptedBufferMetadata* GetDecryptionData() const = 0;
};

#endif // GMP_AUDIO_FRAME_h_

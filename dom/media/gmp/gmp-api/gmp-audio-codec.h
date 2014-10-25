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

#ifndef GMP_AUDIO_CODEC_h_
#define GMP_AUDIO_CODEC_h_

#include <stdint.h>

enum GMPAudioCodecType
{
  kGMPAudioCodecAAC,
  kGMPAudioCodecVorbis,
  kGMPAudioCodecInvalid // Should always be last.
};

struct GMPAudioCodec
{
  GMPAudioCodecType mCodecType;
  uint32_t mChannelCount;
  uint32_t mBitsPerChannel;
  uint32_t mSamplesPerSecond;

  // Codec extra data, such as vorbis setup header, or
  // AAC AudioSpecificConfig.
  // These are null/0 if not externally negotiated
  const uint8_t* mExtraData;
  size_t         mExtraDataLen;
};

#endif // GMP_AUDIO_CODEC_h_

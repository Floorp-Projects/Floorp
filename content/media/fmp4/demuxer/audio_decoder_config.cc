// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/audio_decoder_config.h"

#include <sstream>

namespace mp4_demuxer {

static int SampleFormatToBitsPerChannel(SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return 0;
    case kSampleFormatU8:
      return 8;
    case kSampleFormatS16:
    case kSampleFormatPlanarS16:
      return 16;
    case kSampleFormatS32:
    case kSampleFormatF32:
    case kSampleFormatPlanarF32:
      return 32;
    case kSampleFormatMax:
      break;
  }

  //NOTREACHED() << "Invalid sample format provided: " << sample_format;
  return 0;
}

AudioDecoderConfig::AudioDecoderConfig()
    : codec_(kUnknownAudioCodec),
      sample_format_(kUnknownSampleFormat),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED),
      samples_per_second_(0),
      bytes_per_frame_(0),
      is_encrypted_(false) {
}

AudioDecoderConfig::AudioDecoderConfig(AudioCodec codec,
                                       SampleFormat sample_format,
                                       ChannelLayout channel_layout,
                                       int samples_per_second,
                                       const uint8_t* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, sample_format, channel_layout, samples_per_second,
             extra_data, extra_data_size, is_encrypted);
}

void AudioDecoderConfig::Initialize(AudioCodec codec,
                                    SampleFormat sample_format,
                                    ChannelLayout channel_layout,
                                    int samples_per_second,
                                    const uint8_t* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  codec_ = codec;
  channel_layout_ = channel_layout;
  samples_per_second_ = samples_per_second;
  sample_format_ = sample_format;
  bits_per_channel_ = SampleFormatToBitsPerChannel(sample_format);
  extra_data_.assign(extra_data, extra_data + extra_data_size);
  is_encrypted_ = is_encrypted;

  int channels = ChannelLayoutToChannelCount(channel_layout_);
  bytes_per_frame_ = channels * bits_per_channel_ / 8;
}

AudioDecoderConfig::~AudioDecoderConfig() {}

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
         channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
         bits_per_channel_ > 0 &&
         bits_per_channel_ <= kMaxBitsPerSample &&
         samples_per_second_ > 0 &&
         samples_per_second_ <= kMaxSampleRate &&
         sample_format_ != kUnknownSampleFormat;
}

bool AudioDecoderConfig::Matches(const AudioDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (bits_per_channel() == config.bits_per_channel()) &&
          (channel_layout() == config.channel_layout()) &&
          (samples_per_second() == config.samples_per_second()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()) &&
          (sample_format() == config.sample_format()));
}

std::string AudioDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << codec()
    << " bits/channel: " << bits_per_channel()
    << " samples/s: " << samples_per_second()
    << " has extra data? " << (extra_data() ? "true" : "false")
    << " encrypted? " << (is_encrypted() ? "true" : "false");
  return s.str();
}

}  // namespace mp4_demuxer

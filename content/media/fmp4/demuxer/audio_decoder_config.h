// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
#define MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

#include <vector>

#include "mp4_demuxer/basictypes.h"
#include "mp4_demuxer/channel_layout.h"

namespace mp4_demuxer {

enum AudioCodec {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a codec replace it with a dummy value; when adding a
  // codec, do so at the bottom before kAudioCodecMax.
  kUnknownAudioCodec = 0,
  kCodecAAC,
  kCodecMP3,
  kCodecPCM,
  kCodecVorbis,
  kCodecFLAC,
  kCodecAMR_NB,
  kCodecAMR_WB,
  kCodecPCM_MULAW,
  kCodecGSM_MS,
  kCodecPCM_S16BE,
  kCodecPCM_S24BE,
  kCodecOpus,
  // DO NOT ADD RANDOM AUDIO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.

  // Must always be last!
  kAudioCodecMax
};

enum SampleFormat {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a sample format replace it with a dummy value; when
  // adding a sample format, do so at the bottom before kSampleFormatMax.
  kUnknownSampleFormat = 0,
  kSampleFormatU8,         // Unsigned 8-bit w/ bias of 128.
  kSampleFormatS16,        // Signed 16-bit.
  kSampleFormatS32,        // Signed 32-bit.
  kSampleFormatF32,        // Float 32-bit.
  kSampleFormatPlanarS16,  // Signed 16-bit planar.
  kSampleFormatPlanarF32,  // Float 32-bit planar.

  // Must always be last!
  kSampleFormatMax
};

// TODO(dalecurtis): FFmpeg API uses |bytes_per_channel| instead of
// |bits_per_channel|, we should switch over since bits are generally confusing
// to work with.
class AudioDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  AudioDecoderConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  AudioDecoderConfig(AudioCodec codec, SampleFormat sample_format,
                     ChannelLayout channel_layout, int samples_per_second,
                     const uint8_t* extra_data, size_t extra_data_size,
                     bool is_encrypted);

  ~AudioDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(AudioCodec codec, SampleFormat sample_format,
                  ChannelLayout channel_layout, int samples_per_second,
                  const uint8_t* extra_data, size_t extra_data_size,
                  bool is_encrypted);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  // Returns true if all fields in |config| match this config.
  // Note: The contents of |extra_data_| are compared not the raw pointers.
  bool Matches(const AudioDecoderConfig& config) const;

  AudioCodec codec() const { return codec_; }
  int bits_per_channel() const { return bits_per_channel_; }
  ChannelLayout channel_layout() const { return channel_layout_; }
  int samples_per_second() const { return samples_per_second_; }
  SampleFormat sample_format() const { return sample_format_; }
  int bytes_per_frame() const { return bytes_per_frame_; }

  // Optional byte data required to initialize audio decoders such as Vorbis
  // codebooks.
  const uint8_t* extra_data() const {
    return extra_data_.empty() ? NULL : &extra_data_[0];
  }
  size_t extra_data_size() const { return extra_data_.size(); }

  // Whether the audio stream is potentially encrypted.
  // Note that in a potentially encrypted audio stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const { return is_encrypted_; }

  std::string AsHumanReadableString() const;

 private:
  AudioCodec codec_;
  SampleFormat sample_format_;
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;
  int bytes_per_frame_;
  std::vector<uint8_t> extra_data_;
  bool is_encrypted_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace mp4_demuxer

#endif  // MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

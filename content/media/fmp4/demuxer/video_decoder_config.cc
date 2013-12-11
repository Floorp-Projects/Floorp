// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/video_decoder_config.h"

#include <sstream>

namespace mp4_demuxer {

VideoDecoderConfig::VideoDecoderConfig()
    : codec_(kUnknownVideoCodec),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      format_(VideoFrameFormat::INVALID),
      is_encrypted_(false) {
}

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       VideoCodecProfile profile,
                                       VideoFrameFormat format,
                                       const IntSize& coded_size,
                                       const IntRect& visible_rect,
                                       const IntSize& natural_size,
                                       const uint8_t* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, profile, format, coded_size, visible_rect, natural_size,
             extra_data, extra_data_size, is_encrypted, true);
}

VideoDecoderConfig::~VideoDecoderConfig() {}

// Some videos just want to watch the world burn, with a height of 0; cap the
// "infinite" aspect ratio resulting.
static const int kInfiniteRatio = 99999;

// Common aspect ratios (multiplied by 100 and truncated) used for histogramming
// video sizes.  These were taken on 20111103 from
// http://wikipedia.org/wiki/Aspect_ratio_(image)#Previous_and_currently_used_aspect_ratios
static const int kCommonAspectRatios100[] = {
  100, 115, 133, 137, 143, 150, 155, 160, 166, 175, 177, 185, 200, 210, 220,
  221, 235, 237, 240, 255, 259, 266, 276, 293, 400, 1200, kInfiniteRatio,
};

void VideoDecoderConfig::Initialize(VideoCodec codec,
                                    VideoCodecProfile profile,
                                    VideoFrameFormat format,
                                    const IntSize& coded_size,
                                    const IntRect& visible_rect,
                                    const IntSize& natural_size,
                                    const uint8_t* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted,
                                    bool record_stats) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  codec_ = codec;
  profile_ = profile;
  format_ = format;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  extra_data_.assign(extra_data, extra_data + extra_data_size);
  is_encrypted_ = is_encrypted;
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
      natural_size_.width() > 0 &&
      natural_size_.height() > 0 &&

      // Copied from:
      // VideoFrame::IsValidConfig(format_, coded_size_, visible_rect_, natural_size_)
      format_ != VideoFrameFormat::INVALID &&
      !coded_size_.IsEmpty() &&
      coded_size_.GetArea() <= kMaxCanvas &&
      coded_size_.width() <= kMaxDimension &&
      coded_size_.height() <= kMaxDimension &&
      !visible_rect_.IsEmpty() &&
      visible_rect_.x() >= 0 && visible_rect_.y() >= 0 &&
      visible_rect_.right() <= coded_size_.width() &&
      visible_rect_.bottom() <= coded_size_.height() &&
      !natural_size_.IsEmpty() &&
      natural_size_.GetArea() <= kMaxCanvas &&
      natural_size_.width() <= kMaxDimension &&
      natural_size_.height() <= kMaxDimension;
}

bool VideoDecoderConfig::Matches(const VideoDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (format() == config.format()) &&
          (profile() == config.profile()) &&
          (coded_size() == config.coded_size()) &&
          (visible_rect() == config.visible_rect()) &&
          (natural_size() == config.natural_size()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()));
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << codec()
    << " format: " << format()
    << " profile: " << profile()
    << " coded size: [" << coded_size().width()
    << "," << coded_size().height() << "]"
    << " visible rect: [" << visible_rect().x()
    << "," << visible_rect().y()
    << "," << visible_rect().width()
    << "," << visible_rect().height() << "]"
    << " natural size: [" << natural_size().width()
    << "," << natural_size().height() << "]"
    << " has extra data? " << (extra_data() ? "true" : "false")
    << " encrypted? " << (is_encrypted() ? "true" : "false");
  return s.str();
}

VideoCodec VideoDecoderConfig::codec() const {
  return codec_;
}

VideoCodecProfile VideoDecoderConfig::profile() const {
  return profile_;
}

VideoFrameFormat VideoDecoderConfig::format() const {
  return format_;
}

IntSize VideoDecoderConfig::coded_size() const {
  return coded_size_;
}

IntRect VideoDecoderConfig::visible_rect() const {
  return visible_rect_;
}

IntSize VideoDecoderConfig::natural_size() const {
  return natural_size_;
}

const uint8_t* VideoDecoderConfig::extra_data() const {
  if (extra_data_.empty())
    return NULL;
  return &extra_data_[0];
}

size_t VideoDecoderConfig::extra_data_size() const {
  return extra_data_.size();
}

bool VideoDecoderConfig::is_encrypted() const {
  return is_encrypted_;
}

}  // namespace mp4_demuxer

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_AVC_H_
#define MEDIA_MP4_AVC_H_

#include <vector>

#include "mp4_demuxer/basictypes.h"

namespace mp4_demuxer {

struct AVCDecoderConfigurationRecord;

class AVC {
 public:
  static bool ConvertFrameToAnnexB(int length_size, std::vector<uint8_t>* buffer);

  static bool ConvertConfigToAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8_t>* buffer);
};

}  // namespace mp4_demuxer

#endif  // MEDIA_MP4_AVC_H_

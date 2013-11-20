// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/avc.h"

#include <algorithm>
#include <vector>

#include "mp4_demuxer/box_definitions.h"
#include "mp4_demuxer/box_reader.h"

namespace mp4_demuxer {

static const uint8_t kAnnexBStartCode[] = {0, 0, 0, 1};
static const int kAnnexBStartCodeSize = 4;

static bool ConvertAVCToAnnexBInPlaceForLengthSize4(std::vector<uint8_t>* buf) {
  const int kLengthSize = 4;
  size_t pos = 0;
  while (pos + kLengthSize < buf->size()) {
    int nal_size = (*buf)[pos];
    nal_size = (nal_size << 8) + (*buf)[pos+1];
    nal_size = (nal_size << 8) + (*buf)[pos+2];
    nal_size = (nal_size << 8) + (*buf)[pos+3];
    std::copy(kAnnexBStartCode, kAnnexBStartCode + kAnnexBStartCodeSize,
              buf->begin() + pos);
    pos += kLengthSize + nal_size;
  }
  return pos == buf->size();
}

// static
bool AVC::ConvertFrameToAnnexB(int length_size, std::vector<uint8_t>* buffer) {
  RCHECK(length_size == 1 || length_size == 2 || length_size == 4);

  if (length_size == 4)
    return ConvertAVCToAnnexBInPlaceForLengthSize4(buffer);

  std::vector<uint8_t> temp;
  temp.swap(*buffer);
  buffer->reserve(temp.size() + 32);

  size_t pos = 0;
  while (pos + length_size < temp.size()) {
    int nal_size = temp[pos];
    if (length_size == 2) nal_size = (nal_size << 8) + temp[pos+1];
    pos += length_size;

    RCHECK(pos + nal_size <= temp.size());
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), temp.begin() + pos,
                   temp.begin() + pos + nal_size);
    pos += nal_size;
  }
  return pos == temp.size();
}

// static
bool AVC::ConvertConfigToAnnexB(
    const AVCDecoderConfigurationRecord& avc_config,
    std::vector<uint8_t>* buffer) {
  DCHECK(buffer->empty());
  buffer->clear();
  int total_size = 0;
  for (size_t i = 0; i < avc_config.sps_list.size(); i++)
    total_size += avc_config.sps_list[i].size() + kAnnexBStartCodeSize;
  for (size_t i = 0; i < avc_config.pps_list.size(); i++)
    total_size += avc_config.pps_list[i].size() + kAnnexBStartCodeSize;
  buffer->reserve(total_size);

  for (size_t i = 0; i < avc_config.sps_list.size(); i++) {
    buffer->insert(buffer->end(), kAnnexBStartCode,
                kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), avc_config.sps_list[i].begin(),
                avc_config.sps_list[i].end());
  }

  for (size_t i = 0; i < avc_config.pps_list.size(); i++) {
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), avc_config.pps_list[i].begin(),
                   avc_config.pps_list[i].end());
  }
  return true;
}

}  // namespace mp4_demuxer

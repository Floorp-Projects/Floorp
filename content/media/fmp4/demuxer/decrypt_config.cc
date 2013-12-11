// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/decrypt_config.h"

namespace mp4_demuxer {

DecryptConfig::DecryptConfig(const std::string& key_id,
                             const std::string& iv,
                             const int data_offset,
                             const std::vector<SubsampleEntry>& subsamples)
    : key_id_(key_id),
      iv_(iv),
      data_offset_(data_offset),
      subsamples_(subsamples) {
  DCHECK_GT(key_id.size(), 0u);
  DCHECK(iv.size() == static_cast<size_t>(DecryptConfig::kDecryptionKeySize) ||
        iv.empty());
  DCHECK_GE(data_offset, 0);
}

DecryptConfig::~DecryptConfig() {}

}  // namespace mp4_demuxer

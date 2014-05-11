// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/cenc.h"

#include <cstring>

#include "mp4_demuxer/box_reader.h"

namespace mp4_demuxer {

FrameCENCInfo::FrameCENCInfo() {}
FrameCENCInfo::~FrameCENCInfo() {}

bool FrameCENCInfo::Parse(int iv_size, StreamReader* reader) {
  const int kEntrySize = 6;
  // Mandated by CENC spec
  RCHECK(iv_size == 8 || iv_size == 16);

  memset(iv, 0, sizeof(iv));
  for (int i = 0; i < iv_size; i++)
    RCHECK(reader->Read1(&iv[i]));

  if (!reader->HasBytes(1)) return true;

  uint16_t subsample_count;
  RCHECK(reader->Read2(&subsample_count) &&
         reader->HasBytes(subsample_count * kEntrySize));

  subsamples.resize(subsample_count);
  for (int i = 0; i < subsample_count; i++) {
    uint16_t clear_bytes;
    uint32_t cypher_bytes;
    RCHECK(reader->Read2(&clear_bytes) &&
           reader->Read4(&cypher_bytes));
    subsamples[i].clear_bytes = clear_bytes;
    subsamples[i].cypher_bytes = cypher_bytes;
  }
  return true;
}

size_t FrameCENCInfo::GetTotalSizeOfSubsamples() const {
  size_t size = 0;
  for (size_t i = 0; i < subsamples.size(); i++) {
    size += subsamples[i].clear_bytes + subsamples[i].cypher_bytes;
  }
  return size;
}

}  // namespace mp4_demuxer

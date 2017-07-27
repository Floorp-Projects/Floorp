// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_HDMX_H_
#define OTS_HDMX_H_

#include <vector>

#include "ots.h"

namespace ots {

struct OpenTypeHDMXDeviceRecord {
  uint8_t pixel_size;
  uint8_t max_width;
  std::vector<uint8_t> widths;
};

class OpenTypeHDMX : public Table {
 public:
  explicit OpenTypeHDMX(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  uint16_t version;
  int32_t size_device_record;
  int32_t pad_len;
  std::vector<OpenTypeHDMXDeviceRecord> records;
};

}  // namespace ots

#endif

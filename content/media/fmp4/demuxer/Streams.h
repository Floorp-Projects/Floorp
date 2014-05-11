#pragma once

#include <stdint.h>

namespace mp4_demuxer {

class Stream {
public:

  // Returns true on success, false on failure.
  // Writes number of bytes read into out_bytes_read, or 0 on EOS.
  // Returns true on EOS.
  virtual bool ReadAt(int64_t offset,
                      uint8_t* buffer,
                      uint32_t count,
                      uint32_t* out_bytes_read) = 0;

  virtual int64_t Length() const = 0;
};

} // namespace mp4_demuxer
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/machine_id.h"

#include <stddef.h>

#include "rlz/lib/assert.h"
#include "rlz/lib/crc8.h"
#include "rlz/lib/string_utils.h"

// Note: The original machine_id.cc code depends on Chromium's sha1 implementation.
// Using Mozilla's implmentation as replacement to reduce the dependency of
// some external files.
#include "mozilla/SHA1.h"

namespace rlz_lib {

bool GetMachineId(std::string* machine_id) {
  if (!machine_id)
    return false;

  static std::string calculated_id;
  static bool calculated = false;
  if (calculated) {
    *machine_id = calculated_id;
    return true;
  }

  std::vector<uint8_t> sid_bytes;
  int volume_id;
  if (!GetRawMachineId(&sid_bytes, &volume_id))
    return false;

  if (!testing::GetMachineIdImpl(sid_bytes, volume_id, machine_id))
    return false;

  calculated = true;
  calculated_id = *machine_id;
  return true;
}

namespace testing {

bool GetMachineIdImpl(const std::vector<uint8_t>& sid_bytes,
                      int volume_id,
                      std::string* machine_id) {
  machine_id->clear();

  // The ID should be the SID hash + the Hard Drive SNo. + checksum byte.
  static const int kSizeWithoutChecksum = mozilla::SHA1Sum::kHashSize + sizeof(int);
  std::basic_string<unsigned char> id_binary(kSizeWithoutChecksum + 1, 0);

  if (!sid_bytes.empty()) {
    // In order to be compatible with the old version of RLZ, the hash of the
    // SID must be done with all the original bytes from the unicode string.
    // However, the chromebase SHA1 hash function takes only an std::string as
    // input, so the unicode string needs to be converted to std::string
    // "as is".
    size_t byte_count = sid_bytes.size() * sizeof(std::vector<uint8_t>::value_type);
    const char* buffer = reinterpret_cast<const char*>(sid_bytes.data());

    // Note that digest can have embedded nulls.
    mozilla::SHA1Sum SHA1;
    mozilla::SHA1Sum::Hash hash;
    SHA1.update(buffer, byte_count);
    SHA1.finish(hash);
    std::string digest(reinterpret_cast<char*>(hash), mozilla::SHA1Sum::kHashSize);
    VERIFY(digest.size() == mozilla::SHA1Sum::kHashSize);
    std::copy(digest.begin(), digest.end(), id_binary.begin());
  }

  // Convert from int to binary (makes big-endian).
  for (size_t i = 0; i < sizeof(int); i++) {
    int shift_bits = 8 * (sizeof(int) - i - 1);
    id_binary[mozilla::SHA1Sum::kHashSize + i] = static_cast<unsigned char>(
        (volume_id >> shift_bits) & 0xFF);
  }

  // Append the checksum byte.
  if (!sid_bytes.empty() || (0 != volume_id))
    rlz_lib::Crc8::Generate(id_binary.c_str(),
                            kSizeWithoutChecksum,
                            &id_binary[kSizeWithoutChecksum]);

  return rlz_lib::BytesToString(
      id_binary.c_str(), kSizeWithoutChecksum + 1, machine_id);
}

}  // namespace testing

}  // namespace rlz_lib

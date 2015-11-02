// Copyright 2012 Google Inc. All Rights Reserved.
// Use of this source code is governed by an Apache-style license that can be
// found in the COPYING file.

#ifndef RLZ_LIB_MACHINE_ID_H_
#define RLZ_LIB_MACHINE_ID_H_

#include <vector>

namespace rlz_lib {

// Retrieves a raw machine identifier string and a machine-specific
// 4 byte value. GetMachineId() will SHA1 |data|, append |more_data|, compute
// the Crc8 of that, and return a hex-encoded string of that data.
bool GetRawMachineId(std::vector<uint8_t>* data, int* more_data);

}  // namespace rlz_lib

#endif  // RLZ_LIB_MACHINE_ID_H_

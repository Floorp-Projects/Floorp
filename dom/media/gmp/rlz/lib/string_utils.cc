// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// String manipulation functions used in the RLZ library.

#include "rlz/lib/string_utils.h"

namespace rlz_lib {

bool BytesToString(const unsigned char* data,
                   int data_len,
                   std::string* string) {
  if (!string)
    return false;

  string->clear();
  if (data_len < 1 || !data)
    return false;

  static const char kHex[] = "0123456789ABCDEF";

  // Fix the buffer size to begin with to avoid repeated re-allocation.
  string->resize(data_len * 2);
  int index = data_len;
  while (index--) {
    string->at(2 * index) = kHex[data[index] >> 4];  // high digit
    string->at(2 * index + 1) = kHex[data[index] & 0x0F];  // low digit
  }

  return true;
}

}  // namespace rlz_lib

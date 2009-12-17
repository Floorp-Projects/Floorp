// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHA2_H__
#define BASE_SHA2_H__

#include <string>

namespace base {

// These functions perform SHA-256 operations.
//
// Functions for SHA-384 and SHA-512 can be added when the need arises.

enum {
  SHA256_LENGTH = 32  // length in bytes of a SHA-256 hash
};

// Computes the SHA-256 hash of the input string 'str' and stores the first
// 'len' bytes of the hash in the output buffer 'output'.  If 'len' > 32,
// only 32 bytes (the full hash) are stored in the 'output' buffer.
void SHA256HashString(const std::string& str, void* output, size_t len);

}  // namespace base

#endif // BASE_SHA2_H__

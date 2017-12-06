// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// String manipulation functions used in the RLZ library.

#ifndef RLZ_LIB_STRING_UTILS_H_
#define RLZ_LIB_STRING_UTILS_H_

#include <string>

namespace rlz_lib {

bool BytesToString(const unsigned char* data,
                   int data_len,
                   std::string* string);

};  // namespace

#endif  // RLZ_LIB_STRING_UTILS_H_

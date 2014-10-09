// Copyright 2010 Google Inc. All Rights Reserved.
// Use of this source code is governed by an Apache-style license that can be
// found in the COPYING file.
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

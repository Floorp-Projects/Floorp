//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// string_utils:
//   String helper functions.
//

#ifndef LIBANGLE_STRING_UTILS_H_
#define LIBANGLE_STRING_UTILS_H_

#include <string>
#include <vector>

namespace angle
{

void SplitString(const std::string &input,
                 char delimiter,
                 std::vector<std::string> *tokensOut);

void SplitStringAlongWhitespace(const std::string &input,
                                std::vector<std::string> *tokensOut);

bool HexStringToUInt(const std::string &input, unsigned int *uintOut);

bool ReadFileToString(const std::string &path, std::string *stringOut);

}

#endif // LIBANGLE_STRING_UTILS_H_

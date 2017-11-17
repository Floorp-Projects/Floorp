/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StringOperations_h
#define StringOperations_h

#include <memory>
#include <string>
#include <string.h>

std::string hash(const std::string &Str);

template <typename... Args>
inline std::string stringFormat(const std::string &Format, Args... ArgList) {
  size_t Len = snprintf(nullptr, 0, Format.c_str(), ArgList...);
  std::unique_ptr<char[]> Buf(new char[Len + 1]);
  snprintf(Buf.get(), Len + 1, Format.c_str(), ArgList...);
  return std::string(Buf.get(), Buf.get() + Len);
}

std::string toString(int N);

#endif

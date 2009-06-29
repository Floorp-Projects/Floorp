// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"

#include "base/string_piece.h"
#include "base/string_util.h"

namespace base {

std::string SysWideToUTF8(const std::wstring& wide) {
  // In theory this should be using the system-provided conversion rather
  // than our ICU, but this will do for now.
  return WideToUTF8(wide);
}
std::wstring SysUTF8ToWide(const StringPiece& utf8) {
  // In theory this should be using the system-provided conversion rather
  // than our ICU, but this will do for now.
  std::wstring out;
  UTF8ToWide(utf8.data(), utf8.size(), &out);
  return out;
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  // TODO(evanm): we can't assume Linux is UTF-8.
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(const StringPiece& native_mb) {
  // TODO(evanm): we can't assume Linux is UTF-8.
  return SysUTF8ToWide(native_mb);
}

}  // namespace base

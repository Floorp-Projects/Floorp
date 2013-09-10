// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYS_STRING_CONVERSIONS_H_
#define BASE_SYS_STRING_CONVERSIONS_H_

// Provides system-dependent string type conversions for cases where it's
// necessary to not use ICU. Generally, you should not need this in Chrome,
// but it is used in some shared code. Dependencies should be minimal.

#include <string>
#include "base/basictypes.h"
#include "base/string16.h"

class StringPiece;

namespace base {

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
std::string SysWideToUTF8(const std::wstring& wide);
std::wstring SysUTF8ToWide(const StringPiece& utf8);

// Converts between wide and the system multi-byte representations of a string.
// DANGER: This will lose information and can change (on Windows, this can
// change between reboots).
std::string SysWideToNativeMB(const std::wstring& wide);
std::wstring SysNativeMBToWide(const StringPiece& native_mb);

// Windows-specific ------------------------------------------------------------

#if defined(OS_WIN)

// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
std::wstring SysMultiByteToWide(const StringPiece& mb, uint32_t code_page);
std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page);

#endif  // defined(OS_WIN)

}  // namespace base

#endif  // BASE_SYS_STRING_CONVERSIONS_H_

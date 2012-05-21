/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/string_util.h"

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"

#include "base/string_piece.h"
#include "base/string_util.h"

#include "build/build_config.h"

// FIXME/cjones: these really only pertain to the linux sys string
// converters.
#ifdef WCHAR_T_IS_UTF16
#  define ICONV_WCHAR_T_ENCODING "UTF-16"
#else
#  define ICONV_WCHAR_T_ENCODING "WCHAR_T"
#endif

// FIXME/cjones: BIG assumption here that std::string is a good
// container of UTF8-encoded strings.  this is probably wrong, as its
// API doesn't really make sense for UTF8.

namespace base {
string16 SysWideToUTF16(const std::wstring& wide);

// FIXME/cjones: as its name implies, this function is a hack.
template<typename FromType, typename ToType>
ToType
GhettoStringConvert(const FromType& in)
{
  // FIXME/cjones: assumes no non-ASCII characters in |in|
  ToType out;
  out.resize(in.length());
  for (int i = 0; i < static_cast<int>(in.length()); ++i)
      out[i] = static_cast<typename ToType::value_type>(in[i]);
  return out;
}
}

// Implement functions that were in the chromium ICU library, which
// we're not taking.

std::string
WideToUTF8(const std::wstring& wide)
{
    return base::SysWideToUTF8(wide);
}

string16
UTF8ToUTF16(const std::string& utf8)
{
    // FIXME/cjones: do proper conversion
    return base::GhettoStringConvert<std::string, string16>(utf8);
}

std::wstring
UTF8ToWide(const StringPiece& utf8)
{
    return base::SysUTF8ToWide(utf8);
}

string16
WideToUTF16(const std::wstring& wide)
{
    return base::SysWideToUTF16(wide);
}

std::string
UTF16ToUTF8(const string16& utf16)
{
    // FIXME/cjones: do proper conversion
    return base::GhettoStringConvert<string16, std::string>(utf16);
}

namespace base {

// FIXME/cjones: here we're entirely replacing the linux string
// converters, and implementing the one that doesn't exist for OS X
// and Windows.

#if !defined(OS_MACOSX) && !defined(OS_WIN)
std::string SysWideToUTF8(const std::wstring& wide) {
  // FIXME/cjones: do this with iconv
  return GhettoStringConvert<std::wstring, std::string>(wide);
}
#endif

string16 SysWideToUTF16(const std::wstring& wide)
{
#if defined(WCHAR_T_IS_UTF16)
  return wide;
#else
  // FIXME/cjones: do this with iconv for linux, other for OS X
  return GhettoStringConvert<std::wstring, string16>(wide);
#endif
}

#if !defined(OS_MACOSX) && !defined(OS_WIN)
std::wstring SysUTF8ToWide(const StringPiece& utf8) {
  // FIXME/cjones: do this with iconv
  return GhettoStringConvert<StringPiece, std::wstring>(utf8);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  // TODO(evanm): we can't assume Linux is UTF-8.
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(const StringPiece& native_mb) {
  // TODO(evanm): we can't assume Linux is UTF-8.
  return SysUTF8ToWide(native_mb);
}
#endif

}  // namespace base

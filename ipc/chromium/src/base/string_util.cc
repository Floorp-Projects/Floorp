// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"

#include "build/build_config.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/singleton.h"

namespace {

// Force the singleton used by Empty[W]String[16] to be a unique type. This
// prevents other code that might accidentally use Singleton<string> from
// getting our internal one.
struct EmptyStrings {
  EmptyStrings() {}
  const std::string s;
  const std::wstring ws;
  const string16 s16;
};

// Hack to convert any char-like type to its unsigned counterpart.
// For example, it will convert char, signed char and unsigned char to unsigned
// char.
template<typename T>
struct ToUnsigned {
  typedef T Unsigned;
};

template<>
struct ToUnsigned<char> {
  typedef unsigned char Unsigned;
};
template<>
struct ToUnsigned<signed char> {
  typedef unsigned char Unsigned;
};
template<>
struct ToUnsigned<wchar_t> {
#if defined(WCHAR_T_IS_UTF16)
  typedef unsigned short Unsigned;
#elif defined(WCHAR_T_IS_UTF32)
  typedef uint32 Unsigned;
#endif
};
template<>
struct ToUnsigned<short> {
  typedef unsigned short Unsigned;
};

// Used by ReplaceStringPlaceholders to track the position in the string of
// replaced parameters.
struct ReplacementOffset {
  ReplacementOffset(int parameter, size_t offset)
      : parameter(parameter),
        offset(offset) {}

  // Index of the parameter.
  int parameter;

  // Starting position in the string.
  size_t offset;
};

static bool CompareParameter(const ReplacementOffset& elem1,
                             const ReplacementOffset& elem2) {
  return elem1.parameter < elem2.parameter;
}

// Generalized string-to-number conversion.
//
// StringToNumberTraits should provide:
//  - a typedef for string_type, the STL string type used as input.
//  - a typedef for value_type, the target numeric type.
//  - a static function, convert_func, which dispatches to an appropriate
//    strtol-like function and returns type value_type.
//  - a static function, valid_func, which validates |input| and returns a bool
//    indicating whether it is in proper form.  This is used to check for
//    conditions that convert_func tolerates but should result in
//    StringToNumber returning false.  For strtol-like funtions, valid_func
//    should check for leading whitespace.
template<typename StringToNumberTraits>
bool StringToNumber(const typename StringToNumberTraits::string_type& input,
                    typename StringToNumberTraits::value_type* output) {
  typedef StringToNumberTraits traits;

  errno = 0;  // Thread-safe?  It is on at least Mac, Linux, and Windows.
  typename traits::string_type::value_type* endptr = NULL;
  typename traits::value_type value = traits::convert_func(input.c_str(),
                                                           &endptr);
  *output = value;

  // Cases to return false:
  //  - If errno is ERANGE, there was an overflow or underflow.
  //  - If the input string is empty, there was nothing to parse.
  //  - If endptr does not point to the end of the string, there are either
  //    characters remaining in the string after a parsed number, or the string
  //    does not begin with a parseable number.  endptr is compared to the
  //    expected end given the string's stated length to correctly catch cases
  //    where the string contains embedded NUL characters.
  //  - valid_func determines that the input is not in preferred form.
  return errno == 0 &&
         !input.empty() &&
         input.c_str() + input.length() == endptr &&
         traits::valid_func(input);
}

class StringToLongTraits {
 public:
  typedef std::string string_type;
  typedef long value_type;
  static const int kBase = 10;
  static inline value_type convert_func(const string_type::value_type* str,
                                        string_type::value_type** endptr) {
    return strtol(str, endptr, kBase);
  }
  static inline bool valid_func(const string_type& str) {
    return !str.empty() && !isspace(str[0]);
  }
};

class String16ToLongTraits {
 public:
  typedef string16 string_type;
  typedef long value_type;
  static const int kBase = 10;
  static inline value_type convert_func(const string_type::value_type* str,
                                        string_type::value_type** endptr) {
#if defined(WCHAR_T_IS_UTF16)
    return wcstol(str, endptr, kBase);
#elif defined(WCHAR_T_IS_UTF32)
    std::string ascii_string = UTF16ToASCII(string16(str));
    char* ascii_end = NULL;
    value_type ret = strtol(ascii_string.c_str(), &ascii_end, kBase);
    if (ascii_string.c_str() + ascii_string.length() == ascii_end) {
      *endptr =
          const_cast<string_type::value_type*>(str) + ascii_string.length();
    }
    return ret;
#endif
  }
  static inline bool valid_func(const string_type& str) {
    return !str.empty() && !iswspace(str[0]);
  }
};

class StringToInt64Traits {
 public:
  typedef std::string string_type;
  typedef int64 value_type;
  static const int kBase = 10;
  static inline value_type convert_func(const string_type::value_type* str,
                                        string_type::value_type** endptr) {
#ifdef OS_WIN
    return _strtoi64(str, endptr, kBase);
#else  // assume OS_POSIX
    return strtoll(str, endptr, kBase);
#endif
  }
  static inline bool valid_func(const string_type& str) {
    return !str.empty() && !isspace(str[0]);
  }
};

class String16ToInt64Traits {
 public:
  typedef string16 string_type;
  typedef int64 value_type;
  static const int kBase = 10;
  static inline value_type convert_func(const string_type::value_type* str,
                                        string_type::value_type** endptr) {
#ifdef OS_WIN
    return _wcstoi64(str, endptr, kBase);
#else  // assume OS_POSIX
    std::string ascii_string = UTF16ToASCII(string16(str));
    char* ascii_end = NULL;
    value_type ret = strtoll(ascii_string.c_str(), &ascii_end, kBase);
    if (ascii_string.c_str() + ascii_string.length() == ascii_end) {
      *endptr =
          const_cast<string_type::value_type*>(str) + ascii_string.length();
    }
    return ret;
#endif
  }
  static inline bool valid_func(const string_type& str) {
    return !str.empty() && !iswspace(str[0]);
  }
};

// For the HexString variants, use the unsigned variants like strtoul for
// convert_func so that input like "0x80000000" doesn't result in an overflow.

class HexStringToLongTraits {
 public:
  typedef std::string string_type;
  typedef long value_type;
  static const int kBase = 16;
  static inline value_type convert_func(const string_type::value_type* str,
                                        string_type::value_type** endptr) {
    return strtoul(str, endptr, kBase);
  }
  static inline bool valid_func(const string_type& str) {
    return !str.empty() && !isspace(str[0]);
  }
};

class HexString16ToLongTraits {
 public:
  typedef string16 string_type;
  typedef long value_type;
  static const int kBase = 16;
  static inline value_type convert_func(const string_type::value_type* str,
                                        string_type::value_type** endptr) {
#if defined(WCHAR_T_IS_UTF16)
    return wcstoul(str, endptr, kBase);
#elif defined(WCHAR_T_IS_UTF32)
    std::string ascii_string = UTF16ToASCII(string16(str));
    char* ascii_end = NULL;
    value_type ret = strtoul(ascii_string.c_str(), &ascii_end, kBase);
    if (ascii_string.c_str() + ascii_string.length() == ascii_end) {
      *endptr =
          const_cast<string_type::value_type*>(str) + ascii_string.length();
    }
    return ret;
#endif
  }
  static inline bool valid_func(const string_type& str) {
    return !str.empty() && !iswspace(str[0]);
  }
};

}  // namespace


namespace base {

bool IsWprintfFormatPortable(const wchar_t* format) {
  for (const wchar_t* position = format; *position != '\0'; ++position) {

    if (*position == '%') {
      bool in_specification = true;
      bool modifier_l = false;
      while (in_specification) {
        // Eat up characters until reaching a known specifier.
        if (*++position == '\0') {
          // The format string ended in the middle of a specification.  Call
          // it portable because no unportable specifications were found.  The
          // string is equally broken on all platforms.
          return true;
        }

        if (*position == 'l') {
          // 'l' is the only thing that can save the 's' and 'c' specifiers.
          modifier_l = true;
        } else if (((*position == 's' || *position == 'c') && !modifier_l) ||
                   *position == 'S' || *position == 'C' || *position == 'F' ||
                   *position == 'D' || *position == 'O' || *position == 'U') {
          // Not portable.
          return false;
        }

        if (wcschr(L"diouxXeEfgGaAcspn%", *position)) {
          // Portable, keep scanning the rest of the format string.
          in_specification = false;
        }
      }
    }

  }

  return true;
}


}  // namespace base

namespace base {

const std::string& EmptyString() {
  return Singleton<EmptyStrings>::get()->s;
}

const std::wstring& EmptyWString() {
  return Singleton<EmptyStrings>::get()->ws;
}

const string16& EmptyString16() {
  return Singleton<EmptyStrings>::get()->s16;
}

}

const wchar_t kWhitespaceWide[] = {
  0x0009,  // <control-0009> to <control-000D>
  0x000A,
  0x000B,
  0x000C,
  0x000D,
  0x0020,  // Space
  0x0085,  // <control-0085>
  0x00A0,  // No-Break Space
  0x1680,  // Ogham Space Mark
  0x180E,  // Mongolian Vowel Separator
  0x2000,  // En Quad to Hair Space
  0x2001,
  0x2002,
  0x2003,
  0x2004,
  0x2005,
  0x2006,
  0x2007,
  0x2008,
  0x2009,
  0x200A,
  0x200C,  // Zero Width Non-Joiner
  0x2028,  // Line Separator
  0x2029,  // Paragraph Separator
  0x202F,  // Narrow No-Break Space
  0x205F,  // Medium Mathematical Space
  0x3000,  // Ideographic Space
  0
};
const char kWhitespaceASCII[] = {
  0x09,    // <control-0009> to <control-000D>
  0x0A,
  0x0B,
  0x0C,
  0x0D,
  0x20,    // Space
  0
};
const char* const kCodepageUTF8 = "UTF-8";

template<typename STR>
TrimPositions TrimStringT(const STR& input,
                          const typename STR::value_type trim_chars[],
                          TrimPositions positions,
                          STR* output) {
  // Find the edges of leading/trailing whitespace as desired.
  const typename STR::size_type last_char = input.length() - 1;
  const typename STR::size_type first_good_char = (positions & TRIM_LEADING) ?
      input.find_first_not_of(trim_chars) : 0;
  const typename STR::size_type last_good_char = (positions & TRIM_TRAILING) ?
      input.find_last_not_of(trim_chars) : last_char;

  // When the string was all whitespace, report that we stripped off whitespace
  // from whichever position the caller was interested in.  For empty input, we
  // stripped no whitespace, but we still need to clear |output|.
  if (input.empty() ||
      (first_good_char == STR::npos) || (last_good_char == STR::npos)) {
    bool input_was_empty = input.empty();  // in case output == &input
    output->clear();
    return input_was_empty ? TRIM_NONE : positions;
  }

  // Trim the whitespace.
  *output =
      input.substr(first_good_char, last_good_char - first_good_char + 1);

  // Return where we trimmed from.
  return static_cast<TrimPositions>(
      ((first_good_char == 0) ? TRIM_NONE : TRIM_LEADING) |
      ((last_good_char == last_char) ? TRIM_NONE : TRIM_TRAILING));
}

bool TrimString(const std::wstring& input,
                const wchar_t trim_chars[],
                std::wstring* output) {
  return TrimStringT(input, trim_chars, TRIM_ALL, output) != TRIM_NONE;
}

bool TrimString(const std::string& input,
                const char trim_chars[],
                std::string* output) {
  return TrimStringT(input, trim_chars, TRIM_ALL, output) != TRIM_NONE;
}

TrimPositions TrimWhitespace(const std::wstring& input,
                             TrimPositions positions,
                             std::wstring* output) {
  return TrimStringT(input, kWhitespaceWide, positions, output);
}

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string* output) {
  return TrimStringT(input, kWhitespaceASCII, positions, output);
}

// This function is only for backward-compatibility.
// To be removed when all callers are updated.
TrimPositions TrimWhitespace(const std::string& input,
                             TrimPositions positions,
                             std::string* output) {
  return TrimWhitespaceASCII(input, positions, output);
}

std::wstring CollapseWhitespace(const std::wstring& text,
                                bool trim_sequences_with_line_breaks) {
  std::wstring result;
  result.resize(text.size());

  // Set flags to pretend we're already in a trimmed whitespace sequence, so we
  // will trim any leading whitespace.
  bool in_whitespace = true;
  bool already_trimmed = true;

  int chars_written = 0;
  for (std::wstring::const_iterator i(text.begin()); i != text.end(); ++i) {
    if (IsWhitespace(*i)) {
      if (!in_whitespace) {
        // Reduce all whitespace sequences to a single space.
        in_whitespace = true;
        result[chars_written++] = L' ';
      }
      if (trim_sequences_with_line_breaks && !already_trimmed &&
          ((*i == '\n') || (*i == '\r'))) {
        // Whitespace sequences containing CR or LF are eliminated entirely.
        already_trimmed = true;
        --chars_written;
      }
    } else {
      // Non-whitespace chracters are copied straight across.
      in_whitespace = false;
      already_trimmed = false;
      result[chars_written++] = *i;
    }
  }

  if (in_whitespace && !already_trimmed) {
    // Any trailing whitespace is eliminated.
    --chars_written;
  }

  result.resize(chars_written);
  return result;
}

std::string WideToASCII(const std::wstring& wide) {
  DCHECK(IsStringASCII(wide));
  return std::string(wide.begin(), wide.end());
}

std::wstring ASCIIToWide(const std::string& ascii) {
  DCHECK(IsStringASCII(ascii));
  return std::wstring(ascii.begin(), ascii.end());
}

std::string UTF16ToASCII(const string16& utf16) {
  DCHECK(IsStringASCII(utf16));
  return std::string(utf16.begin(), utf16.end());
}

string16 ASCIIToUTF16(const std::string& ascii) {
  DCHECK(IsStringASCII(ascii));
  return string16(ascii.begin(), ascii.end());
}

// Latin1 is just the low range of Unicode, so we can copy directly to convert.
bool WideToLatin1(const std::wstring& wide, std::string* latin1) {
  std::string output;
  output.resize(wide.size());
  latin1->clear();
  for (size_t i = 0; i < wide.size(); i++) {
    if (wide[i] > 255)
      return false;
    output[i] = static_cast<char>(wide[i]);
  }
  latin1->swap(output);
  return true;
}

bool IsString8Bit(const std::wstring& str) {
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] > 255)
      return false;
  }
  return true;
}

template<class STR>
static bool DoIsStringASCII(const STR& str) {
  for (size_t i = 0; i < str.length(); i++) {
    typename ToUnsigned<typename STR::value_type>::Unsigned c = str[i];
    if (c > 0x7F)
      return false;
  }
  return true;
}

bool IsStringASCII(const std::wstring& str) {
  return DoIsStringASCII(str);
}

#if !defined(WCHAR_T_IS_UTF16)
bool IsStringASCII(const string16& str) {
  return DoIsStringASCII(str);
}
#endif

bool IsStringASCII(const std::string& str) {
  return DoIsStringASCII(str);
}

// Helper functions that determine whether the given character begins a
// UTF-8 sequence of bytes with the given length. A character satisfies
// "IsInUTF8Sequence" if it is anything but the first byte in a multi-byte
// character.
static inline bool IsBegin2ByteUTF8(int c) {
  return (c & 0xE0) == 0xC0;
}
static inline bool IsBegin3ByteUTF8(int c) {
  return (c & 0xF0) == 0xE0;
}
static inline bool IsBegin4ByteUTF8(int c) {
  return (c & 0xF8) == 0xF0;
}
static inline bool IsInUTF8Sequence(int c) {
  return (c & 0xC0) == 0x80;
}

// This function was copied from Mozilla, with modifications. The original code
// was 'IsUTF8' in xpcom/string/src/nsReadableUtils.cpp. The license block for
// this function is:
//   This function subject to the Mozilla Public License Version
//   1.1 (the "License"); you may not use this code except in compliance with
//   the License. You may obtain a copy of the License at
//   http://www.mozilla.org/MPL/
//
//   Software distributed under the License is distributed on an "AS IS" basis,
//   WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
//   for the specific language governing rights and limitations under the
//   License.
//
//   The Original Code is mozilla.org code.
//
//   The Initial Developer of the Original Code is
//   Netscape Communications Corporation.
//   Portions created by the Initial Developer are Copyright (C) 2000
//   the Initial Developer. All Rights Reserved.
//
//   Contributor(s):
//     Scott Collins <scc@mozilla.org> (original author)
//
// This is a template so that it can be run on wide and 8-bit strings. We want
// to run it on wide strings when we have input that we think may have
// originally been UTF-8, but has been converted to wide characters because
// that's what we (and Windows) use internally.
template<typename CHAR>
static bool IsStringUTF8T(const CHAR* str, int length) {
  bool overlong = false;
  bool surrogate = false;
  bool nonchar = false;

  // overlong byte upper bound
  typename ToUnsigned<CHAR>::Unsigned olupper = 0;

  // surrogate byte lower bound
  typename ToUnsigned<CHAR>::Unsigned slower = 0;

  // incremented when inside a multi-byte char to indicate how many bytes
  // are left in the sequence
  int positions_left = 0;

  for (int i = 0; i < length; i++) {
    // This whole function assume an unsigned value so force its conversion to
    // an unsigned value.
    typename ToUnsigned<CHAR>::Unsigned c = str[i];
    if (c < 0x80)
      continue;  // ASCII

    if (c <= 0xC1) {
      // [80-BF] where not expected, [C0-C1] for overlong
      return false;
    } else if (IsBegin2ByteUTF8(c)) {
      positions_left = 1;
    } else if (IsBegin3ByteUTF8(c)) {
      positions_left = 2;
      if (c == 0xE0) {
        // to exclude E0[80-9F][80-BF]
        overlong = true;
        olupper = 0x9F;
      } else if (c == 0xED) {
        // ED[A0-BF][80-BF]: surrogate codepoint
        surrogate = true;
        slower = 0xA0;
      } else if (c == 0xEF) {
        // EF BF [BE-BF] : non-character
        // TODO(jungshik): EF B7 [90-AF] should be checked as well.
        nonchar = true;
      }
    } else if (c <= 0xF4) {
      positions_left = 3;
      nonchar = true;
      if (c == 0xF0) {
        // to exclude F0[80-8F][80-BF]{2}
        overlong = true;
        olupper = 0x8F;
      } else if (c == 0xF4) {
        // to exclude F4[90-BF][80-BF]
        // actually not surrogates but codepoints beyond 0x10FFFF
        surrogate = true;
        slower = 0x90;
      }
    } else {
      return false;
    }

    // eat the rest of this multi-byte character
    while (positions_left) {
      positions_left--;
      i++;
      c = str[i];
      if (!c)
        return false;  // end of string but not end of character sequence

      // non-character : EF BF [BE-BF] or F[0-7] [89AB]F BF [BE-BF]
      if (nonchar && ((!positions_left && c < 0xBE) ||
                      (positions_left == 1 && c != 0xBF) ||
                      (positions_left == 2 && 0x0F != (0x0F & c) ))) {
        nonchar = false;
      }
      if (!IsInUTF8Sequence(c) || (overlong && c <= olupper) ||
          (surrogate && slower <= c) || (nonchar && !positions_left) ) {
        return false;
      }
      overlong = surrogate = false;
    }
  }
  return true;
}

bool IsStringUTF8(const std::string& str) {
  return IsStringUTF8T(str.data(), str.length());
}

bool IsStringWideUTF8(const std::wstring& str) {
  return IsStringUTF8T(str.data(), str.length());
}

template<typename Iter>
static inline bool DoLowerCaseEqualsASCII(Iter a_begin,
                                          Iter a_end,
                                          const char* b) {
  for (Iter it = a_begin; it != a_end; ++it, ++b) {
    if (!*b || ToLowerASCII(*it) != *b)
      return false;
  }
  return *b == 0;
}

// Front-ends for LowerCaseEqualsASCII.
bool LowerCaseEqualsASCII(const std::string& a, const char* b) {
  return DoLowerCaseEqualsASCII(a.begin(), a.end(), b);
}

bool LowerCaseEqualsASCII(const std::wstring& a, const char* b) {
  return DoLowerCaseEqualsASCII(a.begin(), a.end(), b);
}

bool LowerCaseEqualsASCII(std::string::const_iterator a_begin,
                          std::string::const_iterator a_end,
                          const char* b) {
  return DoLowerCaseEqualsASCII(a_begin, a_end, b);
}

bool LowerCaseEqualsASCII(std::wstring::const_iterator a_begin,
                          std::wstring::const_iterator a_end,
                          const char* b) {
  return DoLowerCaseEqualsASCII(a_begin, a_end, b);
}

#ifndef ANDROID
bool LowerCaseEqualsASCII(const char* a_begin,
                          const char* a_end,
                          const char* b) {
  return DoLowerCaseEqualsASCII(a_begin, a_end, b);
}
bool LowerCaseEqualsASCII(const wchar_t* a_begin,
                          const wchar_t* a_end,
                          const char* b) {
  return DoLowerCaseEqualsASCII(a_begin, a_end, b);
}
#endif
bool StartsWithASCII(const std::string& str,
                     const std::string& search,
                     bool case_sensitive) {
  if (case_sensitive)
    return str.compare(0, search.length(), search) == 0;
  else
    return base::strncasecmp(str.c_str(), search.c_str(), search.length()) == 0;
}

bool StartsWith(const std::wstring& str,
                const std::wstring& search,
                bool case_sensitive) {
  if (case_sensitive)
    return str.compare(0, search.length(), search) == 0;
  else {
    if (search.size() > str.size())
      return false;
    return std::equal(search.begin(), search.end(), str.begin(),
                      chromium_CaseInsensitiveCompare<wchar_t>());
  }
}

DataUnits GetByteDisplayUnits(int64 bytes) {
  // The byte thresholds at which we display amounts.  A byte count is displayed
  // in unit U when kUnitThresholds[U] <= bytes < kUnitThresholds[U+1].
  // This must match the DataUnits enum.
  static const int64 kUnitThresholds[] = {
    0,              // DATA_UNITS_BYTE,
    3*1024,         // DATA_UNITS_KILOBYTE,
    2*1024*1024,    // DATA_UNITS_MEGABYTE,
    1024*1024*1024  // DATA_UNITS_GIGABYTE,
  };

  if (bytes < 0) {
    NOTREACHED() << "Negative bytes value";
    return DATA_UNITS_BYTE;
  }

  int unit_index = arraysize(kUnitThresholds);
  while (--unit_index > 0) {
    if (bytes >= kUnitThresholds[unit_index])
      break;
  }

  DCHECK(unit_index >= DATA_UNITS_BYTE && unit_index <= DATA_UNITS_GIGABYTE);
  return DataUnits(unit_index);
}

// TODO(mpcomplete): deal with locale
// Byte suffixes.  This must match the DataUnits enum.
static const wchar_t* const kByteStrings[] = {
  L"B",
  L"kB",
  L"MB",
  L"GB"
};

static const wchar_t* const kSpeedStrings[] = {
  L"B/s",
  L"kB/s",
  L"MB/s",
  L"GB/s"
};

std::wstring FormatBytesInternal(int64 bytes,
                                 DataUnits units,
                                 bool show_units,
                                 const wchar_t* const* suffix) {
  if (bytes < 0) {
    NOTREACHED() << "Negative bytes value";
    return std::wstring();
  }

  DCHECK(units >= DATA_UNITS_BYTE && units <= DATA_UNITS_GIGABYTE);

  // Put the quantity in the right units.
  double unit_amount = static_cast<double>(bytes);
  for (int i = 0; i < units; ++i)
    unit_amount /= 1024.0;

  wchar_t tmp[64];
  // If the first decimal digit is 0, don't show it.
  double int_part;
  double fractional_part = modf(unit_amount, &int_part);
  modf(fractional_part * 10, &int_part);
  if (int_part == 0) {
    base::swprintf(tmp, arraysize(tmp),
                   L"%lld", static_cast<int64>(unit_amount));
  } else {
    base::swprintf(tmp, arraysize(tmp), L"%.1lf", unit_amount);
  }

  std::wstring ret(tmp);
  if (show_units) {
    ret += L" ";
    ret += suffix[units];
  }

  return ret;
}

std::wstring FormatBytes(int64 bytes, DataUnits units, bool show_units) {
  return FormatBytesInternal(bytes, units, show_units, kByteStrings);
}

std::wstring FormatSpeed(int64 bytes, DataUnits units, bool show_units) {
  return FormatBytesInternal(bytes, units, show_units, kSpeedStrings);
}

template<class StringType>
void DoReplaceSubstringsAfterOffset(StringType* str,
                                    typename StringType::size_type start_offset,
                                    const StringType& find_this,
                                    const StringType& replace_with,
                                    bool replace_all) {
  if ((start_offset == StringType::npos) || (start_offset >= str->length()))
    return;

  DCHECK(!find_this.empty());
  for (typename StringType::size_type offs(str->find(find_this, start_offset));
      offs != StringType::npos; offs = str->find(find_this, offs)) {
    str->replace(offs, find_this.length(), replace_with);
    offs += replace_with.length();

    if (!replace_all)
      break;
  }
}

void ReplaceFirstSubstringAfterOffset(string16* str,
                                      string16::size_type start_offset,
                                      const string16& find_this,
                                      const string16& replace_with) {
  DoReplaceSubstringsAfterOffset(str, start_offset, find_this, replace_with,
                                 false);  // replace first instance
}

void ReplaceFirstSubstringAfterOffset(std::string* str,
                                      std::string::size_type start_offset,
                                      const std::string& find_this,
                                      const std::string& replace_with) {
  DoReplaceSubstringsAfterOffset(str, start_offset, find_this, replace_with,
                                 false);  // replace first instance
}

void ReplaceSubstringsAfterOffset(string16* str,
                                  string16::size_type start_offset,
                                  const string16& find_this,
                                  const string16& replace_with) {
  DoReplaceSubstringsAfterOffset(str, start_offset, find_this, replace_with,
                                 true);  // replace all instances
}

void ReplaceSubstringsAfterOffset(std::string* str,
                                  std::string::size_type start_offset,
                                  const std::string& find_this,
                                  const std::string& replace_with) {
  DoReplaceSubstringsAfterOffset(str, start_offset, find_this, replace_with,
                                 true);  // replace all instances
}

// Overloaded wrappers around vsnprintf and vswprintf. The buf_size parameter
// is the size of the buffer. These return the number of characters in the
// formatted string excluding the NUL terminator. If the buffer is not
// large enough to accommodate the formatted string without truncation, they
// return the number of characters that would be in the fully-formatted string
// (vsnprintf, and vswprintf on Windows), or -1 (vswprintf on POSIX platforms).
inline int vsnprintfT(char* buffer,
                      size_t buf_size,
                      const char* format,
                      va_list argptr) {
  return base::vsnprintf(buffer, buf_size, format, argptr);
}

inline int vsnprintfT(wchar_t* buffer,
                      size_t buf_size,
                      const wchar_t* format,
                      va_list argptr) {
  return base::vswprintf(buffer, buf_size, format, argptr);
}

// Templatized backend for StringPrintF/StringAppendF. This does not finalize
// the va_list, the caller is expected to do that.
template <class StringType>
static void StringAppendVT(StringType* dst,
                           const typename StringType::value_type* format,
                           va_list ap) {
  // First try with a small fixed size buffer.
  // This buffer size should be kept in sync with StringUtilTest.GrowBoundary
  // and StringUtilTest.StringPrintfBounds.
  typename StringType::value_type stack_buf[1024];

  va_list backup_ap;
  base_va_copy(backup_ap, ap);

#if !defined(OS_WIN)
  errno = 0;
#endif
  int result = vsnprintfT(stack_buf, arraysize(stack_buf), format, backup_ap);
  va_end(backup_ap);

  if (result >= 0 && result < static_cast<int>(arraysize(stack_buf))) {
    // It fit.
    dst->append(stack_buf, result);
    return;
  }

  // Repeatedly increase buffer size until it fits.
  int mem_length = arraysize(stack_buf);
  while (true) {
    if (result < 0) {
#if !defined(OS_WIN)
      // On Windows, vsnprintfT always returns the number of characters in a
      // fully-formatted string, so if we reach this point, something else is
      // wrong and no amount of buffer-doubling is going to fix it.
      if (errno != 0 && errno != EOVERFLOW)
#endif
      {
        // If an error other than overflow occurred, it's never going to work.
        DLOG(WARNING) << "Unable to printf the requested string due to error.";
        return;
      }
      // Try doubling the buffer size.
      mem_length *= 2;
    } else {
      // We need exactly "result + 1" characters.
      mem_length = result + 1;
    }

    if (mem_length > 32 * 1024 * 1024) {
      // That should be plenty, don't try anything larger.  This protects
      // against huge allocations when using vsnprintfT implementations that
      // return -1 for reasons other than overflow without setting errno.
      DLOG(WARNING) << "Unable to printf the requested string due to size.";
      return;
    }

    std::vector<typename StringType::value_type> mem_buf(mem_length);

    // Restore the va_list before we use it again.
    base_va_copy(backup_ap, ap);

    result = vsnprintfT(&mem_buf[0], mem_length, format, ap);
    va_end(backup_ap);

    if ((result >= 0) && (result < mem_length)) {
      // It fit.
      dst->append(&mem_buf[0], result);
      return;
    }
  }
}

namespace {

template <typename STR, typename INT, typename UINT, bool NEG>
struct IntToStringT {

  // This is to avoid a compiler warning about unary minus on unsigned type.
  // For example, say you had the following code:
  //   template <typename INT>
  //   INT abs(INT value) { return value < 0 ? -value : value; }
  // Even though if INT is unsigned, it's impossible for value < 0, so the
  // unary minus will never be taken, the compiler will still generate a
  // warning.  We do a little specialization dance...
  template <typename INT2, typename UINT2, bool NEG2>
  struct ToUnsignedT { };

  template <typename INT2, typename UINT2>
  struct ToUnsignedT<INT2, UINT2, false> {
    static UINT2 ToUnsigned(INT2 value) {
      return static_cast<UINT2>(value);
    }
  };

  template <typename INT2, typename UINT2>
  struct ToUnsignedT<INT2, UINT2, true> {
    static UINT2 ToUnsigned(INT2 value) {
      return static_cast<UINT2>(value < 0 ? -value : value);
    }
  };

  static STR IntToString(INT value) {
    // log10(2) ~= 0.3 bytes needed per bit or per byte log10(2**8) ~= 2.4.
    // So round up to allocate 3 output characters per byte, plus 1 for '-'.
    const int kOutputBufSize = 3 * sizeof(INT) + 1;

    // Allocate the whole string right away, we will right back to front, and
    // then return the substr of what we ended up using.
    STR outbuf(kOutputBufSize, 0);

    bool is_neg = value < 0;
    // Even though is_neg will never be true when INT is parameterized as
    // unsigned, even the presence of the unary operation causes a warning.
    UINT res = ToUnsignedT<INT, UINT, NEG>::ToUnsigned(value);

    for (typename STR::iterator it = outbuf.end();;) {
      --it;
      DCHECK(it != outbuf.begin());
      *it = static_cast<typename STR::value_type>((res % 10) + '0');
      res /= 10;

      // We're done..
      if (res == 0) {
        if (is_neg) {
          --it;
          DCHECK(it != outbuf.begin());
          *it = static_cast<typename STR::value_type>('-');
        }
        return STR(it, outbuf.end());
      }
    }
    NOTREACHED();
    return STR();
  }
};

}

std::string IntToString(int value) {
  return IntToStringT<std::string, int, unsigned int, true>::
      IntToString(value);
}
std::wstring IntToWString(int value) {
  return IntToStringT<std::wstring, int, unsigned int, true>::
      IntToString(value);
}
std::string UintToString(unsigned int value) {
  return IntToStringT<std::string, unsigned int, unsigned int, false>::
      IntToString(value);
}
std::wstring UintToWString(unsigned int value) {
  return IntToStringT<std::wstring, unsigned int, unsigned int, false>::
      IntToString(value);
}
std::string Int64ToString(int64 value) {
  return IntToStringT<std::string, int64, uint64, true>::
      IntToString(value);
}
std::wstring Int64ToWString(int64 value) {
  return IntToStringT<std::wstring, int64, uint64, true>::
      IntToString(value);
}
std::string Uint64ToString(uint64 value) {
  return IntToStringT<std::string, uint64, uint64, false>::
      IntToString(value);
}
std::wstring Uint64ToWString(uint64 value) {
  return IntToStringT<std::wstring, uint64, uint64, false>::
      IntToString(value);
}

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  StringAppendVT(dst, format, ap);
}

void StringAppendV(std::wstring* dst, const wchar_t* format, va_list ap) {
  StringAppendVT(dst, format, ap);
}

std::string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

std::wstring StringPrintf(const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::wstring result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

const std::string& SStringPrintf(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  dst->clear();
  StringAppendV(dst, format, ap);
  va_end(ap);
  return *dst;
}

const std::wstring& SStringPrintf(std::wstring* dst,
                                  const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  dst->clear();
  StringAppendV(dst, format, ap);
  va_end(ap);
  return *dst;
}

void StringAppendF(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

void StringAppendF(std::wstring* dst, const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

template<typename STR>
static void SplitStringT(const STR& str,
                         const typename STR::value_type s,
                         bool trim_whitespace,
                         std::vector<STR>* r) {
  size_t last = 0;
  size_t i;
  size_t c = str.size();
  for (i = 0; i <= c; ++i) {
    if (i == c || str[i] == s) {
      size_t len = i - last;
      STR tmp = str.substr(last, len);
      if (trim_whitespace) {
        STR t_tmp;
        TrimWhitespace(tmp, TRIM_ALL, &t_tmp);
        r->push_back(t_tmp);
      } else {
        r->push_back(tmp);
      }
      last = i + 1;
    }
  }
}

void SplitString(const std::wstring& str,
                 wchar_t s,
                 std::vector<std::wstring>* r) {
  SplitStringT(str, s, true, r);
}

void SplitString(const std::string& str,
                 char s,
                 std::vector<std::string>* r) {
  SplitStringT(str, s, true, r);
}

void SplitStringDontTrim(const std::wstring& str,
                         wchar_t s,
                         std::vector<std::wstring>* r) {
  SplitStringT(str, s, false, r);
}

void SplitStringDontTrim(const std::string& str,
                         char s,
                         std::vector<std::string>* r) {
  SplitStringT(str, s, false, r);
}

template<typename STR>
static STR JoinStringT(const std::vector<STR>& parts,
                       typename STR::value_type sep) {
  if (parts.size() == 0) return STR();

  STR result(parts[0]);
  typename std::vector<STR>::const_iterator iter = parts.begin();
  ++iter;

  for (; iter != parts.end(); ++iter) {
    result += sep;
    result += *iter;
  }

  return result;
}

std::string JoinString(const std::vector<std::string>& parts, char sep) {
  return JoinStringT(parts, sep);
}

std::wstring JoinString(const std::vector<std::wstring>& parts, wchar_t sep) {
  return JoinStringT(parts, sep);
}

void SplitStringAlongWhitespace(const std::wstring& str,
                                std::vector<std::wstring>* result) {
  const size_t length = str.length();
  if (!length)
    return;

  bool last_was_ws = false;
  size_t last_non_ws_start = 0;
  for (size_t i = 0; i < length; ++i) {
    switch(str[i]) {
      // HTML 5 defines whitespace as: space, tab, LF, line tab, FF, or CR.
      case L' ':
      case L'\t':
      case L'\xA':
      case L'\xB':
      case L'\xC':
      case L'\xD':
        if (!last_was_ws) {
          if (i > 0) {
            result->push_back(
                str.substr(last_non_ws_start, i - last_non_ws_start));
          }
          last_was_ws = true;
        }
        break;

      default:  // Not a space character.
        if (last_was_ws) {
          last_was_ws = false;
          last_non_ws_start = i;
        }
        break;
    }
  }
  if (!last_was_ws) {
    result->push_back(
              str.substr(last_non_ws_start, length - last_non_ws_start));
  }
}

string16 ReplaceStringPlaceholders(const string16& format_string,
                                   const string16& a,
                                   size_t* offset) {
  std::vector<size_t> offsets;
  string16 result = ReplaceStringPlaceholders(format_string, a,
                                              string16(),
                                              string16(),
                                              string16(), &offsets);
  DCHECK(offsets.size() == 1);
  if (offset) {
    *offset = offsets[0];
  }
  return result;
}

string16 ReplaceStringPlaceholders(const string16& format_string,
                                   const string16& a,
                                   const string16& b,
                                   std::vector<size_t>* offsets) {
  return ReplaceStringPlaceholders(format_string, a, b, string16(),
                                   string16(), offsets);
}

string16 ReplaceStringPlaceholders(const string16& format_string,
                                   const string16& a,
                                   const string16& b,
                                   const string16& c,
                                   std::vector<size_t>* offsets) {
  return ReplaceStringPlaceholders(format_string, a, b, c, string16(),
                                   offsets);
}

string16 ReplaceStringPlaceholders(const string16& format_string,
                                       const string16& a,
                                       const string16& b,
                                       const string16& c,
                                       const string16& d,
                                       std::vector<size_t>* offsets) {
  // We currently only support up to 4 place holders ($1 through $4), although
  // it's easy enough to add more.
  const string16* subst_texts[] = { &a, &b, &c, &d };

  string16 formatted;
  formatted.reserve(format_string.length() + a.length() +
      b.length() + c.length() + d.length());

  std::vector<ReplacementOffset> r_offsets;

  // Replace $$ with $ and $1-$4 with placeholder text if it exists.
  for (string16::const_iterator i = format_string.begin();
       i != format_string.end(); ++i) {
    if ('$' == *i) {
      if (i + 1 != format_string.end()) {
        ++i;
        DCHECK('$' == *i || ('1' <= *i && *i <= '4')) <<
            "Invalid placeholder: " << *i;
        if ('$' == *i) {
          formatted.push_back('$');
        } else {
          int index = *i - '1';
          if (offsets) {
            ReplacementOffset r_offset(index,
                                       static_cast<int>(formatted.size()));
            r_offsets.insert(std::lower_bound(r_offsets.begin(),
                                              r_offsets.end(), r_offset,
                                              &CompareParameter),
                             r_offset);
          }
          formatted.append(*subst_texts[index]);
        }
      }
    } else {
      formatted.push_back(*i);
    }
  }
  if (offsets) {
    for (std::vector<ReplacementOffset>::const_iterator i = r_offsets.begin();
         i != r_offsets.end(); ++i) {
      offsets->push_back(i->offset);
    }
  }
  return formatted;
}

template <class CHAR>
static bool IsWildcard(CHAR character) {
  return character == '*' || character == '?';
}

// Move the strings pointers to the point where they start to differ.
template <class CHAR>
static void EatSameChars(const CHAR** pattern, const CHAR** string) {
  bool escaped = false;
  while (**pattern && **string) {
    if (!escaped && IsWildcard(**pattern)) {
      // We don't want to match wildcard here, except if it's escaped.
      return;
    }

    // Check if the escapement char is found. If so, skip it and move to the
    // next character.
    if (!escaped && **pattern == L'\\') {
      escaped = true;
      (*pattern)++;
      continue;
    }

    // Check if the chars match, if so, increment the ptrs.
    if (**pattern == **string) {
      (*pattern)++;
      (*string)++;
    } else {
      // Uh ho, it did not match, we are done. If the last char was an
      // escapement, that means that it was an error to advance the ptr here,
      // let's put it back where it was. This also mean that the MatchPattern
      // function will return false because if we can't match an escape char
      // here, then no one will.
      if (escaped) {
        (*pattern)--;
      }
      return;
    }

    escaped = false;
  }
}

template <class CHAR>
static void EatWildcard(const CHAR** pattern) {
  while(**pattern) {
    if (!IsWildcard(**pattern))
      return;
    (*pattern)++;
  }
}

template <class CHAR>
static bool MatchPatternT(const CHAR* eval, const CHAR* pattern) {
  // Eat all the matching chars.
  EatSameChars(&pattern, &eval);

  // If the string is empty, then the pattern must be empty too, or contains
  // only wildcards.
  if (*eval == 0) {
    EatWildcard(&pattern);
    if (*pattern)
      return false;
    return true;
  }

  // Pattern is empty but not string, this is not a match.
  if (*pattern == 0)
    return false;

  // If this is a question mark, then we need to compare the rest with
  // the current string or the string with one character eaten.
  if (pattern[0] == '?') {
    if (MatchPatternT(eval, pattern + 1) ||
        MatchPatternT(eval + 1, pattern + 1))
      return true;
  }

  // This is a *, try to match all the possible substrings with the remainder
  // of the pattern.
  if (pattern[0] == '*') {
    while (*eval) {
      if (MatchPatternT(eval, pattern + 1))
        return true;
      eval++;
    }

    // We reached the end of the string, let see if the pattern contains only
    // wildcards.
    if (*eval == 0) {
      EatWildcard(&pattern);
      if (*pattern)
        return false;
      return true;
    }
  }

  return false;
}

bool MatchPattern(const std::wstring& eval, const std::wstring& pattern) {
  return MatchPatternT(eval.c_str(), pattern.c_str());
}

bool MatchPattern(const std::string& eval, const std::string& pattern) {
  return MatchPatternT(eval.c_str(), pattern.c_str());
}

// For the various *ToInt conversions, there are no *ToIntTraits classes to use
// because there's no such thing as strtoi.  Use *ToLongTraits through a cast
// instead, requiring that long and int are compatible and equal-width.  They
// are on our target platforms.

// XXX Sigh.

#if !defined(ARCH_CPU_64_BITS)
bool StringToInt(const std::string& input, int* output) {
  COMPILE_ASSERT(sizeof(int) == sizeof(long), cannot_strtol_to_int);
  return StringToNumber<StringToLongTraits>(input,
                                            reinterpret_cast<long*>(output));
}

bool StringToInt(const string16& input, int* output) {
  COMPILE_ASSERT(sizeof(int) == sizeof(long), cannot_wcstol_to_int);
  return StringToNumber<String16ToLongTraits>(input,
                                              reinterpret_cast<long*>(output));
}

#else
bool StringToInt(const std::string& input, int* output) {
  long tmp;
  bool ok = StringToNumber<StringToLongTraits>(input, &tmp);
  if (!ok || tmp > kint32max) {
    return false;
  }
  *output = static_cast<int>(tmp);
  return true;
}

bool StringToInt(const string16& input, int* output) {
  long tmp;
  bool ok = StringToNumber<String16ToLongTraits>(input, &tmp);
  if (!ok || tmp > kint32max) {
    return false;
  }
  *output = static_cast<int>(tmp);
  return true;
}
#endif //  !defined(ARCH_CPU_64_BITS)

bool StringToInt64(const std::string& input, int64* output) {
  return StringToNumber<StringToInt64Traits>(input, output);
}

bool StringToInt64(const string16& input, int64* output) {
  return StringToNumber<String16ToInt64Traits>(input, output);
}

#if !defined(ARCH_CPU_64_BITS)
bool HexStringToInt(const std::string& input, int* output) {
  COMPILE_ASSERT(sizeof(int) == sizeof(long), cannot_strtol_to_int);
  return StringToNumber<HexStringToLongTraits>(input,
                                               reinterpret_cast<long*>(output));
}

bool HexStringToInt(const string16& input, int* output) {
  COMPILE_ASSERT(sizeof(int) == sizeof(long), cannot_wcstol_to_int);
  return StringToNumber<HexString16ToLongTraits>(
      input, reinterpret_cast<long*>(output));
}

#else
bool HexStringToInt(const std::string& input, int* output) {
  long tmp;
  bool ok = StringToNumber<HexStringToLongTraits>(input, &tmp);
  if (!ok || tmp > kint32max) {
    return false;
  }
  *output = static_cast<int>(tmp);
  return true;
}

bool HexStringToInt(const string16& input, int* output) {
  long tmp;
  bool ok = StringToNumber<HexString16ToLongTraits>(input, &tmp);
  if (!ok || tmp > kint32max) {
    return false;
  }
  *output = static_cast<int>(tmp);
  return true;
}

#endif // !defined(ARCH_CPU_64_BITS)

namespace {

template<class CHAR>
bool HexDigitToIntT(const CHAR digit, uint8* val) {
  if (digit >= '0' && digit <= '9')
    *val = digit - '0';
  else if (digit >= 'a' && digit <= 'f')
    *val = 10 + digit - 'a';
  else if (digit >= 'A' && digit <= 'F')
    *val = 10 + digit - 'A';
  else
    return false;
  return true;
}

template<typename STR>
bool HexStringToBytesT(const STR& input, std::vector<uint8>* output) {
  DCHECK(output->size() == 0);
  int count = input.size();
  if (count == 0 || (count % 2) != 0)
    return false;
  for (int i = 0; i < count / 2; ++i) {
    uint8 msb = 0;  // most significant 4 bits
    uint8 lsb = 0;  // least significant 4 bits
    if (!HexDigitToIntT(input[i * 2], &msb) ||
        !HexDigitToIntT(input[i * 2 + 1], &lsb))
      return false;
    output->push_back((msb << 4) | lsb);
  }
  return true;
}

}  // namespace

bool HexStringToBytes(const std::string& input, std::vector<uint8>* output) {
  return HexStringToBytesT(input, output);
}

bool HexStringToBytes(const string16& input, std::vector<uint8>* output) {
  return HexStringToBytesT(input, output);
}

int StringToInt(const std::string& value) {
  int result;
  StringToInt(value, &result);
  return result;
}

int StringToInt(const string16& value) {
  int result;
  StringToInt(value, &result);
  return result;
}

int64 StringToInt64(const std::string& value) {
  int64 result;
  StringToInt64(value, &result);
  return result;
}

int64 StringToInt64(const string16& value) {
  int64 result;
  StringToInt64(value, &result);
  return result;
}

int HexStringToInt(const std::string& value) {
  int result;
  HexStringToInt(value, &result);
  return result;
}

int HexStringToInt(const string16& value) {
  int result;
  HexStringToInt(value, &result);
  return result;
}

// The following code is compatible with the OpenBSD lcpy interface.  See:
//   http://www.gratisoft.us/todd/papers/strlcpy.html
//   ftp://ftp.openbsd.org/pub/OpenBSD/src/lib/libc/string/{wcs,str}lcpy.c

namespace {

template <typename CHAR>
size_t lcpyT(CHAR* dst, const CHAR* src, size_t dst_size) {
  for (size_t i = 0; i < dst_size; ++i) {
    if ((dst[i] = src[i]) == 0)  // We hit and copied the terminating NULL.
      return i;
  }

  // We were left off at dst_size.  We over copied 1 byte.  Null terminate.
  if (dst_size != 0)
    dst[dst_size - 1] = 0;

  // Count the rest of the |src|, and return it's length in characters.
  while (src[dst_size]) ++dst_size;
  return dst_size;
}

}  // namespace

size_t base::strlcpy(char* dst, const char* src, size_t dst_size) {
  return lcpyT<char>(dst, src, dst_size);
}
size_t base::wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size) {
  return lcpyT<wchar_t>(dst, src, dst_size);
}

bool ElideString(const std::wstring& input, int max_len, std::wstring* output) {
  DCHECK(max_len >= 0);
  if (static_cast<int>(input.length()) <= max_len) {
    output->assign(input);
    return false;
  }

  switch (max_len) {
    case 0:
      output->clear();
      break;
    case 1:
      output->assign(input.substr(0, 1));
      break;
    case 2:
      output->assign(input.substr(0, 2));
      break;
    case 3:
      output->assign(input.substr(0, 1) + L"." +
                     input.substr(input.length() - 1));
      break;
    case 4:
      output->assign(input.substr(0, 1) + L".." +
                     input.substr(input.length() - 1));
      break;
    default: {
      int rstr_len = (max_len - 3) / 2;
      int lstr_len = rstr_len + ((max_len - 3) % 2);
      output->assign(input.substr(0, lstr_len) + L"..." +
                     input.substr(input.length() - rstr_len));
      break;
    }
  }

  return true;
}

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

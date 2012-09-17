// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"

#include <string.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "unicode/ucnv.h"
#include "unicode/numfmt.h"
#include "unicode/ustring.h"

namespace {

// ReadUnicodeCharacter --------------------------------------------------------

// Reads a UTF-8 stream, placing the next code point into the given output
// |*code_point|. |src| represents the entire string to read, and |*char_index|
// is the character offset within the string to start reading at. |*char_index|
// will be updated to index the last character read, such that incrementing it
// (as in a for loop) will take the reader to the next character.
//
// Returns true on success. On false, |*code_point| will be invalid.
bool ReadUnicodeCharacter(const char* src, int32_t src_len,
                          int32_t* char_index, uint32_t* code_point_out) {
  // U8_NEXT expects to be able to use -1 to signal an error, so we must
  // use a signed type for code_point.  But this function returns false
  // on error anyway, so code_point_out is unsigned.
  int32_t code_point;
  U8_NEXT(src, *char_index, src_len, code_point);
  *code_point_out = static_cast<uint32_t>(code_point);

  // The ICU macro above moves to the next char, we want to point to the last
  // char consumed.
  (*char_index)--;

  // Validate the decoded value.
  return U_IS_UNICODE_CHAR(code_point);
}

// Reads a UTF-16 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const char16* src, int32_t src_len,
                          int32_t* char_index, uint32_t* code_point) {
  if (U16_IS_SURROGATE(src[*char_index])) {
    if (!U16_IS_SURROGATE_LEAD(src[*char_index]) ||
        *char_index + 1 >= src_len ||
        !U16_IS_TRAIL(src[*char_index + 1])) {
      // Invalid surrogate pair.
      return false;
    }

    // Valid surrogate pair.
    *code_point = U16_GET_SUPPLEMENTARY(src[*char_index],
                                        src[*char_index + 1]);
    (*char_index)++;
  } else {
    // Not a surrogate, just one 16-bit word.
    *code_point = src[*char_index];
  }

  return U_IS_UNICODE_CHAR(*code_point);
}

#if defined(WCHAR_T_IS_UTF32)
// Reads UTF-32 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const wchar_t* src, int32_t src_len,
                        int32_t* char_index, uint32_t* code_point) {
  // Conversion is easy since the source is 32-bit.
  *code_point = src[*char_index];

  // Validate the value.
  return U_IS_UNICODE_CHAR(*code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// WriteUnicodeCharacter -------------------------------------------------------

// Appends a UTF-8 character to the given 8-bit string.
void WriteUnicodeCharacter(uint32_t code_point, std::string* output) {
  if (code_point <= 0x7f) {
    // Fast path the common case of one byte.
    output->push_back(code_point);
    return;
  }

  // U8_APPEND_UNSAFE can append up to 4 bytes.
  int32_t char_offset = static_cast<int32_t>(output->length());
  output->resize(char_offset + U8_MAX_LENGTH);

  U8_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);

  // U8_APPEND_UNSAFE will advance our pointer past the inserted character, so
  // it will represent the new length of the string.
  output->resize(char_offset);
}

// Appends the given code point as a UTF-16 character to the STL string.
void WriteUnicodeCharacter(uint32_t code_point, string16* output) {
  if (U16_LENGTH(code_point) == 1) {
    // Thie code point is in the Basic Multilingual Plane (BMP).
    output->push_back(static_cast<char16>(code_point));
  } else {
    // Non-BMP characters use a double-character encoding.
    int32_t char_offset = static_cast<int32_t>(output->length());
    output->resize(char_offset + U16_MAX_LENGTH);
    U16_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);
  }
}

#if defined(WCHAR_T_IS_UTF32)
// Appends the given UTF-32 character to the given 32-bit string.
inline void WriteUnicodeCharacter(uint32_t code_point, std::wstring* output) {
  // This is the easy case, just append the character.
  output->push_back(code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// Generalized Unicode converter -----------------------------------------------

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SRC_CHAR, typename DEST_STRING>
bool ConvertUnicode(const SRC_CHAR* src, size_t src_len, DEST_STRING* output) {
  output->clear();

  // ICU requires 32-bit numbers.
  bool success = true;
  int32_t src_len32 = static_cast<int32_t>(src_len);
  for (int32_t i = 0; i < src_len32; i++) {
    uint32_t code_point;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point))
      WriteUnicodeCharacter(code_point, output);
    else
      success = false;
  }
  return success;
}


// Guesses the length of the output in UTF-8 in bytes, and reserves that amount
// of space in the given string. We also assume that the input character types
// are unsigned, which will be true for UTF-16 and -32 on our systems. We assume
// the string length is greater than zero.
template<typename CHAR>
void ReserveUTF8Output(const CHAR* src, size_t src_len, std::string* output) {
  if (src[0] < 0x80) {
    // Assume that the entire input will be ASCII.
    output->reserve(src_len);
  } else {
    // Assume that the entire input is non-ASCII and will have 3 bytes per char.
    output->reserve(src_len * 3);
  }
}

// Guesses the size of the output buffer (containing either UTF-16 or -32 data)
// given some UTF-8 input that will be converted to it. See ReserveUTF8Output.
// We assume the source length is > 0.
template<typename STRING>
void ReserveUTF16Or32Output(const char* src, size_t src_len, STRING* output) {
  if (static_cast<unsigned char>(src[0]) < 0x80) {
    // Assume the input is all ASCII, which means 1:1 correspondence.
    output->reserve(src_len);
  } else {
    // Otherwise assume that the UTF-8 sequences will have 2 bytes for each
    // character.
    output->reserve(src_len / 2);
  }
}

}  // namespace

// UTF-8 <-> Wide --------------------------------------------------------------

std::string WideToUTF8(const std::wstring& wide) {
  std::string ret;
  if (wide.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUTF8(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF8Output(src, src_len, output);
  return ConvertUnicode<wchar_t, std::string>(src, src_len, output);
}

std::wstring UTF8ToWide(const StringPiece& utf8) {
  std::wstring ret;
  if (utf8.empty())
    return ret;

  UTF8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}

bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF16Or32Output(src, src_len, output);
  return ConvertUnicode<char, std::wstring>(src, src_len, output);
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF16)

// When wide == UTF-16, then conversions are a NOP.
string16 WideToUTF16(const std::wstring& wide) {
  return wide;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  output->assign(src, src_len);
  return true;
}

std::wstring UTF16ToWide(const string16& utf16) {
  return utf16;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  output->assign(src, src_len);
  return true;
}

#elif defined(WCHAR_T_IS_UTF32)

string16 WideToUTF16(const std::wstring& wide) {
  string16 ret;
  if (wide.empty())
    return ret;

  WideToUTF16(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<wchar_t, string16>(src, src_len, output);
}

std::wstring UTF16ToWide(const string16& utf16) {
  std::wstring ret;
  if (utf16.empty())
    return ret;

  UTF16ToWide(utf16.data(), utf16.length(), &ret);
  return ret;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<char16, std::wstring>(src, src_len, output);
}

#endif  // defined(WCHAR_T_IS_UTF32)

// UTF16 <-> UTF8 --------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF32)

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF16Or32Output(src, src_len, output);
  return ConvertUnicode<char, string16>(src, src_len, output);
}

string16 UTF8ToUTF16(const std::string& utf8) {
  string16 ret;
  if (utf8.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF8ToUTF16(utf8.data(), utf8.length(), &ret);
  return ret;
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF8Output(src, src_len, output);
  return ConvertUnicode<char16, std::string>(src, src_len, output);
}

std::string UTF16ToUTF8(const string16& utf16) {
  std::string ret;
  if (utf16.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF16ToUTF8(utf16.data(), utf16.length(), &ret);
  return ret;
}

#elif defined(WCHAR_T_IS_UTF16)
// Easy case since we can use the "wide" versions we already wrote above.

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  return UTF8ToWide(src, src_len, output);
}

string16 UTF8ToUTF16(const std::string& utf8) {
  return UTF8ToWide(utf8);
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  return WideToUTF8(src, src_len, output);
}

std::string UTF16ToUTF8(const string16& utf16) {
  return WideToUTF8(utf16);
}

#endif

// Codepage <-> Wide -----------------------------------------------------------

// Convert a unicode string into the specified codepage_name.  If the codepage
// isn't found, return false.
bool WideToCodepage(const std::wstring& wide,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::string* encoded) {
  encoded->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  const UChar* uchar_src;
  int uchar_len;
#if defined(WCHAR_T_IS_UTF16)
  uchar_src = wide.c_str();
  uchar_len = static_cast<int>(wide.length());
#elif defined(WCHAR_T_IS_UTF32)
  // When wchar_t is wider than UChar (16 bits), transform |wide| into a
  // UChar* string.  Size the UChar* buffer to be large enough to hold twice
  // as many UTF-16 code points as there are UTF-16 characters, in case each
  // character translates to a UTF-16 surrogate pair, and leave room for a NUL
  // terminator.
  std::vector<UChar> wide_uchar(wide.length() * 2 + 1);
  u_strFromWCS(&wide_uchar[0], wide_uchar.size(), &uchar_len,
               wide.c_str(), wide.length(), &status);
  uchar_src = &wide_uchar[0];
  DCHECK(U_SUCCESS(status)) << "failed to convert wstring to UChar*";
#endif  // defined(WCHAR_T_IS_UTF32)

  int encoded_max_length = UCNV_GET_MAX_BYTES_FOR_STRING(uchar_len,
    ucnv_getMaxCharSize(converter));
  encoded->resize(encoded_max_length);

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, 0,
                            NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SKIP, 0,
                            NULL, NULL, &status);
      break;
    default:
      NOTREACHED();
  }

  // ucnv_fromUChars returns size not including terminating null
  int actual_size = ucnv_fromUChars(converter, &(*encoded)[0],
    encoded_max_length, uchar_src, uchar_len, &status);
  encoded->resize(actual_size);
  ucnv_close(converter);
  if (U_SUCCESS(status))
    return true;
  encoded->clear();  // Make sure the output is empty on error.
  return false;
}

// Converts a string of the given codepage into unicode.
// If the codepage isn't found, return false.
bool CodepageToWide(const std::string& encoded,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::wstring* wide) {
  wide->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  // The worst case is all the input characters are non-BMP (32-bit) ones.
  size_t uchar_max_length = encoded.length() * 2 + 1;

  UChar* uchar_dst;
#if defined(WCHAR_T_IS_UTF16)
  uchar_dst = WriteInto(wide, uchar_max_length);
#elif defined(WCHAR_T_IS_UTF32)
  // When wchar_t is wider than UChar (16 bits), convert into a temporary
  // UChar* buffer.
  std::vector<UChar> wide_uchar(uchar_max_length);
  uchar_dst = &wide_uchar[0];
#endif  // defined(WCHAR_T_IS_UTF32)

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_STOP, 0,
                          NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_SKIP, 0,
                          NULL, NULL, &status);
      break;
    default:
      NOTREACHED();
  }

  int actual_size = ucnv_toUChars(converter,
                                  uchar_dst,
                                  static_cast<int>(uchar_max_length),
                                  encoded.data(),
                                  static_cast<int>(encoded.length()),
                                  &status);
  ucnv_close(converter);
  if (!U_SUCCESS(status)) {
    wide->clear();  // Make sure the output is empty on error.
    return false;
  }

#ifdef WCHAR_T_IS_UTF32
  // When wchar_t is wider than UChar (16 bits), it's not possible to wind up
  // with any more wchar_t elements than UChar elements.  ucnv_toUChars
  // returns the number of UChar elements not including the NUL terminator, so
  // leave extra room for that.
  u_strToWCS(WriteInto(wide, actual_size + 1), actual_size + 1, &actual_size,
             uchar_dst, actual_size, &status);
  DCHECK(U_SUCCESS(status)) << "failed to convert UChar* to wstring";
#endif  // WCHAR_T_IS_UTF32

  wide->resize(actual_size);
  return true;
}

// Number formatting -----------------------------------------------------------

namespace {

struct NumberFormatSingletonTraits
    : public DefaultSingletonTraits<NumberFormat> {
  static NumberFormat* New() {
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* formatter = NumberFormat::createInstance(status);
    DCHECK(U_SUCCESS(status));
    return formatter;
  }
  // There's no ICU call to destroy a NumberFormat object other than
  // operator delete, so use the default Delete, which calls operator delete.
  // This can cause problems if a different allocator is used by this file than
  // by ICU.
};

}  // namespace

std::wstring FormatNumber(int64_t number) {
  NumberFormat* number_format =
      Singleton<NumberFormat, NumberFormatSingletonTraits>::get();

  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  UnicodeString ustr;
  number_format->format(number, ustr);

#if defined(WCHAR_T_IS_UTF16)
  return std::wstring(ustr.getBuffer(),
                      static_cast<std::wstring::size_type>(ustr.length()));
#elif defined(WCHAR_T_IS_UTF32)
  wchar_t buffer[64];  // A int64_t is less than 20 chars long,  so 64 chars
                       // leaves plenty of room for formating stuff.
  int length = 0;
  UErrorCode error = U_ZERO_ERROR;
  u_strToWCS(buffer, 64, &length, ustr.getBuffer(), ustr.length() , &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  return std::wstring(buffer, static_cast<std::wstring::size_type>(length));
#endif  // defined(WCHAR_T_IS_UTF32)
}

TrimPositions TrimWhitespaceUTF8(const std::string& input,
                                 TrimPositions positions,
                                 std::string* output) {
  // This implementation is not so fast since it converts the text encoding
  // twice. Please feel free to file a bug if this function hurts the
  // performance of Chrome.
  DCHECK(IsStringUTF8(input));
  std::wstring input_wide = UTF8ToWide(input);
  std::wstring output_wide;
  TrimPositions result = TrimWhitespace(input_wide, positions, &output_wide);
  *output = WideToUTF8(output_wide);
  return result;
}

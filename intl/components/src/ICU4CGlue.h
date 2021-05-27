/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_ICUUtils_h
#define intl_components_ICUUtils_h

#include "unicode/utypes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Result.h"
#include "mozilla/Vector.h"

namespace mozilla::intl {

enum class ICUError : uint8_t {
  OutOfMemory,
  InternalError,
};

using ICUResult = Result<Ok, ICUError>;

/**
 * The ICU status can complain about a string not being terminated, but this
 * is fine for this API, as it deals with the mozilla::Span that has a pointer
 * and a length.
 */
static inline bool ICUSuccessForStringSpan(UErrorCode status) {
  return U_SUCCESS(status) || status == U_STRING_NOT_TERMINATED_WARNING;
}

/**
 * Calling into ICU with the C-API can be a bit tricky. This function wraps up
 * the relatively risky operations involving pointers, lengths, and buffers into
 * a simpler call. This function accepts a lambda that performs the ICU call,
 * and returns the length of characters in the buffer. When using a temporary
 * stack-based buffer, the calls can often be done in one trip. However, if
 * additional memory is needed, this function will call the C-API twice, in
 * order to first get the size of the result, and then second to copy the result
 * over to the buffer.
 */
template <typename ICUStringFunction, typename Buffer>
static ICUResult FillBufferWithICUCall(Buffer& buffer,
                                       const ICUStringFunction& strFn) {
  static_assert(std::is_same<typename Buffer::CharType, char16_t>::value);

  UErrorCode status = U_ZERO_ERROR;
  int32_t length = strFn(buffer.data(), buffer.capacity(), &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    MOZ_ASSERT(length >= 0);

    if (!buffer.allocate(length)) {
      return Err(ICUError::OutOfMemory);
    }

    status = U_ZERO_ERROR;
    mozilla::DebugOnly<int32_t> length2 = strFn(buffer.data(), length, &status);
    MOZ_ASSERT(length == length2);
  }
  if (!ICUSuccessForStringSpan(status)) {
    return Err(ICUError::InternalError);
  }

  buffer.written(length);

  return Ok{};
}

/**
 * A variant of FillBufferWithICUCall that accepts a mozilla::Vector rather than
 * a Buffer.
 */
template <typename ICUStringFunction, size_t InlineSize>
static ICUResult FillVectorWithICUCall(Vector<char16_t, InlineSize>& vector,
                                       const ICUStringFunction& strFn) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t length = strFn(vector.begin(), vector.capacity(), &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    MOZ_ASSERT(length >= 0);

    if (!vector.reserve(length)) {
      return Err(ICUError::OutOfMemory);
    }

    status = U_ZERO_ERROR;
    mozilla::DebugOnly<int32_t> length2 =
        strFn(vector.begin(), length, &status);
    MOZ_ASSERT(length == length2);
  }
  if (!ICUSuccessForStringSpan(status)) {
    return Err(ICUError::InternalError);
  }

  mozilla::DebugOnly<bool> result = vector.resizeUninitialized(length);
  MOZ_ASSERT(result);
  return Ok{};
}

/**
 * ICU4C works with UTF-16 strings, but consumers of mozilla::intl may require
 * UTF-8 strings.
 */
template <typename Buffer>
[[nodiscard]] bool FillUTF8Buffer(Span<const char16_t> utf16Span,
                                  Buffer& utf8TargetBuffer) {
  static_assert(std::is_same<typename Buffer::CharType, char>::value ||
                std::is_same<typename Buffer::CharType, unsigned char>::value);

  if (utf16Span.Length() & mozilla::tl::MulOverflowMask<3>::value) {
    // Tripling the size of the buffer overflows the size_t.
    return false;
  }

  if (!utf8TargetBuffer.allocate(3 * utf16Span.Length())) {
    return false;
  }

  size_t amount = ConvertUtf16toUtf8(
      utf16Span, Span(reinterpret_cast<char*>(std::data(utf8TargetBuffer)),
                      std::size(utf8TargetBuffer)));

  utf8TargetBuffer.written(amount);

  return true;
}

/**
 * It is convenient for callers to be able to pass in UTF-8 strings to the API.
 * This function can be used to convert that to a stack-allocated UTF-16
 * mozilla::Vector that can then be passed into ICU calls.
 */
template <size_t StackSize>
[[nodiscard]] static bool FillUTF16Vector(
    Span<const char> utf8Span,
    mozilla::Vector<char16_t, StackSize>& utf16TargetVec) {
  // Per ConvertUtf8toUtf16: The length of aDest must be at least one greater
  // than the length of aSource
  if (!utf16TargetVec.reserve(utf8Span.Length() + 1)) {
    return false;
  }
  // ConvertUtf8toUtf16 fills the buffer with the data, but the length of the
  // vector is unchanged. The call to resizeUninitialized notifies the vector of
  // how much was written.
  return utf16TargetVec.resizeUninitialized(ConvertUtf8toUtf16(
      utf8Span, Span(utf16TargetVec.begin(), utf16TargetVec.capacity())));
}

}  // namespace mozilla::intl

#endif /* intl_components_ICUUtils_h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_gtest_TestBuffer_h_
#define intl_components_gtest_TestBuffer_h_

#include <string_view>
#include "mozilla/DebugOnly.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"

namespace mozilla::intl {

/**
 * A test buffer for interfacing with unified intl classes.
 * Closely resembles the FormatBuffer class, but without
 * JavaScript-specific implementation details.
 */
template <typename C, size_t inlineCapacity = 0>
class TestBuffer {
 public:
  using CharType = C;

  // Only allow moves, and not copies, as this class owns the mozilla::Vector.
  TestBuffer(TestBuffer&& other) noexcept = default;
  TestBuffer& operator=(TestBuffer&& other) noexcept = default;

  explicit TestBuffer(const size_t aSize = 0) { reserve(aSize); }

  /**
   * Ensures the buffer has enough space to accommodate |aSize| elemtns.
   */
  bool reserve(const size_t aSize) { return mBuffer.reserve(aSize); }

  /**
   * Returns the raw data inside the buffer.
   */
  CharType* data() { return mBuffer.begin(); }

  /**
   * Returns the count of elements in written to the buffer.
   */
  size_t length() const { return mBuffer.length(); }

  /**
   * Returns the buffer's overall capacity.
   */
  size_t capacity() const { return mBuffer.capacity(); }

  /**
   * Resizes the buffer to the given amount of written elements.
   * This is necessary because the buffer gets written to across
   * FFI boundaries, so this needs to happen in a separate step.
   */
  void written(size_t aAmount) {
    MOZ_ASSERT(aAmount <= mBuffer.capacity());
    mozilla::DebugOnly<bool> result = mBuffer.resizeUninitialized(aAmount);
    MOZ_ASSERT(result);
  }

  /**
   * Get a string view into the buffer, which is useful for test assertions.
   */
  std::basic_string_view<CharType> get_string_view() {
    return std::basic_string_view<CharType>(data(), length());
  }

  /**
   * Clear the buffer, allowing it to be re-used.
   */
  void clear() { mBuffer.clear(); }

  /**
   * A utility function to convert UTF-16 strings to UTF-8 strings so that they
   * can be logged to stderr.
   */
  static std::string toUtf8(mozilla::Span<const char16_t> input) {
    size_t buff_len = input.Length() * 3;
    std::string result(buff_len, ' ');
    result.reserve(buff_len);
    size_t result_len =
        ConvertUtf16toUtf8(input, mozilla::Span(result.data(), buff_len));
    result.resize(result_len);
    return result;
  }

  /**
   * String buffers, especially UTF-16, do not assert nicely, and are difficult
   * to debug. This function is verbose in that it prints the buffer contents
   * and expected contents to stderr when they do not match.
   *
   * Usage:
   *   ASSERT_TRUE(buffer.assertStringView(u"9/23/2002, 8:07:30 PM"));
   *
   * Here is what gtests output:
   *
   *   Expected equality of these values:
   *   buffer.get_string_view()
   *     Which is: { '0' (48, 0x30), '9' (57, 0x39), '/' (47, 0x2F), ... }
   *   "9/23/2002, 8:07:30 PM"
   *     Which is: 0x11600afb9
   *
   * Here is what this method outputs:
   *
   *   The buffer did not match:
   *     Buffer:
   *      u"9/23/2002, 8:07:30 PM"
   *     Expected:
   *      u"09/23/2002, 08:07:30 PM"
   */
  [[nodiscard]] bool verboseMatches(const CharType* aExpected) {
    std::basic_string_view<CharType> actualSV(data(), length());
    std::basic_string_view<CharType> expectedSV(aExpected);

    if (actualSV.compare(expectedSV) == 0) {
      return true;
    }

    static_assert(std::is_same_v<CharType, char> ||
                  std::is_same_v<CharType, char16_t>);

    std::string actual;
    std::string expected;
    const char* startQuote;

    if constexpr (std::is_same_v<CharType, char>) {
      actual = std::string(actualSV);
      expected = std::string(expectedSV);
      startQuote = "\"";
    }
    if constexpr (std::is_same_v<CharType, char16_t>) {
      actual = toUtf8(actualSV);
      expected = toUtf8(expectedSV);
      startQuote = "u\"";
    }

    fprintf(stderr, "The buffer did not match:\n");
    fprintf(stderr, "  Buffer:\n    %s%s\"\n", startQuote, actual.c_str());
    fprintf(stderr, "  Expected:\n    %s%s\"\n", startQuote, expected.c_str());

    return false;
  }

  Vector<C, inlineCapacity> mBuffer{};
};

}  // namespace mozilla::intl

#endif

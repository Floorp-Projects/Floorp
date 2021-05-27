/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_gtest_TestBuffer_h_
#define intl_components_gtest_TestBuffer_h_

#include <string_view>

namespace mozilla::intl {

/**
 * This buffer class is useful for testing formatting operations.
 */
template <typename C>
class TestBuffer {
 public:
  using CharType = C;

  bool allocate(size_t aSize) {
    mBuffer.resize(aSize);
    return true;
  }

  CharType* data() { return mBuffer.data(); }

  size_t size() const { return mBuffer.size(); }

  size_t capacity() const { return mBuffer.capacity(); }

  void written(size_t aAmount) { mWritten = aAmount; }

  template <typename C2>
  std::basic_string_view<const C2> get_string_view() {
    return std::basic_string_view<const C2>(
        reinterpret_cast<const C2*>(mBuffer.data()), mWritten);
  }

  std::vector<C> mBuffer;
  size_t mWritten = 0;
};

}  // namespace mozilla::intl

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_ICUError_h
#define intl_components_ICUError_h

#include <cstdint>

namespace mozilla::intl {

/**
 * General purpose error type for operations that can result in an ICU error.
 */
enum class ICUError : uint8_t {
  OutOfMemory,
  InternalError,
  OverflowError,
};

/**
 * Error type when a method call can only result in an internal ICU error.
 */
struct InternalError {};

}  // namespace mozilla::intl

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_ICUError_h
#define intl_components_ICUError_h

#include "mozilla/Attributes.h"
#include "mozilla/Result.h"

#include <cstdint>
#include <type_traits>

namespace mozilla::intl {

/**
 * General purpose error type for operations that can result in an ICU error.
 */
enum class ICUError : uint8_t {
  // Since we claim UnusedZero<ICUError>::value and
  // HasFreeLSB<ICUError>::value == true below, we must only use positive,
  // even enum values.

  OutOfMemory = 2,
  InternalError = 4,
  OverflowError = 6,
};

/**
 * Error type when a method call can only result in an internal ICU error.
 */
struct InternalError {
  // Since we claim UnusedZero<InternalError>::value and
  // HasFreeLSB<InternalError>::value == true below, we must only use positive,
  // even enum values.
  enum class ErrorKind : uint8_t { Unspecified = 2 };

  const ErrorKind kind = ErrorKind::Unspecified;

  constexpr InternalError() = default;

 private:
  friend struct mozilla::detail::UnusedZero<InternalError>;

  constexpr MOZ_IMPLICIT InternalError(ErrorKind aKind) : kind(aKind) {}
};

}  // namespace mozilla::intl

namespace mozilla::detail {

// Provide specializations for UnusedZero and HasFreeLSB to enable more
// efficient packing for mozilla::Result. This also avoids having to include
// the ResultVariant.h header.
//
// UnusedZero specialization:
//
// The UnusedZero specialization makes it possible to use CompactPair as the
// underlying storage type for Result. For this optimization to work, it is
// necessary that a distinct null-value is present for the error type. The
// null-value represents the success case and must be different from all actual
// error values.
// This optimization can be easily enabled when the error type is a scoped enum.
// No enum value must use zero as its value and UnusedZero must be specialized
// through the helper struct UnusedZeroEnum.
// For non-enum error types, a more complicated setup is necessary. The
// UnusedZero specialization must implement all necessary interface methods
// (i.e. `Inspect`, `Unwrap`, and `Store`) as well as all necessary constants
// and types (i.e. `StorageType`, `value`, and `nullValue`).
//
// HasFreeLSB specialization:
//
// When the value and the error type are both providing specializations for
// HasFreeLSB, Result uses an optimization to store both types within a single
// storage location. This optimization uses the least significant bit as a tag
// bit to mark the error case. And because the least significant bit is used for
// tagging, it can't be used by the error type. That means for example when the
// error type is an enum, all enum values must be even, because odd integer
// values have the least significant bit set.
// The actual HasFreeLSB specialization just needs to define `value` as a static
// constant with the value `true`.

template <>
struct UnusedZero<mozilla::intl::ICUError>
    : UnusedZeroEnum<mozilla::intl::ICUError> {};

template <>
struct UnusedZero<mozilla::intl::InternalError> {
  using Error = mozilla::intl::InternalError;
  using StorageType = std::underlying_type_t<Error::ErrorKind>;

  static constexpr bool value = true;
  static constexpr StorageType nullValue = 0;

  static constexpr Error Inspect(const StorageType& aValue) {
    return static_cast<Error::ErrorKind>(aValue);
  }
  static constexpr Error Unwrap(StorageType aValue) {
    return static_cast<Error::ErrorKind>(aValue);
  }
  static constexpr StorageType Store(Error aValue) {
    return static_cast<StorageType>(aValue.kind);
  }
};

template <>
struct HasFreeLSB<mozilla::intl::ICUError> {
  static constexpr bool value = true;
};

template <>
struct HasFreeLSB<mozilla::intl::InternalError> {
  static constexpr bool value = true;
};

}  // namespace mozilla::detail

#endif

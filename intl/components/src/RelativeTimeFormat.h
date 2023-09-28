/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_RelativeTimeFormat_h_
#define intl_components_RelativeTimeFormat_h_

#include "mozilla/Try.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/intl/NumberPart.h"

#include "unicode/ureldatefmt.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

struct RelativeTimeFormatOptions {
  enum class Style { Short, Narrow, Long };
  Style style = Style::Long;

  enum class Numeric {
    /**
     * Only strings with numeric components like `1 day ago`.
     */
    Always,
    /**
     * Natural-language strings like `yesterday` when possible,
     * otherwise strings with numeric components as in `7 months ago`.
     */
    Auto,
  };
  Numeric numeric = Numeric::Always;
};

/**
 * A RelativeTimeFormat implementation that roughly mirrors the API provided by
 * the ECMA-402 Intl.RelativeTimeFormat object.
 *
 * https://tc39.es/ecma402/#relativetimeformat-objects
 */
class RelativeTimeFormat final {
 public:
  /**
   *
   * Initialize a new RelativeTimeFormat for the provided locale and using the
   * provided options.
   *
   * https://tc39.es/ecma402/#sec-InitializeRelativeTimeFormat
   */
  static Result<UniquePtr<RelativeTimeFormat>, ICUError> TryCreate(
      const char* aLocale, const RelativeTimeFormatOptions& aOptions);

  RelativeTimeFormat() = default;

  RelativeTimeFormat(RelativeTimeFormatOptions::Numeric aNumeric,
                     URelativeDateTimeFormatter* aFormatter,
                     UFormattedRelativeDateTime* aFormattedRelativeDateTime);

  RelativeTimeFormat(const RelativeTimeFormat&) = delete;
  RelativeTimeFormat& operator=(const RelativeTimeFormat&) = delete;
  ~RelativeTimeFormat();

  enum class FormatUnit {
    Second,
    Minute,
    Hour,
    Day,
    Week,
    Month,
    Quarter,
    Year
  };

  /**
   * Formats a double to the provider buffer (either utf-8 or utf-16)
   *
   * https://tc39.es/ecma402/#sec-FormatRelativeTime
   */
  template <typename B>
  Result<Ok, ICUError> format(double aNumber, FormatUnit aUnit,
                              B& aBuffer) const {
    static_assert(
        std::is_same_v<typename B::CharType, char> ||
            std::is_same_v<typename B::CharType, char16_t>,
        "The only buffer CharTypes supported by RelativeTimeFormat are char "
        "(for UTF-8 support) and char16_t (for UTF-16 support).");

    auto fmt = mNumeric == RelativeTimeFormatOptions::Numeric::Auto
                   ? ureldatefmt_format
                   : ureldatefmt_formatNumeric;

    if constexpr (std::is_same_v<typename B::CharType, char>) {
      mozilla::Vector<char16_t, StackU16VectorSize> u16Vec;

      MOZ_TRY(FillBufferWithICUCall(
          u16Vec, [this, aNumber, aUnit, fmt](UChar* target, int32_t length,
                                              UErrorCode* status) {
            return fmt(mFormatter, aNumber, ToURelativeDateTimeUnit(aUnit),
                       target, length, status);
          }));

      if (!FillBuffer(u16Vec, aBuffer)) {
        return Err(ICUError::OutOfMemory);
      }
      return Ok{};
    } else {
      static_assert(std::is_same_v<typename B::CharType, char16_t>);

      return FillBufferWithICUCall(
          aBuffer, [this, aNumber, aUnit, fmt](UChar* target, int32_t length,
                                               UErrorCode* status) {
            return fmt(mFormatter, aNumber, ToURelativeDateTimeUnit(aUnit),
                       target, length, status);
          });
    }
  }

  /**
   * Formats the relative time to a utf-16 string, and fills the provided parts
   * vector. The string view is valid until another time is formatted.
   * Accessing the string view after this event is undefined behavior.
   *
   * This is utf-16 only because the only current use case is in
   * SpiderMonkey. Supporting utf-8 would require recalculating the offsets
   * in NumberPartVector from fixed width to variable width, which might be
   * tricky to get right and is work that won't be necessary if we switch to
   * ICU4X (see Bug 1723120).
   *
   * https://tc39.es/ecma402/#sec-FormatRelativeTimeToParts
   */
  Result<Span<const char16_t>, ICUError> formatToParts(
      double aNumber, FormatUnit aUnit, NumberPartVector& aParts) const;

 private:
  RelativeTimeFormatOptions::Numeric mNumeric =
      RelativeTimeFormatOptions::Numeric::Always;
  URelativeDateTimeFormatter* mFormatter = nullptr;
  UFormattedRelativeDateTime* mFormattedRelativeDateTime = nullptr;

  static constexpr size_t StackU16VectorSize = 128;

  URelativeDateTimeUnit ToURelativeDateTimeUnit(FormatUnit unit) const;
};

}  // namespace mozilla::intl

#endif

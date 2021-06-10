/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_DateTimePatternGenerator_h_
#define intl_components_DateTimePatternGenerator_h_

#include "unicode/udatpg.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/intl/ICU4CGlue.h"

namespace mozilla::intl {

class DateTimePatternGenerator final {
 public:
  explicit DateTimePatternGenerator(UDateTimePatternGenerator* aGenerator)
      : mGenerator(aGenerator) {
    MOZ_ASSERT(mGenerator);
  };

  // Allow moves.
  DateTimePatternGenerator(DateTimePatternGenerator&& other) noexcept
      : mGenerator(other.mGenerator) {
    other.mGenerator = nullptr;
  }
  DateTimePatternGenerator& operator=(
      DateTimePatternGenerator&& other) noexcept {
    if (this != &other) {
      mGenerator = other.mGenerator;
      other.mGenerator = nullptr;
    }
    return *this;
  }

  // Disallow copy.
  DateTimePatternGenerator(const DateTimePatternGenerator&) = delete;
  DateTimePatternGenerator& operator=(const DateTimePatternGenerator&) = delete;

  ~DateTimePatternGenerator();

  enum class Error { InternalError };

  static Result<UniquePtr<DateTimePatternGenerator>, Error> TryCreate(
      const char* aLocale);

  /**
   * Given a skeleton (a string with unordered datetime fields), get a best
   * pattern that will fit for that locale. This pattern will be filled into the
   * buffer. e.g. The skeleton "yMd" would return the pattern "M/d/y" for en-US,
   * or "dd/MM/y" for en-GB.
   */
  template <typename B>
  ICUResult GetBestPattern(Span<const char16_t> aSkeleton, B& aBuffer) {
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return udatpg_getBestPattern(mGenerator, aSkeleton.data(),
                                       static_cast<int32_t>(aSkeleton.Length()),
                                       target, length, status);
        });
  }

  /**
   * Get a skeleton (a string with unordered datetime fields) from a pattern.
   * For example, both "MMM-dd" and "dd/MMM" produce the skeleton "MMMdd".
   */
  template <typename B>
  static ICUResult GetSkeleton(Span<const char16_t> aPattern, B& aBuffer) {
    // At one time udatpg_getSkeleton required a UDateTimePatternGenerator*, but
    // now it is valid to pass in a nullptr.
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return udatpg_getSkeleton(nullptr, aPattern.data(),
                                    static_cast<int32_t>(aPattern.Length()),
                                    target, length, status);
        });
  }

  /**
   * TODO(Bug 1686965) - Temporarily get the underlying ICU object while
   * migrating to the unified API. This should be removed when completing the
   * migration.
   */
  UDateTimePatternGenerator* UnsafeGetUDateTimePatternGenerator() const {
    return mGenerator;
  }

 private:
  UDateTimePatternGenerator* mGenerator = nullptr;
};

}  // namespace mozilla::intl
#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_NumberingSystem_h_
#define intl_components_NumberingSystem_h_

#include "mozilla/intl/ICUError.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

struct UNumberingSystem;

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for working with numbering systems in
 * internationalization code. It is used in coordination with other operations
 * such as number formatting.
 */
class NumberingSystem final {
 public:
  explicit NumberingSystem(UNumberingSystem* aNumberingSystem)
      : mNumberingSystem(aNumberingSystem) {
    MOZ_ASSERT(aNumberingSystem);
  };

  // Do not allow copy as this class owns the ICU resource. Move is not
  // currently implemented, but a custom move operator could be created if
  // needed.
  NumberingSystem(const NumberingSystem&) = delete;
  NumberingSystem& operator=(const NumberingSystem&) = delete;

  ~NumberingSystem();

  /**
   * Create a NumberingSystem.
   */
  static Result<UniquePtr<NumberingSystem>, ICUError> TryCreate(
      const char* aLocale);

  /**
   * Returns the name of this numbering system.
   *
   * The returned string has the same lifetime as this NumberingSystem object.
   */
  Result<Span<const char>, ICUError> GetName();

 private:
  UNumberingSystem* mNumberingSystem = nullptr;
};

}  // namespace mozilla::intl

#endif

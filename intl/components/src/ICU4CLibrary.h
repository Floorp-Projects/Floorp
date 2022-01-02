/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_ICU4CLibrary_h
#define intl_components_ICU4CLibrary_h

#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Span.h"

#include <stddef.h>

namespace mozilla::intl {
/**
 * Wrapper around non-portable, ICU4C specific functions.
 */
class ICU4CLibrary final {
 public:
  ICU4CLibrary() = delete;

  /**
   * Initializes the ICU4C library.
   *
   * Note: This function should only be called once.
   */
  static ICUResult Initialize();

  /**
   * Releases any memory held by ICU. Any open ICU objects and resources are
   * left in an undefined state after this operation.
   *
   * NOTE: This function is not thread-safe.
   */
  static void Cleanup();

  struct MemoryFunctions {
    // These are equivalent to ICU's |UMemAllocFn|, |UMemReallocFn|, and
    // |UMemFreeFn| types. The first argument (called |context| in the ICU
    // docs) will always be nullptr and should be ignored.
    using AllocFn = void* (*)(const void*, size_t);
    using ReallocFn = void* (*)(const void*, void*, size_t);
    using FreeFn = void (*)(const void*, void*);

    /**
     * Function called when allocating memory.
     */
    AllocFn mAllocFn = nullptr;

    /**
     * Function called when reallocating memory.
     */
    ReallocFn mReallocFn = nullptr;

    /**
     * Function called when freeing memory.
     */
    FreeFn mFreeFn = nullptr;
  };

  /**
   * Sets the ICU memory functions.
   *
   * This function can only be called before the initial call to Initialize()!
   */
  static ICUResult SetMemoryFunctions(MemoryFunctions aMemoryFunctions);

  /**
   * Return the ICU version number.
   */
  static Span<const char> GetVersion();
};
}  // namespace mozilla::intl

#endif

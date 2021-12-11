/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_LocalizationBindings_h
#define mozilla_intl_l10n_LocalizationBindings_h

#include "mozilla/intl/localization_ffi_generated.h"

#include "mozilla/RefPtr.h"

namespace mozilla {

template <>
struct RefPtrTraits<intl::ffi::LocalizationRc> {
  static void AddRef(const intl::ffi::LocalizationRc* aPtr) {
    intl::ffi::localization_addref(aPtr);
  }
  static void Release(const intl::ffi::LocalizationRc* aPtr) {
    intl::ffi::localization_release(aPtr);
  }
};

}  // namespace mozilla

#endif

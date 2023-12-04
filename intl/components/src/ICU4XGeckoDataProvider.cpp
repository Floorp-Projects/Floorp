/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ICU4XGeckoDataProvider.h"

#include "ICU4XDataProvider.h"
#include "mozilla/Assertions.h"

#include <mutex>

namespace mozilla::intl {

static capi::ICU4XDataProvider* sICU4XDataProvider = nullptr;

capi::ICU4XDataProvider* GetDataProvider() {
  static std::once_flag sOnce;

  std::call_once(sOnce, []() {
    sICU4XDataProvider = capi::ICU4XDataProvider_create_compiled();
  });

  return sICU4XDataProvider;
}

void CleanupDataProvider() {
  if (!sICU4XDataProvider) {
    return;
  }

  capi::ICU4XDataProvider_destroy(sICU4XDataProvider);
  sICU4XDataProvider = nullptr;
}

}  // namespace mozilla::intl

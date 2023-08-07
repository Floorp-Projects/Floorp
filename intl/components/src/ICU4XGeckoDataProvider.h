/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_ICU4XGeckoDataProvider_h
#define intl_components_ICU4XGeckoDataProvider_h

/**
 * This component is a C/C++ API to get ICU4X data provider.
 */

namespace capi {
struct ICU4XDataProvider;
}

namespace mozilla::intl {
capi::ICU4XDataProvider* GetDataProvider();
void CleanupDataProvider();
}  // namespace mozilla::intl

#endif

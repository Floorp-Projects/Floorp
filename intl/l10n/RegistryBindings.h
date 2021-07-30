/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_RegistryBindings_h
#define mozilla_intl_l10n_RegistryBindings_h

#include "mozilla/intl/l10nregistry_ffi_generated.h"

#include "mozilla/RefPtr.h"

namespace mozilla {

template <>
struct RefPtrTraits<intl::ffi::FileSource> {
  static void AddRef(const intl::ffi::FileSource* aPtr) {
    intl::ffi::l10nfilesource_addref(aPtr);
  }
  static void Release(const intl::ffi::FileSource* aPtr) {
    intl::ffi::l10nfilesource_release(aPtr);
  }
};

template <>
class DefaultDelete<intl::ffi::GeckoFluentBundleIterator> {
 public:
  void operator()(intl::ffi::GeckoFluentBundleIterator* aPtr) const {
    fluent_bundle_iterator_destroy(aPtr);
  }
};

template <>
class DefaultDelete<intl::ffi::GeckoFluentBundleAsyncIteratorWrapper> {
 public:
  void operator()(
      intl::ffi::GeckoFluentBundleAsyncIteratorWrapper* aPtr) const {
    fluent_bundle_async_iterator_destroy(aPtr);
  }
};

template <>
struct RefPtrTraits<intl::ffi::GeckoL10nRegistry> {
  static void AddRef(const intl::ffi::GeckoL10nRegistry* aPtr) {
    intl::ffi::l10nregistry_addref(aPtr);
  }
  static void Release(const intl::ffi::GeckoL10nRegistry* aPtr) {
    intl::ffi::l10nregistry_release(aPtr);
  }
};

}  // namespace mozilla

#endif

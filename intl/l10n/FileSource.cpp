/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSource.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla::dom;

namespace mozilla::intl {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(L10nFileSource, mGlobal)

L10nFileSource::L10nFileSource(RefPtr<const ffi::FileSource> aRaw,
                               nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mRaw(std::move(aRaw)) {}

/* static */
already_AddRefed<L10nFileSource> L10nFileSource::Constructor(
    const GlobalObject& aGlobal, const nsACString& aName,
    const nsACString& aMetaSource, const nsTArray<nsCString>& aLocales,
    const nsACString& aPrePath, const dom::FileSourceOptions& aOptions,
    const Optional<Sequence<nsCString>>& aIndex, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  ffi::L10nFileSourceStatus status;

  bool allowOverrides = aOptions.mAddResourceOptions.mAllowOverrides;

  RefPtr<const ffi::FileSource> raw;
  if (aIndex.WasPassed()) {
    raw = dont_AddRef(ffi::l10nfilesource_new_with_index(
        &aName, &aMetaSource, &aLocales, &aPrePath, aIndex.Value().Elements(),
        aIndex.Value().Length(), allowOverrides, &status));
  } else {
    raw = dont_AddRef(ffi::l10nfilesource_new(
        &aName, &aMetaSource, &aLocales, &aPrePath, allowOverrides, &status));
  }

  if (PopulateError(aRv, status)) {
    return nullptr;
  }
  return MakeAndAddRef<L10nFileSource>(std::move(raw), global);
}

/* static */
already_AddRefed<L10nFileSource> L10nFileSource::CreateMock(
    const GlobalObject& aGlobal, const nsACString& aName,
    const nsACString& aMetaSource, const nsTArray<nsCString>& aLocales,
    const nsACString& aPrePath, const nsTArray<L10nFileSourceMockFile>& aFS,
    ErrorResult& aRv) {
  nsTArray<ffi::L10nFileSourceMockFile> fs(aFS.Length());
  for (const auto& file : aFS) {
    auto f = fs.AppendElement();
    f->path = file.mPath;
    f->source = file.mSource;
  }
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  ffi::L10nFileSourceStatus status;

  RefPtr<const ffi::FileSource> raw(dont_AddRef(ffi::l10nfilesource_new_mock(
      &aName, &aMetaSource, &aLocales, &aPrePath, &fs, &status)));

  if (PopulateError(aRv, status)) {
    return nullptr;
  }
  return MakeAndAddRef<L10nFileSource>(std::move(raw), global);
}

JSObject* L10nFileSource::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return L10nFileSource_Binding::Wrap(aCx, this, aGivenProto);
}

void L10nFileSource::GetName(nsCString& aRetVal) {
  ffi::l10nfilesource_get_name(mRaw.get(), &aRetVal);
}

void L10nFileSource::GetMetaSource(nsCString& aRetVal) {
  ffi::l10nfilesource_get_metasource(mRaw.get(), &aRetVal);
}

void L10nFileSource::GetLocales(nsTArray<nsCString>& aRetVal) {
  ffi::l10nfilesource_get_locales(mRaw.get(), &aRetVal);
}

void L10nFileSource::GetPrePath(nsCString& aRetVal) {
  ffi::l10nfilesource_get_prepath(mRaw.get(), &aRetVal);
}

void L10nFileSource::GetIndex(Nullable<nsTArray<nsCString>>& aRetVal) {
  bool hasIndex =
      ffi::l10nfilesource_get_index(mRaw.get(), &aRetVal.SetValue());
  if (!hasIndex) {
    aRetVal.SetNull();
  }
}

L10nFileSourceHasFileStatus L10nFileSource::HasFile(const nsACString& aLocale,
                                                    const nsACString& aPath,
                                                    ErrorResult& aRv) {
  ffi::L10nFileSourceStatus status;

  bool isPresent = false;
  bool hasValue = ffi::l10nfilesource_has_file(mRaw.get(), &aLocale, &aPath,
                                               &status, &isPresent);

  if (!PopulateError(aRv, status) && hasValue) {
    if (isPresent) {
      return L10nFileSourceHasFileStatus::Present;
    }

    return L10nFileSourceHasFileStatus::Missing;
  }
  return L10nFileSourceHasFileStatus::Unknown;
}

already_AddRefed<FluentResource> L10nFileSource::FetchFileSync(
    const nsACString& aLocale, const nsACString& aPath, ErrorResult& aRv) {
  ffi::L10nFileSourceStatus status;

  RefPtr<const ffi::FluentResource> raw =
      dont_AddRef(ffi::l10nfilesource_fetch_file_sync(mRaw.get(), &aLocale,
                                                      &aPath, &status));

  if (!PopulateError(aRv, status) && raw) {
    return MakeAndAddRef<FluentResource>(mGlobal, raw);
  }

  return nullptr;
}

already_AddRefed<Promise> L10nFileSource::FetchFile(const nsACString& aLocale,
                                                    const nsACString& aPath,
                                                    ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  ffi::L10nFileSourceStatus status;

  ffi::l10nfilesource_fetch_file(
      mRaw.get(), &aLocale, &aPath, promise,
      [](const Promise* aPromise, const ffi::FluentResource* aRes) {
        Promise* promise = const_cast<Promise*>(aPromise);

        if (aRes) {
          nsIGlobalObject* global = promise->GetGlobalObject();
          RefPtr<FluentResource> res = new FluentResource(global, aRes);
          promise->MaybeResolve(res);
        } else {
          promise->MaybeResolve(JS::NullHandleValue);
        }
      },
      &status);

  if (PopulateError(aRv, status)) {
    return nullptr;
  }

  return promise.forget();
}

/* static */
bool L10nFileSource::PopulateError(ErrorResult& aError,
                                   ffi::L10nFileSourceStatus& aStatus) {
  switch (aStatus) {
    case ffi::L10nFileSourceStatus::InvalidLocaleCode:
      aError.ThrowTypeError("Invalid locale code");
      return true;
    case ffi::L10nFileSourceStatus::EmptyName:
      aError.ThrowTypeError("Name cannot be empty.");
      return true;
    case ffi::L10nFileSourceStatus::EmptyPrePath:
      aError.ThrowTypeError("prePath cannot be empty.");
      return true;
    case ffi::L10nFileSourceStatus::EmptyResId:
      aError.ThrowTypeError("resId cannot be empty.");
      return true;

    case ffi::L10nFileSourceStatus::None:
      return false;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown status");
  return false;
}

}  // namespace mozilla::intl

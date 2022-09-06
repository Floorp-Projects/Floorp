/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_FileSource_h
#define mozilla_intl_l10n_FileSource_h

#include "nsWrapperCache.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/L10nRegistryBinding.h"
#include "mozilla/dom/FluentBinding.h"
#include "mozilla/intl/RegistryBindings.h"

class nsIGlobalObject;

namespace mozilla::intl {

class L10nFileSource : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(L10nFileSource)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(L10nFileSource)

  explicit L10nFileSource(RefPtr<const ffi::FileSource> aRaw,
                          nsIGlobalObject* aGlobal = nullptr);

  static already_AddRefed<L10nFileSource> Constructor(
      const dom::GlobalObject& aGlobal, const nsACString& aName,
      const nsACString& aMetaSource, const nsTArray<nsCString>& aLocales,
      const nsACString& aPrePath, const dom::FileSourceOptions& aOptions,
      const dom::Optional<dom::Sequence<nsCString>>& aIndex, ErrorResult& aRv);

  static already_AddRefed<L10nFileSource> CreateMock(
      const dom::GlobalObject& aGlobal, const nsACString& aName,
      const nsACString& aMetaSource, const nsTArray<nsCString>& aLocales,
      const nsACString& aPrePath,
      const nsTArray<dom::L10nFileSourceMockFile>& aFS, ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetName(nsCString& aRetVal);
  void GetMetaSource(nsCString& aRetVal);
  void GetLocales(nsTArray<nsCString>& aRetVal);
  void GetPrePath(nsCString& aRetVal);
  void GetIndex(dom::Nullable<nsTArray<nsCString>>& aRetVal);

  dom::L10nFileSourceHasFileStatus HasFile(const nsACString& aLocale,
                                           const nsACString& aPath,
                                           ErrorResult& aRv);
  already_AddRefed<dom::Promise> FetchFile(const nsACString& aLocale,
                                           const nsACString& aPath,
                                           ErrorResult& aRv);
  already_AddRefed<FluentResource> FetchFileSync(const nsACString& aLocale,
                                                 const nsACString& aPath,
                                                 ErrorResult& aRv);

  const ffi::FileSource* Raw() const { return mRaw; }

 protected:
  virtual ~L10nFileSource() = default;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  const RefPtr<const ffi::FileSource> mRaw;
  static bool PopulateError(ErrorResult& aError,
                            ffi::L10nFileSourceStatus& aStatus);
};

}  // namespace mozilla::intl

#endif

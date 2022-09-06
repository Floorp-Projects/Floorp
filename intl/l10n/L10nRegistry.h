/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_l10n_L10nRegistry_h
#define mozilla_intl_l10n_L10nRegistry_h

#include "nsIStreamLoader.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/L10nRegistryBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/intl/FluentBindings.h"
#include "mozilla/intl/RegistryBindings.h"

class nsIGlobalObject;

namespace mozilla::dom {
class L10nFileSourceDescriptor;
}

namespace mozilla::intl {

class FluentBundleAsyncIterator final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(FluentBundleAsyncIterator)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(FluentBundleAsyncIterator)

  FluentBundleAsyncIterator(
      nsIGlobalObject* aGlobal,
      UniquePtr<ffi::GeckoFluentBundleAsyncIteratorWrapper> aRaw);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  // WebIDL
  already_AddRefed<dom::Promise> Next(ErrorResult& aError);
  already_AddRefed<FluentBundleAsyncIterator> Values();

 protected:
  ~FluentBundleAsyncIterator() = default;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  UniquePtr<ffi::GeckoFluentBundleAsyncIteratorWrapper> mRaw;
};

class FluentBundleIterator final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(FluentBundleIterator)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(FluentBundleIterator)

  FluentBundleIterator(nsIGlobalObject* aGlobal,
                       UniquePtr<ffi::GeckoFluentBundleIterator> aRaw);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  // WebIDL
  void Next(dom::FluentBundleIteratorResult& aResult);
  already_AddRefed<FluentBundleIterator> Values();

 protected:
  ~FluentBundleIterator() = default;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  UniquePtr<ffi::GeckoFluentBundleIterator> mRaw;
};

class L10nRegistry final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(L10nRegistry)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(L10nRegistry)

  L10nRegistry(nsIGlobalObject* aGlobal, bool aUseIsolating);

  L10nRegistry(nsIGlobalObject* aGlobal,
               RefPtr<const ffi::GeckoL10nRegistry> aRaw);

  static already_AddRefed<L10nRegistry> Constructor(
      const dom::GlobalObject& aGlobal,
      const dom::L10nRegistryOptions& aOptions);

  static already_AddRefed<L10nRegistry> GetInstance(
      const dom::GlobalObject& aGlobal);

  static void GetParentProcessFileSourceDescriptors(
      nsTArray<dom::L10nFileSourceDescriptor>& aRetVal);
  static void RegisterFileSourcesFromParentProcess(
      const nsTArray<dom::L10nFileSourceDescriptor>& aDescriptors);

  static nsresult Load(const nsACString& aPath,
                       nsIStreamLoaderObserver* aObserver);
  static nsresult LoadSync(const nsACString& aPath, void** aData,
                           uint64_t* aSize);

  static ffi::GeckoResourceId ResourceIdToFFI(const nsCString& aResourceId);
  static ffi::GeckoResourceId ResourceIdToFFI(
      const dom::OwningUTF8StringOrResourceId& aResourceId);
  static nsTArray<ffi::GeckoResourceId> ResourceIdsToFFI(
      const nsTArray<nsCString>& aResourceIds);
  static nsTArray<ffi::GeckoResourceId> ResourceIdsToFFI(
      const nsTArray<dom::OwningUTF8StringOrResourceId>& aResourceIds);

  void GetAvailableLocales(nsTArray<nsCString>& aRetVal);

  void RegisterSources(
      const dom::Sequence<OwningNonNull<L10nFileSource>>& aSources);
  void UpdateSources(
      const dom::Sequence<OwningNonNull<L10nFileSource>>& aSources);
  void RemoveSources(const dom::Sequence<nsCString>& aSources);
  bool HasSource(const nsACString& aName, ErrorResult& aRv);
  already_AddRefed<L10nFileSource> GetSource(const nsACString& aName,
                                             ErrorResult& aRv);
  void GetSourceNames(nsTArray<nsCString>& aRetVal);
  void ClearSources();

  already_AddRefed<FluentBundleIterator> GenerateBundlesSync(
      const nsTArray<nsCString>& aLocales,
      const nsTArray<ffi::GeckoResourceId>& aResourceIds, ErrorResult& aRv);
  already_AddRefed<FluentBundleIterator> GenerateBundlesSync(
      const dom::Sequence<nsCString>& aLocales,
      const dom::Sequence<dom::OwningUTF8StringOrResourceId>& aResourceIds,
      ErrorResult& aRv);

  already_AddRefed<FluentBundleAsyncIterator> GenerateBundles(
      const nsTArray<nsCString>& aLocales,
      const nsTArray<ffi::GeckoResourceId>& aResourceIds, ErrorResult& aRv);
  already_AddRefed<FluentBundleAsyncIterator> GenerateBundles(
      const dom::Sequence<nsCString>& aLocales,
      const dom::Sequence<dom::OwningUTF8StringOrResourceId>& aResourceIds,
      ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  const ffi::GeckoL10nRegistry* Raw() const { return mRaw; }

 protected:
  virtual ~L10nRegistry() = default;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  const RefPtr<const ffi::GeckoL10nRegistry> mRaw;
  static bool PopulateError(ErrorResult& aError,
                            ffi::L10nRegistryStatus& aStatus);
};

}  // namespace mozilla::intl

#endif

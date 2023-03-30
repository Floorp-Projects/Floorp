/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "L10nRegistry.h"
#include "mozilla/RefPtr.h"
#include "mozilla/URLPreloader.h"
#include "nsIChannel.h"
#include "nsILoadInfo.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsContentUtils.h"
#include "FluentResource.h"
#include "FileSource.h"
#include "nsICategoryManager.h"
#include "mozilla/SimpleEnumerator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla::intl {

/* FluentBundleIterator */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FluentBundleIterator, mGlobal)

FluentBundleIterator::FluentBundleIterator(
    nsIGlobalObject* aGlobal, UniquePtr<ffi::GeckoFluentBundleIterator> aRaw)
    : mGlobal(aGlobal), mRaw(std::move(aRaw)) {}

JSObject* FluentBundleIterator::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return FluentBundleIterator_Binding::Wrap(aCx, this, aGivenProto);
}

void FluentBundleIterator::Next(FluentBundleIteratorResult& aResult) {
  UniquePtr<ffi::FluentBundleRc> raw(
      ffi::fluent_bundle_iterator_next(mRaw.get()));
  if (!raw) {
    aResult.mDone = true;
    return;
  }
  aResult.mDone = false;
  aResult.mValue = new FluentBundle(mGlobal, std::move(raw));
}

already_AddRefed<FluentBundleIterator> FluentBundleIterator::Values() {
  return do_AddRef(this);
}

/* FluentBundleAsyncIterator */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FluentBundleAsyncIterator, mGlobal)

FluentBundleAsyncIterator::FluentBundleAsyncIterator(
    nsIGlobalObject* aGlobal,
    UniquePtr<ffi::GeckoFluentBundleAsyncIteratorWrapper> aRaw)
    : mGlobal(aGlobal), mRaw(std::move(aRaw)) {}

JSObject* FluentBundleAsyncIterator::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FluentBundleAsyncIterator_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> FluentBundleAsyncIterator::Next(ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(mGlobal, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  ffi::fluent_bundle_async_iterator_next(
      mRaw.get(), promise,
      // callback function which will be invoked by the rust code, passing the
      // promise back in.
      [](auto* aPromise, ffi::FluentBundleRc* aBundle) {
        Promise* promise = const_cast<Promise*>(aPromise);

        FluentBundleIteratorResult res;

        if (aBundle) {
          // The Rust caller will transfer the ownership to us.
          UniquePtr<ffi::FluentBundleRc> b(aBundle);
          nsIGlobalObject* global = promise->GetGlobalObject();
          res.mValue = new FluentBundle(global, std::move(b));
          res.mDone = false;
        } else {
          res.mDone = true;
        }
        promise->MaybeResolve(res);
      });

  return promise.forget();
}

already_AddRefed<FluentBundleAsyncIterator>
FluentBundleAsyncIterator::Values() {
  return do_AddRef(this);
}

/* L10nRegistry */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(L10nRegistry, mGlobal)

L10nRegistry::L10nRegistry(nsIGlobalObject* aGlobal, bool aUseIsolating)
    : mGlobal(aGlobal),
      mRaw(dont_AddRef(ffi::l10nregistry_new(aUseIsolating))) {}

L10nRegistry::L10nRegistry(nsIGlobalObject* aGlobal,
                           RefPtr<const ffi::GeckoL10nRegistry> aRaw)
    : mGlobal(aGlobal), mRaw(std::move(aRaw)) {}

/* static */
already_AddRefed<L10nRegistry> L10nRegistry::Constructor(
    const GlobalObject& aGlobal, const L10nRegistryOptions& aOptions) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return MakeAndAddRef<L10nRegistry>(global,
                                     aOptions.mBundleOptions.mUseIsolating);
}

/* static */
already_AddRefed<L10nRegistry> L10nRegistry::GetInstance(
    const GlobalObject& aGlobal) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return MakeAndAddRef<L10nRegistry>(
      global, dont_AddRef(ffi::l10nregistry_instance_get()));
}

JSObject* L10nRegistry::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return L10nRegistry_Binding::Wrap(aCx, this, aGivenProto);
}

void L10nRegistry::GetAvailableLocales(nsTArray<nsCString>& aRetVal) {
  ffi::l10nregistry_get_available_locales(mRaw.get(), &aRetVal);
}

void L10nRegistry::RegisterSources(
    const Sequence<OwningNonNull<L10nFileSource>>& aSources) {
  nsTArray<const ffi::FileSource*> sources(aSources.Length());
  for (const auto& source : aSources) {
    sources.AppendElement(source->Raw());
  }

  ffi::l10nregistry_register_sources(mRaw.get(), &sources);
}

void L10nRegistry::UpdateSources(
    const Sequence<OwningNonNull<L10nFileSource>>& aSources) {
  nsTArray<const ffi::FileSource*> sources(aSources.Length());
  for (const auto& source : aSources) {
    sources.AppendElement(source->Raw());
  }

  ffi::l10nregistry_update_sources(mRaw.get(), &sources);
}

void L10nRegistry::RemoveSources(const Sequence<nsCString>& aSources) {
  ffi::l10nregistry_remove_sources(mRaw.get(), aSources.Elements(),
                                   aSources.Length());
}

bool L10nRegistry::HasSource(const nsACString& aName, ErrorResult& aRv) {
  ffi::L10nRegistryStatus status;

  bool result = ffi::l10nregistry_has_source(mRaw.get(), &aName, &status);
  PopulateError(aRv, status);
  return result;
}

already_AddRefed<L10nFileSource> L10nRegistry::GetSource(
    const nsACString& aName, ErrorResult& aRv) {
  ffi::L10nRegistryStatus status;

  RefPtr<const ffi::FileSource> raw(
      dont_AddRef(ffi::l10nregistry_get_source(mRaw.get(), &aName, &status)));
  if (PopulateError(aRv, status)) {
    return nullptr;
  }

  return MakeAndAddRef<L10nFileSource>(std::move(raw));
}

void L10nRegistry::GetSourceNames(nsTArray<nsCString>& aRetVal) {
  ffi::l10nregistry_get_source_names(mRaw.get(), &aRetVal);
}

void L10nRegistry::ClearSources() {
  ffi::l10nregistry_clear_sources(mRaw.get());
}

/* static */
ffi::GeckoResourceId L10nRegistry::ResourceIdToFFI(
    const nsCString& aResourceId) {
  return ffi::GeckoResourceId{
      aResourceId,
      ffi::GeckoResourceType::Required,
  };
}

/* static */
ffi::GeckoResourceId L10nRegistry::ResourceIdToFFI(
    const dom::OwningUTF8StringOrResourceId& aResourceId) {
  if (aResourceId.IsUTF8String()) {
    return ffi::GeckoResourceId{
        aResourceId.GetAsUTF8String(),
        ffi::GeckoResourceType::Required,
    };
  }
  return ffi::GeckoResourceId{
      aResourceId.GetAsResourceId().mPath,
      aResourceId.GetAsResourceId().mOptional
          ? ffi::GeckoResourceType::Optional
          : ffi::GeckoResourceType::Required,
  };
}

/* static */
nsTArray<ffi::GeckoResourceId> L10nRegistry::ResourceIdsToFFI(
    const nsTArray<nsCString>& aResourceIds) {
  nsTArray<ffi::GeckoResourceId> ffiResourceIds;
  for (const auto& resourceId : aResourceIds) {
    ffiResourceIds.EmplaceBack(ResourceIdToFFI(resourceId));
  }
  return ffiResourceIds;
}

/* static */
nsTArray<ffi::GeckoResourceId> L10nRegistry::ResourceIdsToFFI(
    const nsTArray<dom::OwningUTF8StringOrResourceId>& aResourceIds) {
  nsTArray<ffi::GeckoResourceId> ffiResourceIds;
  for (const auto& resourceId : aResourceIds) {
    ffiResourceIds.EmplaceBack(ResourceIdToFFI(resourceId));
  }
  return ffiResourceIds;
}

already_AddRefed<FluentBundleIterator> L10nRegistry::GenerateBundlesSync(
    const nsTArray<nsCString>& aLocales,
    const nsTArray<ffi::GeckoResourceId>& aResourceIds, ErrorResult& aRv) {
  ffi::L10nRegistryStatus status;
  UniquePtr<ffi::GeckoFluentBundleIterator> iter(
      ffi::l10nregistry_generate_bundles_sync(
          mRaw, aLocales.Elements(), aLocales.Length(), aResourceIds.Elements(),
          aResourceIds.Length(), &status));

  if (PopulateError(aRv, status) || !iter) {
    return nullptr;
  }

  return do_AddRef(new FluentBundleIterator(mGlobal, std::move(iter)));
}

already_AddRefed<FluentBundleIterator> L10nRegistry::GenerateBundlesSync(
    const dom::Sequence<nsCString>& aLocales,
    const dom::Sequence<dom::OwningUTF8StringOrResourceId>& aResourceIds,
    ErrorResult& aRv) {
  auto ffiResourceIds{ResourceIdsToFFI(aResourceIds)};
  return GenerateBundlesSync(aLocales, ffiResourceIds, aRv);
}

already_AddRefed<FluentBundleAsyncIterator> L10nRegistry::GenerateBundles(
    const nsTArray<nsCString>& aLocales,
    const nsTArray<ffi::GeckoResourceId>& aResourceIds, ErrorResult& aRv) {
  ffi::L10nRegistryStatus status;
  UniquePtr<ffi::GeckoFluentBundleAsyncIteratorWrapper> iter(
      ffi::l10nregistry_generate_bundles(
          mRaw, aLocales.Elements(), aLocales.Length(), aResourceIds.Elements(),
          aResourceIds.Length(), &status));
  if (PopulateError(aRv, status) || !iter) {
    return nullptr;
  }

  return do_AddRef(new FluentBundleAsyncIterator(mGlobal, std::move(iter)));
}

already_AddRefed<FluentBundleAsyncIterator> L10nRegistry::GenerateBundles(
    const dom::Sequence<nsCString>& aLocales,
    const dom::Sequence<dom::OwningUTF8StringOrResourceId>& aResourceIds,
    ErrorResult& aRv) {
  nsTArray<ffi::GeckoResourceId> resourceIds;
  for (const auto& resourceId : aResourceIds) {
    resourceIds.EmplaceBack(ResourceIdToFFI(resourceId));
  }
  return GenerateBundles(aLocales, resourceIds, aRv);
}

/* static */
void L10nRegistry::GetParentProcessFileSourceDescriptors(
    nsTArray<L10nFileSourceDescriptor>& aRetVal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsTArray<ffi::L10nFileSourceDescriptor> sources;
  ffi::l10nregistry_get_parent_process_sources(&sources);
  for (const auto& source : sources) {
    auto descriptor = aRetVal.AppendElement();
    descriptor->name() = source.name;
    descriptor->metasource() = source.metasource;
    descriptor->locales().AppendElements(std::move(source.locales));
    descriptor->prePath() = source.pre_path;
    descriptor->index().AppendElements(std::move(source.index));
  }
}

/* static */
void L10nRegistry::RegisterFileSourcesFromParentProcess(
    const nsTArray<L10nFileSourceDescriptor>& aDescriptors) {
  // This means that in content processes the L10nRegistry
  // service instance is created eagerly, not lazily.
  // It is necessary so that the instance can store the sources
  // provided in the IPC init, which, in turn, is necessary
  // for the service to be avialable for sync bundle generation.
  //
  // L10nRegistry is lightweight and performs no operations, so
  // we believe this behavior to be acceptable.
  MOZ_ASSERT(XRE_IsContentProcess());
  nsTArray<ffi::L10nFileSourceDescriptor> sources;
  for (const auto& desc : aDescriptors) {
    auto source = sources.AppendElement();
    source->name = desc.name();
    source->metasource = desc.metasource();
    source->locales.AppendElements(desc.locales());
    source->pre_path = desc.prePath();
    source->index.AppendElements(desc.index());
  }
  ffi::l10nregistry_register_parent_process_sources(&sources);
}

/* static */
nsresult L10nRegistry::Load(const nsACString& aPath,
                            nsIStreamLoaderObserver* aObserver) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aPath);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_INVALID_ARG);

  RefPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(
      getter_AddRefs(loader), uri, aObserver,
      nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_OTHER);

  return rv;
}

/* static */
nsresult L10nRegistry::LoadSync(const nsACString& aPath, void** aData,
                                uint64_t* aSize) {
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aPath);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(uri, NS_ERROR_INVALID_ARG);

  auto result = URLPreloader::ReadURI(uri);
  if (result.isOk()) {
    auto uri = result.unwrap();
    *aData = ToNewCString(uri);
    *aSize = uri.Length();
    return NS_OK;
  }

  auto err = result.unwrapErr();
  if (err != NS_ERROR_INVALID_ARG && err != NS_ERROR_NOT_INITIALIZED) {
    return err;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(
      getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_OTHER, nullptr, /* nsICookieJarSettings */
      nullptr,                               /* aPerformanceStorage */
      nullptr,                               /* aLoadGroup */
      nullptr,                               /* aCallbacks */
      nsIRequest::LOAD_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> input;
  rv = channel->Open(getter_AddRefs(input));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

  return NS_ReadInputStreamToBuffer(input, aData, -1, aSize);
}

/* static */
bool L10nRegistry::PopulateError(ErrorResult& aError,
                                 ffi::L10nRegistryStatus& aStatus) {
  switch (aStatus) {
    case ffi::L10nRegistryStatus::InvalidLocaleCode:
      aError.ThrowTypeError("Invalid locale code");
      return true;
    case ffi::L10nRegistryStatus::EmptyName:
      aError.ThrowTypeError("Name cannot be empty.");
      return true;

    case ffi::L10nRegistryStatus::None:
      return false;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown status");
  return false;
}

extern "C" {
nsresult L10nRegistryLoad(const nsACString* aPath,
                          const nsIStreamLoaderObserver* aObserver) {
  if (!aPath || !aObserver) {
    return NS_ERROR_INVALID_ARG;
  }

  return mozilla::intl::L10nRegistry::Load(
      *aPath, const_cast<nsIStreamLoaderObserver*>(aObserver));
}

nsresult L10nRegistryLoadSync(const nsACString* aPath, void** aData,
                              uint64_t* aSize) {
  if (!aPath || !aData || !aSize) {
    return NS_ERROR_INVALID_ARG;
  }

  return mozilla::intl::L10nRegistry::LoadSync(*aPath, aData, aSize);
}

void L10nRegistrySendUpdateL10nFileSources() {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsTArray<L10nFileSourceDescriptor> sources;
  L10nRegistry::GetParentProcessFileSourceDescriptors(sources);

  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  for (ContentParent* parent : parents) {
    Unused << parent->SendUpdateL10nFileSources(sources);
  }
}

}  // extern "C"

}  // namespace mozilla::intl

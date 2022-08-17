/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SharedSubResourceCache_h__
#define mozilla_SharedSubResourceCache_h__

// A cache that allows us to share subresources across documents. In order to
// use it you need to provide some types, mainly:
//
// * Loader, which implements LoaderPrincipal() and allows you to key per
//   principal. The idea is that this would be the
//   {CSS,Script,Image}Loader object.
//
// * Key (self explanatory). We might want to introduce a common key to
//   share the cache partitioning logic.
//
// * Value, which represents the final cached value. This is expected to
//   be a StyleSheet / Stencil / imgRequestProxy.
//
// * LoadingValue, which must inherit from
//   SharedSubResourceCacheLoadingValueBase (which contains the linked
//   list and the state that the cache manages). It also must provide a
//   ValueForCache() and ExpirationTime() members. For style, this is the
//   SheetLoadData.

#include "mozilla/PrincipalHashKey.h"
#include "mozilla/WeakPtr.h"
#include "nsTHashMap.h"
#include "nsIMemoryReporter.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

namespace mozilla {

enum class CachedSubResourceState {
  Miss,
  Loading,
  Pending,
  Complete,
};

template <typename Derived>
struct SharedSubResourceCacheLoadingValueBase {
  // Whether we're in the "loading" hash table.
  RefPtr<Derived> mNext;

  bool mIsLoading = false;
  bool mIsCancelled = false;

  ~SharedSubResourceCacheLoadingValueBase() {
    // Do this iteratively to avoid blowing up the stack.
    RefPtr<Derived> next = std::move(mNext);
    while (next) {
      next = std::move(next->mNext);
    }
  }
};

template <typename Traits, typename Derived>
class SharedSubResourceCache {
 private:
  using Loader = typename Traits::Loader;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  using LoadingValue = typename Traits::LoadingValue;
  static Key KeyFromLoadingValue(const LoadingValue& aValue) {
    return Traits::KeyFromLoadingValue(aValue);
  }

  const Derived& AsDerived() const {
    return *static_cast<const Derived*>(this);
  }
  Derived& AsDerived() { return *static_cast<Derived*>(this); }

 public:
  SharedSubResourceCache(const SharedSubResourceCache&) = delete;
  SharedSubResourceCache(SharedSubResourceCache&&) = delete;
  SharedSubResourceCache() = default;

  static already_AddRefed<Derived> Get() {
    static_assert(
        std::is_base_of_v<SharedSubResourceCacheLoadingValueBase<LoadingValue>,
                          LoadingValue>);

    if (sInstance) {
      return do_AddRef(sInstance);
    }
    MOZ_DIAGNOSTIC_ASSERT(!sInstance);
    RefPtr<Derived> cache = new Derived();
    cache->Init();
    sInstance = cache.get();
    return cache.forget();
  }

 public:
  struct Result {
    Value* mCompleteValue = nullptr;
    LoadingValue* mLoadingOrPendingValue = nullptr;
    CachedSubResourceState mState = CachedSubResourceState::Miss;
  };

  Result Lookup(Loader&, const Key&, bool aSyncLoad);

  // Tries to coalesce with an already existing load. The sheet state must be
  // the one that Lookup returned, if it returned a sheet.
  //
  // TODO(emilio): Maybe try to merge this with the lookup? Most consumers could
  // have a data there already.
  [[nodiscard]] bool CoalesceLoad(const Key&, LoadingValue& aNewLoad,
                                  CachedSubResourceState aExistingLoadState);

  size_t SizeOfIncludingThis(MallocSizeOf) const;

  // Puts the load into the "loading" set.
  void LoadStarted(const Key&, LoadingValue&);

  // Removes the load from the "loading" set if there.
  void LoadCompleted(LoadingValue&);

  // Inserts a value into the cache.
  void Insert(LoadingValue&);

  // Puts a load into the "pending" set.
  void DeferLoad(const Key&, LoadingValue&);

  template <typename Callback>
  void StartPendingLoadsForLoader(Loader&, const Callback& aShouldStartLoad);
  void CancelLoadsForLoader(Loader&);

  // Register a loader into the cache. This has the effect of keeping alive all
  // subresources for the origin of the loader's document until UnregisterLoader
  // is called.
  void RegisterLoader(Loader&);

  // Unregister a loader from the cache.
  //
  // If this is the loader for the last document of a given origin, then all the
  // subresources for that document will be removed from the cache. This needs
  // to be called when the document goes away, or when its principal changes.
  void UnregisterLoader(Loader&);

  void ClearInProcess(nsIPrincipal* aForPrincipal = nullptr,
                      const nsACString* aBaseDomain = nullptr);

 protected:
  void CancelPendingLoadsForLoader(Loader&);

  ~SharedSubResourceCache() {
    MOZ_DIAGNOSTIC_ASSERT(sInstance == this);
    sInstance = nullptr;
  }

  struct CompleteSubResource {
    uint32_t mExpirationTime = 0;
    RefPtr<Value> mResource;

    inline bool Expired() const;
  };

  void WillStartPendingLoad(LoadingValue&);

  nsTHashMap<Key, CompleteSubResource> mComplete;
  nsRefPtrHashtable<Key, LoadingValue> mPending;
  // The SheetLoadData pointers in mLoadingDatas below are weak references that
  // get cleaned up when StreamLoader::OnStopRequest gets called.
  //
  // Note that we hold on to all sheet loads, even if in the end they happen not
  // to be cacheable.
  nsTHashMap<Key, WeakPtr<LoadingValue>> mLoading;

  // An origin-to-number-of-registered-documents count, in order to manage cache
  // eviction as described in RegisterLoader / UnregisterLoader.
  nsTHashMap<PrincipalHashKey, uint32_t> mLoaderPrincipalRefCnt;

 protected:
  inline static Derived* sInstance;
};

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::ClearInProcess(
    nsIPrincipal* aForPrincipal, const nsACString* aBaseDomain) {
  if (!aForPrincipal && !aBaseDomain) {
    mComplete.Clear();
    return;
  }

  for (auto iter = mComplete.Iter(); !iter.Done(); iter.Next()) {
    const bool shouldRemove = [&] {
      if (aForPrincipal && iter.Key().Principal()->Equals(aForPrincipal)) {
        return true;
      }
      if (!aBaseDomain) {
        return false;
      }
      // Clear by baseDomain.
      nsIPrincipal* partitionPrincipal = iter.Key().PartitionPrincipal();

      // Clear entries with matching base domain. This includes entries
      // which are partitioned under other top level sites (= have a
      // partitionKey set).
      nsAutoCString principalBaseDomain;
      nsresult rv = partitionPrincipal->GetBaseDomain(principalBaseDomain);
      if (NS_SUCCEEDED(rv) && principalBaseDomain.Equals(*aBaseDomain)) {
        return true;
      }

      // Clear entries partitioned under aBaseDomain.
      return StoragePrincipalHelper::PartitionKeyHasBaseDomain(
          partitionPrincipal->OriginAttributesRef().mPartitionKey,
          *aBaseDomain);
    }();

    if (shouldRemove) {
      iter.Remove();
    }
  }
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::RegisterLoader(Loader& aLoader) {
  mLoaderPrincipalRefCnt.LookupOrInsert(aLoader.LoaderPrincipal(), 0) += 1;
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::UnregisterLoader(
    Loader& aLoader) {
  nsIPrincipal* prin = aLoader.LoaderPrincipal();
  auto lookup = mLoaderPrincipalRefCnt.Lookup(prin);
  MOZ_RELEASE_ASSERT(lookup);
  MOZ_RELEASE_ASSERT(lookup.Data());
  if (!--lookup.Data()) {
    lookup.Remove();
    // TODO(emilio): Do this off a timer or something maybe.
    for (auto iter = mComplete.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Key().LoaderPrincipal()->Equals(prin)) {
        iter.Remove();
      }
    }
  }
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::CancelPendingLoadsForLoader(
    Loader& aLoader) {
  AutoTArray<RefPtr<LoadingValue>, 10> arr;

  for (auto iter = mPending.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<LoadingValue>& first = iter.Data();
    LoadingValue* prev = nullptr;
    LoadingValue* current = iter.Data();
    do {
      if (&current->Loader() != &aLoader) {
        prev = current;
        current = current->mNext;
        continue;
      }
      // Detach the load from the list, mark it as cancelled, and then below
      // call SheetComplete on it.
      RefPtr<LoadingValue> strong =
          prev ? std::move(prev->mNext) : std::move(first);
      MOZ_ASSERT(strong == current);
      if (prev) {
        prev->mNext = std::move(strong->mNext);
        current = prev->mNext;
      } else {
        first = std::move(strong->mNext);
        current = first;
      }
      arr.AppendElement(std::move(strong));
    } while (current);

    if (!first) {
      iter.Remove();
    }
  }

  for (auto& loading : arr) {
    loading->DidCancelLoad();
  }
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::WillStartPendingLoad(
    LoadingValue& aData) {
  LoadingValue* curr = &aData;
  do {
    curr->Loader().WillStartPendingLoad();
  } while ((curr = curr->mNext));
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::CancelLoadsForLoader(
    Loader& aLoader) {
  CancelPendingLoadsForLoader(aLoader);

  // We can't stop in-progress loads because some other loader may care about
  // them.
  for (LoadingValue* data : mLoading.Values()) {
    MOZ_DIAGNOSTIC_ASSERT(data,
                          "We weren't properly notified and the load was "
                          "incorrectly dropped on the floor");
    for (; data; data = data->mNext) {
      if (&data->Loader() == &aLoader) {
        data->mIsCancelled = true;
      }
    }
  }
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::DeferLoad(const Key& aKey,
                                                        LoadingValue& aValue) {
  MOZ_ASSERT(KeyFromLoadingValue(aValue).KeyEquals(aKey));
  MOZ_DIAGNOSTIC_ASSERT(!aValue.mNext, "Should only defer loads once");

  mPending.InsertOrUpdate(aKey, RefPtr{&aValue});
}

template <typename Traits, typename Derived>
template <typename Callback>
void SharedSubResourceCache<Traits, Derived>::StartPendingLoadsForLoader(
    Loader& aLoader, const Callback& aShouldStartLoad) {
  AutoTArray<RefPtr<LoadingValue>, 10> arr;

  for (auto iter = mPending.Iter(); !iter.Done(); iter.Next()) {
    bool startIt = false;
    {
      LoadingValue* data = iter.Data();
      do {
        if (&data->Loader() == &aLoader) {
          if (aShouldStartLoad(*data)) {
            startIt = true;
            break;
          }
        }
      } while ((data = data->mNext));
    }
    if (startIt) {
      arr.AppendElement(std::move(iter.Data()));
      iter.Remove();
    }
  }
  for (auto& data : arr) {
    WillStartPendingLoad(*data);
    data->StartPendingLoad();
  }
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::Insert(LoadingValue& aValue) {
  auto key = KeyFromLoadingValue(aValue);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  for (const auto& entry : mComplete) {
    if (key.KeyEquals(entry.GetKey())) {
      MOZ_ASSERT(
          entry.GetData().Expired() || aValue.Loader().ShouldBypassCache(),
          "Overriding existing complete entry?");
    }
  }
#endif

  // TODO(emilio): Use counters!
  mComplete.InsertOrUpdate(key, CompleteSubResource{aValue.ExpirationTime(),
                                                    aValue.ValueForCache()});
}

template <typename Traits, typename Derived>
bool SharedSubResourceCache<Traits, Derived>::CoalesceLoad(
    const Key& aKey, LoadingValue& aNewLoad,
    CachedSubResourceState aExistingLoadState) {
  MOZ_ASSERT(KeyFromLoadingValue(aNewLoad).KeyEquals(aKey));
  // TODO(emilio): If aExistingLoadState is inconvenient, we could get rid of it
  // by paying two hash lookups...
  LoadingValue* existingLoad = nullptr;
  if (aExistingLoadState == CachedSubResourceState::Loading) {
    existingLoad = mLoading.Get(aKey);
    MOZ_ASSERT(existingLoad, "Caller lied about the state");
  } else if (aExistingLoadState == CachedSubResourceState::Pending) {
    existingLoad = mPending.GetWeak(aKey);
    MOZ_ASSERT(existingLoad, "Caller lied about the state");
  }

  if (!existingLoad) {
    return false;
  }

  if (aExistingLoadState == CachedSubResourceState::Pending &&
      !aNewLoad.ShouldDefer()) {
    // Kick the load off; someone cares about it right away
    RefPtr<LoadingValue> removedLoad;
    mPending.Remove(aKey, getter_AddRefs(removedLoad));
    MOZ_ASSERT(removedLoad == existingLoad, "Bad loading table");

    WillStartPendingLoad(*removedLoad);

    // We insert to the front instead of the back, to keep the invariant that
    // the front sheet always is the one that triggers the load.
    aNewLoad.mNext = std::move(removedLoad);
    return false;
  }

  LoadingValue* data = existingLoad;
  while (data->mNext) {
    data = data->mNext;
  }
  data->mNext = &aNewLoad;
  return true;
}

template <typename Traits, typename Derived>
auto SharedSubResourceCache<Traits, Derived>::Lookup(Loader& aLoader,
                                                     const Key& aKey,
                                                     bool aSyncLoad) -> Result {
  // Now complete sheets.
  if (auto lookup = mComplete.Lookup(aKey)) {
    const CompleteSubResource& completeSubResource = lookup.Data();
    if ((!aLoader.ShouldBypassCache() && !completeSubResource.Expired()) ||
        aLoader.HasLoaded(aKey)) {
      return {completeSubResource.mResource.get(), nullptr,
              CachedSubResourceState::Complete};
    }
  }

  if (aSyncLoad) {
    return {};
  }

  if (LoadingValue* data = mLoading.Get(aKey)) {
    return {nullptr, data, CachedSubResourceState::Loading};
  }

  if (LoadingValue* data = mPending.GetWeak(aKey)) {
    return {nullptr, data, CachedSubResourceState::Pending};
  }

  return {};
}

template <typename Traits, typename Derived>
size_t SharedSubResourceCache<Traits, Derived>::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(&AsDerived());

  n += mComplete.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& data : mComplete.Values()) {
    n += data.mResource->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::LoadStarted(
    const Key& aKey, LoadingValue& aValue) {
  MOZ_DIAGNOSTIC_ASSERT(!aValue.mIsLoading, "Already loading? How?");
  MOZ_DIAGNOSTIC_ASSERT(KeyFromLoadingValue(aValue).KeyEquals(aKey));
  MOZ_DIAGNOSTIC_ASSERT(!mLoading.Contains(aKey), "Load not coalesced?");
  aValue.mIsLoading = true;
  mLoading.InsertOrUpdate(aKey, &aValue);
}

template <typename Traits, typename Derived>
bool SharedSubResourceCache<Traits, Derived>::CompleteSubResource::Expired()
    const {
  return mExpirationTime &&
         mExpirationTime <= nsContentUtils::SecondsFromPRTime(PR_Now());
}

template <typename Traits, typename Derived>
void SharedSubResourceCache<Traits, Derived>::LoadCompleted(
    LoadingValue& aValue) {
  if (!aValue.mIsLoading) {
    return;
  }
  auto key = KeyFromLoadingValue(aValue);
  Maybe<LoadingValue*> value = mLoading.Extract(key);
  MOZ_DIAGNOSTIC_ASSERT(value);
  MOZ_DIAGNOSTIC_ASSERT(value.value() == &aValue);
  Unused << value;
  aValue.mIsLoading = false;
}

}  // namespace mozilla

#endif

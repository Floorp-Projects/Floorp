/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_inl_h
#define mozilla_dom_Promise_inl_h

#include <type_traits>

#include "mozilla/TupleCycleCollection.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla {
namespace dom {

class PromiseNativeThenHandlerBase : public PromiseNativeHandler {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseNativeThenHandlerBase)

  PromiseNativeThenHandlerBase(Promise& aPromise) : mPromise(&aPromise) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

 protected:
  virtual ~PromiseNativeThenHandlerBase() = default;

  virtual already_AddRefed<Promise> CallResolveCallback(
      JSContext* aCx, JS::Handle<JS::Value> aValue) = 0;

  virtual void Traverse(nsCycleCollectionTraversalCallback&) = 0;
  virtual void Unlink() = 0;

  RefPtr<Promise> mPromise;
};

namespace {

template <typename T, bool = IsRefcounted<std::remove_pointer_t<T>>::value,
          bool = (IsConvertible<T, nsISupports*>::value ||
                  IsConvertible<T*, nsISupports*>::value)>
struct StorageTypeHelper {
  using Type = T;
};

template <typename T>
struct StorageTypeHelper<T, true, true> {
  using Type = nsCOMPtr<T>;
};

template <typename T>
struct StorageTypeHelper<nsCOMPtr<T>, true, true> {
  using Type = nsCOMPtr<T>;
};

template <typename T>
struct StorageTypeHelper<T*, true, false> {
  using Type = RefPtr<T>;
};

template <template <typename> class SmartPtr, typename T>
struct StorageTypeHelper<SmartPtr<T>, true, false>
    : EnableIf<IsConvertible<SmartPtr<T>, T*>::value, RefPtr<T>> {};

template <typename T>
using StorageType = typename StorageTypeHelper<std::decay_t<T>>::Type;

// Helpers to choose the correct argument type based on the storage type. Smart
// pointers are converted to the corresponding raw pointer type. Everything else
// is passed by move reference.
//
// Note: We can't just use std::forward for this because the input type may be a
// raw pointer which does not match the argument type, and while the
// spec-compliant behavior there should still give us the expected results, MSVC
// considers it an illegal use of std::forward.
template <template <typename> class SmartPtr, typename T>
decltype(DeclVal<SmartPtr<T>>().get()) ArgType(SmartPtr<T>& aVal) {
  return aVal.get();
}

template <typename T>
T&& ArgType(T& aVal) {
  return std::move(aVal);
}

using ::ImplCycleCollectionUnlink;

template <typename Callback, typename... Args>
class NativeThenHandler final : public PromiseNativeThenHandlerBase {
 public:
  NativeThenHandler(Promise& aPromise, Callback&& aOnResolve, Args&&... aArgs)
      : PromiseNativeThenHandlerBase(aPromise),
        mOnResolve(std::forward<Callback>(aOnResolve)),
        mArgs(std::forward<Args>(aArgs)...) {}

 protected:
  void Traverse(nsCycleCollectionTraversalCallback& cb) override {
    auto* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mArgs)
  }

  void Unlink() override {
    auto* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mArgs)
  }

  already_AddRefed<Promise> CallResolveCallback(
      JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    return CallCallback(aCx, mOnResolve, aValue);
  }

  template <size_t... Indices>
  already_AddRefed<Promise> CallCallback(JSContext* aCx,
                                         const Callback& aHandler,
                                         JS::Handle<JS::Value> aValue,
                                         std::index_sequence<Indices...>) {
    return mOnResolve(aCx, aValue, ArgType(Get<Indices>(mArgs))...);
  }

  already_AddRefed<Promise> CallCallback(JSContext* aCx,
                                         const Callback& aHandler,
                                         JS::Handle<JS::Value> aValue) {
    return CallCallback(aCx, aHandler, aValue,
                        std::index_sequence_for<Args...>{});
  }

  Callback mOnResolve;

  Tuple<StorageType<Args>...> mArgs;
};

}  // anonymous namespace

template <typename Callback, typename... Args>
Promise::ThenResult<Callback, Args...> Promise::ThenWithCycleCollectedArgs(
    Callback&& aOnResolve, Args&&... aArgs) {
  using HandlerType = NativeThenHandler<Callback, Args...>;

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), rv);
  if (rv.Failed()) {
    return Err(rv.StealNSResult());
  }

  auto* handler =
      new (fallible) HandlerType(*promise, std::forward<Callback>(aOnResolve),
                                 std::forward<Args>(aArgs)...);

  if (!handler) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  AppendNativeHandler(handler);
  return std::move(promise);
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Promise_inl_h

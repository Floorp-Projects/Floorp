/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_inl_h
#define mozilla_dom_Promise_inl_h

#include <type_traits>
#include <utility>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/TupleCycleCollection.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

class PromiseNativeThenHandlerBase : public PromiseNativeHandler {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseNativeThenHandlerBase)

  PromiseNativeThenHandlerBase(Promise* aPromise) : mPromise(aPromise) {}

  virtual bool HasResolvedCallback() = 0;
  virtual bool HasRejectedCallback() = 0;

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

 protected:
  virtual ~PromiseNativeThenHandlerBase() = default;

  virtual already_AddRefed<Promise> CallResolveCallback(
      JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) = 0;
  virtual already_AddRefed<Promise> CallRejectCallback(
      JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) = 0;

  virtual void Traverse(nsCycleCollectionTraversalCallback&) = 0;
  virtual void Unlink() = 0;

  RefPtr<Promise> mPromise;
};

namespace {

template <typename T, bool = IsRefcounted<std::remove_pointer_t<T>>::value,
          bool = (std::is_convertible_v<T, nsISupports*> ||
                  std::is_convertible_v<T*, nsISupports*>)>
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
    : std::enable_if<std::is_convertible_v<SmartPtr<T>, T*>, RefPtr<T>> {
  using Type = typename StorageTypeHelper::enable_if::type;
};

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
decltype(std::declval<SmartPtr<T>>().get()) ArgType(SmartPtr<T>& aVal) {
  return aVal.get();
}

template <typename T>
T&& ArgType(T& aVal) {
  return std::move(aVal);
}

using ::ImplCycleCollectionUnlink;

template <typename ResolveCallback, typename RejectCallback, typename... Args>
class NativeThenHandler final : public PromiseNativeThenHandlerBase {
 public:
  /**
   * @param aPromise A promise that will be settled by the result of the
   * callbacks. Any thrown value to ErrorResult passed to those callbacks will
   * be used to reject the promise, otherwise the promise will be resolved with
   * the return value.
   * @param aOnResolve A resolve callback
   * @param aOnReject A reject callback
   * @param aArgs The custom arguments to be passed to the both callbacks. The
   * handler class will grab them to make them live long enough and to allow
   * cycle collection.
   *
   * XXX(krosylight): ideally there should be two signatures, with or without a
   * promise parameter. Unfortunately doing so confuses the compiler and errors
   * out, because nothing prevents promise from being ResolveCallback.
   */
  NativeThenHandler(Promise* aPromise, Maybe<ResolveCallback>&& aOnResolve,
                    Maybe<RejectCallback>&& aOnReject, Args&&... aArgs)
      : PromiseNativeThenHandlerBase(aPromise),
        mOnResolve(std::forward<Maybe<ResolveCallback>>(aOnResolve)),
        mOnReject(std::forward<Maybe<RejectCallback>>(aOnReject)),
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

  bool HasResolvedCallback() override { return mOnResolve.isSome(); }
  bool HasRejectedCallback() override { return mOnReject.isSome(); }

  already_AddRefed<Promise> CallResolveCallback(JSContext* aCx,
                                                JS::Handle<JS::Value> aValue,
                                                ErrorResult& aRv) override {
    return CallCallback(aCx, *mOnResolve, aValue, aRv);
  }
  already_AddRefed<Promise> CallRejectCallback(JSContext* aCx,
                                               JS::Handle<JS::Value> aValue,
                                               ErrorResult& aRv) override {
    return CallCallback(aCx, *mOnReject, aValue, aRv);
  }

  template <typename TCallback, size_t... Indices>
  already_AddRefed<Promise> CallCallback(JSContext* aCx,
                                         const TCallback& aHandler,
                                         JS::Handle<JS::Value> aValue,
                                         ErrorResult& aRv,
                                         std::index_sequence<Indices...>) {
    return aHandler(aCx, aValue, aRv, ArgType(std::get<Indices>(mArgs))...);
  }

  template <typename TCallback>
  auto CallCallback(JSContext* aCx, const TCallback& aHandler,
                    JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
    return CallCallback(aCx, aHandler, aValue, aRv,
                        std::index_sequence_for<Args...>{});
  }

  Maybe<ResolveCallback> mOnResolve;
  Maybe<RejectCallback> mOnReject;

  std::tuple<StorageType<Args>...> mArgs;
};

}  // anonymous namespace

template <typename ResolveCallback, typename RejectCallback, typename... Args>
Promise::ThenResult<ResolveCallback, Args...>
Promise::ThenCatchWithCycleCollectedArgsImpl(
    Maybe<ResolveCallback>&& aOnResolve, Maybe<RejectCallback>&& aOnReject,
    Args&&... aArgs) {
  using HandlerType =
      NativeThenHandler<ResolveCallback, RejectCallback, Args...>;

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), rv);
  if (rv.Failed()) {
    return Err(rv.StealNSResult());
  }

  auto* handler = new (fallible)
      HandlerType(promise, std::forward<Maybe<ResolveCallback>>(aOnResolve),
                  std::forward<Maybe<RejectCallback>>(aOnReject),
                  std::forward<Args>(aArgs)...);

  if (!handler) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  AppendNativeHandler(handler);
  return std::move(promise);
}

template <typename ResolveCallback, typename RejectCallback, typename... Args>
Promise::ThenResult<ResolveCallback, Args...>
Promise::ThenCatchWithCycleCollectedArgs(ResolveCallback&& aOnResolve,
                                         RejectCallback&& aOnReject,
                                         Args&&... aArgs) {
  return ThenCatchWithCycleCollectedArgsImpl(Some(aOnResolve), Some(aOnReject),
                                             std::forward<Args>(aArgs)...);
}

template <typename Callback, typename... Args>
Promise::ThenResult<Callback, Args...> Promise::ThenWithCycleCollectedArgs(
    Callback&& aOnResolve, Args&&... aArgs) {
  return ThenCatchWithCycleCollectedArgsImpl(Some(aOnResolve),
                                             Maybe<Callback>(Nothing()),
                                             std::forward<Args>(aArgs)...);
}

template <typename Callback, typename... Args>
Promise::ThenResult<Callback, Args...> Promise::CatchWithCycleCollectedArgs(
    Callback&& aOnReject, Args&&... aArgs) {
  return ThenCatchWithCycleCollectedArgsImpl(Maybe<Callback>(Nothing()),
                                             Some(aOnReject),
                                             std::forward<Args>(aArgs)...);
}

template <typename ResolveCallback, typename RejectCallback, typename... Args>
void Promise::AddCallbacksWithCycleCollectedArgs(ResolveCallback&& aOnResolve,
                                                 RejectCallback&& aOnReject,
                                                 Args&&... aArgs) {
  auto onResolve =
      [aOnResolve](JSContext* aCx, JS::HandleValue value, ErrorResult& aRv,
                   StorageType<Args>&&... aArgs) -> already_AddRefed<Promise> {
    aOnResolve(aCx, value, aRv, aArgs...);
    return nullptr;
  };
  auto onReject =
      [aOnReject](JSContext* aCx, JS::HandleValue value, ErrorResult& aRv,
                  StorageType<Args>&&... aArgs) -> already_AddRefed<Promise> {
    aOnReject(aCx, value, aRv, aArgs...);
    return nullptr;
  };

  // Note: explicit template parameters for clang<7/gcc<8 without "Template
  // argument deduction for class templates" support
  AppendNativeHandler(
      new NativeThenHandler<decltype(onResolve), decltype(onReject), Args...>(
          nullptr, Some(onResolve), Some(onReject),
          std::forward<Args>(aArgs)...));
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_Promise_inl_h

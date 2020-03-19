/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_initializedonce_h__
#define mozilla_dom_indexeddb_initializedonce_h__

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"

#include <type_traits>

// This file is not exported and is only meant to be included in IndexedDB
// source files.

// TODO: However, it is meant to be moved to mfbt, and therefore already placed
// in the mozilla namespace.

namespace mozilla {

enum struct LazyInit { Allow, AllowResettable, ForbidResettable };

namespace ValueCheckPolicies {
template <typename T>
struct AllowAnyValue {
  constexpr static bool Check(const T& /*aValue*/) { return true; }
};

template <typename T>
struct ConvertsToTrue {
  constexpr static bool Check(const T& aValue) {
    return static_cast<bool>(aValue);
  }
};
}  // namespace ValueCheckPolicies

// A kind of mozilla::Maybe that can only be initialized and cleared once. It
// cannot be re-initialized. This is a more stateful than a const Maybe<T> in
// that it can be cleared, but much less stateful than a non-const Maybe<T>
// which could be reinitialized multiple times. Can only be used with const T
// to ensure that the contents cannot be modified either.
// TODO: Make constructors constexpr when Maybe's constructors are constexpr
// (Bug 1601336).
template <typename T, LazyInit LazyInitVal = LazyInit::ForbidResettable,
          template <typename> class ValueCheckPolicy =
              ValueCheckPolicies::AllowAnyValue>
class InitializedOnce final {
  static_assert(std::is_const_v<T>);

 public:
  template <typename Dummy = void>
  explicit InitializedOnce(
      std::enable_if_t<LazyInitVal == LazyInit::Allow ||
                           LazyInitVal == LazyInit::AllowResettable,
                       Dummy>* = nullptr) {}

  template <typename Arg0, typename... Args>
  explicit InitializedOnce(Arg0&& aArg0, Args&&... aArgs)
      : mMaybe{Some(std::remove_const_t<T>{std::forward<Arg0>(aArg0),
                                           std::forward<Args>(aArgs)...})} {
    MOZ_ASSERT(ValueCheckPolicy<T>::Check(*mMaybe));
  }

  InitializedOnce(const InitializedOnce&) = delete;
  InitializedOnce(InitializedOnce&& aOther) : mMaybe{std::move(aOther.mMaybe)} {
    static_assert(LazyInitVal == LazyInit::AllowResettable ||
                  LazyInitVal == LazyInit::ForbidResettable);
#ifdef DEBUG
    aOther.mWasReset = true;
#endif
  }
  InitializedOnce& operator=(const InitializedOnce&) = delete;
  InitializedOnce& operator=(InitializedOnce&& aOther) {
    static_assert(LazyInitVal == LazyInit::AllowResettable);
    MOZ_ASSERT(!mWasReset);
    MOZ_ASSERT(!mMaybe);
    mMaybe.~Maybe<std::remove_const_t<T>>();
    new (&mMaybe) Maybe<T>{std::move(aOther.mMaybe)};
#ifdef DEBUG
    aOther.mWasReset = true;
#endif
    return *this;
  }

  template <typename... Args, typename Dummy = void>
  std::enable_if_t<LazyInitVal == LazyInit::Allow ||
                       LazyInitVal == LazyInit::AllowResettable,
                   Dummy>
  init(Args&&... aArgs) {
    MOZ_ASSERT(mMaybe.isNothing());
    MOZ_ASSERT(!mWasReset);
    mMaybe.emplace(std::remove_const_t<T>{std::forward<Args>(aArgs)...});
    MOZ_ASSERT(ValueCheckPolicy<T>::Check(*mMaybe));
  }

  explicit operator bool() const { return isSome(); }
  bool isSome() const { return mMaybe.isSome(); }
  bool isNothing() const { return mMaybe.isNothing(); }

  T& operator*() const { return *mMaybe; }
  T* operator->() const { return mMaybe.operator->(); }

  template <typename Dummy = void>
  std::enable_if_t<LazyInitVal == LazyInit::AllowResettable ||
                       LazyInitVal == LazyInit::ForbidResettable,
                   Dummy>
  reset() {
    MOZ_ASSERT(mMaybe.isSome());
    maybeReset();
  }

  template <typename Dummy = void>
  std::enable_if_t<LazyInitVal == LazyInit::AllowResettable ||
                       LazyInitVal == LazyInit::ForbidResettable,
                   Dummy>
  maybeReset() {
    mMaybe.reset();
#ifdef DEBUG
    mWasReset = true;
#endif
  }

  template <typename Dummy = T>
  std::enable_if_t<LazyInitVal == LazyInit::AllowResettable ||
                       LazyInitVal == LazyInit::ForbidResettable,
                   Dummy>
  release() {
    MOZ_ASSERT(mMaybe.isSome());
    auto res = std::move(mMaybe.ref());
    maybeReset();
    return res;
  }

 private:
  Maybe<std::remove_const_t<T>> mMaybe;
#ifdef DEBUG
  bool mWasReset = false;
#endif
};

template <typename T, LazyInit LazyInitVal = LazyInit::ForbidResettable>
using InitializedOnceMustBeTrue =
    InitializedOnce<T, LazyInitVal, ValueCheckPolicies::ConvertsToTrue>;

}  // namespace mozilla

#endif

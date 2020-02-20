/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MaybeDiscarded_h
#define mozilla_dom_MaybeDiscarded_h

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

// Wrapper type for a WindowContext or BrowsingContext instance which may be
// discarded, and thus unavailable in the current process. This type is used to
// pass WindowContext and BrowsingContext instances over IPC, as they may be
// discarded in the receiving process.
//
// A MaybeDiscarded can generally be implicitly converted to from a
// BrowsingContext* or WindowContext*, but requires an explicit check of
// |IsDiscarded| and call to |get| to read from.
template <typename T>
class MaybeDiscarded {
 public:
  MaybeDiscarded() = default;
  MaybeDiscarded(MaybeDiscarded<T>&&) = default;
  MaybeDiscarded(const MaybeDiscarded<T>&) = default;

  // Construct from raw pointers and |nullptr|.
  MOZ_IMPLICIT MaybeDiscarded(T* aRawPtr)
      : mId(aRawPtr ? aRawPtr->Id() : 0), mPtr(aRawPtr) {}
  MOZ_IMPLICIT MaybeDiscarded(decltype(nullptr)) {}

  // Construct from |RefPtr<I>|
  template <typename I,
            typename = std::enable_if_t<std::is_convertible_v<I*, T*>>>
  MOZ_IMPLICIT MaybeDiscarded(RefPtr<I>&& aPtr)
      : mId(aPtr ? aPtr->Id() : 0), mPtr(std::move(aPtr)) {}
  template <typename I,
            typename = std::enable_if_t<std::is_convertible_v<I*, T*>>>
  MOZ_IMPLICIT MaybeDiscarded(const RefPtr<I>& aPtr)
      : mId(aPtr ? aPtr->Id() : 0), mPtr(aPtr) {}

  // Basic assignment operators.
  MaybeDiscarded<T>& operator=(const MaybeDiscarded<T>&) = default;
  MaybeDiscarded<T>& operator=(MaybeDiscarded<T>&&) = default;
  MaybeDiscarded<T>& operator=(decltype(nullptr)) {
    mId = 0;
    mPtr = nullptr;
    return *this;
  }
  MaybeDiscarded<T>& operator=(T* aRawPtr) {
    mId = aRawPtr ? aRawPtr->Id() : 0;
    mPtr = aRawPtr;
    return *this;
  }
  template <typename I>
  MaybeDiscarded<T>& operator=(const RefPtr<I>& aRhs) {
    mId = aRhs ? aRhs->Id() : 0;
    mPtr = aRhs;
    return *this;
  }
  template <typename I>
  MaybeDiscarded<T>& operator=(RefPtr<I>&& aRhs) {
    mId = aRhs ? aRhs->Id() : 0;
    mPtr = std::move(aRhs);
    return *this;
  }

  // Validate that the value is neither discarded nor null.
  bool IsNullOrDiscarded() const { return !mPtr || mPtr->IsDiscarded(); }
  bool IsDiscarded() const { return IsNullOrDiscarded() && !IsNull(); }
  bool IsNull() const { return mId == 0; }

  // Extract the wrapped |T|. Must not be called on a discarded |T|.
  T* get() const {
    MOZ_DIAGNOSTIC_ASSERT(!IsDiscarded());
    return mPtr.get();
  }
  already_AddRefed<T> forget() {
    MOZ_DIAGNOSTIC_ASSERT(!IsDiscarded());
    return mPtr.forget();
  }

  // Like "get", but gets the "Canonical" version of the type. This method may
  // only be called in the parent process.
  auto get_canonical() const -> decltype(get()->Canonical()) {
    if (get()) {
      return get()->Canonical();
    } else {
      return nullptr;
    }
  }

  // The ID for the context wrapped by this MaybeDiscarded. This ID comes from a
  // remote process, and should generally only be used for logging. A
  // BrowsingContext with this ID may not exist in the current process.
  uint64_t ContextId() const { return mId; }

  // Tries to get the wrapped value, disregarding discarded status.
  // This may return |nullptr| for a non-null |MaybeDiscarded|, in the case that
  // the target is no longer available in this process.
  T* GetMaybeDiscarded() const { return mPtr.get(); }

  // Clear the value to a discarded state with the given ID.
  void SetDiscarded(uint64_t aId) {
    mId = aId;
    mPtr = nullptr;
  }

  // Comparison operators required by IPDL
  bool operator==(const MaybeDiscarded<T>& aRhs) const {
    return mId == aRhs.mId && mPtr == aRhs.mPtr;
  }
  bool operator!=(const MaybeDiscarded<T>& aRhs) const {
    return !operator==(aRhs);
  }

 private:
  uint64_t mId = 0;
  RefPtr<T> mPtr;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MaybeDiscarded_h

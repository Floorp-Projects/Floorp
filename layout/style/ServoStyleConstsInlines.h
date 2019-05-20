/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Some inline functions declared in cbindgen.toml */

#ifndef mozilla_ServoStyleConstsInlines_h
#define mozilla_ServoStyleConstsInlines_h

#include "mozilla/ServoStyleConsts.h"
#include "nsGkAtoms.h"
#include <type_traits>
#include <new>

// TODO(emilio): there are quite a few other implementations scattered around
// that should move here.

namespace mozilla {

template <typename T>
inline StyleOwnedSlice<T>::StyleOwnedSlice(const StyleOwnedSlice& aOther) {
  len = aOther.len;
  if (!len) {
    ptr = (T*)alignof(T);
  } else {
    ptr = (T*)malloc(len * sizeof(T));
    size_t i = 0;
    for (const T& elem : aOther.AsSpan()) {
      new (ptr + i++) T(elem);
    }
  }
}

template <typename T>
inline StyleOwnedSlice<T>::StyleOwnedSlice(StyleOwnedSlice&& aOther) {
  len = aOther.len;
  ptr = aOther.ptr;
  aOther.ptr = (T*)alignof(T);
  aOther.len = 0;
}

template <typename T>
inline void StyleOwnedSlice<T>::Clear() {
  if (!len) {
    return;
  }
  for (size_t i : IntegerRange(len)) {
    ptr[i].~T();
  }
  free(ptr);
  ptr = (T*)alignof(T);
  len = 0;
}

template <typename T>
inline StyleOwnedSlice<T>::~StyleOwnedSlice() {
  Clear();
}

// This code is basically a C++ port of the Arc::clone() implementation in
// servo/components/servo_arc/lib.rs.
static constexpr const size_t kStaticRefcount =
    std::numeric_limits<size_t>::max();
static constexpr const size_t kMaxRefcount =
    std::numeric_limits<intptr_t>::max();
static constexpr const uint64_t kArcSliceCanary = 0xf3f3f3f3f3f3f3f3;

#define ASSERT_CANARY \
  MOZ_DIAGNOSTIC_ASSERT(_0.ptr->data.header.header == kArcSliceCanary, "Uh?");

template <typename T>
inline StyleArcSlice<T>::StyleArcSlice() {
  _0.ptr = reinterpret_cast<decltype(_0.ptr)>(Servo_StyleArcSlice_EmptyPtr());
}

template <typename T>
inline StyleArcSlice<T>::StyleArcSlice(const StyleArcSlice& aOther) {
  MOZ_DIAGNOSTIC_ASSERT(aOther._0.ptr);
  _0.ptr = aOther._0.ptr;
  if (_0.ptr->count.load(std::memory_order_relaxed) != kStaticRefcount) {
    auto old_size = _0.ptr->count.fetch_add(1, std::memory_order_relaxed);
    if (MOZ_UNLIKELY(old_size > kMaxRefcount)) {
      ::abort();
    }
  }
  ASSERT_CANARY
}

template <typename T>
inline StyleArcSlice<T>::StyleArcSlice(
    const StyleForgottenArcSlicePtr<T>& aPtr) {
  // See the forget() implementation to see why reinterpret_cast() is ok.
  _0.ptr = reinterpret_cast<decltype(_0.ptr)>(aPtr._0);
  ASSERT_CANARY
}

template <typename T>
inline size_t StyleArcSlice<T>::Length() const {
  ASSERT_CANARY
  return _0.ptr->data.header.length;
}

template <typename T>
inline bool StyleArcSlice<T>::IsEmpty() const {
  ASSERT_CANARY
  return Length() == 0;
}

template <typename T>
inline Span<const T> StyleArcSlice<T>::AsSpan() const {
  ASSERT_CANARY
  return MakeSpan(_0.ptr->data.slice, Length());
}

template <typename T>
inline bool StyleArcSlice<T>::operator==(const StyleArcSlice& aOther) const {
  ASSERT_CANARY
  return AsSpan() == aOther.AsSpan();
}

template <typename T>
inline bool StyleArcSlice<T>::operator!=(const StyleArcSlice& aOther) const {
  return !(*this == aOther);
}

// This is a C++ port-ish of Arc::drop().
template <typename T>
inline StyleArcSlice<T>::~StyleArcSlice() {
  ASSERT_CANARY
  if (_0.ptr->count.load(std::memory_order_relaxed) == kStaticRefcount) {
    return;
  }
  if (_0.ptr->count.fetch_sub(1, std::memory_order_release) != 1) {
    return;
  }
  _0.ptr->count.load(std::memory_order_acquire);
  for (T& elem : MakeSpan(_0.ptr->data.slice, Length())) {
    elem.~T();
  }
  free(_0.ptr);  // Drop the allocation now.
}

#undef ASSERT_CANARY

inline bool StyleAtom::IsStatic() const { return !!(_0 & 1); }

inline nsAtom* StyleAtom::AsAtom() const {
  if (IsStatic()) {
    return const_cast<nsStaticAtom*>(&detail::gGkAtoms.mAtoms[(_0 & ~1) >> 1]);
  }
  return reinterpret_cast<nsAtom*>(_0);
}

inline StyleAtom::~StyleAtom() {
  if (!IsStatic()) {
    AsAtom()->Release();
  }
}

inline StyleAtom::StyleAtom(const StyleAtom& aOther) : _0(aOther._0) {
  if (!IsStatic()) {
    reinterpret_cast<nsAtom*>(_0)->AddRef();
  }
}

inline nsAtom* StyleCustomIdent::AsAtom() const { return _0.AsAtom(); }

inline nsDependentCSubstring StyleOwnedStr::AsString() const {
  Span<const uint8_t> s = _0.AsSpan();
  return nsDependentCSubstring(reinterpret_cast<const char*>(s.Elements()),
                               s.Length());
}

template <typename T>
inline Span<const T> StyleGenericTransform<T>::Operations() const {
  return _0.AsSpan();
}

template <typename T>
inline bool StyleGenericTransform<T>::IsNone() const {
  return Operations().IsEmpty();
}

inline StyleAngle StyleAngle::Zero() { return {0.0f}; }

inline float StyleAngle::ToDegrees() const { return _0; }

inline double StyleAngle::ToRadians() const {
  return double(ToDegrees()) * M_PI / 180.0;
}

}  // namespace mozilla

#endif

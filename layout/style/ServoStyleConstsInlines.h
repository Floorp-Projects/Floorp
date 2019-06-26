/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Some inline functions declared in cbindgen.toml */

#ifndef mozilla_ServoStyleConstsInlines_h
#define mozilla_ServoStyleConstsInlines_h

#include "mozilla/ServoStyleConsts.h"
#include "mozilla/URLExtraData.h"
#include "nsGkAtoms.h"
#include "MainThreadUtils.h"
#include "nsNetUtil.h"
#include <type_traits>
#include <new>

// TODO(emilio): there are quite a few other implementations scattered around
// that should move here.

namespace mozilla {

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
inline void StyleOwnedSlice<T>::CopyFrom(const StyleOwnedSlice& aOther) {
  Clear();
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
inline void StyleOwnedSlice<T>::SwapElements(StyleOwnedSlice& aOther) {
  std::swap(ptr, aOther.ptr);
  std::swap(len, aOther.len);
}

template <typename T>
inline StyleOwnedSlice<T>::StyleOwnedSlice(const StyleOwnedSlice& aOther)
    : StyleOwnedSlice() {
  CopyFrom(aOther);
}

template <typename T>
inline StyleOwnedSlice<T>::StyleOwnedSlice(StyleOwnedSlice&& aOther)
    : StyleOwnedSlice() {
  SwapElements(aOther);
}

template <typename T>
inline StyleOwnedSlice<T>& StyleOwnedSlice<T>::operator=(
    const StyleOwnedSlice& aOther) {
  CopyFrom(aOther);
  return *this;
}

template <typename T>
inline StyleOwnedSlice<T>& StyleOwnedSlice<T>::operator=(
    StyleOwnedSlice&& aOther) {
  Clear();
  SwapElements(aOther);
  return *this;
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

template <typename T>
inline void StyleArcInner<T>::IncrementRef() {
  if (count.load(std::memory_order_relaxed) != kStaticRefcount) {
    auto old_size = count.fetch_add(1, std::memory_order_relaxed);
    if (MOZ_UNLIKELY(old_size > kMaxRefcount)) {
      ::abort();
    }
  }
}

// This is a C++ port-ish of Arc::drop().
template <typename T>
inline bool StyleArcInner<T>::DecrementRef() {
  if (count.load(std::memory_order_relaxed) == kStaticRefcount) {
    return false;
  }
  if (count.fetch_sub(1, std::memory_order_release) != 1) {
    return false;
  }
  count.load(std::memory_order_acquire);
  MOZ_LOG_DTOR(this, "ServoArc", 8);
  return true;
}

static constexpr const uint64_t kArcSliceCanary = 0xf3f3f3f3f3f3f3f3;

#define ASSERT_CANARY \
  MOZ_DIAGNOSTIC_ASSERT(_0.ptr->data.header.header == kArcSliceCanary, "Uh?");

template <typename T>
inline StyleArcSlice<T>::StyleArcSlice() {
  _0.ptr = reinterpret_cast<decltype(_0.ptr)>(Servo_StyleArcSlice_EmptyPtr());
  ASSERT_CANARY
}

template <typename T>
inline StyleArcSlice<T>::StyleArcSlice(const StyleArcSlice& aOther) {
  MOZ_DIAGNOSTIC_ASSERT(aOther._0.ptr);
  _0.ptr = aOther._0.ptr;
  _0.ptr->IncrementRef();
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

template <typename T>
inline StyleArcSlice<T>::~StyleArcSlice() {
  ASSERT_CANARY
  if (MOZ_LIKELY(!_0.ptr->DecrementRef())) {
    return;
  }
  for (T& elem : MakeSpan(_0.ptr->data.slice, Length())) {
    elem.~T();
  }
  free(_0.ptr);  // Drop the allocation now.
}

#undef ASSERT_CANARY

template <typename T>
inline StyleArc<T>::StyleArc(const StyleArc& aOther) : p(aOther.p) {
  p->IncrementRef();
}

template <typename T>
inline void StyleArc<T>::Release() {
  if (MOZ_LIKELY(!p->DecrementRef())) {
    return;
  }
  p->data.~T();
  free(p);
}

template <typename T>
inline StyleArc<T>& StyleArc<T>::operator=(const StyleArc& aOther) {
  if (p != aOther.p) {
    Release();
    p = aOther.p;
    p->IncrementRef();
  }
  return *this;
}

template <typename T>
inline StyleArc<T>& StyleArc<T>::operator=(StyleArc&& aOther) {
  std::swap(p, aOther.p);
  return *this;
}

template <typename T>
inline StyleArc<T>::~StyleArc() {
  Release();
}

inline bool StyleAtom::IsStatic() const { return !!(_0 & 1); }

inline nsAtom* StyleAtom::AsAtom() const {
  if (IsStatic()) {
    auto* atom = reinterpret_cast<const nsStaticAtom*>(
        reinterpret_cast<const uint8_t*>(&detail::gGkAtoms) + (_0 >> 1));
    MOZ_ASSERT(atom->IsStatic());
    return const_cast<nsStaticAtom*>(atom);
  }
  return reinterpret_cast<nsAtom*>(_0);
}

inline StyleAtom::~StyleAtom() {
  if (!IsStatic()) {
    AsAtom()->Release();
  }
}

inline StyleAtom::StyleAtom(already_AddRefed<nsAtom> aAtom) {
  nsAtom* atom = aAtom.take();
  if (atom->IsStatic()) {
    ptrdiff_t offset = reinterpret_cast<const uint8_t*>(atom->AsStatic()) -
        reinterpret_cast<const uint8_t*>(&detail::gGkAtoms);
    _0 = (offset << 1) | 1;
  } else {
    _0 = reinterpret_cast<uintptr_t>(atom);
  }
  MOZ_ASSERT(IsStatic() == atom->IsStatic());
  MOZ_ASSERT(AsAtom() == atom);
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

inline bool StyleUrlExtraData::IsShared() const { return !!(_0 & 1); }

inline StyleUrlExtraData::~StyleUrlExtraData() {
  if (!IsShared()) {
    reinterpret_cast<URLExtraData*>(_0)->Release();
  }
}

inline const URLExtraData& StyleUrlExtraData::get() const {
  if (IsShared()) {
    return *URLExtraData::sShared[_0 >> 1].get();
  }
  return *reinterpret_cast<const URLExtraData*>(_0);
}

inline nsDependentCSubstring StyleCssUrl::SpecifiedSerialization() const {
  return _0->serialization.AsString();
}

inline const URLExtraData& StyleCssUrl::ExtraData() const {
  return _0->extra_data.get();
}

inline StyleLoadData& StyleCssUrl::LoadData() const {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  if (MOZ_LIKELY(_0->load_data.tag == StyleLoadDataSource::Tag::Owned)) {
    return const_cast<StyleLoadData&>(_0->load_data.owned._0);
  }
  return const_cast<StyleLoadData&>(*Servo_LoadData_GetLazy(&_0->load_data));
}

inline nsIURI* StyleCssUrl::GetURI() const {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  auto& loadData = LoadData();
  if (!loadData.tried_to_resolve) {
    loadData.tried_to_resolve = true;
    NS_NewURI(getter_AddRefs(loadData.resolved), SpecifiedSerialization(),
              nullptr, ExtraData().BaseURI());
  }
  return loadData.resolved.get();
}

inline nsDependentCSubstring StyleComputedUrl::SpecifiedSerialization() const {
  return _0.SpecifiedSerialization();
}
inline const URLExtraData& StyleComputedUrl::ExtraData() const {
  return _0.ExtraData();
}
inline StyleLoadData& StyleComputedUrl::LoadData() const {
  return _0.LoadData();
}
inline CORSMode StyleComputedUrl::CorsMode() const {
  switch (_0._0->cors_mode) {
    case StyleCorsMode::Anonymous:
      return CORSMode::CORS_ANONYMOUS;
    case StyleCorsMode::None:
      return CORSMode::CORS_NONE;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown cors-mode from style?");
      return CORSMode::CORS_NONE;
  }
}
inline nsIURI* StyleComputedUrl::GetURI() const { return _0.GetURI(); }

inline bool StyleComputedUrl::IsLocalRef() const {
  return Servo_CssUrl_IsLocalRef(&_0);
}

inline bool StyleComputedUrl::HasRef() const {
  if (IsLocalRef()) {
    return true;
  }
  if (nsIURI* uri = GetURI()) {
    bool hasRef = false;
    return NS_SUCCEEDED(uri->GetHasRef(&hasRef)) && hasRef;
  }
  return false;
}

template <>
bool StyleGradient::IsOpaque() const;

}  // namespace mozilla

#endif

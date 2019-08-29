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
#ifdef MOZ_TSAN
  // TSan doesn't understand std::atomic_thread_fence, so in order
  // to avoid a false positive for every time a refcounted object
  // is deleted, we replace the fence with an atomic operation.
  count.load(std::memory_order_acquire);
#else
  std::atomic_thread_fence(std::memory_order_acquire);
#endif
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

inline void StyleAtom::AddRef() {
  if (!IsStatic()) {
    AsAtom()->AddRef();
  }
}

inline void StyleAtom::Release() {
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
  AddRef();
}

inline StyleAtom& StyleAtom::operator=(const StyleAtom& aOther) {
  if (MOZ_LIKELY(this != &aOther)) {
    Release();
    _0 = aOther._0;
    AddRef();
  }
  return *this;
}

inline StyleAtom::~StyleAtom() { Release(); }

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
inline StyleCorsMode StyleComputedUrl::CorsMode() const {
  return _0._0->cors_mode;
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

template <typename Integer>
inline StyleGenericGridLine<Integer>::StyleGenericGridLine()
    : ident(do_AddRef(static_cast<nsAtom*>(nsGkAtoms::_empty))),
      line_num(0),
      is_span(false) {}

template <>
inline nsAtom* StyleGridLine::LineName() const {
  return ident.AsAtom();
}

template <>
inline bool StyleGridLine::IsAuto() const {
  return LineName()->IsEmpty() && line_num == 0 && !is_span;
}

class WritingMode;

// Logical axis, edge, side and corner constants for use in various places.
enum LogicalAxis { eLogicalAxisBlock = 0x0, eLogicalAxisInline = 0x1 };
enum LogicalEdge { eLogicalEdgeStart = 0x0, eLogicalEdgeEnd = 0x1 };
enum LogicalSide : uint8_t {
  eLogicalSideBStart = (eLogicalAxisBlock << 1) | eLogicalEdgeStart,   // 0x0
  eLogicalSideBEnd = (eLogicalAxisBlock << 1) | eLogicalEdgeEnd,       // 0x1
  eLogicalSideIStart = (eLogicalAxisInline << 1) | eLogicalEdgeStart,  // 0x2
  eLogicalSideIEnd = (eLogicalAxisInline << 1) | eLogicalEdgeEnd       // 0x3
};

enum LogicalCorner {
  eLogicalCornerBStartIStart = 0,
  eLogicalCornerBStartIEnd = 1,
  eLogicalCornerBEndIEnd = 2,
  eLogicalCornerBEndIStart = 3
};

using LengthPercentage = StyleLengthPercentage;
using LengthPercentageOrAuto = StyleLengthPercentageOrAuto;
using NonNegativeLengthPercentage = StyleNonNegativeLengthPercentage;
using NonNegativeLengthPercentageOrAuto =
    StyleNonNegativeLengthPercentageOrAuto;
using NonNegativeLengthPercentageOrNormal =
    StyleNonNegativeLengthPercentageOrNormal;
using Length = StyleLength;
using LengthOrAuto = StyleLengthOrAuto;
using NonNegativeLength = StyleNonNegativeLength;
using NonNegativeLengthOrAuto = StyleNonNegativeLengthOrAuto;
using BorderRadius = StyleBorderRadius;

bool StyleCSSPixelLength::IsZero() const { return _0 == 0.0f; }

nscoord StyleCSSPixelLength::ToAppUnits() const {
  // We want to resolve the length part of the calc() expression rounding 0.5
  // away from zero, instead of the default behavior of
  // NSToCoordRound{,WithClamp} which do floor(x + 0.5).
  //
  // This is what the rust code in the app_units crate does, and not doing this
  // would regress bug 1323735, for example.
  //
  // FIXME(emilio, bug 1528114): Probably we should do something smarter.
  float length = _0 * float(mozilla::AppUnitsPerCSSPixel());
  if (length >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (length <= nscoord_MIN) {
    return nscoord_MIN;
  }
  return NSToIntRound(length);
}

constexpr LengthPercentage LengthPercentage::Zero() {
  return {{0.}, {0.}, StyleAllowedNumericType::All, false, false};
}

LengthPercentage LengthPercentage::FromPixels(CSSCoord aCoord) {
  return {{aCoord}, {0.}, StyleAllowedNumericType::All, false, false};
}

LengthPercentage LengthPercentage::FromAppUnits(nscoord aCoord) {
  return LengthPercentage::FromPixels(CSSPixel::FromAppUnits(aCoord));
}

LengthPercentage LengthPercentage::FromPercentage(float aPercentage) {
  return {{0.}, {aPercentage}, StyleAllowedNumericType::All, true, false};
}

CSSCoord LengthPercentage::LengthInCSSPixels() const { return length._0; }

float LengthPercentage::Percentage() const { return percentage._0; }

bool LengthPercentage::HasPercent() const { return has_percentage; }

bool LengthPercentage::ConvertsToLength() const { return !HasPercent(); }

nscoord LengthPercentage::ToLength() const {
  MOZ_ASSERT(ConvertsToLength());
  return length.ToAppUnits();
}

bool LengthPercentage::ConvertsToPercentage() const {
  return has_percentage && length.IsZero();
}

float LengthPercentage::ToPercentage() const {
  MOZ_ASSERT(ConvertsToPercentage());
  return Percentage();
}

bool LengthPercentage::HasLengthAndPercentage() const {
  return !ConvertsToLength() && !ConvertsToPercentage();
}

bool LengthPercentage::IsDefinitelyZero() const {
  return length.IsZero() && Percentage() == 0.0f;
}

CSSCoord LengthPercentage::ResolveToCSSPixels(CSSCoord aPercentageBasis) const {
  return LengthInCSSPixels() + Percentage() * aPercentageBasis;
}

template <typename T>
CSSCoord LengthPercentage::ResolveToCSSPixelsWith(T aPercentageGetter) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), CSSCoord>::value,
                "Should return CSS pixels");
  if (ConvertsToLength()) {
    return LengthInCSSPixels();
  }
  return ResolveToCSSPixels(aPercentageGetter());
}

template <typename T, typename U>
nscoord LengthPercentage::Resolve(T aPercentageGetter,
                                  U aPercentageRounder) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), nscoord>::value,
                "Should return app units");
  static_assert(
      std::is_same<decltype(aPercentageRounder(1.0f)), nscoord>::value,
      "Should return app units");
  if (ConvertsToLength()) {
    return ToLength();
  }
  nscoord basis = aPercentageGetter();
  return length.ToAppUnits() + aPercentageRounder(basis * Percentage());
}

nscoord LengthPercentage::Resolve(nscoord aPercentageBasis) const {
  return Resolve([=] { return aPercentageBasis; }, NSToCoordFloorClamped);
}

template <typename T>
nscoord LengthPercentage::Resolve(T aPercentageGetter) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), nscoord>::value,
                "Should return app units");
  return Resolve(aPercentageGetter, NSToCoordFloorClamped);
}

template <typename T>
nscoord LengthPercentage::Resolve(nscoord aPercentageBasis,
                                  T aPercentageRounder) const {
  return Resolve([=] { return aPercentageBasis; }, aPercentageRounder);
}

#define IMPL_LENGTHPERCENTAGE_FORWARDS(ty_)                                 \
  template <>                                                               \
  inline bool ty_::HasPercent() const {                                     \
    return IsLengthPercentage() && AsLengthPercentage().HasPercent();       \
  }                                                                         \
  template <>                                                               \
  inline bool ty_::ConvertsToLength() const {                               \
    return IsLengthPercentage() && AsLengthPercentage().ConvertsToLength(); \
  }                                                                         \
  template <>                                                               \
  inline bool ty_::HasLengthAndPercentage() const {                         \
    return IsLengthPercentage() &&                                          \
           AsLengthPercentage().HasLengthAndPercentage();                   \
  }                                                                         \
  template <>                                                               \
  inline nscoord ty_::ToLength() const {                                    \
    MOZ_ASSERT(ConvertsToLength());                                         \
    return AsLengthPercentage().ToLength();                                 \
  }                                                                         \
  template <>                                                               \
  inline bool ty_::ConvertsToPercentage() const {                           \
    return IsLengthPercentage() &&                                          \
           AsLengthPercentage().ConvertsToPercentage();                     \
  }                                                                         \
  template <>                                                               \
  inline float ty_::ToPercentage() const {                                  \
    MOZ_ASSERT(ConvertsToPercentage());                                     \
    return AsLengthPercentage().ToPercentage();                             \
  }

IMPL_LENGTHPERCENTAGE_FORWARDS(LengthPercentageOrAuto)
IMPL_LENGTHPERCENTAGE_FORWARDS(StyleSize)
IMPL_LENGTHPERCENTAGE_FORWARDS(StyleMaxSize)

template <>
inline bool LengthOrAuto::IsLength() const {
  return IsLengthPercentage();
}

template <>
inline const Length& LengthOrAuto::AsLength() const {
  return AsLengthPercentage();
}

template <>
inline nscoord LengthOrAuto::ToLength() const {
  return AsLength().ToAppUnits();
}

template <>
inline bool StyleFlexBasis::IsAuto() const {
  return IsSize() && AsSize().IsAuto();
}

template <>
inline bool StyleSize::BehavesLikeInitialValueOnBlockAxis() const {
  return IsAuto() || IsExtremumLength();
}

template <>
inline bool StyleMaxSize::BehavesLikeInitialValueOnBlockAxis() const {
  return IsNone() || IsExtremumLength();
}

template <>
inline bool StyleBackgroundSize::IsInitialValue() const {
  return IsExplicitSize() && explicit_size.width.IsAuto() &&
         explicit_size.height.IsAuto();
}

template <typename T>
const T& StyleRect<T>::Get(mozilla::Side aSide) const {
  static_assert(sizeof(StyleRect<T>) == sizeof(T) * 4, "");
  static_assert(alignof(StyleRect<T>) == alignof(T), "");
  return reinterpret_cast<const T*>(this)[aSide];
}

template <typename T>
template <typename Predicate>
bool StyleRect<T>::All(Predicate aPredicate) const {
  return aPredicate(_0) && aPredicate(_1) && aPredicate(_2) && aPredicate(_3);
}

template <typename T>
template <typename Predicate>
bool StyleRect<T>::Any(Predicate aPredicate) const {
  return aPredicate(_0) || aPredicate(_1) || aPredicate(_2) || aPredicate(_3);
}

template <>
inline const LengthPercentage& BorderRadius::Get(HalfCorner aCorner) const {
  static_assert(sizeof(BorderRadius) == sizeof(LengthPercentage) * 8, "");
  static_assert(alignof(BorderRadius) == alignof(LengthPercentage), "");
  auto* self = reinterpret_cast<const LengthPercentage*>(this);
  return self[aCorner];
}

template <>
inline bool StyleTrackBreadth::HasPercent() const {
  return IsBreadth() && AsBreadth().HasPercent();
}

// Implemented in nsStyleStructs.cpp
template <>
bool StyleTransform::HasPercent() const;

template <>
inline bool StyleTransformOrigin::HasPercent() const {
  // NOTE(emilio): `depth` is just a `<length>` so doesn't have a percentage at
  // all.
  return horizontal.HasPercent() || vertical.HasPercent();
}

template <>
inline Maybe<size_t> StyleGridTemplateComponent::RepeatAutoIndex() const {
  if (!IsTrackList()) {
    return Nothing();
  }
  auto& list = *AsTrackList();
  return list.auto_repeat_index < list.values.Length()
             ? Some(list.auto_repeat_index)
             : Nothing();
}

template <>
inline bool StyleGridTemplateComponent::HasRepeatAuto() const {
  return RepeatAutoIndex().isSome();
}

template <>
inline Span<const StyleGenericTrackListValue<LengthPercentage, StyleInteger>>
StyleGridTemplateComponent::TrackListValues() const {
  if (IsTrackList()) {
    return AsTrackList()->values.AsSpan();
  }
  return {};
}

template <>
inline const StyleGenericTrackRepeat<LengthPercentage, StyleInteger>*
StyleGridTemplateComponent::GetRepeatAutoValue() const {
  auto index = RepeatAutoIndex();
  if (!index) {
    return nullptr;
  }
  return &TrackListValues()[*index].AsTrackRepeat();
}

constexpr const auto kPaintOrderShift = StylePAINT_ORDER_SHIFT;
constexpr const auto kPaintOrderMask = StylePAINT_ORDER_MASK;

template <>
inline nsRect StyleGenericClipRect<LengthOrAuto>::ToLayoutRect(nscoord aAutoSize) const {
  nscoord x = left.IsLength() ? left.ToLength() : 0;
  nscoord y = top.IsLength() ? top.ToLength() : 0;
  nscoord width = right.IsLength() ? right.ToLength() - x : aAutoSize;
  nscoord height = bottom.IsLength() ? bottom.ToLength() - y : aAutoSize;
  return nsRect(x, y, width, height);
}

}  // namespace mozilla

#endif

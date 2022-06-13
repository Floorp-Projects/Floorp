/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Some inline functions declared in cbindgen.toml */

#ifndef mozilla_ServoStyleConstsInlines_h
#define mozilla_ServoStyleConstsInlines_h

#include "mozilla/ServoStyleConsts.h"
#include "mozilla/AspectRatio.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/URLExtraData.h"
#include "nsGkAtoms.h"
#include "MainThreadUtils.h"
#include "nsNetUtil.h"
#include <type_traits>
#include <new>

// TODO(emilio): there are quite a few other implementations scattered around
// that should move here.

namespace mozilla {

// We need to explicitly instantiate these so that the clang plugin can see that
// they're trivially copiable...
//
// https://github.com/eqrion/cbindgen/issues/402 tracks doing something like
// this automatically from cbindgen.
template struct StyleOwned<RawServoAnimationValueMap>;
template struct StyleOwned<RawServoAuthorStyles>;
template struct StyleOwned<RawServoSourceSizeList>;
template struct StyleOwned<StyleUseCounters>;
template struct StyleOwnedOrNull<StyleUseCounters>;
template struct StyleOwnedOrNull<RawServoSelectorList>;
template struct StyleStrong<ComputedStyle>;
template struct StyleStrong<ServoCssRules>;
template struct StyleStrong<RawServoAnimationValue>;
template struct StyleStrong<RawServoDeclarationBlock>;
template struct StyleStrong<RawServoStyleSheetContents>;
template struct StyleStrong<RawServoKeyframe>;
template struct StyleStrong<RawServoLayerBlockRule>;
template struct StyleStrong<RawServoLayerStatementRule>;
template struct StyleStrong<RawServoMediaList>;
template struct StyleStrong<RawServoStyleRule>;
template struct StyleStrong<RawServoImportRule>;
template struct StyleStrong<RawServoKeyframesRule>;
template struct StyleStrong<RawServoMediaRule>;
template struct StyleStrong<RawServoMozDocumentRule>;
template struct StyleStrong<RawServoNamespaceRule>;
template struct StyleStrong<RawServoPageRule>;
template struct StyleStrong<RawServoSupportsRule>;
template struct StyleStrong<RawServoFontFeatureValuesRule>;
template struct StyleStrong<RawServoFontFaceRule>;
template struct StyleStrong<RawServoCounterStyleRule>;
template struct StyleStrong<RawServoScrollTimelineRule>;
template struct StyleStrong<RawServoContainerRule>;

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
inline StyleOwnedSlice<T>::StyleOwnedSlice(Vector<T>&& aVector)
    : StyleOwnedSlice() {
  if (!aVector.length()) {
    return;
  }

  // We could handle this if Vector provided the relevant APIs, see bug 1610702.
  MOZ_DIAGNOSTIC_ASSERT(aVector.length() == aVector.capacity(),
                        "Shouldn't over-allocate");
  len = aVector.length();
  ptr = aVector.extractRawBuffer();
  MOZ_ASSERT(ptr,
             "How did extractRawBuffer return null if we're not using inline "
             "capacity?");
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
  // Explicitly specify template argument here to avoid instantiating Span<T>
  // first and then implicitly converting to Span<const T>
  return Span<const T>{_0.ptr->data.slice, Length()};
}

template <typename T>
inline bool StyleArcSlice<T>::operator==(const StyleArcSlice& aOther) const {
  ASSERT_CANARY
  return _0.ptr == aOther._0.ptr || AsSpan() == aOther.AsSpan();
}

template <typename T>
inline bool StyleArcSlice<T>::operator!=(const StyleArcSlice& aOther) const {
  return !(*this == aOther);
}

template <typename T>
inline void StyleArcSlice<T>::Release() {
  ASSERT_CANARY
  if (MOZ_LIKELY(!_0.ptr->DecrementRef())) {
    return;
  }
  for (T& elem : Span(_0.ptr->data.slice, Length())) {
    elem.~T();
  }
  free(_0.ptr);  // Drop the allocation now.
}

template <typename T>
inline StyleArcSlice<T>::~StyleArcSlice() {
  Release();
}

template <typename T>
inline StyleArcSlice<T>& StyleArcSlice<T>::operator=(StyleArcSlice&& aOther) {
  ASSERT_CANARY
  std::swap(_0.ptr, aOther._0.ptr);
  ASSERT_CANARY
  return *this;
}

template <typename T>
inline StyleArcSlice<T>& StyleArcSlice<T>::operator=(
    const StyleArcSlice& aOther) {
  ASSERT_CANARY

  if (_0.ptr == aOther._0.ptr) {
    return *this;
  }

  Release();

  _0.ptr = aOther._0.ptr;
  _0.ptr->IncrementRef();

  ASSERT_CANARY
  return *this;
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
  if (!(loadData.flags & StyleLoadDataFlags::TRIED_TO_RESOLVE_URI)) {
    loadData.flags |= StyleLoadDataFlags::TRIED_TO_RESOLVE_URI;
    nsDependentCSubstring serialization = SpecifiedSerialization();
    // https://drafts.csswg.org/css-values-4/#url-empty:
    //
    //     If the value of the url() is the empty string (like url("") or
    //     url()), the url must resolve to an invalid resource (similar to what
    //     the url about:invalid does).
    //
    if (!serialization.IsEmpty()) {
      RefPtr<nsIURI> resolved;
      NS_NewURI(getter_AddRefs(resolved), serialization, nullptr,
                ExtraData().BaseURI());
      loadData.resolved_uri = resolved.forget().take();
    }
  }
  return loadData.resolved_uri;
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

inline bool StyleComputedImageUrl::IsImageResolved() const {
  return bool(LoadData().flags & StyleLoadDataFlags::TRIED_TO_RESOLVE_IMAGE);
}

inline imgRequestProxy* StyleComputedImageUrl::GetImage() const {
  MOZ_ASSERT(IsImageResolved());
  return LoadData().resolved_image;
}

template <>
inline bool StyleGradient::Repeating() const {
  if (IsLinear()) {
    return AsLinear().repeating;
  }
  if (IsRadial()) {
    return AsRadial().repeating;
  }
  return AsConic().repeating;
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

void StyleCSSPixelLength::ScaleBy(float aScale) { _0 *= aScale; }

StyleCSSPixelLength StyleCSSPixelLength::ScaledBy(float aScale) const {
  return FromPixels(ToCSSPixels() * aScale);
}

nscoord StyleCSSPixelLength::ToAppUnits() const {
  // We want to resolve the length part of the calc() expression rounding 0.5
  // away from zero, instead of the default behavior of
  // NSToCoordRound{,WithClamp} which do floor(x + 0.5).
  //
  // This is what the rust code in the app_units crate does, and not doing this
  // would regress bug 1323735, for example.
  //
  // FIXME(emilio, bug 1528114): Probably we should do something smarter.
  if (IsZero()) {
    // Avoid the expensive FP math below.
    return 0;
  }
  float length = _0 * float(mozilla::AppUnitsPerCSSPixel());
  if (length >= float(nscoord_MAX)) {
    return nscoord_MAX;
  }
  if (length <= float(nscoord_MIN)) {
    return nscoord_MIN;
  }
  return NSToIntRound(length);
}

bool LengthPercentage::IsLength() const { return Tag() == TAG_LENGTH; }

StyleLengthPercentageUnion::StyleLengthPercentageUnion() {
  length = {TAG_LENGTH, {0.0f}};
  MOZ_ASSERT(IsLength());
}

static_assert(sizeof(LengthPercentage) == sizeof(uint64_t), "");

Length& LengthPercentage::AsLength() {
  MOZ_ASSERT(IsLength());
  return length.length;
}

const Length& LengthPercentage::AsLength() const {
  return const_cast<LengthPercentage*>(this)->AsLength();
}

bool LengthPercentage::IsPercentage() const { return Tag() == TAG_PERCENTAGE; }

StylePercentage& LengthPercentage::AsPercentage() {
  MOZ_ASSERT(IsPercentage());
  return percentage.percentage;
}

const StylePercentage& LengthPercentage::AsPercentage() const {
  return const_cast<LengthPercentage*>(this)->AsPercentage();
}

bool LengthPercentage::IsCalc() const { return Tag() == TAG_CALC; }

StyleCalcLengthPercentage& LengthPercentage::AsCalc() {
  MOZ_ASSERT(IsCalc());
  // NOTE: in 32-bits, the pointer is not swapped, and goes along with the tag.
#ifdef SERVO_32_BITS
  return *calc.ptr;
#else
  return *reinterpret_cast<StyleCalcLengthPercentage*>(
      NativeEndian::swapFromLittleEndian(calc.ptr));
#endif
}

const StyleCalcLengthPercentage& LengthPercentage::AsCalc() const {
  return const_cast<LengthPercentage*>(this)->AsCalc();
}

StyleLengthPercentageUnion::StyleLengthPercentageUnion(const Self& aOther) {
  if (aOther.IsLength()) {
    length = {TAG_LENGTH, aOther.AsLength()};
  } else if (aOther.IsPercentage()) {
    percentage = {TAG_PERCENTAGE, aOther.AsPercentage()};
  } else {
    MOZ_ASSERT(aOther.IsCalc());
    auto* ptr = new StyleCalcLengthPercentage(aOther.AsCalc());
    // NOTE: in 32-bits, the pointer is not swapped, and goes along with the
    // tag.
    calc = {
#ifdef SERVO_32_BITS
        TAG_CALC,
        ptr,
#else
        NativeEndian::swapToLittleEndian(reinterpret_cast<uintptr_t>(ptr)),
#endif
    };
  }
  MOZ_ASSERT(Tag() == aOther.Tag());
}

StyleLengthPercentageUnion::~StyleLengthPercentageUnion() {
  if (IsCalc()) {
    delete &AsCalc();
  }
}

LengthPercentage& LengthPercentage::operator=(const LengthPercentage& aOther) {
  if (this != &aOther) {
    this->~LengthPercentage();
    new (this) LengthPercentage(aOther);
  }
  return *this;
}

bool LengthPercentage::operator==(const LengthPercentage& aOther) const {
  if (Tag() != aOther.Tag()) {
    return false;
  }
  if (IsLength()) {
    return AsLength() == aOther.AsLength();
  }
  if (IsPercentage()) {
    return AsPercentage() == aOther.AsPercentage();
  }
  return AsCalc() == aOther.AsCalc();
}

bool LengthPercentage::operator!=(const LengthPercentage& aOther) const {
  return !(*this == aOther);
}

LengthPercentage LengthPercentage::Zero() { return {}; }

LengthPercentage LengthPercentage::FromPixels(CSSCoord aCoord) {
  LengthPercentage l;
  MOZ_ASSERT(l.IsLength());
  l.length.length = {aCoord};
  return l;
}

LengthPercentage LengthPercentage::FromAppUnits(nscoord aCoord) {
  return FromPixels(CSSPixel::FromAppUnits(aCoord));
}

LengthPercentage LengthPercentage::FromPercentage(float aPercentage) {
  LengthPercentage l;
  l.percentage = {TAG_PERCENTAGE, {aPercentage}};
  return l;
}

bool LengthPercentage::HasPercent() const { return IsPercentage() || IsCalc(); }

bool LengthPercentage::ConvertsToLength() const { return IsLength(); }

nscoord LengthPercentage::ToLength() const {
  MOZ_ASSERT(ConvertsToLength());
  return AsLength().ToAppUnits();
}

CSSCoord LengthPercentage::ToLengthInCSSPixels() const {
  MOZ_ASSERT(ConvertsToLength());
  return AsLength().ToCSSPixels();
}

bool LengthPercentage::ConvertsToPercentage() const { return IsPercentage(); }

float LengthPercentage::ToPercentage() const {
  MOZ_ASSERT(ConvertsToPercentage());
  return AsPercentage()._0;
}

bool LengthPercentage::HasLengthAndPercentage() const {
  if (!IsCalc()) {
    return false;
  }
  MOZ_ASSERT(!ConvertsToLength() && !ConvertsToPercentage(),
             "Should've been simplified earlier");
  return true;
}

bool LengthPercentage::IsDefinitelyZero() const {
  if (IsLength()) {
    return AsLength().IsZero();
  }
  if (IsPercentage()) {
    return AsPercentage()._0 == 0.0f;
  }
  // calc() should've been simplified to a percentage.
  return false;
}

template <>
CSSCoord StyleCalcNode::ResolveToCSSPixels(CSSCoord aPercentageBasis) const;

template <>
void StyleCalcNode::ScaleLengthsBy(float);

CSSCoord LengthPercentage::ResolveToCSSPixels(CSSCoord aPercentageBasis) const {
  if (IsLength()) {
    return AsLength().ToCSSPixels();
  }
  if (IsPercentage()) {
    return AsPercentage()._0 * aPercentageBasis;
  }
  return AsCalc().node.ResolveToCSSPixels(aPercentageBasis);
}

template <typename T>
CSSCoord LengthPercentage::ResolveToCSSPixelsWith(T aPercentageGetter) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), CSSCoord>::value,
                "Should return CSS pixels");
  if (ConvertsToLength()) {
    return ToLengthInCSSPixels();
  }
  return ResolveToCSSPixels(aPercentageGetter());
}

template <typename T, typename U>
nscoord LengthPercentage::Resolve(T aPercentageGetter, U aRounder) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), nscoord>::value,
                "Should return app units");
  static_assert(std::is_same<decltype(aRounder(1.0f)), nscoord>::value,
                "Should return app units");
  if (ConvertsToLength()) {
    return ToLength();
  }
  if (IsPercentage() && AsPercentage()._0 == 0.0f) {
    return 0.0f;
  }
  nscoord basis = aPercentageGetter();
  if (IsPercentage()) {
    return aRounder(basis * AsPercentage()._0);
  }
  return AsCalc().node.Resolve(basis, aRounder);
}

// Note: the static_cast<> wrappers below are needed to disambiguate between
// the versions of NSToCoordTruncClamped that take float vs. double as the arg.
nscoord LengthPercentage::Resolve(nscoord aPercentageBasis) const {
  return Resolve([=] { return aPercentageBasis; },
                 static_cast<nscoord (*)(float)>(NSToCoordTruncClamped));
}

template <typename T>
nscoord LengthPercentage::Resolve(T aPercentageGetter) const {
  return Resolve(aPercentageGetter,
                 static_cast<nscoord (*)(float)>(NSToCoordTruncClamped));
}

template <typename T>
nscoord LengthPercentage::Resolve(nscoord aPercentageBasis,
                                  T aPercentageRounder) const {
  return Resolve([aPercentageBasis] { return aPercentageBasis; },
                 aPercentageRounder);
}

void LengthPercentage::ScaleLengthsBy(float aScale) {
  if (IsLength()) {
    AsLength().ScaleBy(aScale);
  }
  if (IsCalc()) {
    AsCalc().node.ScaleLengthsBy(aScale);
  }
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
  return IsAuto() || !IsLengthPercentage();
}

template <>
inline bool StyleMaxSize::BehavesLikeInitialValueOnBlockAxis() const {
  return IsNone() || !IsLengthPercentage();
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
T& StyleRect<T>::Get(mozilla::Side aSide) {
  return const_cast<T&>(static_cast<const StyleRect&>(*this).Get(aSide));
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
inline nsRect StyleGenericClipRect<LengthOrAuto>::ToLayoutRect(
    nscoord aAutoSize) const {
  nscoord x = left.IsLength() ? left.ToLength() : 0;
  nscoord y = top.IsLength() ? top.ToLength() : 0;
  nscoord width = right.IsLength() ? right.ToLength() - x : aAutoSize;
  nscoord height = bottom.IsLength() ? bottom.ToLength() - y : aAutoSize;
  return nsRect(x, y, width, height);
}

using RestyleHint = StyleRestyleHint;

inline RestyleHint RestyleHint::RestyleSubtree() {
  return RESTYLE_SELF | RESTYLE_DESCENDANTS;
}

inline RestyleHint RestyleHint::RecascadeSubtree() {
  return RECASCADE_SELF | RECASCADE_DESCENDANTS;
}

inline RestyleHint RestyleHint::ForAnimations() {
  return RESTYLE_CSS_TRANSITIONS | RESTYLE_CSS_ANIMATIONS | RESTYLE_SMIL;
}

inline bool RestyleHint::DefinitelyRecascadesAllSubtree() const {
  if (!(*this & (RECASCADE_DESCENDANTS | RESTYLE_DESCENDANTS))) {
    return false;
  }
  return bool(*this & (RESTYLE_SELF | RECASCADE_SELF));
}

template <>
ImageResolution StyleImage::GetResolution() const;

template <>
inline const StyleImage& StyleImage::FinalImage() const {
  if (!IsImageSet()) {
    return *this;
  }
  auto& set = AsImageSet();
  auto& selectedItem = set->items.AsSpan()[set->selected_index];
  return selectedItem.image.FinalImage();
}

template <>
Maybe<CSSIntSize> StyleImage::GetIntrinsicSize() const;

template <>
inline bool StyleImage::IsImageRequestType() const {
  auto& finalImage = FinalImage();
  return finalImage.IsUrl() || finalImage.IsRect();
}

template <>
inline const StyleComputedImageUrl* StyleImage::GetImageRequestURLValue()
    const {
  auto& finalImage = FinalImage();
  if (finalImage.IsUrl()) {
    return &finalImage.AsUrl();
  }
  if (finalImage.IsRect()) {
    return &finalImage.AsRect()->url;
  }
  return nullptr;
}

template <>
inline imgRequestProxy* StyleImage::GetImageRequest() const {
  auto* url = GetImageRequestURLValue();
  return url ? url->GetImage() : nullptr;
}

template <>
inline bool StyleImage::IsResolved() const {
  auto* url = GetImageRequestURLValue();
  return !url || url->IsImageResolved();
}

template <>
bool StyleImage::IsOpaque() const;
template <>
bool StyleImage::IsSizeAvailable() const;
template <>
bool StyleImage::IsComplete() const;
template <>
Maybe<StyleImage::ActualCropRect> StyleImage::ComputeActualCropRect() const;
template <>
void StyleImage::ResolveImage(dom::Document&, const StyleImage*);

template <>
inline AspectRatio StyleRatio<StyleNonNegativeNumber>::ToLayoutRatio(
    UseBoxSizing aUseBoxSizing) const {
  // 0/1, 1/0, and 0/0 are all degenerate ratios (which behave as auto), and we
  // always return 0.0f.
  // https://drafts.csswg.org/css-values-4/#degenerate-ratio
  return AspectRatio::FromSize(_0, _1, aUseBoxSizing);
}

template <>
inline AspectRatio StyleAspectRatio::ToLayoutRatio() const {
  return HasRatio() ? ratio.AsRatio().ToLayoutRatio(auto_ ? UseBoxSizing::No
                                                          : UseBoxSizing::Yes)
                    : AspectRatio();
}

inline void StyleFontWeight::ToString(nsACString& aString) const {
  Servo_FontWeight_ToCss(this, &aString);
}

inline void StyleFontStretch::ToString(nsACString& aString) const {
  Servo_FontStretch_ToCss(this, &aString);
}

inline void StyleFontStyle::ToString(nsACString& aString) const {
  Servo_FontStyle_ToCss(this, &aString);
}

inline bool StyleFontWeight::IsBold() const { return *this >= BOLD_THRESHOLD; }

inline bool StyleFontStyle::IsItalic() const { return *this == ITALIC; }

inline bool StyleFontStyle::IsOblique() const {
  return !IsItalic() && !IsNormal();
}

inline float StyleFontStyle::ObliqueAngle() const { return ToFloat(); }

using FontStretch = StyleFontStretch;
using FontSlantStyle = StyleFontStyle;
using FontWeight = StyleFontWeight;

}  // namespace mozilla

#endif

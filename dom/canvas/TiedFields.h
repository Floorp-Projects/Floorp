/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CANVAS_TIED_FIELDS_H
#define DOM_CANVAS_TIED_FIELDS_H

#include "TupleUtils.h"

namespace mozilla {

// -

/**
 * TiedFields(T&) -> std::tuple<Fields&...>
 * TiedFields(const T&) -> std::tuple<const Fields&...>
 *
 * You can also overload TiedFields without adding T::MutTiedFields:
 * template<>
 * inline auto TiedFields<gfx::IntSize>(gfx::IntSize& a) {
 *   return std::tie(a.width, a.height);
 * }
 */
template <class T>
constexpr auto TiedFields(T& t) {
  const auto fields = t.MutTiedFields();
  return fields;
}
template <class T, class... Args, class Tup = std::tuple<Args&...>>
constexpr auto TiedFields(const T& t) {
  // Uncast const to get mutable-fields tuple, but reapply const to tuple args.
  const auto mutFields = TiedFields(const_cast<T&>(t));
  return ToTupleOfConstRefs(mutFields);
}

/**
 * Returns true if all bytes in T are accounted for via size of all tied fields.
 * Returns false if there's bytes unaccounted for, which might indicate either
 * unaccounted-for padding or missing fields.
 * The goal is to check that TiedFields returns every field in T, and this
 * returns false if it suspects there are bytes that are not accounted for by
 * TiedFields.
 *
 * `constexpr` effectively cannot do math on pointers, so it's not possible to
 * figure out via `constexpr` whether fields are consecutive or dense.
 * However, we can at least compare `sizeof(T)` to the sum of `sizeof(Args...)`
 * for `TiedFields(T) -> std::tuple<Args...>`.
 *
 * See TiedFieldsExamples.
 */
template <class T>
constexpr bool AreAllBytesTiedFields(const T& t) {
  const auto fields = TiedFields(t);
  const auto fields_size_sum = SizeofTupleArgs(fields);
  const auto t_size = sizeof(T);
  return fields_size_sum == t_size;
}

// -

/**
 * Padding<T> can be used to pad out a struct so that it's not implicitly
 * padded by struct rules.
 * You can also just add your padding to TiedFields, but by explicitly typing
 * padding like this, serialization can make a choice whether to copy Padding,
 * or instead to omit the copy.
 *
 * Omitting the copy isn't always faster.
 * struct Entry {
 *   uint16_t key;
 *   Padding<uint16_t> padding;
 *   uint32_t val;
 *   auto MutTiedFields() { return std::tie(key, padding, val); }
 * };
 * If you serialize Padding, the serialized size is 8, and the compiler will
 * optimize serialization to a single 8-byte memcpy.
 * If your serialization omits Padding, the serialized size of Entry shrinks
 * by 25%. If you have a big list of Entrys, maybe this is a big savings!
 * However, by optimizing for size here you sacrifice speed, because this splits
 * the single memcpy into two: a 2-byte memcpy and a 4-byte memcpy.
 *
 * Explicitly marking padding gives callers the option of choosing.
 */
template <class T>
struct Padding {
  T ignored;

  friend constexpr bool operator==(const Padding&, const Padding&) {
    return true;
  }
  friend constexpr bool operator<(const Padding&, const Padding&) {
    return false;
  }
};
static_assert(sizeof(Padding<bool>) == 1);
static_assert(sizeof(Padding<bool[2]>) == 2);
static_assert(sizeof(Padding<int>) == 4);

// -

template <size_t I>
struct DecayDownT : public DecayDownT<I - 1> {};

template <>
struct DecayDownT<0> {};

// -

/// Warning, IsDenseNestedScalars should not be used to indicate the safety of
/// a memcpy.
/// If you trust both the source and destination, you should use
/// std::is_trivially_copyable.
template <class T>
constexpr bool IsDenseNestedScalars(const T& t);

// -

namespace IsDenseNestedScalarsDetails {

template <class T, std::enable_if_t<std::is_scalar<T>::value, bool> = true>
constexpr bool Impl(const T&, DecayDownT<3>) {
  return true;
}

template <class T, size_t N>
constexpr bool Impl(const T (&t)[N], DecayDownT<3>) {
  return IsDenseNestedScalars(t[0]);
}

template <class T, size_t N>
constexpr bool Impl(const std::array<T, N>& t, DecayDownT<3>) {
  return IsDenseNestedScalars(t[0]);
}

template <class T>
constexpr bool Impl(const Padding<T>& t, DecayDownT<3>) {
  return IsDenseNestedScalars(t.ignored);
}

// -

template <class T>
constexpr bool Impl(const T& t, DecayDownT<1>) {
  const auto tup = TiedFields(t);
  bool ok = AreAllBytesTiedFields(t);
  MapTuple(tup, [&](const auto& field) {
    ok &= IsDenseNestedScalars(field);
    return true;
  });
  return ok;
}

}  // namespace IsDenseNestedScalarsDetails

// -

template <class T>
constexpr bool IsDenseNestedScalars(const T& t) {
  return IsDenseNestedScalarsDetails::Impl(t, DecayDownT<5>{});
}
static_assert(IsDenseNestedScalars(1));
// Compile error: Missing TiedFields:
// static_assert(IsDenseNestedScalars(std::pair<int,int>{}));

namespace TiedFieldsExamples {

struct Cat {
  int i;
  bool b;

  constexpr auto MutTiedFields() { return std::tie(i, b); }
};
static_assert(sizeof(Cat) == 8);
static_assert(!AreAllBytesTiedFields(Cat{}));
static_assert(!IsDenseNestedScalars(Cat{}));

struct Dog {
  bool b;
  int i;

  constexpr auto MutTiedFields() { return std::tie(i, b); }
};
static_assert(sizeof(Dog) == 8);
static_assert(!AreAllBytesTiedFields(Dog{}));
static_assert(!IsDenseNestedScalars(Dog{}));

struct Fish {
  bool b;
  bool padding[3];
  int i;

  constexpr auto MutTiedFields() { return std::tie(i, b, padding); }
};
static_assert(sizeof(Fish) == 8);
static_assert(AreAllBytesTiedFields(Fish{}));
static_assert(IsDenseNestedScalars(Fish{}));

struct Eel {  // Like a Fish, but you can skip serializing the padding.
  bool b;
  Padding<bool> padding[3];
  int i;

  constexpr auto MutTiedFields() { return std::tie(i, b, padding); }
};
static_assert(sizeof(Eel) == 8);
static_assert(AreAllBytesTiedFields(Eel{}));
static_assert(IsDenseNestedScalars(Eel{}));

// -

//#define LETS_USE_BIT_FIELDS
#ifdef LETS_USE_BIT_FIELDS
#  undef LETS_USE_BIT_FIELDS

struct Platypus {
  short s : 1;
  short s2 : 1;
  int i;

  constexpr auto MutTiedFields() {
    return std::tie(s, s2, i);  // Error: Can't take reference to bit-field.
  }
};

#endif

// -

struct FishTank {
  Fish f;
  int i2;

  constexpr auto MutTiedFields() { return std::tie(f, i2); }
};
static_assert(sizeof(FishTank) == 12);
static_assert(AreAllBytesTiedFields(FishTank{}));
static_assert(IsDenseNestedScalars(FishTank{}));

struct CatCarrier {
  Cat c;
  int i2;

  constexpr auto MutTiedFields() { return std::tie(c, i2); }
};
static_assert(sizeof(CatCarrier) == 12);
static_assert(AreAllBytesTiedFields(CatCarrier{}));
static_assert(!IsDenseNestedScalars(CatCarrier{}));  // Because:
static_assert(!AreAllBytesTiedFields(CatCarrier{}.c));

}  // namespace TiedFieldsExamples
}  // namespace mozilla

#endif  // DOM_CANVAS_TIED_FIELDS_H

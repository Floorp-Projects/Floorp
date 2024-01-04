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
  // We should do better than this when C++ gets a solution other than macros.
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
constexpr bool AreAllBytesTiedFields() {
  using fieldsT = decltype(TiedFields(std::declval<T>()));
  const auto fields_size_sum = SizeofTupleArgs<fieldsT>::value;
  const auto t_size = sizeof(T);
  return fields_size_sum == t_size;
}

// It's also possible to determine AreAllBytesRecursiveTiedFields:
// https://hackmd.io/@jgilbert/B16qa0Fa9

// -

template <class StructT, size_t FieldId, size_t PrevFieldBeginOffset,
          class PrevFieldT, size_t PrevFieldEndOffset, class FieldT,
          size_t FieldAlignment = alignof(FieldT)>
struct FieldDebugInfoT {
  static constexpr bool IsTightlyPacked() {
    return PrevFieldEndOffset % FieldAlignment == 0;
  }
};

template <class StructT, class TupleOfFields, size_t FieldId>
struct TightlyPackedFieldEndOffsetT {
  template <size_t I>
  using FieldTAt = std::remove_reference_t<
      typename std::tuple_element<I, TupleOfFields>::type>;

  static constexpr size_t Fn() {
    constexpr auto num_fields = std::tuple_size_v<TupleOfFields>;
    static_assert(FieldId < num_fields);

    using PrevFieldT = FieldTAt<FieldId - 1>;
    using FieldT = FieldTAt<FieldId>;
    constexpr auto prev_field_end_offset =
        TightlyPackedFieldEndOffsetT<StructT, TupleOfFields, FieldId - 1>::Fn();
    constexpr auto prev_field_begin_offset =
        prev_field_end_offset - sizeof(PrevFieldT);

    using FieldDebugInfoT =
        FieldDebugInfoT<StructT, FieldId, prev_field_begin_offset, PrevFieldT,
                        prev_field_end_offset, FieldT>;
    static_assert(FieldDebugInfoT::IsTightlyPacked(),
                  "This field was not tightly packed. Is there padding between "
                  "it and its predecessor?");

    return prev_field_end_offset + sizeof(FieldT);
  }
};

template <class StructT, class TupleOfFields>
struct TightlyPackedFieldEndOffsetT<StructT, TupleOfFields, 0> {
  static constexpr size_t Fn() {
    using FieldT = typename std::tuple_element<0, TupleOfFields>::type;
    return sizeof(FieldT);
  }
};
template <class StructT, class TupleOfFields>
struct TightlyPackedFieldEndOffsetT<StructT, TupleOfFields, size_t(-1)> {
  static constexpr size_t Fn() {
    // -1 means tuple_size_v<TupleOfFields> -> 0.
    static_assert(sizeof(StructT) == 0);
    return 0;
  }
};

template <class StructT>
constexpr bool AssertTiedFieldsAreExhaustive() {
  using TupleOfFields = decltype(std::declval<StructT>().MutTiedFields());
  constexpr auto num_fields = std::tuple_size_v<TupleOfFields>;
  constexpr auto end_offset_of_last_field =
      TightlyPackedFieldEndOffsetT<StructT, TupleOfFields,
                                   num_fields - 1>::Fn();
  static_assert(
      end_offset_of_last_field == sizeof(StructT),
      "Incorrect field list in MutTiedFields()? (or not tightly-packed?)");
  return true;  // Support `static_assert(AssertTiedFieldsAreExhaustive())`.
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

namespace TiedFieldsExamples {

struct Cat {
  int i;
  bool b;

  constexpr auto MutTiedFields() { return std::tie(i, b); }
};
static_assert(sizeof(Cat) == 8);
static_assert(!AreAllBytesTiedFields<Cat>());

struct Dog {
  bool b;
  int i;

  constexpr auto MutTiedFields() { return std::tie(i, b); }
};
static_assert(sizeof(Dog) == 8);
static_assert(!AreAllBytesTiedFields<Dog>());

struct Fish {
  bool b;
  bool padding[3];
  int i;

  constexpr auto MutTiedFields() { return std::tie(i, b, padding); }
};
static_assert(sizeof(Fish) == 8);
static_assert(AreAllBytesTiedFields<Fish>());

struct Eel {  // Like a Fish, but you can skip serializing the padding.
  bool b;
  Padding<bool> padding[3];
  int i;

  constexpr auto MutTiedFields() { return std::tie(i, b, padding); }
};
static_assert(sizeof(Eel) == 8);
static_assert(AreAllBytesTiedFields<Eel>());

// -

// #define LETS_USE_BIT_FIELDS
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
static_assert(AreAllBytesTiedFields<FishTank>());

struct CatCarrier {
  Cat c;
  int i2;

  constexpr auto MutTiedFields() { return std::tie(c, i2); }
};
static_assert(sizeof(CatCarrier) == 12);
static_assert(AreAllBytesTiedFields<CatCarrier>());
static_assert(
    !AreAllBytesTiedFields<decltype(CatCarrier::c)>());  // BUT BEWARE THIS!
// For example, if we had AreAllBytesRecursiveTiedFields:
// static_assert(!AreAllBytesRecursiveTiedFields<CatCarrier>());

}  // namespace TiedFieldsExamples
}  // namespace mozilla

#endif  // DOM_CANVAS_TIED_FIELDS_H

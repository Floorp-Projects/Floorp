/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COORD_H_
#define MOZILLA_GFX_COORD_H_

#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"
#include "Types.h"
#include "BaseCoord.h"

#include <cmath>
#include <type_traits>

namespace mozilla {

namespace gfx {

template <class Units, class Rep = int32_t>
struct IntCoordTyped;
template <class Units, class F = Float>
struct CoordTyped;

}  // namespace gfx

}  // namespace mozilla

namespace std {

template <class Units, class Rep>
struct common_type<mozilla::gfx::IntCoordTyped<Units, Rep>, float> {
  using type = mozilla::gfx::CoordTyped<Units, common_type_t<Rep, float>>;
};

template <class Units, class Rep>
struct common_type<mozilla::gfx::IntCoordTyped<Units, Rep>, double> {
  using type = mozilla::gfx::CoordTyped<Units, common_type_t<Rep, double>>;
};

template <class Units, class Rep>
struct common_type<mozilla::gfx::IntCoordTyped<Units, Rep>, int32_t> {
  using type = mozilla::gfx::IntCoordTyped<Units, common_type_t<Rep, int32_t>>;
};

template <class Units, class Rep>
struct common_type<mozilla::gfx::IntCoordTyped<Units, Rep>, uint32_t> {
  using type = mozilla::gfx::IntCoordTyped<Units, common_type_t<Rep, uint32_t>>;
};

template <class Units, class F, class T>
struct common_type<mozilla::gfx::CoordTyped<Units, F>, T> {
  using type = mozilla::gfx::CoordTyped<Units, common_type_t<F, T>>;
};

// With a few exceptions, we use CoordTyped values with a float representation.
// These are the types for which we have short typedefs like
// CSSCoord, and the types expected in most interfaces.
// So, for float inputs, keep the results as float even if the other
// operand is a double, accepting a slight loss of precision.
template <class Units, class T>
struct common_type<mozilla::gfx::CoordTyped<Units, float>, T> {
  using type = mozilla::gfx::CoordTyped<Units, float>;
};

}  // namespace std

namespace mozilla {

template <typename>
struct IsPixel;

namespace gfx {

// Should only be used to define generic typedefs like Coord, Point, etc.
struct UnknownUnits {};

// This is a base class that provides mixed-type operator overloads between
// a strongly-typed Coord and a Primitive value. It is needed to avoid
// ambiguities at mixed-type call sites, because Coord classes are implicitly
// convertible to their underlying value type. As we transition more of our code
// to strongly-typed classes, we may be able to remove some or all of these
// overloads.

template <bool Enable, class Coord, class Primitive>
struct CoordOperatorsHelper {
  // Using SFINAE (Substitution Failure Is Not An Error) to suppress redundant
  // operators
};

template <class Coord, class Primitive>
struct CoordOperatorsHelper<true, Coord, Primitive> {
  friend bool operator==(Coord aA, Primitive aB) { return aA.value == aB; }
  friend bool operator==(Primitive aA, Coord aB) { return aA == aB.value; }
  friend bool operator!=(Coord aA, Primitive aB) { return aA.value != aB; }
  friend bool operator!=(Primitive aA, Coord aB) { return aA != aB.value; }

  friend auto operator+(Coord aA, Primitive aB) { return aA.value + aB; }
  friend auto operator+(Primitive aA, Coord aB) { return aA + aB.value; }
  friend auto operator-(Coord aA, Primitive aB) { return aA.value - aB; }
  friend auto operator-(Primitive aA, Coord aB) { return aA - aB.value; }
  friend auto operator*(Coord aCoord, Primitive aScale) {
    return std::common_type_t<Coord, Primitive>(aCoord.value * aScale);
  }
  friend auto operator*(Primitive aScale, Coord aCoord) {
    return aCoord * aScale;
  }
  friend auto operator/(Coord aCoord, Primitive aScale) {
    return std::common_type_t<Coord, Primitive>(aCoord.value / aScale);
  }
  // 'scale / coord' is intentionally omitted because it doesn't make sense.
};

template <class Units, class Rep>
struct MOZ_EMPTY_BASES IntCoordTyped
    : public BaseCoord<Rep, IntCoordTyped<Units, Rep>>,
      public CoordOperatorsHelper<true, IntCoordTyped<Units, Rep>, float>,
      public CoordOperatorsHelper<true, IntCoordTyped<Units, Rep>, double> {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  using Super = BaseCoord<Rep, IntCoordTyped<Units, Rep>>;

  constexpr IntCoordTyped() : Super() {
    static_assert(sizeof(IntCoordTyped) == sizeof(Rep),
                  "Would be unfortunate otherwise!");
  }
  template <class T, typename = typename std::enable_if_t<
                         std::is_integral_v<T> || std::is_enum_v<T>>>
  constexpr MOZ_IMPLICIT IntCoordTyped(T aValue) : Super(aValue) {
    static_assert(sizeof(IntCoordTyped) == sizeof(Rep),
                  "Would be unfortunate otherwise!");
  }
};

template <class Units, class F>
struct MOZ_EMPTY_BASES CoordTyped
    : public BaseCoord<F, CoordTyped<Units, F>>,
      public CoordOperatorsHelper<!std::is_same_v<F, int32_t>,
                                  CoordTyped<Units, F>, int32_t>,
      public CoordOperatorsHelper<!std::is_same_v<F, uint32_t>,
                                  CoordTyped<Units, F>, uint32_t>,
      public CoordOperatorsHelper<!std::is_same_v<F, double>,
                                  CoordTyped<Units, F>, double>,
      public CoordOperatorsHelper<!std::is_same_v<F, float>,
                                  CoordTyped<Units, F>, float> {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  using Super = BaseCoord<F, CoordTyped<Units, F>>;

  constexpr CoordTyped() : Super() {
    static_assert(sizeof(CoordTyped) == sizeof(F),
                  "Would be unfortunate otherwise!");
  }
  constexpr MOZ_IMPLICIT CoordTyped(F aValue) : Super(aValue) {
    static_assert(sizeof(CoordTyped) == sizeof(F),
                  "Would be unfortunate otherwise!");
  }
  explicit constexpr CoordTyped(const IntCoordTyped<Units>& aCoord)
      : Super(F(aCoord.value)) {
    static_assert(sizeof(CoordTyped) == sizeof(F),
                  "Would be unfortunate otherwise!");
  }

  void Round() { this->value = floor(this->value + 0.5); }
  void Truncate() { this->value = int32_t(this->value); }

  IntCoordTyped<Units> Rounded() const {
    return IntCoordTyped<Units>(int32_t(floor(this->value + 0.5)));
  }
  IntCoordTyped<Units> Truncated() const {
    return IntCoordTyped<Units>(int32_t(this->value));
  }
};

typedef CoordTyped<UnknownUnits> Coord;

}  // namespace gfx

template <class Units, class F>
static MOZ_ALWAYS_INLINE bool FuzzyEqualsAdditive(
    gfx::CoordTyped<Units, F> aValue1, gfx::CoordTyped<Units, F> aValue2,
    gfx::CoordTyped<Units, F> aEpsilon =
        detail::FuzzyEqualsEpsilon<F>::value()) {
  return FuzzyEqualsAdditive(aValue1.value, aValue2.value, aEpsilon.value);
}

template <class Units, class F>
static MOZ_ALWAYS_INLINE bool FuzzyEqualsMultiplicative(
    gfx::CoordTyped<Units, F> aValue1, gfx::CoordTyped<Units, F> aValue2,
    gfx::CoordTyped<Units, F> aEpsilon =
        detail::FuzzyEqualsEpsilon<F>::value()) {
  return FuzzyEqualsMultiplicative(aValue1.value, aValue2.value,
                                   aEpsilon.value);
}

}  // namespace mozilla

#endif /* MOZILLA_GFX_COORD_H_ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COORD_H_
#define MOZILLA_GFX_COORD_H_

#include "mozilla/Attributes.h"
#include "Types.h"
#include "BaseCoord.h"

#include <cmath>
#include <type_traits>

namespace mozilla {

template <typename>
struct IsPixel;

namespace gfx {

template <class units, class Rep = int32_t>
struct IntCoordTyped;
template <class units, class F = Float>
struct CoordTyped;

// CommonType<coord, primitive> is a metafunction that returns the type of the
// result of an arithmetic operation on the underlying type of a strongly-typed
// coordinate type 'coord', and a primitive type 'primitive'. C++ rules for
// arithmetic conversions are designed to avoid losing information - for
// example, the result of adding an int and a float is a float - and we want
// the same behaviour when mixing our coordinate types with primitive types.
// We get C++ to compute the desired result type using 'decltype'.

template <class coord, class primitive>
struct CommonType;

template <class units, class Rep, class primitive>
struct CommonType<IntCoordTyped<units, Rep>, primitive> {
  using type = decltype(Rep() + primitive());
};

template <class units, class F, class primitive>
struct CommonType<CoordTyped<units, F>, primitive> {
  using type = decltype(F() + primitive());
};

// This is a base class that provides mixed-type operator overloads between
// a strongly-typed Coord and a primitive value. It is needed to avoid
// ambiguities at mixed-type call sites, because Coord classes are implicitly
// convertible to their underlying value type. As we transition more of our code
// to strongly-typed classes, we may be able to remove some or all of these
// overloads.

template <bool B, class coord, class primitive>
struct CoordOperatorsHelper {
  // Using SFINAE (Substitution Failure Is Not An Error) to suppress redundant
  // operators
};

template <class coord, class primitive>
struct CoordOperatorsHelper<true, coord, primitive> {
  friend bool operator==(coord aA, primitive aB) { return aA.value == aB; }
  friend bool operator==(primitive aA, coord aB) { return aA == aB.value; }
  friend bool operator!=(coord aA, primitive aB) { return aA.value != aB; }
  friend bool operator!=(primitive aA, coord aB) { return aA != aB.value; }

  using result_type = typename CommonType<coord, primitive>::type;

  friend result_type operator+(coord aA, primitive aB) { return aA.value + aB; }
  friend result_type operator+(primitive aA, coord aB) { return aA + aB.value; }
  friend result_type operator-(coord aA, primitive aB) { return aA.value - aB; }
  friend result_type operator-(primitive aA, coord aB) { return aA - aB.value; }
  friend result_type operator*(coord aCoord, primitive aScale) {
    return aCoord.value * aScale;
  }
  friend result_type operator*(primitive aScale, coord aCoord) {
    return aScale * aCoord.value;
  }
  friend result_type operator/(coord aCoord, primitive aScale) {
    return aCoord.value / aScale;
  }
  // 'scale / coord' is intentionally omitted because it doesn't make sense.
};

template <class units, class Rep>
struct IntCoordTyped
    : public BaseCoord<Rep, IntCoordTyped<units, Rep>>,
      public units,
      public CoordOperatorsHelper<true, IntCoordTyped<units, Rep>, float>,
      public CoordOperatorsHelper<true, IntCoordTyped<units, Rep>, double> {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  using Super = BaseCoord<Rep, IntCoordTyped<units, Rep>>;

  constexpr IntCoordTyped() : Super() {}
  constexpr MOZ_IMPLICIT IntCoordTyped(Rep aValue) : Super(aValue) {}
};

template <class units, class F>
struct CoordTyped : public BaseCoord<F, CoordTyped<units, F>>,
                    public units,
                    public CoordOperatorsHelper<!std::is_same_v<F, int32_t>,
                                                CoordTyped<units, F>, int32_t>,
                    public CoordOperatorsHelper<!std::is_same_v<F, uint32_t>,
                                                CoordTyped<units, F>, uint32_t>,
                    public CoordOperatorsHelper<!std::is_same_v<F, double>,
                                                CoordTyped<units, F>, double>,
                    public CoordOperatorsHelper<!std::is_same_v<F, float>,
                                                CoordTyped<units, F>, float> {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  using Super = BaseCoord<F, CoordTyped<units, F>>;

  constexpr CoordTyped() : Super() {}
  constexpr MOZ_IMPLICIT CoordTyped(F aValue) : Super(aValue) {}
  explicit constexpr CoordTyped(const IntCoordTyped<units>& aCoord)
      : Super(F(aCoord.value)) {}

  void Round() { this->value = floor(this->value + 0.5); }
  void Truncate() { this->value = int32_t(this->value); }

  IntCoordTyped<units> Rounded() const {
    return IntCoordTyped<units>(int32_t(floor(this->value + 0.5)));
  }
  IntCoordTyped<units> Truncated() const {
    return IntCoordTyped<units>(int32_t(this->value));
  }
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_COORD_H_ */

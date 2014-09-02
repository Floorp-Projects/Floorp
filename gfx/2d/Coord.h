/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COORD_H_
#define MOZILLA_GFX_COORD_H_

#include "mozilla/Attributes.h"
#include "Types.h"
#include "BaseCoord.h"

#include <cmath>

namespace mozilla {

template <typename> struct IsPixel;

namespace gfx {

template <class units> struct IntCoordTyped;
template <class units> struct CoordTyped;

// CommonType<coord, primitive> is a metafunction that returns the type of the
// result of an arithmetic operation on the underlying type of a strongly-typed
// coordinate type 'coord', and a primitive type 'primitive'. C++ rules for
// arithmetic conversions are designed to avoid losing information - for
// example, the result of adding an int and a float is a float - and we want
// the same behaviour when mixing our coordinate types with primitive types.
// We get C++ to compute the desired result type using 'decltype'.

template <class coord, class primitive>
struct CommonType;

template <class units, class primitive>
struct CommonType<IntCoordTyped<units>, primitive> {
    typedef decltype(int32_t() + primitive()) type;
};

template <class units, class primitive>
struct CommonType<CoordTyped<units>, primitive> {
    typedef decltype(Float() + primitive()) type;
};

// This is a base class that provides mixed-type operator overloads between
// a strongly-typed Coord and a primitive value. It is needed to avoid
// ambiguities at mixed-type call sites, because Coord classes are implicitly
// convertible to their underlying value type. As we transition more of our code
// to strongly-typed classes, we may be able to remove some or all of these
// overloads.
template <class coord, class primitive>
struct CoordOperatorsHelper {
  friend bool operator==(coord aA, primitive aB) {
    return aA.value == aB;
  }
  friend bool operator==(primitive aA, coord aB) {
    return aA == aB.value;
  }
  friend bool operator!=(coord aA, primitive aB) {
    return aA.value != aB;
  }
  friend bool operator!=(primitive aA, coord aB) {
    return aA != aB.value;
  }

  typedef typename CommonType<coord, primitive>::type result_type;

  friend result_type operator+(coord aA, primitive aB) {
    return aA.value + aB;
  }
  friend result_type operator+(primitive aA, coord aB) {
    return aA + aB.value;
  }
  friend result_type operator-(coord aA, primitive aB) {
    return aA.value - aB;
  }
  friend result_type operator-(primitive aA, coord aB) {
    return aA - aB.value;
  }
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

// Note: 'IntCoordTyped<units>' and 'CoordTyped<units>' do not derive from
// 'units' to work around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61959.

template<class units>
struct IntCoordTyped :
  public BaseCoord< int32_t, IntCoordTyped<units> >,
  public CoordOperatorsHelper< IntCoordTyped<units>, float >,
  public CoordOperatorsHelper< IntCoordTyped<units>, double > {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BaseCoord< int32_t, IntCoordTyped<units> > Super;

  MOZ_CONSTEXPR IntCoordTyped() : Super() {}
  MOZ_CONSTEXPR MOZ_IMPLICIT IntCoordTyped(int32_t aValue) : Super(aValue) {}
};

template<class units>
struct CoordTyped :
  public BaseCoord< Float, CoordTyped<units> >,
  public CoordOperatorsHelper< CoordTyped<units>, int32_t >,
  public CoordOperatorsHelper< CoordTyped<units>, uint32_t >,
  public CoordOperatorsHelper< CoordTyped<units>, double > {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BaseCoord< Float, CoordTyped<units> > Super;

  MOZ_CONSTEXPR CoordTyped() : Super() {}
  MOZ_CONSTEXPR MOZ_IMPLICIT CoordTyped(Float aValue) : Super(aValue) {}
  explicit MOZ_CONSTEXPR CoordTyped(const IntCoordTyped<units>& aCoord) : Super(float(aCoord.value)) {}

  void Round() {
    this->value = floor(this->value + 0.5);
  }
  void Truncate() {
    this->value = int32_t(this->value);
  }

  IntCoordTyped<units> Rounded() const {
    return IntCoordTyped<units>(int32_t(floor(this->value + 0.5)));
  }
  IntCoordTyped<units> Truncated() const {
    return IntCoordTyped<units>(int32_t(this->value));
  }
};

}
}

#endif /* MOZILLA_GFX_COORD_H_ */

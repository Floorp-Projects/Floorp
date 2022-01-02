/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_MeasureUnit_h_
#define intl_components_MeasureUnit_h_

#include "mozilla/Assertions.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <utility>

struct UResourceBundle;

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for working with measurement units in
 * internationalization code. It is used in coordination with other operations
 * such as number formatting.
 */
class MeasureUnit final {
  class UResourceBundleDeleter {
   public:
    void operator()(UResourceBundle* aPtr);
  };

  using UniqueUResourceBundle =
      UniquePtr<UResourceBundle, UResourceBundleDeleter>;

 public:
  MeasureUnit() = delete;

  class Enumeration final {
    // Resource bundle for the root locale.
    UniqueUResourceBundle mRootLocale = nullptr;

    // Resource bundle for the root locale's "units" resource table.
    UniqueUResourceBundle mUnits = nullptr;

    // The overall amount of available units.
    int32_t mUnitsSize = 0;

   public:
    Enumeration(UniqueUResourceBundle aRootLocale,
                UniqueUResourceBundle aUnits);

    class Iterator {
     public:
      // std::iterator traits.
      using iterator_category = std::input_iterator_tag;
      using value_type = SpanResult<char>;
      using difference_type = ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;

     private:
      const Enumeration& mEnumeration;

      // Resource bundle to a measurement type within the "units" table.
      //
      // Measurement types describe various categories, like "area", "length",
      // or "mass".
      UniqueUResourceBundle mType = nullptr;

      // Resource bundle to a specific subtype within the type table.
      //
      // Measurement subtypes describe concrete measure units, like "acre",
      // "meter", or "kilogram".
      UniqueUResourceBundle mSubtype = nullptr;

      // The next position within the "units" table.
      int32_t mUnitsPos = 0;

      // The overall amount of types within the |mType| table.
      int32_t mTypeSize = 0;

      // The next position within the |mType| table.
      int32_t mTypePos = 0;

      // Flag set when an ICU error has occurred. All further operations on this
      // iterator will return an error result when this flag is set.
      bool mHasError = false;

      void advance();

     public:
      Iterator(const Enumeration& aEnumeration, int32_t aUnitsPos)
          : mEnumeration(aEnumeration), mUnitsPos(aUnitsPos) {
        advance();
      }

      Iterator& operator++() {
        advance();
        return *this;
      }

      // The post-increment operator would return an invalid iterator, so it's
      // not implemented.
      Iterator operator++(int) = delete;

      bool operator==(const Iterator& aOther) const {
        // It's an error to compare an iterator against an iterator from a
        // different enumeration.
        MOZ_ASSERT(&mEnumeration == &aOther.mEnumeration);

        return mUnitsPos == aOther.mUnitsPos && mTypeSize == aOther.mTypeSize &&
               mTypePos == aOther.mTypePos && mHasError == aOther.mHasError;
      }

      bool operator!=(const Iterator& aOther) const {
        return !(*this == aOther);
      }

      value_type operator*() const;
    };

    friend class Iterator;

    // std::iterator begin() and end() methods.

    /**
     * Return an iterator pointing to the start of the "units" table.
     */
    Iterator begin() { return Iterator(*this, 0); }

    /**
     * Return an iterator pointing to the end of the "units" table.
     */
    Iterator end() { return Iterator(*this, mUnitsSize); }

    /**
     * Create a new measurement unit enumeration.
     */
    static Result<Enumeration, ICUError> TryCreate();
  };

  /**
   * Return an enumeration over all available measurement units.
   */
  static Result<Enumeration, ICUError> GetAvailable() {
    return Enumeration::TryCreate();
  }
};

}  // namespace mozilla::intl

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/MeasureUnit.h"

#include "unicode/udata.h"
#include "unicode/ures.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

void MeasureUnit::UResourceBundleDeleter::operator()(UResourceBundle* aPtr) {
  ures_close(aPtr);
}

MeasureUnit::Enumeration::Enumeration(UniqueUResourceBundle aRootLocale,
                                      UniqueUResourceBundle aUnits)
    : mRootLocale(std::move(aRootLocale)), mUnits(std::move(aUnits)) {
  mUnitsSize = ures_getSize(mUnits.get());
}

MeasureUnit::Enumeration::Iterator::value_type
MeasureUnit::Enumeration::Iterator::operator*() const {
  // Return an error result after an ICU error has occurred.
  if (mHasError) {
    return Err(InternalError{});
  }

  // Otherwise return the name of the current measurement unit.
  const char* unitIdentifier = ures_getKey(mSubtype.get());
  MOZ_ASSERT(unitIdentifier);
  return MakeStringSpan(unitIdentifier);
}

void MeasureUnit::Enumeration::Iterator::advance() {
  // Reject any attempts to modify this iterator after an error has occurred.
  if (mHasError) {
    return;
  }

  while (true) {
    // Read the next measurement unit in the types table.
    if (mTypePos < mTypeSize) {
      UErrorCode status = U_ZERO_ERROR;
      UResourceBundle* rawSubtype =
          ures_getByIndex(mType.get(), mTypePos, nullptr, &status);
      if (U_FAILURE(status)) {
        mHasError = true;
        return;
      }

      mTypePos += 1;
      mSubtype.reset(rawSubtype);
      return;
    }

    // Read the next measurement unit type in the "units" table.
    if (mUnitsPos < mEnumeration.mUnitsSize) {
      UErrorCode status = U_ZERO_ERROR;
      UResourceBundle* rawType = ures_getByIndex(mEnumeration.mUnits.get(),
                                                 mUnitsPos, nullptr, &status);
      if (U_FAILURE(status)) {
        mHasError = true;
        return;
      }

      mUnitsPos += 1;
      mType.reset(rawType);
      mTypeSize = ures_getSize(rawType);
      mTypePos = 0;
      continue;
    }

    // All measurement units have been processed. Reset the two |mType*| fields
    // to zero to match the end-iterator state and then return.
    MOZ_ASSERT(mUnitsPos == mEnumeration.mUnitsSize);
    mTypePos = 0;
    mTypeSize = 0;
    return;
  }
}

Result<MeasureUnit::Enumeration, ICUError>
MeasureUnit::Enumeration::TryCreate() {
  // Look up the available measurement units in the resource bundle of the root
  // locale.

  static const char packageName[] =
      U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "unit";
  static const char rootLocale[] = "";

  UErrorCode status = U_ZERO_ERROR;
  UResourceBundle* rawRes = ures_open(packageName, rootLocale, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  UniqueUResourceBundle res(rawRes);

  UResourceBundle* rawUnits =
      ures_getByKey(res.get(), "units", nullptr, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  UniqueUResourceBundle units(rawUnits);

  return MeasureUnit::Enumeration(std::move(res), std::move(units));
}

}  // namespace mozilla::intl

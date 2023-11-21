/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGLengthSMILType.h"

#include "mozilla/SMILValue.h"
#include "SVGAnimatedLengthList.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

using namespace dom::SVGLength_Binding;

void SVGLengthSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mPtr = new SVGLengthAndInfo();
  aValue.mType = this;
}

void SVGLengthSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value");
  delete static_cast<SVGLengthAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGLengthSMILType::Assign(SMILValue& aDest,
                                   const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  auto* src = static_cast<const SVGLengthAndInfo*>(aSrc.mU.mPtr);
  auto* dest = static_cast<SVGLengthAndInfo*>(aDest.mU.mPtr);
  dest->CopyFrom(*src);

  return NS_OK;
}

bool SVGLengthSMILType::IsEqual(const SMILValue& aLeft,
                                const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGLengthAndInfo*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGLengthAndInfo*>(aRight.mU.mPtr);
}

nsresult SVGLengthSMILType::Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                                uint32_t aCount) const {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType, "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");

  auto& dest = *static_cast<SVGLengthAndInfo*>(aDest.mU.mPtr);
  const auto& valueToAdd =
      *static_cast<const SVGLengthAndInfo*>(aValueToAdd.mU.mPtr);

  dest.Add(valueToAdd, aCount);
  return NS_OK;
}

nsresult SVGLengthSMILType::ComputeDistance(const SMILValue& aFrom,
                                            const SMILValue& aTo,
                                            double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");

  const auto& from = *static_cast<SVGLengthAndInfo*>(aFrom.mU.mPtr);
  const auto& to = *static_cast<const SVGLengthAndInfo*>(aTo.mU.mPtr);

  MOZ_ASSERT(from.Element() == to.Element());

  dom::SVGElementMetrics metrics(from.Element());

  // Normalize both to pixels in case they're different units:
  aDistance = fabs(to.ValueInPixels(metrics) - from.ValueInPixels(metrics));

  return NS_OK;
}

nsresult SVGLengthSMILType::Interpolate(const SMILValue& aStartVal,
                                        const SMILValue& aEndVal,
                                        double aUnitDistance,
                                        SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const auto& start = *static_cast<SVGLengthAndInfo*>(aStartVal.mU.mPtr);
  const auto& end = *static_cast<const SVGLengthAndInfo*>(aEndVal.mU.mPtr);
  auto& result = *static_cast<SVGLengthAndInfo*>(aResult.mU.mPtr);

  SVGLengthAndInfo::Interpolate(start, end, aUnitDistance, result);

  return NS_OK;
}

}  // namespace mozilla

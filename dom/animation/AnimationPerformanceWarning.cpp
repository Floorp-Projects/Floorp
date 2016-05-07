/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPerformanceWarning.h"

#include "nsContentUtils.h"

namespace mozilla {

template<uint32_t N> nsresult
AnimationPerformanceWarning::ToLocalizedStringWithIntParams(
  const char* aKey, nsXPIDLString& aLocalizedString) const
{
  nsAutoString strings[N];
  const char16_t* charParams[N];

  for (size_t i = 0, n = mParams->Length(); i < n; i++) {
    strings[i].AppendInt((*mParams)[i]);
    charParams[i] = strings[i].get();
  }

  return nsContentUtils::FormatLocalizedString(
      nsContentUtils::eLAYOUT_PROPERTIES, aKey, charParams, aLocalizedString);
}

bool
AnimationPerformanceWarning::ToLocalizedString(
  nsXPIDLString& aLocalizedString) const
{
  const char* key = nullptr;

  switch (mType) {
    case Type::ContentTooSmall:
      MOZ_ASSERT(mParams && mParams->Length() == 2,
                 "Parameter's length should be 2 for ContentTooSmall");

      return NS_SUCCEEDED(
        ToLocalizedStringWithIntParams<2>("AnimationWarningContentTooSmall",
                                          aLocalizedString));
    case Type::ContentTooLarge:
      MOZ_ASSERT(mParams && mParams->Length() == 7,
                 "Parameter's length should be 7 for ContentTooLarge");

      return NS_SUCCEEDED(
        ToLocalizedStringWithIntParams<7>("AnimationWarningContentTooLarge",
                                          aLocalizedString));
    case Type::TransformBackfaceVisibilityHidden:
      key = "AnimationWarningTransformBackfaceVisibilityHidden";
      break;
    case Type::TransformPreserve3D:
      key = "AnimationWarningTransformPreserve3D";
      break;
    case Type::TransformSVG:
      key = "AnimationWarningTransformSVG";
      break;
    case Type::TransformWithGeometricProperties:
      key = "AnimationWarningTransformWithGeometricProperties";
      break;
    case Type::TransformFrameInactive:
      key = "AnimationWarningTransformFrameInactive";
      break;
    case Type::OpacityFrameInactive:
      key = "AnimationWarningOpacityFrameInactive";
      break;
  }

  nsresult rv =
    nsContentUtils::GetLocalizedString(nsContentUtils::eLAYOUT_PROPERTIES,
                                       key, aLocalizedString);
  return NS_SUCCEEDED(rv);
}

} // namespace mozilla

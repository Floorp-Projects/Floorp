/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPerformanceWarning.h"

#include "nsContentUtils.h"

namespace mozilla {

bool
AnimationPerformanceWarning::ToLocalizedString(
  nsXPIDLString& aLocalizedString) const
{
  const char* key = nullptr;

  switch (mType) {
    case Type::ContentTooLarge:
    {
      MOZ_ASSERT(mParams && mParams->Length() == 7,
                 "Parameter's length should be 7 for ContentTooLarge");

      MOZ_ASSERT(mParams->Length() <= kMaxParamsForLocalization,
                 "Parameter's length should be less than "
                 "kMaxParamsForLocalization");
      // We can pass an array of parameters whose length is greater than 7 to
      // nsContentUtils::FormatLocalizedString because
      // nsTextFormatter drops those extra parameters in the end.
      nsAutoString strings[kMaxParamsForLocalization];
      const char16_t* charParams[kMaxParamsForLocalization];

      for (size_t i = 0, n = mParams->Length(); i < n; i++) {
        strings[i].AppendInt((*mParams)[i]);
        charParams[i] = strings[i].get();
      }

      nsresult rv = nsContentUtils::FormatLocalizedString(
        nsContentUtils::eLAYOUT_PROPERTIES,
        "AnimationWarningContentTooLarge",
        charParams,
        aLocalizedString);
      return NS_SUCCEEDED(rv);
    }
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

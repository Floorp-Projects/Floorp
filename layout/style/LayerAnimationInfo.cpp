/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerAnimationInfo.h"

#include "nsCSSProps.h"          // For nsCSSProps::PropHasFlags
#include "nsCSSPropertyIDSet.h"  // For nsCSSPropertyIDSet::CompositorAnimatable

namespace mozilla {

/* static */ const Array<
    DisplayItemType, nsCSSPropertyIDSet::CompositorAnimatableDisplayItemCount()>
    LayerAnimationInfo::sDisplayItemTypes = {
        DisplayItemType::TYPE_BACKGROUND_COLOR,
        DisplayItemType::TYPE_OPACITY,
        DisplayItemType::TYPE_TRANSFORM,
};

/* static */
DisplayItemType LayerAnimationInfo::GetDisplayItemTypeForProperty(
    nsCSSPropertyID aProperty) {
  switch (aProperty) {
    case eCSSProperty_background_color:
      return DisplayItemType::TYPE_BACKGROUND_COLOR;
    case eCSSProperty_opacity:
      return DisplayItemType::TYPE_OPACITY;
    case eCSSProperty_transform:
    case eCSSProperty_translate:
    case eCSSProperty_scale:
    case eCSSProperty_rotate:
      return DisplayItemType::TYPE_TRANSFORM;
    default:
      break;
  }
  return DisplayItemType::TYPE_ZERO;
}

}  // namespace mozilla

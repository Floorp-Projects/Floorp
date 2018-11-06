/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerAnimationInfo.h"

#include "nsCSSProps.h" // For nsCSSProps::PropHasFlags
#include "nsCSSPropertyIDSet.h" // For nsCSSPropertyIDSet::CompositorAnimatable

namespace mozilla {

/* static */ const LayerAnimationInfo::Record LayerAnimationInfo::sRecords[] =
  { { eCSSProperty_transform,
      DisplayItemType::TYPE_TRANSFORM,
      nsChangeHint_UpdateTransformLayer },
    { eCSSProperty_opacity,
      DisplayItemType::TYPE_OPACITY,
      nsChangeHint_UpdateOpacityLayer } };

/* static */ DisplayItemType
LayerAnimationInfo::GetDisplayItemTypeForProperty(nsCSSPropertyID aProperty)
{
  switch (aProperty) {
    case eCSSProperty_opacity:
      return DisplayItemType::TYPE_OPACITY;
    case eCSSProperty_transform:
      return DisplayItemType::TYPE_TRANSFORM;
    default:
      break;
  }
  return DisplayItemType::TYPE_ZERO;
}

#ifdef DEBUG
/* static */ void
LayerAnimationInfo::Initialize()
{
  for (const Record& record : sRecords) {
    MOZ_ASSERT(nsCSSProps::PropHasFlags(record.mProperty,
                                        CSSPropFlags::CanAnimateOnCompositor),
               "CSS property with entry in LayerAnimation::sRecords does not "
               "have the CSSPropFlags::CanAnimateOnCompositor flag");
  }

  nsCSSPropertyIDSet properties;
  // Check that every property with the flag for animating on the
  // compositor has an entry in LayerAnimationInfo::sRecords.
  for (nsCSSPropertyID prop = nsCSSPropertyID(0);
       prop < eCSSProperty_COUNT;
       prop = nsCSSPropertyID(prop + 1)) {
    if (nsCSSProps::PropHasFlags(prop,
                                 CSSPropFlags::CanAnimateOnCompositor)) {
      bool found = false;
      for (const Record& record : sRecords) {
        if (record.mProperty == prop) {
          found = true;
          properties.AddProperty(record.mProperty);
          break;
        }
      }
      MOZ_ASSERT(found,
                 "CSS property with the CSSPropFlags::CanAnimateOnCompositor "
                 "flag does not have an entry in LayerAnimationInfo::sRecords");
      MOZ_ASSERT(GetDisplayItemTypeForProperty(prop) !=
                   DisplayItemType::TYPE_ZERO,
                 "GetDisplayItemTypeForProperty should return a valid display "
                 "item type");
    }
  }
  MOZ_ASSERT(properties.Equals(nsCSSPropertyIDSet::CompositorAnimatables()));
}
#endif

} // namespace mozilla

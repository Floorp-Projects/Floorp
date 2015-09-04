/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerAnimationInfo.h"

#include "nsCSSProps.h" // For nsCSSProps::PropHasFlags

namespace mozilla {

/* static */ const LayerAnimationInfo::Record LayerAnimationInfo::sRecords[] =
  { { eCSSProperty_transform,
      nsDisplayItem::TYPE_TRANSFORM,
      nsChangeHint_UpdateTransformLayer },
    { eCSSProperty_opacity,
      nsDisplayItem::TYPE_OPACITY,
      nsChangeHint_UpdateOpacityLayer } };

#ifdef DEBUG
/* static */ void
LayerAnimationInfo::Initialize()
{
  for (const Record& record : sRecords) {
    MOZ_ASSERT(nsCSSProps::PropHasFlags(record.mProperty,
                                        CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
               "CSS property with entry in LayerAnimation::sRecords does not "
               "have the CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR flag");
  }

  // Check that every property with the flag for animating on the
  // compositor has an entry in LayerAnimationInfo::sRecords.
  for (nsCSSProperty prop = nsCSSProperty(0);
       prop < eCSSProperty_COUNT;
       prop = nsCSSProperty(prop + 1)) {
    if (nsCSSProps::PropHasFlags(prop,
                                 CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR)) {
      bool found = false;
      for (const Record& record : sRecords) {
        if (record.mProperty == prop) {
          found = true;
          break;
        }
      }
      MOZ_ASSERT(found,
                 "CSS property with the CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR "
                 "flag does not have an entry in LayerAnimationInfo::sRecords");
    }
  }
}
#endif

} // namespace mozilla

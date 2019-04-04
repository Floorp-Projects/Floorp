/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LayerAnimationInfo_h
#define mozilla_LayerAnimationInfo_h

#include "nsChangeHint.h"
#include "nsCSSPropertyID.h"
#include "nsCSSPropertyIDSet.h"
#include "nsDisplayItemTypes.h"  // For nsDisplayItem::Type
#include "mozilla/Array.h"

namespace mozilla {

struct LayerAnimationInfo {
  // Returns the corresponding display item type for |aProperty| when it is
  // animated on the compositor.
  // Returns DisplayItemType::TYPE_ZERO if |aProperty| cannot be animated on the
  // compositor.
  static DisplayItemType GetDisplayItemTypeForProperty(
      nsCSSPropertyID aProperty);

  // Returns the corresponding CSS properties for |aDisplayItemType|.
  //
  // This function works only for display items tied to CSS properties that can
  // be animated on the compositor.
  static inline const nsCSSPropertyIDSet& GetCSSPropertiesFor(
      DisplayItemType aDisplayItemType) {
    static const nsCSSPropertyIDSet transformProperties =
        nsCSSPropertyIDSet{eCSSProperty_transform, eCSSProperty_translate,
                           eCSSProperty_scale, eCSSProperty_rotate};
    static const nsCSSPropertyIDSet opacityProperties =
        nsCSSPropertyIDSet{eCSSProperty_opacity};
    static const nsCSSPropertyIDSet backgroundColorProperties =
        nsCSSPropertyIDSet{eCSSProperty_background_color};
    static const nsCSSPropertyIDSet empty = nsCSSPropertyIDSet();

    switch (aDisplayItemType) {
      case DisplayItemType::TYPE_BACKGROUND_COLOR:
        return backgroundColorProperties;
      case DisplayItemType::TYPE_OPACITY:
        return opacityProperties;
      case DisplayItemType::TYPE_TRANSFORM:
        return transformProperties;
      default:
        MOZ_ASSERT_UNREACHABLE(
            "Should not be called for display item types "
            "that are not able to have animations on the "
            "compositor");
        return empty;
    }
  }

  // Returns the appropriate change hint for updating the display item for
  // |aDisplayItemType|.
  //
  // This function works only for display items tied to CSS properties that can
  // be animated on the compositor.
  static inline nsChangeHint GetChangeHintFor(
      DisplayItemType aDisplayItemType) {
    switch (aDisplayItemType) {
      case DisplayItemType::TYPE_BACKGROUND_COLOR:
        return nsChangeHint_RepaintFrame;
      case DisplayItemType::TYPE_OPACITY:
        return nsChangeHint_UpdateOpacityLayer;
      case DisplayItemType::TYPE_TRANSFORM:
        return nsChangeHint_UpdateTransformLayer;
      default:
        MOZ_ASSERT_UNREACHABLE(
            "Should not be called for display item types "
            "that are not able to have animations on the "
            "compositor");
        return nsChangeHint(0);
    }
  }

  // An array of DisplayItemType corresponding to the display item that we can
  // animate on the compositor.
  //
  // This is used to look up the appropriate change hint in cases when
  // animations need updating but no other change hint is generated.
  static const Array<DisplayItemType,
                     nsCSSPropertyIDSet::CompositorAnimatableDisplayItemCount()>
      sDisplayItemTypes;
};

}  // namespace mozilla

#endif /* !defined(mozilla_LayerAnimationInfo_h) */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable CSS property on an element */

#include "nsSMILCSSProperty.h"

#include "mozilla/dom/Element.h"
#include "mozilla/Move.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "nsICSSDeclaration.h"
#include "nsSMILCSSValueType.h"
#include "nsSMILValue.h"
#include "nsCSSProps.h"

using namespace mozilla;
using namespace mozilla::dom;

// Class Methods
nsSMILCSSProperty::nsSMILCSSProperty(nsCSSPropertyID aPropID,
                                     Element* aElement,
                                     nsStyleContext* aBaseStyleContext)
  : mPropID(aPropID)
  , mElement(aElement)
  , mBaseStyleContext(aBaseStyleContext)
{
  MOZ_ASSERT(IsPropertyAnimatable(mPropID,
               aElement->OwnerDoc()->GetStyleBackendType()),
             "Creating a nsSMILCSSProperty for a property "
             "that's not supported for animation");
}

nsSMILValue
nsSMILCSSProperty::GetBaseValue() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  nsSMILValue baseValue;

  // SPECIAL CASE: (a) Shorthands
  //               (b) 'display'
  //               (c) No base style context
  if (nsCSSProps::IsShorthand(mPropID) ||
      mPropID == eCSSProperty_display ||
      !mBaseStyleContext) {
    // We can't look up the base (computed-style) value of shorthand
    // properties because they aren't guaranteed to have a consistent computed
    // value.
    //
    // Also, although we can look up the base value of the display property,
    // doing so involves clearing and resetting the property which can cause
    // frames to be recreated which we'd like to avoid.
    //
    // Furthermore, if we don't (yet) have a base style context we obviously
    // can't resolve a base value.
    //
    // In any case, just return a dummy value (initialized with the right
    // type, so as not to indicate failure).
    nsSMILValue tmpVal(&nsSMILCSSValueType::sSingleton);
    Swap(baseValue, tmpVal);
    return baseValue;
  }

  AnimationValue computedValue;
  if (mElement->IsStyledByServo()) {
    const ServoComputedValues* currentStyle =
      mBaseStyleContext->StyleSource().AsServoComputedValues();
    computedValue.mServo =
      Servo_ComputedValues_ExtractAnimationValue(currentStyle, mPropID)
      .Consume();
    if (!computedValue.mServo) {
      return baseValue;
    }
  } else if (!StyleAnimationValue::ExtractComputedValue(mPropID,
                                                        mBaseStyleContext,
                                                        computedValue.mGecko)) {
    return baseValue;
  }

  baseValue =
    nsSMILCSSValueType::ValueFromAnimationValue(mPropID, mElement,
                                                computedValue);
  return baseValue;
}

nsresult
nsSMILCSSProperty::ValueFromString(const nsAString& aStr,
                                   const SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID,
                   mElement->OwnerDoc()->GetStyleBackendType()),
                 NS_ERROR_FAILURE);

  nsSMILCSSValueType::ValueFromString(mPropID, mElement, aStr, aValue,
      &aPreventCachingOfSandwich);

  if (aValue.IsNull()) {
    return NS_ERROR_FAILURE;
  }

  // XXX Due to bug 536660 (or at least that seems to be the most likely
  // culprit), when we have animation setting display:none on a <use> element,
  // if we DON'T set the property every sample, chaos ensues.
  if (!aPreventCachingOfSandwich && mPropID == eCSSProperty_display) {
    aPreventCachingOfSandwich = true;
  }
  return NS_OK;
}

nsresult
nsSMILCSSProperty::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID,
                   mElement->OwnerDoc()->GetStyleBackendType()),
                 NS_ERROR_FAILURE);

  // Convert nsSMILValue to string
  nsAutoString valStr;
  nsSMILCSSValueType::ValueToString(aValue, valStr);

  // Use string value to style the target element
  nsICSSDeclaration* overrideDecl = mElement->GetSMILOverrideStyle();
  if (overrideDecl) {
    nsAutoString oldValStr;
    overrideDecl->GetPropertyValue(mPropID, oldValStr);
    if (valStr.Equals(oldValStr)) {
      return NS_OK;
    }
    overrideDecl->SetPropertyValue(mPropID, valStr);
  }
  return NS_OK;
}

void
nsSMILCSSProperty::ClearAnimValue()
{
  // Put empty string in override style for our property
  nsICSSDeclaration* overrideDecl = mElement->GetSMILOverrideStyle();
  if (overrideDecl) {
    overrideDecl->SetPropertyValue(mPropID, EmptyString());
  }
}

// Based on http://www.w3.org/TR/SVG/propidx.html
// static
bool
nsSMILCSSProperty::IsPropertyAnimatable(nsCSSPropertyID aPropID,
                                        StyleBackendType aBackend)
{
  // Bug 1353918: Drop this check
  if (aBackend == StyleBackendType::Servo &&
      !Servo_Property_IsAnimatable(aPropID)) {
    return false;
  }

  // NOTE: Right now, Gecko doesn't recognize the following properties from
  // the SVG Property Index:
  //   alignment-baseline
  //   baseline-shift
  //   color-profile
  //   color-rendering
  //   glyph-orientation-horizontal
  //   glyph-orientation-vertical
  //   kerning
  //   writing-mode

  switch (aPropID) {
    case eCSSProperty_clip:
    case eCSSProperty_clip_rule:
    case eCSSProperty_clip_path:
    case eCSSProperty_color:
    case eCSSProperty_color_interpolation:
    case eCSSProperty_color_interpolation_filters:
    case eCSSProperty_cursor:
    case eCSSProperty_display:
    case eCSSProperty_dominant_baseline:
    case eCSSProperty_fill:
    case eCSSProperty_fill_opacity:
    case eCSSProperty_fill_rule:
    case eCSSProperty_filter:
    case eCSSProperty_flood_color:
    case eCSSProperty_flood_opacity:
    case eCSSProperty_font:
    case eCSSProperty_font_family:
    case eCSSProperty_font_size:
    case eCSSProperty_font_size_adjust:
    case eCSSProperty_font_stretch:
    case eCSSProperty_font_style:
    case eCSSProperty_font_variant:
    case eCSSProperty_font_weight:
    case eCSSProperty_height:
    case eCSSProperty_image_rendering:
    case eCSSProperty_letter_spacing:
    case eCSSProperty_lighting_color:
    case eCSSProperty_marker:
    case eCSSProperty_marker_end:
    case eCSSProperty_marker_mid:
    case eCSSProperty_marker_start:
    case eCSSProperty_mask:
    case eCSSProperty_mask_type:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow:
    case eCSSProperty_pointer_events:
    case eCSSProperty_shape_rendering:
    case eCSSProperty_stop_color:
    case eCSSProperty_stop_opacity:
    case eCSSProperty_stroke:
    case eCSSProperty_stroke_dasharray:
    case eCSSProperty_stroke_dashoffset:
    case eCSSProperty_stroke_linecap:
    case eCSSProperty_stroke_linejoin:
    case eCSSProperty_stroke_miterlimit:
    case eCSSProperty_stroke_opacity:
    case eCSSProperty_stroke_width:
    case eCSSProperty_text_anchor:
    case eCSSProperty_text_decoration:
    case eCSSProperty_text_decoration_line:
    case eCSSProperty_text_rendering:
    case eCSSProperty_vector_effect:
    case eCSSProperty_width:
    case eCSSProperty_visibility:
    case eCSSProperty_word_spacing:
      return true;

    // EXPLICITLY NON-ANIMATABLE PROPERTIES:
    // (Some of these aren't supported at all in Gecko -- I've commented those
    // ones out. If/when we add support for them, uncomment their line here)
    // ----------------------------------------------------------------------
    // case eCSSProperty_enable_background:
    // case eCSSProperty_glyph_orientation_horizontal:
    // case eCSSProperty_glyph_orientation_vertical:
    // case eCSSProperty_writing_mode:
    case eCSSProperty_direction:
    case eCSSProperty_unicode_bidi:
      return false;

    default:
      return false;
  }
}

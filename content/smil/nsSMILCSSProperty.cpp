/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable CSS property on an element */

#include "nsSMILCSSProperty.h"
#include "nsSMILCSSValueType.h"
#include "nsSMILValue.h"
#include "nsComputedDOMStyle.h"
#include "nsStyleAnimation.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMElement.h"

using namespace mozilla::dom;

// Helper function
static bool
GetCSSComputedValue(nsIContent* aElem,
                    nsCSSProperty aPropID,
                    nsAString& aResult)
{
  NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aPropID),
                    "Can't look up computed value of shorthand property");
  NS_ABORT_IF_FALSE(nsSMILCSSProperty::IsPropertyAnimatable(aPropID),
                    "Shouldn't get here for non-animatable properties");

  nsIDocument* doc = aElem->GetCurrentDoc();
  if (!doc) {
    // This can happen if we process certain types of restyles mid-sample
    // and remove anonymous animated content from the document as a result.
    // See bug 534975.
    return false;
  }

  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
    NS_WARNING("Unable to look up computed style -- no pres shell");
    return false;
  }

  nsRefPtr<nsComputedDOMStyle> computedStyle;
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(aElem));
  nsresult rv = NS_NewComputedDOMStyle(domElement, EmptyString(), shell,
                                       getter_AddRefs(computedStyle));

  if (NS_SUCCEEDED(rv)) {
    computedStyle->GetPropertyValue(aPropID, aResult);
    return true;
  }
  return false;
}

// Class Methods
nsSMILCSSProperty::nsSMILCSSProperty(nsCSSProperty aPropID,
                                     Element* aElement)
  : mPropID(aPropID), mElement(aElement)
{
  NS_ABORT_IF_FALSE(IsPropertyAnimatable(mPropID),
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
  if (nsCSSProps::IsShorthand(mPropID) || mPropID == eCSSProperty_display) {
    // We can't look up the base (computed-style) value of shorthand
    // properties because they aren't guaranteed to have a consistent computed
    // value.
    //
    // Also, although we can look up the base value of the display property,
    // doing so involves clearing and resetting the property which can cause
    // frames to be recreated which we'd like to avoid.
    //
    // In either case, just return a dummy value (initialized with the right
    // type, so as not to indicate failure).
    nsSMILValue tmpVal(&nsSMILCSSValueType::sSingleton);
    baseValue.Swap(tmpVal);
    return baseValue;
  }

  // GENERAL CASE: Non-Shorthands
  // (1) Put empty string in override style for property mPropID
  // (saving old override style value, so we can set it again when we're done)
  nsICSSDeclaration* overrideDecl = mElement->GetSMILOverrideStyle();
  nsAutoString cachedOverrideStyleVal;
  if (overrideDecl) {
    overrideDecl->GetPropertyValue(mPropID, cachedOverrideStyleVal);
    // (Don't bother clearing override style if it's already empty)
    if (!cachedOverrideStyleVal.IsEmpty()) {
      overrideDecl->SetPropertyValue(mPropID, EmptyString());
    }
  }

  // (2) Get Computed Style
  nsAutoString computedStyleVal;
  bool didGetComputedVal = GetCSSComputedValue(mElement, mPropID,
                                                 computedStyleVal);

  // (3) Put cached override style back (if it's non-empty)
  if (overrideDecl && !cachedOverrideStyleVal.IsEmpty()) {
    overrideDecl->SetPropertyValue(mPropID, cachedOverrideStyleVal);
  }

  // (4) Populate our nsSMILValue from the computed style
  if (didGetComputedVal) {
    // When we parse animation values we check if they are context-sensitive or
    // not so that we don't cache animation values whose meaning may change.
    // For base values however this is unnecessary since on each sample the
    // compositor will fetch the (computed) base value and compare it against
    // the cached (computed) value and detect changes for us.
    nsSMILCSSValueType::ValueFromString(mPropID, mElement,
                                        computedStyleVal, baseValue,
                                        nsnull);
  }
  return baseValue;
}

nsresult
nsSMILCSSProperty::ValueFromString(const nsAString& aStr,
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID), NS_ERROR_FAILURE);

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
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID), NS_ERROR_FAILURE);

  // Convert nsSMILValue to string
  nsAutoString valStr;
  if (!nsSMILCSSValueType::ValueToString(aValue, valStr)) {
    NS_WARNING("Failed to convert nsSMILValue for CSS property into a string");
    return NS_ERROR_FAILURE;
  }

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
nsSMILCSSProperty::IsPropertyAnimatable(nsCSSProperty aPropID)
{
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
    case eCSSProperty_image_rendering:
    case eCSSProperty_letter_spacing:
    case eCSSProperty_lighting_color:
    case eCSSProperty_marker:
    case eCSSProperty_marker_end:
    case eCSSProperty_marker_mid:
    case eCSSProperty_marker_start:
    case eCSSProperty_mask:
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
    case eCSSProperty_text_blink:
    case eCSSProperty_text_decoration:
    case eCSSProperty_text_decoration_line:
    case eCSSProperty_text_rendering:
    case eCSSProperty_vector_effect:
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

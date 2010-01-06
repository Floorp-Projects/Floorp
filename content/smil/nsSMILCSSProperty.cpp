/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* representation of a SMIL-animatable CSS property on an element */

#include "nsSMILCSSProperty.h"
#include "nsSMILCSSValueType.h"
#include "nsSMILValue.h"
#include "nsCSSDeclaration.h"
#include "nsComputedDOMStyle.h"
#include "nsStyleAnimation.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"

static PRBool
GetCSSComputedValue(nsIContent* aElem,
                    nsCSSProperty aPropID,
                    nsAString& aResult)
{
  NS_ENSURE_TRUE(nsSMILCSSProperty::IsPropertyAnimatable(aPropID),
                 PR_FALSE);

  nsIDocument* doc = aElem->GetCurrentDoc();
  if (!doc) {
    // This can happen if we process certain types of restyles mid-sample
    // and remove anonymous animated content from the document as a result.
    // See bug 534975.
    return PR_FALSE;
  }

  nsIPresShell* shell = doc->GetPrimaryShell();
  if (!shell) {
    NS_WARNING("Unable to look up computed style -- no pres shell");
    return PR_FALSE;
  }

  nsRefPtr<nsComputedDOMStyle> computedStyle;
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(aElem));
  nsresult rv = NS_NewComputedDOMStyle(domElement, EmptyString(), shell,
                                       getter_AddRefs(computedStyle));

  if (NS_SUCCEEDED(rv) && computedStyle) {
    // NOTE: This will produce an empty string for shorthand values
    computedStyle->GetPropertyValue(aPropID, aResult);
    return PR_TRUE;
  }
  return PR_FALSE;
}

// Class Methods
nsSMILCSSProperty::nsSMILCSSProperty(nsCSSProperty aPropID,
                                     nsIContent* aElement)
  : mPropID(aPropID), mElement(aElement)
{
  NS_ABORT_IF_FALSE(IsPropertyAnimatable(mPropID),
                    "Creating a nsSMILCSSProperty for a property "
                    "that's not supported for animation");
}

nsSMILValue
nsSMILCSSProperty::GetBaseValue() const
{
  // (1) Put empty string in override style for property mPropID
  // (saving old override style value, so we can set it again when we're done)
  nsCOMPtr<nsIDOMCSSStyleDeclaration> overrideStyle;
  mElement->GetSMILOverrideStyle(getter_AddRefs(overrideStyle));
  nsCOMPtr<nsICSSDeclaration> overrideDecl = do_QueryInterface(overrideStyle);
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
  PRBool didGetComputedVal = GetCSSComputedValue(mElement, mPropID,
                                                 computedStyleVal);

  // (3) Put cached override style back (if it's non-empty)
  if (overrideDecl && !cachedOverrideStyleVal.IsEmpty()) {
    overrideDecl->SetPropertyValue(mPropID, cachedOverrideStyleVal);
  }

  nsSMILValue baseValue;
  if (didGetComputedVal) {
    // (4) Create the nsSMILValue from the computed style value
    nsSMILCSSValueType::sSingleton.Init(baseValue);
    if (!nsCSSProps::IsShorthand(mPropID) &&
        !nsSMILCSSValueType::sSingleton.ValueFromString(mPropID, mElement,
                                                        computedStyleVal,
                                                        baseValue)) {
      nsSMILCSSValueType::sSingleton.Destroy(baseValue);
      NS_ABORT_IF_FALSE(baseValue.IsNull(),
                        "Destroy should leave us with null-typed value");
    }
  }
  return baseValue;
}

nsresult
nsSMILCSSProperty::ValueFromString(const nsAString& aStr,
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue) const
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID), NS_ERROR_FAILURE);
  nsSMILCSSValueType::sSingleton.Init(aValue);
  PRBool success =
    nsSMILCSSValueType::sSingleton.ValueFromString(mPropID, mElement,
                                                   aStr, aValue);
  if (!success) {
    nsSMILCSSValueType::sSingleton.Destroy(aValue);
  }
  return success ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSMILCSSProperty::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID), NS_ERROR_FAILURE);

  nsresult rv = NS_OK;
  nsAutoString valStr;

  if (nsSMILCSSValueType::sSingleton.ValueToString(aValue, valStr)) {
    // Apply the style to the target element
    nsCOMPtr<nsIDOMCSSStyleDeclaration> overrideStyle;
    mElement->GetSMILOverrideStyle(getter_AddRefs(overrideStyle));
    NS_ABORT_IF_FALSE(overrideStyle, "Need a non-null overrideStyle");

    nsCOMPtr<nsICSSDeclaration> overrideDecl =
      do_QueryInterface(overrideStyle);
    if (overrideDecl) {
      overrideDecl->SetPropertyValue(mPropID, valStr);
    }
  } else {
    NS_WARNING("Failed to convert nsSMILValue for CSS property into a string");
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

void
nsSMILCSSProperty::ClearAnimValue()
{
  // Put empty string in override style for property propID
  nsCOMPtr<nsIDOMCSSStyleDeclaration> overrideStyle;
  mElement->GetSMILOverrideStyle(getter_AddRefs(overrideStyle));
  nsCOMPtr<nsICSSDeclaration> overrideDecl = do_QueryInterface(overrideStyle);
  if (overrideDecl) {
    overrideDecl->SetPropertyValue(mPropID, EmptyString());
  }
}

// Based on http://www.w3.org/TR/SVG/propidx.html
// static
PRBool
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
    case eCSSProperty_text_decoration:
    case eCSSProperty_text_rendering:
    case eCSSProperty_visibility:
    case eCSSProperty_word_spacing:
      return PR_TRUE;

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
      return PR_FALSE;

    default:
      return PR_FALSE;
  }
}

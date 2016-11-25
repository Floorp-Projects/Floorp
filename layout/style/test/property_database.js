/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility function. Returns true if the given boolean pref...
//  (a) exists and (b) is set to true.
// Otherwise, returns false.
//
// This function also reports a test failure if the pref isn't set at all. This
// ensures that we remove pref-checks from mochitests (instead of accidentally
// disabling the tests that are controlled by that check) when we remove a
// mature feature's pref from the rest of the codebase.
function IsCSSPropertyPrefEnabled(prefName)
{
  try {
    if (SpecialPowers.getBoolPref(prefName)) {
      return true;
    }
  } catch (ex) {
    ok(false, "Failed to look up property-controlling pref '" +
       prefName + "' (" + ex + ")");
  }

  return false;
}

// True longhand properties.
const CSS_TYPE_LONGHAND = 0;

// True shorthand properties.
const CSS_TYPE_TRUE_SHORTHAND = 1;

// Properties that we handle as shorthands but were longhands either in
// the current spec or earlier versions of the spec.
const CSS_TYPE_SHORTHAND_AND_LONGHAND = 2;

// Each property has the following fields:
//   domProp: The name of the relevant member of nsIDOM[NS]CSS2Properties
//   inherited: Whether the property is inherited by default (stated as
//     yes or no in the property header in all CSS specs)
//   type: see above
//   alias_for: optional, indicates that the property is an alias for
//     some other property that is the preferred serialization.  (Type
//     must not be CSS_TYPE_LONGHAND.)
//   logical: optional, indicates that the property is a logical directional
//     property.  (Type must be CSS_TYPE_LONGHAND.)
//   axis: optional, indicates that the property is an axis-related logical
//     directional property.  (Type must be CSS_TYPE_LONGHAND and 'logical'
//     must be true.)
//   get_computed: if present, the property's computed value shows up on
//     another property, and this is a function used to get it
//   initial_values: Values whose computed value should be the same as the
//     computed value for the property's initial value.
//   other_values: Values whose computed value should be different from the
//     computed value for the property's initial value.
//   XXX Should have a third field for values whose computed value may or
//     may not be the same as for the property's initial value.
//   invalid_values: Things that are not values for the property and
//     should be rejected, but which are balanced and should not absorb
//     what follows
//   quirks_values: Values that should be accepted in quirks mode only,
//     mapped to the values they are equivalent to.
//   unbalanced_values: Things that are not values for the property and
//     should be rejected, and which also contain unbalanced constructs
//     that should absorb what follows
//
// Note: By default, an alias is assumed to accept/reject the same values as
// the property that it aliases, and to have the same prerequisites. So, if
// "alias_for" is set, the "*_values" and "prerequisites" fields can simply
// be omitted, and they'll be populated automatically to match the aliased
// property's fields.

// Helper functions used to construct gCSSProperties.

function initial_font_family_is_sans_serif()
{
  // The initial value of 'font-family' might be 'serif' or
  // 'sans-serif'.
  var div = document.createElement("div");
  div.setAttribute("style", "font: initial");
  return getComputedStyle(div, "").fontFamily == "sans-serif";
}
var gInitialFontFamilyIsSansSerif = initial_font_family_is_sans_serif();

// shared by background-image and border-image-source
var validGradientAndElementValues = [
  "-moz-element(#a)",
  "-moz-element(  #a  )",
  "-moz-element(#a-1)",
  "-moz-element(#a\\:1)",
  /* gradient torture test */
  "linear-gradient(red, blue)",
  "linear-gradient(red, yellow, blue)",
  "linear-gradient(red 1px, yellow 20%, blue 24em, green)",
  "linear-gradient(red, yellow, green, blue 50%)",
  "linear-gradient(red -50%, yellow -25%, green, blue)",
  "linear-gradient(red -99px, yellow, green, blue 120%)",
  "linear-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
  "linear-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",
  "linear-gradient(red, green calc(50% + 20px), blue)",

  "linear-gradient(to top, red, blue)",
  "linear-gradient(to bottom, red, blue)",
  "linear-gradient(to left, red, blue)",
  "linear-gradient(to right, red, blue)",
  "linear-gradient(to top left, red, blue)",
  "linear-gradient(to top right, red, blue)",
  "linear-gradient(to bottom left, red, blue)",
  "linear-gradient(to bottom right, red, blue)",
  "linear-gradient(to left top, red, blue)",
  "linear-gradient(to left bottom, red, blue)",
  "linear-gradient(to right top, red, blue)",
  "linear-gradient(to right bottom, red, blue)",

  "linear-gradient(-33deg, red, blue)",
  "linear-gradient(30grad, red, blue)",
  "linear-gradient(10deg, red, blue)",
  "linear-gradient(1turn, red, blue)",
  "linear-gradient(.414rad, red, blue)",

  "linear-gradient(.414rad, red, 50%, blue)",
  "linear-gradient(.414rad, red, 0%, blue)",
  "linear-gradient(.414rad, red, 100%, blue)",

  "linear-gradient(.414rad, red 50%, 50%, blue 50%)",
  "linear-gradient(.414rad, red 50%, 20%, blue 50%)",
  "linear-gradient(.414rad, red 50%, 30%, blue 10%)",
  "linear-gradient(to right bottom, red, 20%, green 50%, 65%, blue)",
  "linear-gradient(to right bottom, red, 20%, green 10%, blue)",
  "linear-gradient(to right bottom, red, 50%, green 50%, 50%, blue)",
  "linear-gradient(to right bottom, red, 0%, green 50%, 100%, blue)",

  "-moz-linear-gradient(red, blue)",
  "-moz-linear-gradient(red, yellow, blue)",
  "-moz-linear-gradient(red 1px, yellow 20%, blue 24em, green)",
  "-moz-linear-gradient(red, yellow, green, blue 50%)",
  "-moz-linear-gradient(red -50%, yellow -25%, green, blue)",
  "-moz-linear-gradient(red -99px, yellow, green, blue 120%)",
  "-moz-linear-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
  "-moz-linear-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

  "-moz-linear-gradient(to top, red, blue)",
  "-moz-linear-gradient(to bottom, red, blue)",
  "-moz-linear-gradient(to left, red, blue)",
  "-moz-linear-gradient(to right, red, blue)",
  "-moz-linear-gradient(to top left, red, blue)",
  "-moz-linear-gradient(to top right, red, blue)",
  "-moz-linear-gradient(to bottom left, red, blue)",
  "-moz-linear-gradient(to bottom right, red, blue)",
  "-moz-linear-gradient(to left top, red, blue)",
  "-moz-linear-gradient(to left bottom, red, blue)",
  "-moz-linear-gradient(to right top, red, blue)",
  "-moz-linear-gradient(to right bottom, red, blue)",

  "-moz-linear-gradient(top left, red, blue)",
  "-moz-linear-gradient(0 0, red, blue)",
  "-moz-linear-gradient(20% bottom, red, blue)",
  "-moz-linear-gradient(center 20%, red, blue)",
  "-moz-linear-gradient(left 35px, red, blue)",
  "-moz-linear-gradient(10% 10em, red, blue)",
  "-moz-linear-gradient(44px top, red, blue)",

  "-moz-linear-gradient(0px, red, blue)",
  "-moz-linear-gradient(0, red, blue)",
  "-moz-linear-gradient(top left 45deg, red, blue)",
  "-moz-linear-gradient(20% bottom -300deg, red, blue)",
  "-moz-linear-gradient(center 20% 1.95929rad, red, blue)",
  "-moz-linear-gradient(left 35px 30grad, red, blue)",
  "-moz-linear-gradient(left 35px 0.1turn, red, blue)",
  "-moz-linear-gradient(10% 10em 99999deg, red, blue)",
  "-moz-linear-gradient(44px top -33deg, red, blue)",

  "-moz-linear-gradient(-33deg, red, blue)",
  "-moz-linear-gradient(30grad left 35px, red, blue)",
  "-moz-linear-gradient(10deg 20px, red, blue)",
  "-moz-linear-gradient(1turn 20px, red, blue)",
  "-moz-linear-gradient(.414rad bottom, red, blue)",

  "-moz-linear-gradient(blue calc(0px) ,green calc(25%) ,red calc(40px) ,blue calc(60px) , yellow  calc(100px))",
  "-moz-linear-gradient(-33deg, blue calc(-25%) ,red 40px)",
  "-moz-linear-gradient(10deg, blue calc(100px + -25%),red calc(40px))",
  "-moz-linear-gradient(10deg, blue calc(-25px),red calc(100%))",
  "-moz-linear-gradient(.414rad, blue calc(100px + -25px) ,green calc(100px + -25px) ,red calc(100px + -25%) ,blue calc(-25px) , yellow  calc(-25px))",
  "-moz-linear-gradient(1turn, blue calc(-25%) ,green calc(25px) ,red calc(25%),blue calc(0px),white 50px, yellow  calc(-25px))",

  "radial-gradient(red, blue)",
  "radial-gradient(red, yellow, blue)",
  "radial-gradient(red 1px, yellow 20%, blue 24em, green)",
  "radial-gradient(red, yellow, green, blue 50%)",
  "radial-gradient(red -50%, yellow -25%, green, blue)",
  "radial-gradient(red -99px, yellow, green, blue 120%)",
  "radial-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",

  "radial-gradient(0 0, red, blue)",
  "radial-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

  "radial-gradient(at top left, red, blue)",
  "radial-gradient(at 20% bottom, red, blue)",
  "radial-gradient(at center 20%, red, blue)",
  "radial-gradient(at left 35px, red, blue)",
  "radial-gradient(at 10% 10em, red, blue)",
  "radial-gradient(at 44px top, red, blue)",
  "radial-gradient(at 0 0, red, blue)",

  "radial-gradient(farthest-corner, red, blue)",
  "radial-gradient(circle, red, blue)",
  "radial-gradient(ellipse closest-corner, red, blue)",
  "radial-gradient(closest-corner ellipse, red, blue)",

  "radial-gradient(43px, red, blue)",
  "radial-gradient(43px 43px, red, blue)",
  "radial-gradient(50% 50%, red, blue)",
  "radial-gradient(43px 50%, red, blue)",
  "radial-gradient(50% 43px, red, blue)",
  "radial-gradient(circle 43px, red, blue)",
  "radial-gradient(43px circle, red, blue)",
  "radial-gradient(ellipse 43px 43px, red, blue)",
  "radial-gradient(ellipse 50% 50%, red, blue)",
  "radial-gradient(ellipse 43px 50%, red, blue)",
  "radial-gradient(ellipse 50% 43px, red, blue)",
  "radial-gradient(50% 43px ellipse, red, blue)",

  "radial-gradient(farthest-corner at top left, red, blue)",
  "radial-gradient(ellipse closest-corner at 45px, red, blue)",
  "radial-gradient(circle farthest-side at 45px, red, blue)",
  "radial-gradient(closest-side ellipse at 50%, red, blue)",
  "radial-gradient(farthest-corner circle at 4em, red, blue)",

  "radial-gradient(30% 40% at top left, red, blue)",
  "radial-gradient(50px 60px at 15% 20%, red, blue)",
  "radial-gradient(7em 8em at 45px, red, blue)",

  "-moz-radial-gradient(red, blue)",
  "-moz-radial-gradient(red, yellow, blue)",
  "-moz-radial-gradient(red 1px, yellow 20%, blue 24em, green)",
  "-moz-radial-gradient(red, yellow, green, blue 50%)",
  "-moz-radial-gradient(red -50%, yellow -25%, green, blue)",
  "-moz-radial-gradient(red -99px, yellow, green, blue 120%)",
  "-moz-radial-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",

  "-moz-radial-gradient(top left, red, blue)",
  "-moz-radial-gradient(20% bottom, red, blue)",
  "-moz-radial-gradient(center 20%, red, blue)",
  "-moz-radial-gradient(left 35px, red, blue)",
  "-moz-radial-gradient(10% 10em, red, blue)",
  "-moz-radial-gradient(44px top, red, blue)",

  "-moz-radial-gradient(top left 45deg, red, blue)",
  "-moz-radial-gradient(0 0, red, blue)",
  "-moz-radial-gradient(20% bottom -300deg, red, blue)",
  "-moz-radial-gradient(center 20% 1.95929rad, red, blue)",
  "-moz-radial-gradient(left 35px 30grad, red, blue)",
  "-moz-radial-gradient(10% 10em 99999deg, red, blue)",
  "-moz-radial-gradient(44px top -33deg, red, blue)",
  "-moz-radial-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

  "-moz-radial-gradient(-33deg, red, blue)",
  "-moz-radial-gradient(30grad left 35px, red, blue)",
  "-moz-radial-gradient(10deg 20px, red, blue)",
  "-moz-radial-gradient(.414rad bottom, red, blue)",

  "-moz-radial-gradient(cover, red, blue)",
  "-moz-radial-gradient(cover circle, red, blue)",
  "-moz-radial-gradient(contain, red, blue)",
  "-moz-radial-gradient(contain ellipse, red, blue)",
  "-moz-radial-gradient(circle, red, blue)",
  "-moz-radial-gradient(ellipse closest-corner, red, blue)",
  "-moz-radial-gradient(farthest-side circle, red, blue)",

  "-moz-radial-gradient(top left, cover, red, blue)",
  "-moz-radial-gradient(15% 20%, circle, red, blue)",
  "-moz-radial-gradient(45px, ellipse closest-corner, red, blue)",
  "-moz-radial-gradient(45px, farthest-side circle, red, blue)",

  "-moz-radial-gradient(99deg, cover, red, blue)",
  "-moz-radial-gradient(-1.2345rad, circle, red, blue)",
  "-moz-radial-gradient(399grad, ellipse closest-corner, red, blue)",
  "-moz-radial-gradient(399grad, farthest-side circle, red, blue)",

  "-moz-radial-gradient(top left 99deg, cover, red, blue)",
  "-moz-radial-gradient(15% 20% -1.2345rad, circle, red, blue)",
  "-moz-radial-gradient(45px 399grad, ellipse closest-corner, red, blue)",
  "-moz-radial-gradient(45px 399grad, farthest-side circle, red, blue)",

  "-moz-repeating-linear-gradient(red, blue)",
  "-moz-repeating-linear-gradient(red, yellow, blue)",
  "-moz-repeating-linear-gradient(red 1px, yellow 20%, blue 24em, green)",
  "-moz-repeating-linear-gradient(red, yellow, green, blue 50%)",
  "-moz-repeating-linear-gradient(red -50%, yellow -25%, green, blue)",
  "-moz-repeating-linear-gradient(red -99px, yellow, green, blue 120%)",
  "-moz-repeating-linear-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
  "-moz-repeating-linear-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

  "-moz-repeating-linear-gradient(to top, red, blue)",
  "-moz-repeating-linear-gradient(to bottom, red, blue)",
  "-moz-repeating-linear-gradient(to left, red, blue)",
  "-moz-repeating-linear-gradient(to right, red, blue)",
  "-moz-repeating-linear-gradient(to top left, red, blue)",
  "-moz-repeating-linear-gradient(to top right, red, blue)",
  "-moz-repeating-linear-gradient(to bottom left, red, blue)",
  "-moz-repeating-linear-gradient(to bottom right, red, blue)",
  "-moz-repeating-linear-gradient(to left top, red, blue)",
  "-moz-repeating-linear-gradient(to left bottom, red, blue)",
  "-moz-repeating-linear-gradient(to right top, red, blue)",
  "-moz-repeating-linear-gradient(to right bottom, red, blue)",

  "-moz-repeating-linear-gradient(top left, red, blue)",
  "-moz-repeating-linear-gradient(0 0, red, blue)",
  "-moz-repeating-linear-gradient(20% bottom, red, blue)",
  "-moz-repeating-linear-gradient(center 20%, red, blue)",
  "-moz-repeating-linear-gradient(left 35px, red, blue)",
  "-moz-repeating-linear-gradient(10% 10em, red, blue)",
  "-moz-repeating-linear-gradient(44px top, red, blue)",

  "-moz-repeating-linear-gradient(top left 45deg, red, blue)",
  "-moz-repeating-linear-gradient(20% bottom -300deg, red, blue)",
  "-moz-repeating-linear-gradient(center 20% 1.95929rad, red, blue)",
  "-moz-repeating-linear-gradient(left 35px 30grad, red, blue)",
  "-moz-repeating-linear-gradient(10% 10em 99999deg, red, blue)",
  "-moz-repeating-linear-gradient(44px top -33deg, red, blue)",

  "-moz-repeating-linear-gradient(-33deg, red, blue)",
  "-moz-repeating-linear-gradient(30grad left 35px, red, blue)",
  "-moz-repeating-linear-gradient(10deg 20px, red, blue)",
  "-moz-repeating-linear-gradient(.414rad bottom, red, blue)",

  "-moz-repeating-radial-gradient(red, blue)",
  "-moz-repeating-radial-gradient(red, yellow, blue)",
  "-moz-repeating-radial-gradient(red 1px, yellow 20%, blue 24em, green)",
  "-moz-repeating-radial-gradient(red, yellow, green, blue 50%)",
  "-moz-repeating-radial-gradient(red -50%, yellow -25%, green, blue)",
  "-moz-repeating-radial-gradient(red -99px, yellow, green, blue 120%)",
  "-moz-repeating-radial-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
  "-moz-repeating-radial-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

  "repeating-radial-gradient(at top left, red, blue)",
  "repeating-radial-gradient(at 0 0, red, blue)",
  "repeating-radial-gradient(at 20% bottom, red, blue)",
  "repeating-radial-gradient(at center 20%, red, blue)",
  "repeating-radial-gradient(at left 35px, red, blue)",
  "repeating-radial-gradient(at 10% 10em, red, blue)",
  "repeating-radial-gradient(at 44px top, red, blue)",

  "-moz-repeating-radial-gradient(farthest-corner, red, blue)",
  "-moz-repeating-radial-gradient(circle, red, blue)",
  "-moz-repeating-radial-gradient(ellipse closest-corner, red, blue)",

  "repeating-radial-gradient(farthest-corner at top left, red, blue)",
  "repeating-radial-gradient(closest-corner ellipse at 45px, red, blue)",
  "repeating-radial-gradient(farthest-side circle at 45px, red, blue)",
  "repeating-radial-gradient(ellipse closest-side at 50%, red, blue)",
  "repeating-radial-gradient(circle farthest-corner at 4em, red, blue)",

  "repeating-radial-gradient(30% 40% at top left, red, blue)",
  "repeating-radial-gradient(50px 60px at 15% 20%, red, blue)",
  "repeating-radial-gradient(7em 8em at 45px, red, blue)",

  "-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), 2, 10, 10, 2)",
  "-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), 10%, 50%, 30%, 0%)",
  "-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), 10, 50%, 30%, 0)",

  "-moz-radial-gradient(calc(25%) top, red, blue)",
  "-moz-radial-gradient(left calc(25%), red, blue)",
  "-moz-radial-gradient(calc(25px) top, red, blue)",
  "-moz-radial-gradient(left calc(25px), red, blue)",
  "-moz-radial-gradient(calc(-25%) top, red, blue)",
  "-moz-radial-gradient(left calc(-25%), red, blue)",
  "-moz-radial-gradient(calc(-25px) top, red, blue)",
  "-moz-radial-gradient(left calc(-25px), red, blue)",
  "-moz-radial-gradient(calc(100px + -25%) top, red, blue)",
  "-moz-radial-gradient(left calc(100px + -25%), red, blue)",
  "-moz-radial-gradient(calc(100px + -25px) top, red, blue)",
  "-moz-radial-gradient(left calc(100px + -25px), red, blue)"
];
var invalidGradientAndElementValues = [
  "-moz-element(#a:1)",
  "-moz-element(a#a)",
  "-moz-element(#a a)",
  "-moz-element(#a+a)",
  "-moz-element(#a())",
  /* no quirks mode colors */
  "linear-gradient(red, ff00ff)",
  /* no quirks mode colors */
  "-moz-radial-gradient(10% bottom, ffffff, black) scroll no-repeat",
  /* no quirks mode lengths */
  "-moz-linear-gradient(10 10px -45deg, red, blue) repeat",
  "-moz-linear-gradient(10px 10 -45deg, red, blue) repeat",
  "linear-gradient(red -99, yellow, green, blue 120%)",
  /* Unitless 0 is invalid as an <angle> */
  "-moz-linear-gradient(top left 0, red, blue)",
  "-moz-linear-gradient(5px 5px 0, red, blue)",
  "linear-gradient(0, red, blue)",
  /* Invalid color, calc() or -moz-image-rect() function */
  "linear-gradient(red, rgb(0, rubbish, 0) 50%, red)",
  "linear-gradient(red, red calc(50% + rubbish), red)",
  "linear-gradient(to top calc(50% + rubbish), red, blue)",
  /* Old syntax */
  "-moz-linear-gradient(10px 10px, 20px, 30px 30px, 40px, from(blue), to(red))",
  "-moz-radial-gradient(20px 20px, 10px 10px, from(green), to(#ff00ff))",
  "-moz-radial-gradient(10px 10px, 20%, 40px 40px, 10px, from(green), to(#ff00ff))",
  "-moz-linear-gradient(10px, 20px, 30px, 40px, color-stop(0.5, #00ccff))",
  "-moz-linear-gradient(20px 20px, from(blue), to(red))",
  "-moz-linear-gradient(40px 40px, 10px 10px, from(blue) to(red) color-stop(10%, fuchsia))",
  "-moz-linear-gradient(20px 20px 30px, 10px 10px, from(red), to(#ff0000))",
  "-moz-radial-gradient(left top, center, 20px 20px, 10px, from(blue), to(red))",
  "-moz-linear-gradient(left left, top top, from(blue))",
  "-moz-linear-gradient(inherit, 10px 10px, from(blue))",
  /* New syntax */
  "-moz-linear-gradient(10px 10px, 20px, 30px 30px, 40px, blue 0, red 100%)",
  "-moz-radial-gradient(20px 20px, 10px 10px, from(green), to(#ff00ff))",
  "-moz-radial-gradient(10px 10px, 20%, 40px 40px, 10px, from(green), to(#ff00ff))",
  "-moz-linear-gradient(10px, 20px, 30px, 40px, #00ccff 50%)",
  "-moz-linear-gradient(40px 40px, 10px 10px, blue 0 fuchsia 10% red 100%)",
  "-moz-linear-gradient(20px 20px 30px, 10px 10px, red 0, #ff0000 100%)",
  "-moz-radial-gradient(left top, center, 20px 20px, 10px, from(blue), to(red))",
  "-moz-linear-gradient(left left, top top, blue 0)",
  "-moz-linear-gradient(inherit, 10px 10px, blue 0)",
  "-moz-linear-gradient(left left blue red)",
  "-moz-linear-gradient(left left blue, red)",
  "-moz-linear-gradient()",
  "-moz-linear-gradient(cover, red, blue)",
  "-moz-linear-gradient(auto, red, blue)",
  "-moz-linear-gradient(22 top, red, blue)",
  "-moz-linear-gradient(10% red blue)",
  "-moz-linear-gradient(10%, red blue)",
  "-moz-linear-gradient(10%,, red, blue)",
  "-moz-linear-gradient(45px, center, red, blue)",
  "-moz-linear-gradient(45px, center red, blue)",
  "-moz-radial-gradient(contain, ellipse, red, blue)",
  "-moz-radial-gradient(10deg contain, red, blue)",
  "-moz-radial-gradient(10deg, contain,, red, blue)",
  "-moz-radial-gradient(contain contain, red, blue)",
  "-moz-radial-gradient(ellipse circle, red, blue)",
  "-moz-radial-gradient(to top left, red, blue)",
  "-moz-radial-gradient(center, 10%, red, blue)",
  "-moz-radial-gradient(5rad, 20px, red, blue)",
  "-moz-radial-gradient(40%, -100px -10%, red, blue)",

  "-moz-radial-gradient(at top left to cover, red, blue)",
  "-moz-radial-gradient(at 15% 20% circle, red, blue)",

  "-moz-radial-gradient(to cover, red, blue)",
  "-moz-radial-gradient(to contain, red, blue)",
  "-moz-radial-gradient(to closest-side circle, red, blue)",
  "-moz-radial-gradient(to farthest-corner ellipse, red, blue)",

  "-moz-radial-gradient(ellipse at 45px closest-corner, red, blue)",
  "-moz-radial-gradient(circle at 45px farthest-side, red, blue)",
  "-moz-radial-gradient(ellipse 45px, closest-side, red, blue)",
  "-moz-radial-gradient(circle 45px, farthest-corner, red, blue)",
  "-moz-radial-gradient(ellipse, ellipse closest-side, red, blue)",
  "-moz-radial-gradient(circle, circle farthest-corner, red, blue)",

  "-moz-radial-gradient(99deg to farthest-corner, red, blue)",
  "-moz-radial-gradient(-1.2345rad circle, red, blue)",
  "-moz-radial-gradient(ellipse 399grad to closest-corner, red, blue)",
  "-moz-radial-gradient(circle 399grad to farthest-side, red, blue)",

  "-moz-radial-gradient(at top left 99deg, to farthest-corner, red, blue)",
  "-moz-radial-gradient(circle at 15% 20% -1.2345rad, red, blue)",
  "-moz-radial-gradient(to top left at 30% 40%, red, blue)",
  "-moz-radial-gradient(ellipse at 45px 399grad, to closest-corner, red, blue)",
  "-moz-radial-gradient(at 45px 399grad to farthest-side circle, red, blue)",

  "-moz-radial-gradient(to 50%, red, blue)",
  "-moz-radial-gradient(circle to 50%, red, blue)",
  "-moz-radial-gradient(circle to 43px 43px, red, blue)",
  "-moz-radial-gradient(circle to 50% 50%, red, blue)",
  "-moz-radial-gradient(circle to 43px 50%, red, blue)",
  "-moz-radial-gradient(circle to 50% 43px, red, blue)",
  "-moz-radial-gradient(ellipse to 43px, red, blue)",
  "-moz-radial-gradient(ellipse to 50%, red, blue)",

  "-moz-linear-gradient(to 0 0, red, blue)",
  "-moz-linear-gradient(to 20% bottom, red, blue)",
  "-moz-linear-gradient(to center 20%, red, blue)",
  "-moz-linear-gradient(to left 35px, red, blue)",
  "-moz-linear-gradient(to 10% 10em, red, blue)",
  "-moz-linear-gradient(to 44px top, red, blue)",
  "-moz-linear-gradient(to top left 45deg, red, blue)",
  "-moz-linear-gradient(to 20% bottom -300deg, red, blue)",
  "-moz-linear-gradient(to center 20% 1.95929rad, red, blue)",
  "-moz-linear-gradient(to left 35px 30grad, red, blue)",
  "-moz-linear-gradient(to 10% 10em 99999deg, red, blue)",
  "-moz-linear-gradient(to 44px top -33deg, red, blue)",
  "-moz-linear-gradient(to -33deg, red, blue)",
  "-moz-linear-gradient(to 30grad left 35px, red, blue)",
  "-moz-linear-gradient(to 10deg 20px, red, blue)",
  "-moz-linear-gradient(to .414rad bottom, red, blue)",

  "-moz-linear-gradient(to top top, red, blue)",
  "-moz-linear-gradient(to bottom bottom, red, blue)",
  "-moz-linear-gradient(to left left, red, blue)",
  "-moz-linear-gradient(to right right, red, blue)",

  "-moz-repeating-linear-gradient(10px 10px, 20px, 30px 30px, 40px, blue 0, red 100%)",
  "-moz-repeating-radial-gradient(20px 20px, 10px 10px, from(green), to(#ff00ff))",
  "-moz-repeating-radial-gradient(10px 10px, 20%, 40px 40px, 10px, from(green), to(#ff00ff))",
  "-moz-repeating-linear-gradient(10px, 20px, 30px, 40px, #00ccff 50%)",
  "-moz-repeating-linear-gradient(40px 40px, 10px 10px, blue 0 fuchsia 10% red 100%)",
  "-moz-repeating-linear-gradient(20px 20px 30px, 10px 10px, red 0, #ff0000 100%)",
  "-moz-repeating-radial-gradient(left top, center, 20px 20px, 10px, from(blue), to(red))",
  "-moz-repeating-linear-gradient(left left, top top, blue 0)",
  "-moz-repeating-linear-gradient(inherit, 10px 10px, blue 0)",
  "-moz-repeating-linear-gradient(left left blue red)",
  "-moz-repeating-linear-gradient()",

  "-moz-repeating-linear-gradient(to 0 0, red, blue)",
  "-moz-repeating-linear-gradient(to 20% bottom, red, blue)",
  "-moz-repeating-linear-gradient(to center 20%, red, blue)",
  "-moz-repeating-linear-gradient(to left 35px, red, blue)",
  "-moz-repeating-linear-gradient(to 10% 10em, red, blue)",
  "-moz-repeating-linear-gradient(to 44px top, red, blue)",
  "-moz-repeating-linear-gradient(to top left 45deg, red, blue)",
  "-moz-repeating-linear-gradient(to 20% bottom -300deg, red, blue)",
  "-moz-repeating-linear-gradient(to center 20% 1.95929rad, red, blue)",
  "-moz-repeating-linear-gradient(to left 35px 30grad, red, blue)",
  "-moz-repeating-linear-gradient(to 10% 10em 99999deg, red, blue)",
  "-moz-repeating-linear-gradient(to 44px top -33deg, red, blue)",
  "-moz-repeating-linear-gradient(to -33deg, red, blue)",
  "-moz-repeating-linear-gradient(to 30grad left 35px, red, blue)",
  "-moz-repeating-linear-gradient(to 10deg 20px, red, blue)",
  "-moz-repeating-linear-gradient(to .414rad bottom, red, blue)",

  "-moz-repeating-linear-gradient(to top top, red, blue)",
  "-moz-repeating-linear-gradient(to bottom bottom, red, blue)",
  "-moz-repeating-linear-gradient(to left left, red, blue)",
  "-moz-repeating-linear-gradient(to right right, red, blue)",

  "-moz-repeating-radial-gradient(to top left at 30% 40%, red, blue)",
  "-moz-repeating-radial-gradient(ellipse at 45px closest-corner, red, blue)",
  "-moz-repeating-radial-gradient(circle at 45px farthest-side, red, blue)",

  "radial-gradient(circle 175px 20px, black, white)",
  "radial-gradient(175px 20px circle, black, white)",
  "radial-gradient(ellipse 175px, black, white)",
  "radial-gradient(175px ellipse, black, white)",
  "radial-gradient(50%, red, blue)",
  "radial-gradient(circle 50%, red, blue)",
  "radial-gradient(50% circle, red, blue)",

  /* Valid only when prefixed */
  "linear-gradient(top left, red, blue)",
  "linear-gradient(0 0, red, blue)",
  "linear-gradient(20% bottom, red, blue)",
  "linear-gradient(center 20%, red, blue)",
  "linear-gradient(left 35px, red, blue)",
  "linear-gradient(10% 10em, red, blue)",
  "linear-gradient(44px top, red, blue)",

  "linear-gradient(top left 45deg, red, blue)",
  "linear-gradient(20% bottom -300deg, red, blue)",
  "linear-gradient(center 20% 1.95929rad, red, blue)",
  "linear-gradient(left 35px 30grad, red, blue)",
  "linear-gradient(left 35px 0.1turn, red, blue)",
  "linear-gradient(10% 10em 99999deg, red, blue)",
  "linear-gradient(44px top -33deg, red, blue)",

  "linear-gradient(30grad left 35px, red, blue)",
  "linear-gradient(10deg 20px, red, blue)",
  "linear-gradient(1turn 20px, red, blue)",
  "linear-gradient(.414rad bottom, red, blue)",

  "linear-gradient(to top, 0%, blue)",
  "linear-gradient(to top, red, 100%)",
  "linear-gradient(to top, red, 45%, 56%, blue)",
  "linear-gradient(to top, red,, blue)",
  "linear-gradient(to top, red, green 35%, 15%, 54%, blue)",


  "radial-gradient(top left 45deg, red, blue)",
  "radial-gradient(20% bottom -300deg, red, blue)",
  "radial-gradient(center 20% 1.95929rad, red, blue)",
  "radial-gradient(left 35px 30grad, red, blue)",
  "radial-gradient(10% 10em 99999deg, red, blue)",
  "radial-gradient(44px top -33deg, red, blue)",

  "radial-gradient(-33deg, red, blue)",
  "radial-gradient(30grad left 35px, red, blue)",
  "radial-gradient(10deg 20px, red, blue)",
  "radial-gradient(.414rad bottom, red, blue)",

  "radial-gradient(cover, red, blue)",
  "radial-gradient(ellipse contain, red, blue)",
  "radial-gradient(cover circle, red, blue)",

  "radial-gradient(top left, cover, red, blue)",
  "radial-gradient(15% 20%, circle, red, blue)",
  "radial-gradient(45px, ellipse closest-corner, red, blue)",
  "radial-gradient(45px, farthest-side circle, red, blue)",

  "radial-gradient(99deg, cover, red, blue)",
  "radial-gradient(-1.2345rad, circle, red, blue)",
  "radial-gradient(399grad, ellipse closest-corner, red, blue)",
  "radial-gradient(399grad, farthest-side circle, red, blue)",

  "radial-gradient(top left 99deg, cover, red, blue)",
  "radial-gradient(15% 20% -1.2345rad, circle, red, blue)",
  "radial-gradient(45px 399grad, ellipse closest-corner, red, blue)",
  "radial-gradient(45px 399grad, farthest-side circle, red, blue)",

  /* Valid only when unprefixed */
  "-moz-radial-gradient(at top left, red, blue)",
  "-moz-radial-gradient(at 20% bottom, red, blue)",
  "-moz-radial-gradient(at center 20%, red, blue)",
  "-moz-radial-gradient(at left 35px, red, blue)",
  "-moz-radial-gradient(at 10% 10em, red, blue)",
  "-moz-radial-gradient(at 44px top, red, blue)",
  "-moz-radial-gradient(at 0 0, red, blue)",

  "-moz-radial-gradient(circle 43px, red, blue)",
  "-moz-radial-gradient(ellipse 43px 43px, red, blue)",
  "-moz-radial-gradient(ellipse 50% 50%, red, blue)",
  "-moz-radial-gradient(ellipse 43px 50%, red, blue)",
  "-moz-radial-gradient(ellipse 50% 43px, red, blue)",

  "-moz-radial-gradient(farthest-corner at top left, red, blue)",
  "-moz-radial-gradient(ellipse closest-corner at 45px, red, blue)",
  "-moz-radial-gradient(circle farthest-side at 45px, red, blue)",
  "-moz-radial-gradient(closest-side ellipse at 50%, red, blue)",
  "-moz-radial-gradient(farthest-corner circle at 4em, red, blue)",

  "-moz-radial-gradient(30% 40% at top left, red, blue)",
  "-moz-radial-gradient(50px 60px at 15% 20%, red, blue)",
  "-moz-radial-gradient(7em 8em at 45px, red, blue)"
];
var unbalancedGradientAndElementValues = [
  "-moz-element(#a()",
];

if (IsCSSPropertyPrefEnabled("layout.css.prefixes.webkit")) {
  // Extend gradient lists with valid/invalid webkit-prefixed expressions:
  validGradientAndElementValues.push(
    // 2008 GRADIENTS: -webkit-gradient()
    // ----------------------------------
    // linear w/ no color stops (valid) and a variety of position values:
    "-webkit-gradient(linear, 1 2, 3 4)",
    "-webkit-gradient(linear,1 2,3 4)", // (no extra space)
    "-webkit-gradient(linear  ,  1   2  ,  3   4  )", // (lots of extra space)
    "-webkit-gradient(linear, 1 10% , 0% 4)", // percentages
    "-webkit-gradient(linear, +1.0 -2%, +5.3% -0)", // (+/- & decimals are valid)
    "-webkit-gradient(linear, left top, right bottom)", // keywords
    "-webkit-gradient(linear, right center, center top)",
    "-webkit-gradient(linear, center center, center center)",
    "-webkit-gradient(linear, center 5%, 30 top)", // keywords mixed w/ nums

    // linear w/ just 1 color stop:
    "-webkit-gradient(linear, 1 2, 3 4, from(lime))",
    "-webkit-gradient(linear, 1 2, 3 4, to(lime))",
    // * testing the various allowable stop values (<number> & <percent>):
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(-0, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(-30, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(+9999, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(-.1, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0%, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(100%, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(9999%, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(-.5%, lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(+0%, lime))",
    // * testing the various color values:
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, transparent))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, rgb(1,2,3)))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, #00ff00))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, #00f))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, hsla(240, 30%, 50%, 0.8)))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, rgba(255, 230, 10, 0.5)))",

    // linear w/ multiple color stops:
    // * using from()/to() -- note that out-of-order is OK:
    "-webkit-gradient(linear, 1 2, 3 4, from(lime), from(blue))",
    "-webkit-gradient(linear, 1 2, 3 4, to(lime),   to(blue))",
    "-webkit-gradient(linear, 1 2, 3 4, from(lime), to(blue))",
    "-webkit-gradient(linear, 1 2, 3 4, to(lime),   from(blue))",
    "-webkit-gradient(linear, 1 2, 3 4, from(lime), to(blue), from(purple))",
    // * using color-stop():
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, lime), color-stop(30%, blue))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0, lime), color-stop(30%, blue), color-stop(100%, purple))",
    // * using color-stop() intermixed with from()/to() functions:
    "-webkit-gradient(linear, 1 2, 3 4, from(lime), color-stop(30%, blue))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(30%, blue), to(lime))",
    // * overshooting endpoints (0 & 1.0)
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(-30%, lime), color-stop(.4, blue), color-stop(1.5, purple))",
    // * repeating a stop position (valid)
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(30%, lime), color-stop(30%, blue))",
    // * stops out of order (valid)
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(70%, lime), color-stop(20%, blue), color-stop(40%, purple))",

    // radial w/ no color stops (valid) and a several different radius values:
    "-webkit-gradient(radial, 1 2, 8, 3 4, 9)",
    "-webkit-gradient(radial, 0 0, 10, 0 0, 5)",
    "-webkit-gradient(radial, 1 2, -1.5, center center, +99999.9999)",

    // radial w/ color stops
    // (mostly leaning on more-robust 'linear' tests above; just testing a few
    // examples w/ radial as a sanity-check):
    "-webkit-gradient(radial, 1 2, 8, 3 4, 9, from(lime))",
    "-webkit-gradient(radial, 1 2, 8, 3 4, 9, to(blue))",
    "-webkit-gradient(radial, 1 2, 8, 3 4, 9, color-stop(0.5, #00f), color-stop(0.8, rgba(100, 200, 0, 0.5)))",

    // 2011 GRADIENTS: -webkit-linear-gradient(), -webkit-radial -gradient()
    // ---------------------------------------------------------------------
    // Basic linear-gradient syntax (valid when prefixed or unprefixed):
    "-webkit-linear-gradient(red, green, blue)",

    // Angled linear-gradients (valid when prefixed or unprefixed):
    "-webkit-linear-gradient(135deg, red, blue)",
    "-webkit-linear-gradient(280deg, red 60%, blue)",

    // Linear-gradient with unitless-0 <angle> (normally invalid for <angle>
    // but accepted here for better webkit emulation):
    "-webkit-linear-gradient(0, red, blue)",
    "-webkit-linear-gradient(0 red, blue)",

    // Basic radial-gradient syntax (valid when prefixed or unprefixed):
    "-webkit-radial-gradient(circle, white, black)",
    "-webkit-radial-gradient(circle, white, black)",
    "-webkit-radial-gradient(ellipse closest-side, white, black)",
    "-webkit-radial-gradient(circle farthest-corner, white, black)",

    // Contain/cover keywords (valid only for -moz/-webkit prefixed):
    "-webkit-radial-gradient(cover, red, blue)",
    "-webkit-radial-gradient(cover circle, red, blue)",
    "-webkit-radial-gradient(contain, red, blue)",
    "-webkit-radial-gradient(contain ellipse, red, blue)",

    // Initial side/corner/point (valid only for -moz/-webkit prefixed):
    "-webkit-linear-gradient(left, red, blue)",
    "-webkit-linear-gradient(right top, red, blue)",
    "-webkit-linear-gradient(top right, red, blue)",
    "-webkit-radial-gradient(right, red, blue)",
    "-webkit-radial-gradient(left bottom, red, blue)",
    "-webkit-radial-gradient(bottom left, red, blue)",
    "-webkit-radial-gradient(center, red, blue)",
    "-webkit-radial-gradient(center right, red, blue)",
    "-webkit-radial-gradient(center center, red, blue)",
    "-webkit-radial-gradient(center top, red, blue)",
    "-webkit-radial-gradient(left 50%, red, blue)",
    "-webkit-radial-gradient(20px top, red, blue)",
    "-webkit-radial-gradient(20em 30%, red, blue)",

    // Point + keyword-sized shape (valid only for -moz/-webkit prefixed):
    "-webkit-radial-gradient(center, circle closest-corner, red, blue)",
    "-webkit-radial-gradient(10px 20px, cover circle, red, blue)",
    "-webkit-radial-gradient(5em 50%, ellipse contain, red, blue)",

    // Repeating examples:
    "-webkit-repeating-linear-gradient(red 10%, blue 30%)",
    "-webkit-repeating-linear-gradient(30deg, pink 20px, orange 70px)",
    "-webkit-repeating-linear-gradient(left, red, blue)",
    "-webkit-repeating-linear-gradient(left, red 10%, blue 30%)",
    "-webkit-repeating-radial-gradient(circle, red, blue 10%, red 20%)",
    "-webkit-repeating-radial-gradient(circle farthest-corner, gray 10px, yellow 20px)",
    "-webkit-repeating-radial-gradient(top left, circle, red, blue 4%, red 8%)"
  );

  invalidGradientAndElementValues.push(
    // 2008 GRADIENTS: -webkit-gradient()
    // https://www.webkit.org/blog/175/introducing-css-gradients/
    // ----------------------------------
    // Mostly-empty expressions (missing most required pieces):
    "-webkit-gradient()",
    "-webkit-gradient( )",
    "-webkit-gradient(,)",
    "-webkit-gradient(bogus)",
    "-webkit-gradient(linear)",
    "-webkit-gradient(linear,)",
    "-webkit-gradient(,linear)",
    "-webkit-gradient(radial)",
    "-webkit-gradient(radial,)",

    // linear w/ partial/missing <point> expression(s)
    "-webkit-gradient(linear, 1)", // Incomplete <point>
    "-webkit-gradient(linear, left)", // Incomplete <point>
    "-webkit-gradient(linear, center)", // Incomplete <point>
    "-webkit-gradient(linear, top)", // Incomplete <point>
    "-webkit-gradient(linear, 5%)", // Incomplete <point>
    "-webkit-gradient(linear, 1 2)", // Missing 2nd <point>
    "-webkit-gradient(linear, 1, 3)", // 2 incomplete <point>s
    "-webkit-gradient(linear, 1, 3 4)", // Incomplete 1st <point>
    "-webkit-gradient(linear, 1 2, 3)", // Incomplete 2nd <point>
    "-webkit-gradient(linear, 1 2, 3, 4)", // Comma inside <point>
    "-webkit-gradient(linear, 1, 2, 3 4)", // Comma inside <point>
    "-webkit-gradient(linear, 1, 2, 3, 4)", // Comma inside <point>

    // linear w/ invalid units in <point> expression
    "-webkit-gradient(linear, 1px 2, 3 4)",
    "-webkit-gradient(linear, 1 2, 3 4px)",
    "-webkit-gradient(linear, 1px 2px, 3px 4px)",
    "-webkit-gradient(linear, calc(1) 2, 3 4)",
    "-webkit-gradient(linear, 1 2em, 3 4)",

    // linear w/ <radius> (only valid for radial)
    "-webkit-gradient(linear, 1 2, 8, 3 4, 9)",

    // linear w/ out-of-order position keywords in <point> expression
    // (horizontal keyword is supposed to come first, for "x" coord)
    "-webkit-gradient(linear, 0 0, top right)",
    "-webkit-gradient(linear, bottom center, 0 0)",
    "-webkit-gradient(linear, top bottom, 0 0)",
    "-webkit-gradient(linear, bottom top, 0 0)",
    "-webkit-gradient(linear, bottom top, 0 0)",

    // linear w/ trailing comma (which implies missing color-stops):
    "-webkit-gradient(linear, 1 2, 3 4,)",

    // linear w/ invalid color values:
    "-webkit-gradient(linear, 1 2, 3 4, from(invalidcolorname))",
    "-webkit-gradient(linear, 1 2, 3 4, from(inherit))",
    "-webkit-gradient(linear, 1 2, 3 4, from(initial))",
    "-webkit-gradient(linear, 1 2, 3 4, from(currentColor))",
    "-webkit-gradient(linear, 1 2, 3 4, from(00ff00))",
    "-webkit-gradient(linear, 1 2, 3 4, from(##00ff00))",
    "-webkit-gradient(linear, 1 2, 3 4, from(#00fff))", // wrong num hex digits
    "-webkit-gradient(linear, 1 2, 3 4, from(xyz(0,0,0)))", // bogus color func
    // Mixing <number> and <percentage> is invalid.
    "-webkit-gradient(linear, 1 2, 3 4, from(rgb(100, 100%, 30)))",

    // linear w/ color stops that have comma issues
    "-webkit-gradient(linear, 1 2, 3 4 from(lime))",
    "-webkit-gradient(linear, 1 2, 3 4, from(lime,))",
    "-webkit-gradient(linear, 1 2, 3 4, from(lime),)",
    "-webkit-gradient(linear, 1 2, 3 4, from(lime) to(blue))",
    "-webkit-gradient(linear, 1 2, 3 4, from(lime),, to(blue))",
    "-webkit-gradient(linear, 1 2, 3 4, from(rbg(0, 0, 0,)))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0 lime))",
    "-webkit-gradient(linear, 1 2, 3 4, color-stop(0,, lime))",

    // radial w/ broken <point>/radius expression(s)
    "-webkit-gradient(radial, 1)", // Incomplete <point>
    "-webkit-gradient(radial, 1 2)", // Missing radius + 2nd <point>
    "-webkit-gradient(radial, 1 2, 8)", // Missing 2nd <point>
    "-webkit-gradient(radial, 1 2, 8, 3)", // Incomplete 2nd <point>
    "-webkit-gradient(radial, 1 2, 8, 3 4)", // Missing 2nd radius
    "-webkit-gradient(radial, 1 2, 3 4, 9)", // Missing 1st radius

    // radial w/ incorrect units on radius (invalid; expecting <number>)
    "-webkit-gradient(radial, 1 2, 8%,      3 4, 9)",
    "-webkit-gradient(radial, 1 2, 8px,     3 4, 9)",
    "-webkit-gradient(radial, 1 2, calc(8), 3 4, 9)",
    "-webkit-gradient(radial, 1 2, 8em,     3 4, 9)",
    "-webkit-gradient(radial, 1 2, top,     3 4, 9)",

    // radial w/ trailing comma (which implies missing color-stops):
    "-webkit-gradient(linear, 1 2, 8, 3 4, 9,)",

    // radial w/ invalid color value (mostly leaning on 'linear' test above):
    "-webkit-gradient(radial, 1 2, 8, 3 4, 9, from(invalidcolorname))",

    // 2011 GRADIENTS: -webkit-linear-gradient(), -webkit-radial -gradient()
    // ---------------------------------------------------------------------
    // Syntax that's invalid for all types of gradients:
    // * empty gradient expressions:
    "-webkit-linear-gradient()",
    "-webkit-radial-gradient()",
    "-webkit-repeating-linear-gradient()",
    "-webkit-repeating-radial-gradient()",

    // Linear syntax that's invalid for both -webkit & unprefixed, but valid
    // for -moz:
    // * initial <legacy-gradient-line> which includes a length:
    "-webkit-linear-gradient(10px, red, blue)",
    "-webkit-linear-gradient(10px top, red, blue)",
    // * initial <legacy-gradient-line> which includes a side *and* an angle:
    "-webkit-linear-gradient(bottom 30deg, red, blue)",
    "-webkit-linear-gradient(30deg bottom, red, blue)",
    "-webkit-linear-gradient(10px top 50deg, red, blue)",
    "-webkit-linear-gradient(50deg 10px top, red, blue)",
    // * initial <legacy-gradient-line> which includes explicit "center":
    "-webkit-linear-gradient(center, red, blue)",
    "-webkit-linear-gradient(left center, red, blue)",
    "-webkit-linear-gradient(top center, red, blue)",
    "-webkit-linear-gradient(center top, red, blue)",

    // Linear syntax that's invalid for -webkit, but valid for -moz & unprefixed:
    // * "to" syntax:
    "-webkit-linear-gradient(to top, red, blue)",

    // * <shape> followed by angle:
    "-webkit-radial-gradient(circle 10deg, red, blue)",

    // Radial syntax that's invalid for both -webkit & -moz, but valid for
    // unprefixed:
    // * "<shape> at <position>" syntax:
    "-webkit-radial-gradient(circle at left bottom, red, blue)",
    // * explicitly-sized shape:
    "-webkit-radial-gradient(circle 10px, red, blue)",
    "-webkit-radial-gradient(ellipse 40px 20px, red, blue)",

    // Radial syntax that's invalid for both -webkit & unprefixed, but valid
    // for -moz:
    // * initial angle
    "-webkit-radial-gradient(30deg, red, blue)",
    // * initial angle/position combo
    "-webkit-radial-gradient(top 30deg, red, blue)",
    "-webkit-radial-gradient(left top 30deg, red, blue)",
    "-webkit-radial-gradient(10px 20px 30deg, red, blue)"
  );
}

var gCSSProperties = {
  "animation": {
    domProp: "animation",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "animation-name", "animation-duration", "animation-timing-function", "animation-delay", "animation-direction", "animation-fill-mode", "animation-iteration-count", "animation-play-state" ],
    initial_values: [ "none none 0s 0s ease normal running 1.0", "none", "0s", "ease", "normal", "running", "1.0" ],
    other_values: [ "none none 0s 0s cubic-bezier(0.25, 0.1, 0.25, 1.0) normal running 1.0", "bounce 1s linear 2s", "bounce 1s 2s linear", "bounce linear 1s 2s", "linear bounce 1s 2s", "linear 1s bounce 2s", "linear 1s 2s bounce", "1s bounce linear 2s", "1s bounce 2s linear", "1s 2s bounce linear", "1s linear bounce 2s", "1s linear 2s bounce", "1s 2s linear bounce", "bounce linear 1s", "bounce 1s linear", "linear bounce 1s", "linear 1s bounce", "1s bounce linear", "1s linear bounce", "1s 2s bounce", "1s bounce 2s", "bounce 1s 2s", "1s 2s linear", "1s linear 2s", "linear 1s 2s", "bounce 1s", "1s bounce", "linear 1s", "1s linear", "1s 2s", "2s 1s", "bounce", "linear", "1s", "height", "2s", "ease-in-out", "2s ease-in", "opacity linear", "ease-out 2s", "2s color, 1s bounce, 500ms height linear, 1s opacity 4s cubic-bezier(0.0, 0.1, 1.0, 1.0)", "1s \\32bounce linear 2s", "1s -bounce linear 2s", "1s -\\32bounce linear 2s", "1s \\32 0bounce linear 2s", "1s -\\32 0bounce linear 2s", "1s \\2bounce linear 2s", "1s -\\2bounce linear 2s", "2s, 1s bounce", "1s bounce, 2s", "2s all, 1s bounce", "1s bounce, 2s all", "1s bounce, 2s none", "2s none, 1s bounce", "2s bounce, 1s all", "2s all, 1s bounce" ],
    invalid_values: [ "2s inherit", "inherit 2s", "2s bounce, 1s inherit", "2s inherit, 1s bounce", "2s initial", "2s all,, 1s bounce", "2s all, , 1s bounce", "bounce 1s cubic-bezier(0, rubbish) 2s", "bounce 1s steps(rubbish) 2s" ]
  },
  "animation-delay": {
    domProp: "animationDelay",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0s", "0ms" ],
    other_values: [ "1s", "250ms", "-100ms", "-1s", "1s, 250ms, 2.3s"],
    invalid_values: [ "0", "0px" ]
  },
  "animation-direction": {
    domProp: "animationDirection",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "alternate", "normal, alternate", "alternate, normal", "normal, normal", "normal, normal, normal", "reverse", "alternate-reverse", "normal, reverse, alternate-reverse, alternate" ],
    invalid_values: [ "normal normal", "inherit, normal", "reverse-alternate" ]
  },
  "animation-duration": {
    domProp: "animationDuration",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0s", "0ms" ],
    other_values: [ "1s", "250ms", "1s, 250ms, 2.3s"],
    invalid_values: [ "0", "0px", "-1ms", "-2s" ]
  },
  "animation-fill-mode": {
    domProp: "animationFillMode",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "forwards", "backwards", "both", "none, none", "forwards, backwards", "forwards, none", "none, both" ],
    invalid_values: [ "all"]
  },
  "animation-iteration-count": {
    domProp: "animationIterationCount",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1" ],
    other_values: [ "infinite", "0", "0.5", "7.75", "-0.0", "1, 2, 3", "infinite, 2", "1, infinite" ],
    // negatives forbidden per
    // http://lists.w3.org/Archives/Public/www-style/2011Mar/0355.html
    invalid_values: [ "none", "-1", "-0.5", "-1, infinite", "infinite, -3" ]
  },
  "animation-name": {
    domProp: "animationName",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "all", "ball", "mall", "color", "bounce, bubble, opacity", "foobar", "auto", "\\32bounce", "-bounce", "-\\32bounce", "\\32 0bounce", "-\\32 0bounce", "\\2bounce", "-\\2bounce" ],
    invalid_values: [ "bounce, initial", "initial, bounce", "bounce, inherit", "inherit, bounce" ]
  },
  "animation-play-state": {
    domProp: "animationPlayState",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "running" ],
    other_values: [ "paused", "running, running", "paused, running", "paused, paused", "running, paused", "paused, running, running, running, paused, running" ],
    invalid_values: [ "0" ]
  },
  "animation-timing-function": {
    domProp: "animationTimingFunction",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "ease" ],
    other_values: [ "cubic-bezier(0.25, 0.1, 0.25, 1.0)", "linear", "ease-in", "ease-out", "ease-in-out", "linear, ease-in, cubic-bezier(0.1, 0.2, 0.8, 0.9)", "cubic-bezier(0.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.25, 1.5, 0.75, -0.5)", "step-start", "step-end", "steps(1)", "steps(2, start)", "steps(386)", "steps(3, end)" ],
    invalid_values: [ "none", "auto", "cubic-bezier(0.25, 0.1, 0.25)", "cubic-bezier(0.25, 0.1, 0.25, 0.25, 1.0)", "cubic-bezier(-0.5, 0.5, 0.5, 0.5)", "cubic-bezier(1.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.5, 0.5, -0.5, 0.5)", "cubic-bezier(0.5, 0.5, 1.5, 0.5)", "steps(2, step-end)", "steps(0)", "steps(-2)", "steps(0, step-end, 1)" ]
  },
  "-moz-appearance": {
    domProp: "MozAppearance",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "radio", "menulist" ],
    invalid_values: []
  },
  "-moz-binding": {
    domProp: "MozBinding",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(foo.xml)" ],
    invalid_values: []
  },
  "-moz-border-bottom-colors": {
    domProp: "MozBorderBottomColors",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
    invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red", "ff00cc" ]
  },
  "border-inline-end": {
    domProp: "borderInlineEnd",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-inline-end-color", "border-inline-end-style", "border-inline-end-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 green none" ]
  },
  "border-inline-end-color": {
    domProp: "borderInlineEndColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000" ]
  },
  "border-inline-end-style": {
    domProp: "borderInlineEndStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-inline-end-width": {
    domProp: "borderInlineEndWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    prerequisites: { "border-inline-end-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%", "5" ]
  },
  "border-image": {
    domProp: "borderImage",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-image-source", "border-image-slice", "border-image-width", "border-image-outset", "border-image-repeat" ],
    initial_values: [ "none" ],
    other_values: [ "url('border.png') 27 27 27 27",
            "url('border.png') 27",
            "stretch url('border.png')",
            "url('border.png') 27 fill",
            "url('border.png') 27 27 27 27 repeat",
            "repeat url('border.png') 27 27 27 27",
            "url('border.png') repeat 27 27 27 27",
            "url('border.png') fill 27 27 27 27 repeat",
            "url('border.png') fill 27 27 27 27 repeat space",
            "url('border.png') 27 27 27 27 / 1em",
            "27 27 27 27 / 1em url('border.png') ",
            "url('border.png') 27 27 27 27 / 10 10 10 / 10 10 repeat",
            "repeat 27 27 27 27 / 10 10 10 / 10 10 url('border.png')",
            "url('border.png') 27 27 27 27 / / 10 10 1em",
            "fill 27 27 27 27 / / 10 10 1em url('border.png')",
            "url('border.png') 27 27 27 27 / 1em 1em 1em 1em repeat",
            "url('border.png') 27 27 27 27 / 1em 1em 1em 1em stretch round" ],
    invalid_values: [ "url('border.png') 27 27 27 27 27",
              "url('border.png') 27 27 27 27 / 1em 1em 1em 1em 1em",
              "url('border.png') 27 27 27 27 /",
              "url('border.png') fill",
              "url('border.png') fill repeat",
              "fill repeat",
              "url('border.png') fill / 1em",
              "url('border.png') / repeat",
              "url('border.png') 1 /",
              "url('border.png') 1 / /",
              "1 / url('border.png')",
              "url('border.png') / 1",
              "url('border.png') / / 1"]
  },
  "border-image-source": {
    domProp: "borderImageSource",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
    "url('border.png')"
    ].concat(validGradientAndElementValues),
    invalid_values: [
      "url('border.png') url('border.png')",
    ].concat(invalidGradientAndElementValues),
    unbalanced_values: [
    ].concat(unbalancedGradientAndElementValues)
  },
  "border-image-slice": {
    domProp: "borderImageSlice",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "100%", "100% 100% 100% 100%" ],
    other_values: [ "0%", "10", "10 100% 0 2", "0 0 0 0", "fill 10 10", "10 10 fill" ],
    invalid_values: [ "-10%", "-10", "10 10 10 10 10", "10 10 10 10 -10", "10px", "-10px", "fill", "fill fill 10px", "10px fill fill" ]
  },
  "border-image-width": {
    domProp: "borderImageWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1", "1 1 1 1" ],
    other_values: [ "0", "0%", "0px", "auto auto auto auto", "10 10% auto 15px", "10px 10px 10px 10px", "10", "10 10", "10 10 10" ],
    invalid_values: [ "-10", "-10px", "-10%", "10 10 10 10 10", "10 10 10 10 auto", "auto auto auto auto auto", "10px calc(nonsense)", "1px red" ],
    unbalanced_values: [ "10px calc(" ]
  },
  "border-image-outset": {
    domProp: "borderImageOutset",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0 0 0 0" ],
    other_values: [ "10px", "10", "10 10", "10 10 10", "10 10 10 10", "10px 10 10 10px" ],
    invalid_values: [ "-10", "-10px", "-10%", "10%", "10 10 10 10 10", "10px calc(nonsense)", "1px red" ],
    unbalanced_values: [ "10px calc(" ]
  },
  "border-image-repeat": {
    domProp: "borderImageRepeat",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "stretch", "stretch stretch" ],
    other_values: [ "round", "repeat", "stretch round", "repeat round", "stretch repeat", "round round", "repeat repeat",
                    "space", "stretch space", "repeat space", "round space", "space space" ],
    invalid_values: [ "none", "stretch stretch stretch", "0", "10", "0%", "0px" ]
  },
  "-moz-border-left-colors": {
    domProp: "MozBorderLeftColors",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
    invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red", "ff00cc" ]
  },
  "border-radius": {
    domProp: "borderRadius",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    subproperties: [ "border-bottom-left-radius", "border-bottom-right-radius", "border-top-left-radius", "border-top-right-radius" ],
    initial_values: [ "0", "0px", "0px 0 0 0px", "calc(-2px)", "calc(0px) calc(0pt)", "calc(0px) calc(0pt) calc(0px) calc(0em)" ],
    other_values: [ "0%", "3%", "1px", "2em", "3em 2px", "2pt 3% 4em", "2px 2px 2px 2px", // circular
            "3% / 2%", "1px / 4px", "2em / 1em", "3em 2px / 2px 3em", "2pt 3% 4em / 4pt 1% 5em", "2px 2px 2px 2px / 4px 4px 4px 4px", "1pt / 2pt 3pt", "4pt 5pt / 3pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
      "2px 2px calc(2px + 1%) 2px",
      "1px 2px 2px 2px / 2px 2px calc(2px + 1%) 2px",
            ],
    invalid_values: [ "2px -2px", "inherit 2px", "inherit / 2px", "2px inherit", "2px / inherit", "2px 2px 2px 2px 2px", "1px / 2px 2px 2px 2px 2px", "2", "2 2", "2px 2px 2px 2px / 2px 2px 2 2px", "2px calc(0px + rubbish)" ]
  },
  "border-bottom-left-radius": {
    domProp: "borderBottomLeftRadius",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px", "2px calc(0px + rubbish)" ]
  },
  "border-bottom-right-radius": {
    domProp: "borderBottomRightRadius",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px", "2px calc(0px + rubbish)" ]
  },
  "border-top-left-radius": {
    domProp: "borderTopLeftRadius",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px", "2px calc(0px + rubbish)" ]
  },
  "border-top-right-radius": {
    domProp: "borderTopRightRadius",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px", "2px calc(0px + rubbish)" ]
  },
  "-moz-border-right-colors": {
    domProp: "MozBorderRightColors",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
    invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red", "ff00cc" ]
  },
  "border-inline-start": {
    domProp: "borderInlineStart",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-inline-start-color", "border-inline-start-style", "border-inline-start-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 green solid" ]
  },
  "border-inline-start-color": {
    domProp: "borderInlineStartColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000" ]
  },
  "border-inline-start-style": {
    domProp: "borderInlineStartStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-inline-start-width": {
    domProp: "borderInlineStartWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    prerequisites: { "border-inline-start-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%", "5" ]
  },
  "-moz-border-top-colors": {
    domProp: "MozBorderTopColors",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
    invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red", "ff00cc" ]
  },
  "-moz-box-align": {
    domProp: "MozBoxAlign",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "stretch" ],
    other_values: [ "start", "center", "baseline", "end" ],
    invalid_values: []
  },
  "-moz-box-direction": {
    domProp: "MozBoxDirection",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "reverse" ],
    invalid_values: []
  },
  "-moz-box-flex": {
    domProp: "MozBoxFlex",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0.0", "-0.0" ],
    other_values: [ "1", "100", "0.1" ],
    invalid_values: [ "10px", "-1" ]
  },
  "-moz-box-ordinal-group": {
    domProp: "MozBoxOrdinalGroup",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1" ],
    other_values: [ "2", "100", "0" ],
    invalid_values: [ "1.0", "-1", "-1000" ]
  },
  "-moz-box-orient": {
    domProp: "MozBoxOrient",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "horizontal", "inline-axis" ],
    other_values: [ "vertical", "block-axis" ],
    invalid_values: []
  },
  "-moz-box-pack": {
    domProp: "MozBoxPack",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "start" ],
    other_values: [ "center", "end", "justify" ],
    invalid_values: []
  },
  "box-sizing": {
    domProp: "boxSizing",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "content-box" ],
    other_values: [ "border-box" ],
    invalid_values: [ "padding-box", "margin-box", "content", "padding", "border", "margin" ]
  },
  "-moz-box-sizing": {
    domProp: "MozBoxSizing",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "box-sizing",
    subproperties: [ "box-sizing" ],
  },
  "color-adjust": {
    domProp: "colorAdjust",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "economy" ],
    other_values: [ "exact" ],
    invalid_values: []
  },
  "columns": {
    domProp: "columns",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "column-count", "column-width" ],
    initial_values: [ "auto", "auto auto" ],
    other_values: [ "3", "20px", "2 10px", "10px 2", "2 auto", "auto 2", "auto 50px", "50px auto" ],
    invalid_values: [ "5%", "-1px", "-1", "3 5", "10px 4px", "10 2px 5in", "30px -1",
                      "auto 3 5px", "5 auto 20px", "auto auto auto", "calc(50px + rubbish) 2" ]
  },
  "-moz-columns": {
    domProp: "MozColumns",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "columns",
    subproperties: [ "column-count", "column-width" ]
  },
  "column-count": {
    domProp: "columnCount",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "1", "17" ],
    // negative and zero invalid per editor's draft
    invalid_values: [ "-1", "0", "3px" ]
  },
  "-moz-column-count": {
    domProp: "MozColumnCount",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-count",
    subproperties: [ "column-count" ]
  },
  "column-fill": {
    domProp: "columnFill",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "balance" ],
    other_values: [ "auto" ],
    invalid_values: [ "2px", "dotted", "5em" ]
  },
  "-moz-column-fill": {
    domProp: "MozColumnFill",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-fill",
    subproperties: [ "column-fill" ]
  },
  "column-gap": {
    domProp: "columnGap",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal", "1em", "calc(-2em + 3em)" ],
    other_values: [ "2px", "4em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0px)",
      "calc(0pt)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "3%", "-1px", "4" ]
  },
  "-moz-column-gap": {
    domProp: "MozColumnGap",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-gap",
    subproperties: [ "column-gap" ]
  },
  "column-rule": {
    domProp: "columnRule",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    prerequisites: { "color": "green" },
    subproperties: [ "column-rule-width", "column-rule-style", "column-rule-color" ],
    initial_values: [ "medium none currentColor", "none", "medium", "currentColor" ],
    other_values: [ "2px blue solid", "red dotted 1px", "ridge 4px orange", "5px solid" ],
    invalid_values: [ "2px 3px 4px red", "dotted dashed", "5px dashed green 3px", "5 solid", "5 green solid" ]
  },
  "-moz-column-rule": {
    domProp: "MozColumnRule",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "column-rule",
    subproperties: [ "column-rule-width", "column-rule-style", "column-rule-color" ]
  },
  "column-rule-width": {
    domProp: "columnRuleWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "-moz-column-rule-style": "solid" },
    initial_values: [
      "medium",
      "3px",
      "-moz-calc(3px)",
      "-moz-calc(5em + 3px - 5em)",
      "calc(3px)",
      "calc(5em + 3px - 5em)",
    ],
    other_values: [ "thin", "15px",
      /* valid -moz-calc() values */
      "-moz-calc(-2px)",
      "-moz-calc(2px)",
      "-moz-calc(3em)",
      "-moz-calc(3em + 2px)",
      "-moz-calc( 3em + 2px)",
      "-moz-calc(3em + 2px )",
      "-moz-calc( 3em + 2px )",
      "-moz-calc(3*25px)",
      "-moz-calc(3 *25px)",
      "-moz-calc(3 * 25px)",
      "-moz-calc(3* 25px)",
      "-moz-calc(25px*3)",
      "-moz-calc(25px *3)",
      "-moz-calc(25px* 3)",
      "-moz-calc(25px * 3)",
      "-moz-calc(25px * 3 / 4)",
      "-moz-calc((25px * 3) / 4)",
      "-moz-calc(25px * (3 / 4))",
      "-moz-calc(3 * 25px / 4)",
      "-moz-calc((3 * 25px) / 4)",
      "-moz-calc(3 * (25px / 4))",
      "-moz-calc(3em + 25px * 3 / 4)",
      "-moz-calc(3em + (25px * 3) / 4)",
      "-moz-calc(3em + 25px * (3 / 4))",
      "-moz-calc(25px * 3 / 4 + 3em)",
      "-moz-calc((25px * 3) / 4 + 3em)",
      "-moz-calc(25px * (3 / 4) + 3em)",
      "-moz-calc(3em + (25px * 3 / 4))",
      "-moz-calc(3em + ((25px * 3) / 4))",
      "-moz-calc(3em + (25px * (3 / 4)))",
      "-moz-calc((25px * 3 / 4) + 3em)",
      "-moz-calc(((25px * 3) / 4) + 3em)",
      "-moz-calc((25px * (3 / 4)) + 3em)",
      "-moz-calc(3*25px + 1in)",
      "-moz-calc(1in - 3em + 2px)",
      "-moz-calc(1in - (3em + 2px))",
      "-moz-calc((1in - 3em) + 2px)",
      "-moz-calc(50px/2)",
      "-moz-calc(50px/(2 - 1))",
      "-moz-calc(-3px)",
      /* numeric reduction cases */
      "-moz-calc(5 * 3 * 2em)",
      "-moz-calc(2em * 5 * 3)",
      "-moz-calc((5 * 3) * 2em)",
      "-moz-calc(2em * (5 * 3))",
      "-moz-calc((5 + 3) * 2em)",
      "-moz-calc(2em * (5 + 3))",
      "-moz-calc(2em / (5 + 3))",
      "-moz-calc(2em * (5*2 + 3))",
      "-moz-calc(2em * ((5*2) + 3))",
      "-moz-calc(2em * (5*(2 + 3)))",

      "-moz-calc((5 + 7) * 3em)",
      "-moz-calc((5em + 3em) - 2em)",
      "-moz-calc((5em - 3em) + 2em)",
      "-moz-calc(2em - (5em - 3em))",
      "-moz-calc(2em + (5em - 3em))",
      "-moz-calc(2em - (5em + 3em))",
      "-moz-calc(2em + (5em + 3em))",
      "-moz-calc(2em + 5em - 3em)",
      "-moz-calc(2em - 5em - 3em)",
      "-moz-calc(2em + 5em + 3em)",
      "-moz-calc(2em - 5em + 3em)",

      "-moz-calc(2em / 4 * 3)",
      "-moz-calc(2em * 4 / 3)",
      "-moz-calc(2em * 4 * 3)",
      "-moz-calc(2em / 4 / 3)",
      "-moz-calc(4 * 2em / 3)",
      "-moz-calc(4 / 3 * 2em)",

      "-moz-calc((2em / 4) * 3)",
      "-moz-calc((2em * 4) / 3)",
      "-moz-calc((2em * 4) * 3)",
      "-moz-calc((2em / 4) / 3)",
      "-moz-calc((4 * 2em) / 3)",
      "-moz-calc((4 / 3) * 2em)",

      "-moz-calc(2em / (4 * 3))",
      "-moz-calc(2em * (4 / 3))",
      "-moz-calc(2em * (4 * 3))",
      "-moz-calc(2em / (4 / 3))",
      "-moz-calc(4 * (2em / 3))",

      // Valid cases with unitless zero (which is never
      // a length).
      "-moz-calc(0 * 2em)",
      "-moz-calc(2em * 0)",
      "-moz-calc(3em + 0 * 2em)",
      "-moz-calc(3em + 2em * 0)",
      "-moz-calc((0 + 2) * 2em)",
      "-moz-calc((2 + 0) * 2em)",
      // And test zero lengths while we're here.
      "-moz-calc(2 * 0px)",
      "-moz-calc(0 * 0px)",
      "-moz-calc(2 * 0em)",
      "-moz-calc(0 * 0em)",
      "-moz-calc(0px * 0)",
      "-moz-calc(0px * 2)",

      /* valid calc() values */
      "calc(-2px)",
      "calc(2px)",
      "calc(3em)",
      "calc(3em + 2px)",
      "calc( 3em + 2px)",
      "calc(3em + 2px )",
      "calc( 3em + 2px )",
      "calc(3*25px)",
      "calc(3 *25px)",
      "calc(3 * 25px)",
      "calc(3* 25px)",
      "calc(25px*3)",
      "calc(25px *3)",
      "calc(25px* 3)",
      "calc(25px * 3)",
      "calc(25px * 3 / 4)",
      "calc((25px * 3) / 4)",
      "calc(25px * (3 / 4))",
      "calc(3 * 25px / 4)",
      "calc((3 * 25px) / 4)",
      "calc(3 * (25px / 4))",
      "calc(3em + 25px * 3 / 4)",
      "calc(3em + (25px * 3) / 4)",
      "calc(3em + 25px * (3 / 4))",
      "calc(25px * 3 / 4 + 3em)",
      "calc((25px * 3) / 4 + 3em)",
      "calc(25px * (3 / 4) + 3em)",
      "calc(3em + (25px * 3 / 4))",
      "calc(3em + ((25px * 3) / 4))",
      "calc(3em + (25px * (3 / 4)))",
      "calc((25px * 3 / 4) + 3em)",
      "calc(((25px * 3) / 4) + 3em)",
      "calc((25px * (3 / 4)) + 3em)",
      "calc(3*25px + 1in)",
      "calc(1in - 3em + 2px)",
      "calc(1in - (3em + 2px))",
      "calc((1in - 3em) + 2px)",
      "calc(50px/2)",
      "calc(50px/(2 - 1))",
      "calc(-3px)",
      /* numeric reduction cases */
      "calc(5 * 3 * 2em)",
      "calc(2em * 5 * 3)",
      "calc((5 * 3) * 2em)",
      "calc(2em * (5 * 3))",
      "calc((5 + 3) * 2em)",
      "calc(2em * (5 + 3))",
      "calc(2em / (5 + 3))",
      "calc(2em * (5*2 + 3))",
      "calc(2em * ((5*2) + 3))",
      "calc(2em * (5*(2 + 3)))",

      "calc((5 + 7) * 3em)",
      "calc((5em + 3em) - 2em)",
      "calc((5em - 3em) + 2em)",
      "calc(2em - (5em - 3em))",
      "calc(2em + (5em - 3em))",
      "calc(2em - (5em + 3em))",
      "calc(2em + (5em + 3em))",
      "calc(2em + 5em - 3em)",
      "calc(2em - 5em - 3em)",
      "calc(2em + 5em + 3em)",
      "calc(2em - 5em + 3em)",

      "calc(2em / 4 * 3)",
      "calc(2em * 4 / 3)",
      "calc(2em * 4 * 3)",
      "calc(2em / 4 / 3)",
      "calc(4 * 2em / 3)",
      "calc(4 / 3 * 2em)",

      "calc((2em / 4) * 3)",
      "calc((2em * 4) / 3)",
      "calc((2em * 4) * 3)",
      "calc((2em / 4) / 3)",
      "calc((4 * 2em) / 3)",
      "calc((4 / 3) * 2em)",

      "calc(2em / (4 * 3))",
      "calc(2em * (4 / 3))",
      "calc(2em * (4 * 3))",
      "calc(2em / (4 / 3))",
      "calc(4 * (2em / 3))",

      // Valid cases with unitless zero (which is never
      // a length).
      "calc(0 * 2em)",
      "calc(2em * 0)",
      "calc(3em + 0 * 2em)",
      "calc(3em + 2em * 0)",
      "calc((0 + 2) * 2em)",
      "calc((2 + 0) * 2em)",
      // And test zero lengths while we're here.
      "calc(2 * 0px)",
      "calc(0 * 0px)",
      "calc(2 * 0em)",
      "calc(0 * 0em)",
      "calc(0px * 0)",
      "calc(0px * 2)",

    ],
    invalid_values: [ "20", "-1px", "red", "50%",
      /* invalid -moz-calc() values */
      "-moz-calc(2em+ 2px)",
      "-moz-calc(2em +2px)",
      "-moz-calc(2em+2px)",
      "-moz-calc(2em- 2px)",
      "-moz-calc(2em -2px)",
      "-moz-calc(2em-2px)",
      /* invalid calc() values */
      "calc(2em+ 2px)",
      "calc(2em +2px)",
      "calc(2em+2px)",
      "calc(2em- 2px)",
      "calc(2em -2px)",
      "calc(2em-2px)",
      "-moz-min()",
      "calc(min())",
      "-moz-max()",
      "calc(max())",
      "-moz-min(5px)",
      "calc(min(5px))",
      "-moz-max(5px)",
      "calc(max(5px))",
      "-moz-min(5px,2em)",
      "calc(min(5px,2em))",
      "-moz-max(5px,2em)",
      "calc(max(5px,2em))",
      "calc(50px/(2 - 2))",
      "calc(5 + 5)",
      "calc(5 * 5)",
      "calc(5em * 5em)",
      "calc(5em / 5em * 5em)",

      "calc(4 * 3 / 2em)",
      "calc((4 * 3) / 2em)",
      "calc(4 * (3 / 2em))",
      "calc(4 / (3 * 2em))",

      // Tests for handling of unitless zero, which cannot
      // be a length inside calc().
      "calc(0)",
      "calc(0 + 2em)",
      "calc(2em + 0)",
      "calc(0 * 2)",
      "calc(2 * 0)",
      "calc(1 * (2em + 0))",
      "calc((2em + 0))",
      "calc((2em + 0) * 1)",
      "calc(1 * (0 + 2em))",
      "calc((0 + 2em))",
      "calc((0 + 2em) * 1)",
    ]
  },
  "-moz-column-rule-width": {
    domProp: "MozColumnRuleWidth",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-rule-width",
    subproperties: [ "column-rule-width" ]
  },
  "column-rule-style": {
    domProp: "columnRuleStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "solid", "hidden", "ridge", "groove", "inset", "outset", "double", "dotted", "dashed" ],
    invalid_values: [ "20", "foo" ]
  },
  "-moz-column-rule-style": {
    domProp: "MozColumnRuleStyle",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-rule-style",
    subproperties: [ "column-rule-style" ]
  },
  "column-rule-color": {
    domProp: "columnRuleColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "green" },
    initial_values: [ "currentColor" ],
    other_values: [ "red", "blue", "#ffff00" ],
    invalid_values: [ "ffff00" ]
  },
  "-moz-column-rule-color": {
    domProp: "MozColumnRuleColor",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-rule-color",
    subproperties: [ "column-rule-color" ]
  },
  "column-width": {
    domProp: "columnWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [
      "15px",
      "calc(15px)",
      "calc(30px - 3em)",
      "calc(-15px)",
      "0px",
      "calc(0px)"
    ],
    invalid_values: [ "20", "-1px", "50%" ]
  },
  "-moz-column-width": {
    domProp: "MozColumnWidth",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "column-width",
    subproperties: [ "column-width" ]
  },
  "-moz-float-edge": {
    domProp: "MozFloatEdge",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "content-box" ],
    other_values: [ "margin-box" ],
    invalid_values: [ "content", "padding", "border", "margin" ]
  },
  "-moz-force-broken-image-icon": {
    domProp: "MozForceBrokenImageIcon",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0" ],
    other_values: [ "1" ],
    invalid_values: []
  },
  "-moz-image-region": {
    domProp: "MozImageRegion",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "rect(3px 20px 15px 4px)", "rect(17px, 21px, 33px, 2px)" ],
    invalid_values: [ "rect(17px, 21px, 33, 2px)" ]
  },
  "margin-inline-end": {
    domProp: "marginInlineEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* no subproperties */
    /* auto may or may not be initial */
    initial_values: [ "0", "0px", "0%", "0em", "0ex", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "3em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "5", "..25px", ".+5px", ".px", "-.px", "++5px", "-+4px", "+-3px", "--7px", "+-.6px", "-+.5px", "++.7px", "--.4px" ],
  },
  "margin-inline-start": {
    domProp: "marginInlineStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* no subproperties */
    /* auto may or may not be initial */
    initial_values: [ "0", "0px", "0%", "0em", "0ex", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "3em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "5", "..25px", ".+5px", ".px", "-.px", "++5px", "-+4px", "+-3px", "--7px", "+-.6px", "-+.5px", "++.7px", "--.4px" ],
  },
  "mask-type": {
    domProp: "maskType",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "luminance" ],
    other_values: [ "alpha" ],
    invalid_values: [],
  },
  "-moz-outline-radius": {
    domProp: "MozOutlineRadius",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    subproperties: [ "-moz-outline-radius-bottomleft", "-moz-outline-radius-bottomright", "-moz-outline-radius-topleft", "-moz-outline-radius-topright" ],
    initial_values: [ "0", "0px", "calc(-2px)", "calc(0px) calc(0pt)", "calc(0px) calc(0em)" ],
    other_values: [ "0%", "3%", "1px", "2em", "3em 2px", "2pt 3% 4em", "2px 2px 2px 2px", // circular
            "3% / 2%", "1px / 4px", "2em / 1em", "3em 2px / 2px 3em", "2pt 3% 4em / 4pt 1% 5em", "2px 2px 2px 2px / 4px 4px 4px 4px", "1pt / 2pt 3pt", "4pt 5pt / 3pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
      "2px 2px calc(2px + 1%) 2px",
      "1px 2px 2px 2px / 2px 2px calc(2px + 1%) 2px",
            ],
    invalid_values: [ "2px -2px", "inherit 2px", "inherit / 2px", "2px inherit", "2px / inherit", "2px 2px 2px 2px 2px", "1px / 2px 2px 2px 2px 2px", "2", "2 2", "2px 2px 2px 2px / 2px 2px 2 2px" ]
  },
  "-moz-outline-radius-bottomleft": {
    domProp: "MozOutlineRadiusBottomleft",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)", "calc(0px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px" ]
  },
  "-moz-outline-radius-bottomright": {
    domProp: "MozOutlineRadiusBottomright",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)", "calc(0px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px" ]
  },
  "-moz-outline-radius-topleft": {
    domProp: "MozOutlineRadiusTopleft",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)", "calc(0px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px" ]
  },
  "-moz-outline-radius-topright": {
    domProp: "MozOutlineRadiusTopright",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
    initial_values: [ "0", "0px", "calc(-2px)", "calc(0px)" ],
    other_values: [ "0%", "3%", "1px", "2em", // circular
            "3% 2%", "1px 4px", "2em 2pt", // elliptical
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(3*25px) 5px",
      "5px calc(3*25px)",
      "calc(20%) calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
            ],
    invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit", "2", "2px 2", "2 2px" ]
  },
  "padding-inline-end": {
    domProp: "paddingInlineEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* no subproperties */
    initial_values: [ "0", "0px", "0%", "0em", "0ex", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "3em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "5" ]
  },
  "padding-inline-start": {
    domProp: "paddingInlineStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* no subproperties */
    initial_values: [ "0", "0px", "0%", "0em", "0ex", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "3em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "5" ]
  },
  "resize": {
    domProp: "resize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block", "overflow": "auto" },
    initial_values: [ "none" ],
    other_values: [ "both", "horizontal", "vertical" ],
    invalid_values: []
  },
  "-moz-stack-sizing": {
    domProp: "MozStackSizing",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "stretch-to-fit" ],
    other_values: [ "ignore" ],
    invalid_values: []
  },
  "-moz-tab-size": {
    domProp: "MozTabSize",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "8" ],
    other_values: [ "0", "2.5", "3", "99", "12000", "0px", "1em", "calc(1px + 1em)", "calc(1px - 2px)", "calc(1 + 1)", "calc(-2.5)" ],
    invalid_values: [ "9%", "calc(9% + 1px)", "calc(1 + 1em)", "-1", "-808", "auto" ]
  },
  "-moz-text-size-adjust": {
    domProp: "MozTextSizeAdjust",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "none" ],
    invalid_values: [ "-5%", "0", "100", "0%", "50%", "100%", "220.3%" ]
  },
  "transform": {
    domProp: "transform",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "width": "300px", "height": "50px" },
    initial_values: [ "none" ],
    other_values: [ "translatex(1px)", "translatex(4em)",
      "translatex(-4px)", "translatex(3px)",
      "translatex(0px) translatex(1px) translatex(2px) translatex(3px) translatex(4px)",
      "translatey(4em)", "translate(3px)", "translate(10px, -3px)",
      "rotate(45deg)", "rotate(45grad)", "rotate(45rad)",
      "rotate(0.25turn)", "rotate(0)", "scalex(10)", "scaley(10)",
      "scale(10)", "scale(10, 20)", "skewx(30deg)", "skewx(0)",
      "skewy(0)", "skewx(30grad)", "skewx(30rad)", "skewx(0.08turn)",
      "skewy(30deg)", "skewy(30grad)", "skewy(30rad)", "skewy(0.08turn)",
      "rotate(45deg) scale(2, 1)", "skewx(45deg) skewx(-50grad)",
      "translate(0, 0) scale(1, 1) skewx(0) skewy(0) matrix(1, 0, 0, 1, 0, 0)",
      "translatex(50%)", "translatey(50%)", "translate(50%)",
      "translate(3%, 5px)", "translate(5px, 3%)",
      "matrix(1, 2, 3, 4, 5, 6)",
      /* valid calc() values */
      "translatex(calc(5px + 10%))",
      "translatey(calc(0.25 * 5px + 10% / 3))",
      "translate(calc(5px - 10% * 3))",
      "translate(calc(5px - 3 * 10%), 50px)",
      "translate(-50px, calc(5px - 10% * 3))",
      "translatez(1px)", "translatez(4em)", "translatez(-4px)",
      "translatez(0px)", "translatez(2px) translatez(5px)",
      "translate3d(3px, 4px, 5px)", "translate3d(2em, 3px, 1em)",
      "translatex(2px) translate3d(4px, 5px, 6px) translatey(1px)",
      "scale3d(4, 4, 4)", "scale3d(-2, 3, -7)", "scalez(4)",
      "scalez(-6)", "rotate3d(2, 3, 4, 45deg)",
      "rotate3d(-3, 7, 0, 12rad)", "rotatex(15deg)", "rotatey(-12grad)",
      "rotatez(72rad)", "rotatex(0.125turn)",
      "perspective(0px)", "perspective(1000px)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)",
    ],
    invalid_values: ["1px", "#0000ff", "red", "auto",
      "translatex(1)", "translatey(1)", "translate(2)",
      "translate(-3, -4)",
      "translatex(1px 1px)", "translatex(translatex(1px))",
      "translatex(#0000ff)", "translatex(red)", "translatey()",
      "matrix(1px, 2px, 3px, 4px, 5px, 6px)", "scale(150%)",
      "skewx(red)", "matrix(1%, 0, 0, 0, 0px, 0px)",
      "matrix(0, 1%, 2, 3, 4px,5px)", "matrix(0, 1, 2%, 3, 4px, 5px)",
      "matrix(0, 1, 2, 3%, 4%, 5%)", "matrix(1, 2, 3, 4, 5px, 6%)",
      "matrix(1, 2, 3, 4, 5%, 6px)", "matrix(1, 2, 3, 4, 5%, 6%)",
      "matrix(1, 2, 3, 4, 5px, 6em)",
      /* invalid calc() values */
      "translatey(-moz-min(5px,10%))",
      "translatex(-moz-max(5px,10%))",
      "translate(10px, calc(min(5px,10%)))",
      "translate(calc(max(5px,10%)), 10%)",
      "matrix(1, 0, 0, 1, max(5px * 3), calc(10% - 3px))",
      "perspective(-10px)", "matrix3d(dinosaur)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15%, 16)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16px)",
      "rotatey(words)", "rotatex(7)", "translate3d(3px, 4px, 1px, 7px)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13px, 14em, 15px, 16)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 20%, 10%, 15, 16)"
    ],
  },
  "transform-origin": {
    domProp: "transformOrigin",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* no subproperties */
    prerequisites: { "width": "10px", "height": "10px", "display": "block"},
    initial_values: [ "50% 50%", "center", "center center" ],
    other_values: [ "25% 25%", "6px 5px", "20% 3em", "0 0", "0in 1in",
            "top", "bottom","top left", "top right",
            "top center", "center left", "center right",
            "bottom left", "bottom right", "bottom center",
            "20% center", "6px center", "13in bottom",
            "left 50px", "right 13%", "center 40px",
      "calc(20px)",
      "calc(20px) 10px",
      "10px calc(20px)",
      "calc(20px) 25%",
      "25% calc(20px)",
      "calc(20px) calc(20px)",
      "calc(20px + 1em) calc(20px / 2)",
      "calc(20px + 50%) calc(50% - 10px)",
      "calc(-20px) calc(-50%)",
      "calc(-20%) calc(-50%)",
      "6px 5px 5px",
      "top center 10px"
    ],
    invalid_values: ["red", "auto", "none", "0.5 0.5", "40px #0000ff",
             "border", "center red", "right diagonal",
             "#00ffff bottom", "0px calc(0px + rubbish)",
             "0px 0px calc(0px + rubbish)"]
  },
  "perspective-origin": {
    domProp: "perspectiveOrigin",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* no subproperties */
    prerequisites: { "width": "10px", "height": "10px", "display": "block"},
    initial_values: [ "50% 50%", "center", "center center" ],
    other_values: [ "25% 25%", "6px 5px", "20% 3em", "0 0", "0in 1in",
                    "top", "bottom","top left", "top right",
                    "top center", "center left", "center right",
                    "bottom left", "bottom right", "bottom center",
                    "20% center", "6px center", "13in bottom",
                    "left 50px", "right 13%", "center 40px",
                    "calc(20px)",
                    "calc(20px) 10px",
                    "10px calc(20px)",
                    "calc(20px) 25%",
                    "25% calc(20px)",
                    "calc(20px) calc(20px)",
                    "calc(20px + 1em) calc(20px / 2)",
                    "calc(20px + 50%) calc(50% - 10px)",
                    "calc(-20px) calc(-50%)",
                    "calc(-20%) calc(-50%)" ],
    invalid_values: [ "red", "auto", "none", "0.5 0.5", "40px #0000ff",
                      "border", "center red", "right diagonal",
                      "#00ffff bottom"]
  },
  "perspective": {
    domProp: "perspective",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "1000px", "500.2px", "0", "0px" ],
    invalid_values: [ "pants", "200", "-100px", "-27.2em" ]
  },
  "backface-visibility": {
    domProp: "backfaceVisibility",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "visible" ],
    other_values: [ "hidden" ],
    invalid_values: [ "collapse" ]
  },
  "transform-style": {
    domProp: "transformStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "flat" ],
    other_values: [ "preserve-3d" ],
    invalid_values: []
  },
  "-moz-user-focus": {
    domProp: "MozUserFocus",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "normal", "ignore", "select-all", "select-before", "select-after", "select-same", "select-menu" ],
    invalid_values: []
  },
  "-moz-user-input": {
    domProp: "MozUserInput",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "none", "enabled", "disabled" ],
    invalid_values: []
  },
  "-moz-user-modify": {
    domProp: "MozUserModify",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "read-only" ],
    other_values: [ "read-write", "write-only" ],
    invalid_values: []
  },
  "-moz-user-select": {
    domProp: "MozUserSelect",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "none", "text", "element", "elements", "all", "toggle", "tri-state", "-moz-all", "-moz-none" ],
    invalid_values: []
  },
  "background": {
    domProp: "background",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "background-attachment", "background-color", "background-image", "background-position-x", "background-position-y", "background-repeat", "background-clip", "background-origin", "background-size" ],
    initial_values: [ "transparent", "none", "repeat", "scroll", "0% 0%", "top left", "left top", "0% 0% / auto", "top left / auto", "left top / auto", "0% 0% / auto auto",
      "transparent none", "top left none", "left top none", "none left top", "none top left", "none 0% 0%", "left top / auto none", "left top / auto auto none",
      "transparent none repeat scroll top left", "left top repeat none scroll transparent", "transparent none repeat scroll top left / auto", "left top / auto repeat none scroll transparent", "none repeat scroll 0% 0% / auto auto transparent",
      "padding-box border-box" ],
    other_values: [
        /* without multiple backgrounds */
      "green",
      "none green repeat scroll left top",
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)",
      "repeat url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==') transparent left top scroll",
      "repeat-x",
      "repeat-y",
      "no-repeat",
      "none repeat-y transparent scroll 0% 0%",
      "fixed",
      "0% top transparent fixed repeat none",
      "top",
      "left",
      "50% 50%",
      "center",
      "top / 100px",
      "left / contain",
      "left / cover",
      "10px / 10%",
      "10em / calc(20px)",
      "top left / 100px 100px",
      "top left / 100px auto",
      "top left / 100px 10%",
      "top left / 100px calc(20px)",
      "bottom right scroll none transparent repeat",
      "50% transparent",
      "transparent 50%",
      "50%",
      "-moz-radial-gradient(10% bottom, #ffffff, black) scroll no-repeat",
      "-moz-linear-gradient(10px 10px -45deg, red, blue) repeat",
      "-moz-linear-gradient(10px 10px -0.125turn, red, blue) repeat",
      "-moz-repeating-radial-gradient(10% bottom, #ffffff, black) scroll no-repeat",
      "-moz-repeating-linear-gradient(10px 10px -45deg, red, blue) repeat",
      "-moz-element(#test) lime",
        /* multiple backgrounds */
        "url(404.png), url(404.png)",
        "url(404.png), url(404.png) transparent",
        "url(404.png), url(404.png) red",
        "repeat-x, fixed, none",
        "0% top url(404.png), url(404.png) 0% top",
        "fixed repeat-y top left url(404.png), repeat-x green",
        "url(404.png), -moz-linear-gradient(20px 20px -45deg, blue, green), -moz-element(#a) black",
        "top left / contain, bottom right / cover",
        /* test cases with clip+origin in the shorthand */
        "url(404.png) green padding-box",
        "url(404.png) border-box transparent",
        "content-box url(404.png) blue",
        "url(404.png) green padding-box padding-box",
        "url(404.png) green padding-box border-box",
        "content-box border-box url(404.png) blue",
    ],
    invalid_values: [
      /* mixes with keywords have to be in correct order */
      "50% left", "top 50%",
      /* no quirks mode colors */
      "-moz-radial-gradient(10% bottom, ffffff, black) scroll no-repeat",
      /* no quirks mode lengths */
      "-moz-linear-gradient(10 10px -45deg, red, blue) repeat",
      "-moz-linear-gradient(10px 10 -45deg, red, blue) repeat",
      "linear-gradient(red -99, yellow, green, blue 120%)",
      /* bug 258080: don't accept background-position separated */
      "left url(404.png) top", "top url(404.png) left",
      /* not allowed to have color in non-bottom layer */
      "url(404.png) transparent, url(404.png)",
      "url(404.png) red, url(404.png)",
      "url(404.png) transparent, url(404.png) transparent",
      "url(404.png) transparent red, url(404.png) transparent red",
      "url(404.png) red, url(404.png) red",
      "url(404.png) rgba(0, 0, 0, 0), url(404.png)",
      "url(404.png) rgb(255, 0, 0), url(404.png)",
      "url(404.png) rgba(0, 0, 0, 0), url(404.png) rgba(0, 0, 0, 0)",
      "url(404.png) rgba(0, 0, 0, 0) rgb(255, 0, 0), url(404.png) rgba(0, 0, 0, 0) rgb(255, 0, 0)",
      "url(404.png) rgb(255, 0, 0), url(404.png) rgb(255, 0, 0)",
      /* bug 513395: old syntax for gradients */
      "-moz-radial-gradient(10% bottom, 30px, 20px 20px, 10px, from(#ffffff), to(black)) scroll no-repeat",
      "-moz-linear-gradient(10px 10px, 20px 20px, from(red), to(blue)) repeat",
      /* clip and origin separated in the shorthand */
      "url(404.png) padding-box green border-box",
      "url(404.png) padding-box green padding-box",
      "transparent padding-box url(404.png) border-box",
      "transparent padding-box url(404.png) padding-box",
      /* error inside functions */
      "-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), rubbish, 50%, 30%, 0) transparent",
      "-moz-element(#a rubbish) black",
      "text",
      "text border-box",
      "content-box text text",
      "padding-box text url(404.png) text",
    ]
  },
  "background-attachment": {
    domProp: "backgroundAttachment",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "scroll" ],
    other_values: [ "fixed", "local", "scroll,scroll", "fixed, scroll", "scroll, fixed, local, scroll", "fixed, fixed" ],
    invalid_values: []
  },
  "background-clip": {
    /*
     * When we rename this to 'background-clip', we also
     * need to rename the values to match the spec.
     */
    domProp: "backgroundClip",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "border-box" ],
    other_values: [ "content-box", "padding-box", "border-box, padding-box", "padding-box, padding-box, padding-box", "border-box, border-box" ],
    invalid_values: [ "margin-box", "border-box border-box" ]
  },
  "background-color": {
    domProp: "backgroundColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "transparent", "rgba(255, 127, 15, 0)", "hsla(240, 97%, 50%, 0.0)", "rgba(0, 0, 0, 0)", "rgba(255,255,255,-3.7)" ],
    other_values: [ "green", "rgb(255, 0, 128)", "#fc2", "#96ed2a", "black", "rgba(255,255,0,3)", "hsl(240, 50%, 50%)", "rgb(50%, 50%, 50%)", "-moz-default-background-color", "rgb(100, 100.0, 100)" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "rgb(100, 100%, 100)" ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a" },
  },
  "background-image": {
    domProp: "backgroundImage",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
    "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
    "none, none",
    "none, none, none, none, none",
    "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), none",
    "none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), none",
    "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)",
    ].concat(validGradientAndElementValues),
    invalid_values: [
    ].concat(invalidGradientAndElementValues),
    unbalanced_values: [
    ].concat(unbalancedGradientAndElementValues)
  },
  "background-origin": {
    domProp: "backgroundOrigin",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "padding-box" ],
    other_values: [ "border-box", "content-box", "border-box, padding-box", "padding-box, padding-box, padding-box", "border-box, border-box" ],
    invalid_values: [ "margin-box", "padding-box padding-box" ]
  },
  "background-position": {
    domProp: "backgroundPosition",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    initial_values: [ "top 0% left 0%", "top 0% left", "top left", "left top", "0% 0%", "0% top", "left 0%" ],
    other_values: [ "top", "left", "right", "bottom", "center", "center bottom", "bottom center", "center right", "right center", "center top", "top center", "center left", "left center", "right bottom", "bottom right", "50%", "top left, top left", "top left, top right", "top right, top left", "left top, 0% 0%", "10% 20%, 30%, 40%", "top left, bottom right", "right bottom, left top", "0%", "0px", "30px", "0%, 10%, 20%, 30%", "top, top, top, top, top",
      "calc(20px)",
      "calc(20px) 10px",
      "10px calc(20px)",
      "calc(20px) 25%",
      "25% calc(20px)",
      "calc(20px) calc(20px)",
      "calc(20px + 1em) calc(20px / 2)",
      "calc(20px + 50%) calc(50% - 10px)",
      "calc(-20px) calc(-50%)",
      "calc(-20%) calc(-50%)",
      "0px 0px",
      "right 20px top 60px",
      "right 20px bottom 60px",
      "left 20px top 60px",
      "left 20px bottom 60px",
      "right -50px top -50px",
      "left -50px bottom -50px",
      "right 20px top -50px",
      "right -20px top 50px",
      "right 3em bottom 10px",
      "bottom 3em right 10px",
      "top 3em right 10px",
      "left 15px",
      "10px top",
      "left top 15px",
      "left 10px top",
      "left 20%",
      "right 20%"
    ],
    subproperties: [ "background-position-x", "background-position-y" ],
    invalid_values: [ "center 10px center 4px", "center 10px center",
                      "top 20%", "bottom 20%", "50% left", "top 50%",
                      "50% bottom 10%", "right 10% 50%", "left right",
                      "top bottom", "left 10% right",
                      "top 20px bottom 20px", "left left",
                      "0px calc(0px + rubbish)"],
    quirks_values: {
      "20 20": "20px 20px",
      "10 5px": "10px 5px",
      "7px 2": "7px 2px",
    },
  },
  "background-position-x": {
    domProp: "backgroundPositionX",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "left 0%", "left", "0%" ],
    other_values: [ "right", "center", "50%", "left, left", "left, right", "right, left", "left, 0%", "10%, 20%, 40%", "0px", "30px", "0%, 10%, 20%, 30%", "left, left, left, left, left",
      "calc(20px)",
      "calc(20px + 1em)",
      "calc(20px / 2)",
      "calc(20px + 50%)",
      "calc(50% - 10px)",
      "calc(-20px)",
      "calc(-50%)",
      "calc(-20%)",
      "right 20px",
      "left 20px",
      "right -50px",
      "left -50px",
      "right 20px",
      "right 3em",
    ],
    invalid_values: [ "center 10px", "right 10% 50%", "left right", "left left",
                      "bottom 20px", "top 10%", "bottom 3em",
                      "top", "bottom", "top, top", "top, bottom", "bottom, top", "top, 0%", "top, top, top, top, top",
                      "calc(0px + rubbish)"],
  },
  "background-position-y": {
    domProp: "backgroundPositionY",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "top 0%", "top", "0%" ],
    other_values: [ "bottom", "center", "50%", "top, top", "top, bottom", "bottom, top", "top, 0%", "10%, 20%, 40%", "0px", "30px", "0%, 10%, 20%, 30%", "top, top, top, top, top",
      "calc(20px)",
      "calc(20px + 1em)",
      "calc(20px / 2)",
      "calc(20px + 50%)",
      "calc(50% - 10px)",
      "calc(-20px)",
      "calc(-50%)",
      "calc(-20%)",
      "bottom 20px",
      "top 20px",
      "bottom -50px",
      "top -50px",
      "bottom 20px",
      "bottom 3em",
    ],
    invalid_values: [ "center 10px", "bottom 10% 50%", "top bottom", "top top",
                      "right 20px", "left 10%", "right 3em",
                      "left", "right", "left, left", "left, right", "right, left", "left, 0%", "left, left, left, left, left",
                      "calc(0px + rubbish)"],
  },
  "background-repeat": {
    domProp: "backgroundRepeat",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "repeat", "repeat repeat" ],
    other_values: [ "repeat-x", "repeat-y", "no-repeat",
      "repeat-x, repeat-x",
      "repeat, no-repeat",
      "repeat-y, no-repeat, repeat-y",
      "repeat, repeat, repeat",
      "repeat no-repeat",
      "no-repeat repeat",
      "no-repeat no-repeat",
      "repeat repeat, repeat repeat",
      "round, repeat",
      "round repeat, repeat-x",
      "round no-repeat, repeat-y",
      "round round",
      "space, repeat",
      "space repeat, repeat-x",
      "space no-repeat, repeat-y",
      "space space",
      "space round"
    ],
    invalid_values: [ "repeat repeat repeat",
                      "repeat-x repeat-y",
                      "repeat repeat-x",
                      "repeat repeat-y",
                      "repeat-x repeat",
                      "repeat-y repeat",
                      "round round round",
                      "repeat-x round",
                      "round repeat-x",
                      "repeat-y round",
                      "round repeat-y",
                      "space space space",
                      "repeat-x space",
                      "space repeat-x",
                      "repeat-y space",
                      "space repeat-y" ]
  },
  "background-size": {
    domProp: "backgroundSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto", "auto auto" ],
    other_values: [ "contain", "cover", "100px auto", "auto 100px", "100% auto", "auto 100%", "25% 50px", "3em 40%",
      "calc(20px)",
      "calc(20px) 10px",
      "10px calc(20px)",
      "calc(20px) 25%",
      "25% calc(20px)",
      "calc(20px) calc(20px)",
      "calc(20px + 1em) calc(20px / 2)",
      "calc(20px + 50%) calc(50% - 10px)",
      "calc(-20px) calc(-50%)",
      "calc(-20%) calc(-50%)"
    ],
    invalid_values: [ "contain contain", "cover cover", "cover auto", "auto cover", "contain cover", "cover contain", "-5px 3px", "3px -5px", "auto -5px", "-5px auto", "5 3", "10px calc(10px + rubbish)" ]
  },
  "border": {
    domProp: "border",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-bottom-color", "border-bottom-style", "border-bottom-width", "border-left-color", "border-left-style", "border-left-width", "border-right-color", "border-right-style", "border-right-width", "border-top-color", "border-top-style", "border-top-width", "-moz-border-top-colors", "-moz-border-right-colors", "-moz-border-bottom-colors", "-moz-border-left-colors", "border-image-source", "border-image-slice", "border-image-width", "border-image-outset", "border-image-repeat" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor", "calc(4px - 1px) none" ],
    other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid", "calc(2px) solid blue" ],
    invalid_values: [ "5%", "medium solid ff00ff", "5 solid green" ]
  },
  "border-bottom": {
    domProp: "borderBottom",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-bottom-color", "border-bottom-style", "border-bottom-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 solid green" ]
  },
  "border-bottom-color": {
    domProp: "borderBottomColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000" ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a" },
  },
  "border-bottom-style": {
    domProp: "borderBottomStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-bottom-width": {
    domProp: "borderBottomWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "border-bottom-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%" ],
    quirks_values: { "5": "5px" },
  },
  "border-collapse": {
    domProp: "borderCollapse",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "separate" ],
    other_values: [ "collapse" ],
    invalid_values: []
  },
  "border-color": {
    domProp: "borderColor",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-top-color", "border-right-color", "border-bottom-color", "border-left-color" ],
    initial_values: [ "currentColor", "currentColor currentColor", "currentColor currentColor currentColor", "currentColor currentColor currentcolor CURRENTcolor" ],
    other_values: [ "green", "currentColor green", "currentColor currentColor green", "currentColor currentColor currentColor green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "red rgb(nonsense)", "red 1px" ],
    unbalanced_values: [ "red rgb(" ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a" },
  },
  "border-left": {
    domProp: "borderLeft",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-left-color", "border-left-style", "border-left-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 solid green", "calc(5px + rubbish) green solid", "5px rgb(0, rubbish, 0) solid" ]
  },
  "border-left-color": {
    domProp: "borderLeftColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000" ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a" },
  },
  "border-left-style": {
    domProp: "borderLeftStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-left-width": {
    domProp: "borderLeftWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "border-left-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%" ],
    quirks_values: { "5": "5px" },
  },
  "border-right": {
    domProp: "borderRight",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-right-color", "border-right-style", "border-right-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 solid green" ]
  },
  "border-right-color": {
    domProp: "borderRightColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000" ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a" },
  },
  "border-right-style": {
    domProp: "borderRightStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-right-width": {
    domProp: "borderRightWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "border-right-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%" ],
    quirks_values: { "5": "5px" },
  },
  "border-spacing": {
    domProp: "borderSpacing",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0 0", "0px", "0 0px", "calc(0px)", "calc(0px) calc(0em)", "calc(2em - 2em) calc(3px + 7px - 10px)", "calc(-5px)", "calc(-5px) calc(-5px)" ],
    other_values: [ "3px", "4em 2px", "4em 0", "0px 2px", "calc(7px)", "0 calc(7px)", "calc(7px) 0", "calc(0px) calc(7px)", "calc(7px) calc(0px)", "7px calc(0px)", "calc(0px) 7px", "7px calc(0px)", "3px calc(2em)" ],
    invalid_values: [ "0%", "0 0%", "-5px", "-5px -5px", "0 -5px", "-5px 0", "0 calc(0px + rubbish)" ],
    quirks_values: {
      "2px 5": "2px 5px",
      "7": "7px",
      "3 4px": "3px 4px",
    },
  },
  "border-style": {
    domProp: "borderStyle",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-top-style", "border-right-style", "border-bottom-style", "border-left-style" ],
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none", "none none", "none none none", "none none none none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge", "none solid", "none none solid", "none none none solid", "groove none none none", "none ridge none none", "none none double none", "none none none dotted" ],
    invalid_values: []
  },
  "border-top": {
    domProp: "borderTop",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-top-color", "border-top-style", "border-top-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 solid green" ]
  },
  "border-top-color": {
    domProp: "borderTopColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000" ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a" },
  },
  "border-top-style": {
    domProp: "borderTopStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-top-width": {
    domProp: "borderTopWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "border-top-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%" ],
    quirks_values: { "5": "5px" },
  },
  "border-width": {
    domProp: "borderWidth",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-top-width", "border-right-width", "border-bottom-width", "border-left-width" ],
    prerequisites: { "border-style": "solid" },
    initial_values: [ "medium", "3px", "medium medium", "3px medium medium", "medium 3px medium medium", "calc(3px) 3px calc(5px - 2px) calc(2px - -1px)" ],
    other_values: [ "thin", "thick", "1px", "2em", "2px 0 0px 1em", "calc(2em)" ],
    invalid_values: [ "5%", "1px calc(nonsense)", "1px red" ],
    unbalanced_values: [ "1px calc(" ],
    quirks_values: { "5": "5px" },
  },
  "bottom": {
    domProp: "bottom",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [],
    quirks_values: { "5": "5px" },
  },
  "box-shadow": {
    domProp: "boxShadow",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    prerequisites: { "color": "blue" },
    other_values: [ "2px 2px", "2px 2px 1px", "2px 2px 2px 2px", "blue 3px 2px", "2px 2px 1px 5px green", "2px 2px red", "green 2px 2px 1px", "green 2px 2px, blue 1px 3px 4px", "currentColor 3px 3px", "blue 2px 2px, currentColor 1px 2px, 1px 2px 3px 2px orange", "3px 0 0 0", "inset 2px 2px 3px 4px black", "2px -2px green inset, 4px 4px 3px blue, inset 2px 2px",
      /* calc() values */
      "2px 2px calc(-5px)", /* clamped */
      "calc(3em - 2px) 2px green",
      "green calc(3em - 2px) 2px",
      "2px calc(2px + 0.2em)",
      "blue 2px calc(2px + 0.2em)",
      "2px calc(2px + 0.2em) blue",
      "calc(-2px) calc(-2px)",
      "-2px -2px",
      "calc(2px) calc(2px)",
      "calc(2px) calc(2px) calc(2px)",
      "calc(2px) calc(2px) calc(2px) calc(2px)"
    ],
    invalid_values: [ "3% 3%", "1px 1px 1px 1px 1px", "2px 2px, none", "red 2px 2px blue", "inherit, 2px 2px", "2px 2px, inherit", "2px 2px -5px", "inset 4px 4px black inset", "inset inherit", "inset none", "3 3", "3px 3", "3 3px", "3px 3px 3", "3px 3px 3px 3", "3px calc(3px + rubbish)", "3px 3px calc(3px + rubbish)", "3px 3px 3px calc(3px + rubbish)", "3px 3px 3px 3px rgb(0, rubbish, 0)" ]
  },
  "caption-side": {
    domProp: "captionSide",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "top" ],
    other_values: [ "bottom", "left", "right", "top-outside", "bottom-outside" ],
    invalid_values: []
  },
  "clear": {
    domProp: "clear",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "left", "right", "both" ],
    invalid_values: []
  },
  "clip": {
    domProp: "clip",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "rect(0 0 0 0)", "rect(auto,auto,auto,auto)", "rect(3px, 4px, 4em, 0)", "rect(auto, 3em, 4pt, 2px)", "rect(2px 3px 4px 5px)" ],
    invalid_values: [ "rect(auto, 3em, 2%, 5px)" ],
    quirks_values: { "rect(1, 2, 3, 4)": "rect(1px, 2px, 3px, 4px)" },
  },
  "color": {
    domProp: "color",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    /* XXX should test currentColor, but may or may not be initial */
    initial_values: [ "black", "#000", "#000f", "#000000ff", "-moz-default-color", "rgb(0, 0, 0)", "rgb(0%, 0%, 0%)",
      /* css-color-4: */
      /* rgb() and rgba() are aliases of each other. */
      "rgb(0, 0, 0)", "rgba(0, 0, 0)", "rgb(0, 0, 0, 1)", "rgba(0, 0, 0, 1)",
      /* hsl() and hsla() are aliases of each other. */
      "hsl(0, 0%, 0%)", "hsla(0, 0%, 0%)", "hsl(0, 0%, 0%, 1)", "hsla(0, 0%, 0%, 1)",
      /* rgb() and rgba() functions now accept <number> rather than <integer>. */
      "rgb(0.0, 0.0, 0.0)", "rgba(0.0, 0.0, 0.0)", "rgb(0.0, 0.0, 0.0, 1)", "rgba(0.0, 0.0, 0.0, 1)",
      /* <alpha-value> now accepts <percentage> as well as <number> in rgba() and hsla(). */
      "rgb(0.0, 0.0, 0.0, 100%)", "hsl(0, 0%, 0%, 100%)",
      /* rgb() and hsl() now support comma-less expression. */
      "rgb(0 0 0)", "rgb(0 0 0 / 1)", "rgb(0/* comment */0/* comment */0)", "rgb(0/* comment */0/* comment*/0/1.0)",
      "hsl(0 0% 0%)", "hsl(0 0% 0% / 1)", "hsl(0/* comment */0%/* comment */0%)", "hsl(0/* comment */0%/* comment */0%/1)",
      /* Support <angle> for hsl() hue component. */
      "hsl(0deg, 0%, 0%)", "hsl(360deg, 0%, 0%)", "hsl(0grad, 0%, 0%)", "hsl(400grad, 0%, 0%)", "hsl(0rad, 0%, 0%)", "hsl(0turn, 0%, 0%)", "hsl(1turn, 0%, 0%)",
    ],
    other_values: [ "green", "#f3c", "#fed292", "rgba(45,300,12,2)", "transparent", "-moz-nativehyperlinktext", "rgba(255,128,0,0.5)", "#e0fc", "#10fcee72",
      /* css-color-4: */
      "rgb(100, 100.0, 100)", "rgb(300 300 300 / 200%)", "rgb(300.0 300.0 300.0 / 2.0)", "hsl(720, 200%, 200%, 2.0)", "hsla(720 200% 200% / 200%)",
      "hsl(480deg, 20%, 30%, 0.3)", "hsl(55grad, 400%, 30%)", "hsl(0.5grad 400% 500% / 9.0)", "hsl(33rad 100% 90% / 4)", "hsl(0.33turn, 40%, 40%, 10%)",
    ],
    invalid_values: [ "#f", "#ff", "#fffff", "#fffffff", "#fffffffff",
      "rgb(100%, 0, 100%)", "rgba(100, 0, 100%, 30%)",
      "hsl(0, 0, 0%)", "hsla(0%, 0%, 0%, 0.1)",
      /* trailing commas */
      "rgb(0, 0, 0,)", "rgba(0, 0, 0, 0,)",
      "hsl(0, 0%, 0%,)", "hsla(0, 0%, 0%, 1,)",
      /* css-color-4: */
      /* comma and comma-less expressions should not mix together. */
      "rgb(0, 0, 0 / 1)", "rgb(0 0 0, 1)", "rgb(0, 0 0, 1)", "rgb(0 0, 0 / 1)",
      "hsl(0, 0%, 0% / 1)", "hsl(0 0% 0%, 1)", "hsl(0 0% 0%, 1)", "hsl(0 0%, 0% / 1)",
      /* trailing slash */
      "rgb(0 0 0 /)", "rgb(0, 0, 0 /)",
      "hsl(0 0% 0% /)", "hsl(0, 0%, 0% /)",
    ],
    quirks_values: { "000000": "#000000", "96ed2a": "#96ed2a", "fff": "#ffffff", "ffffff": "#ffffff", },
  },
  "content": {
    domProp: "content",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX needs to be on pseudo-elements */
    initial_values: [ "normal", "none" ],
    other_values: [ '""', "''", '"hello"', "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")', 'counter(foo)', 'counter(bar, upper-roman)', 'counters(foo, ".")', "counters(bar, '-', lower-greek)", "'-' counter(foo) '.'", "attr(title)", "open-quote", "close-quote", "no-open-quote", "no-close-quote", "close-quote attr(title) counters(foo, '.', upper-alpha)", "counter(foo, none)", "counters(bar, '.', none)", "attr(\\32)", "attr(\\2)", "attr(-\\2)", "attr(-\\32)", "counter(\\2)", "counters(\\32, '.')", "counter(-\\32, upper-roman)", "counters(-\\2, '-', lower-greek)", "counter(\\()", "counters(a\\+b, '.')", "counter(\\}, upper-alpha)", "-moz-alt-content", "counter(foo, symbols('*'))", "counter(foo, symbols(numeric '0' '1'))", "counters(foo, '.', symbols('*'))", "counters(foo, '.', symbols(numeric '0' '1'))" ],
    invalid_values: [ 'counters(foo)', 'counter(foo, ".")', 'attr("title")', "attr('title')", "attr(2)", "attr(-2)", "counter(2)", "counters(-2, '.')", "-moz-alt-content 'foo'", "'foo' -moz-alt-content", "counter(one, two, three) 'foo'" ]
  },
  "counter-increment": {
    domProp: "counterIncrement",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "foo 1", "bar", "foo 3 bar baz 2", "\\32  1", "-\\32  1", "-c 1", "\\32 1", "-\\32 1", "\\2  1", "-\\2  1", "-c 1", "\\2 1", "-\\2 1", "-\\7f \\9e 1" ],
    invalid_values: [ "none foo", "none foo 3", "foo none", "foo 3 none" ],
    unbalanced_values: [ "foo 1 (" ]
  },
  "counter-reset": {
    domProp: "counterReset",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "foo 1", "bar", "foo 3 bar baz 2", "\\32  1", "-\\32  1", "-c 1", "\\32 1", "-\\32 1", "\\2  1", "-\\2  1", "-c 1", "\\2 1", "-\\2 1", "-\\7f \\9e 1" ],
    invalid_values: [ "none foo", "none foo 3", "foo none", "foo 3 none" ]
  },
  "cursor": {
    domProp: "cursor",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "crosshair", "default", "pointer", "move", "e-resize", "ne-resize", "nw-resize", "n-resize", "se-resize", "sw-resize", "s-resize", "w-resize", "text", "wait", "help", "progress", "copy", "alias", "context-menu", "cell", "not-allowed", "col-resize", "row-resize", "no-drop", "vertical-text", "all-scroll", "nesw-resize", "nwse-resize", "ns-resize", "ew-resize", "none", "grab", "grabbing", "zoom-in", "zoom-out", "-moz-grab", "-moz-grabbing", "-moz-zoom-in", "-moz-zoom-out", "url(foo.png), move", "url(foo.png) 5 7, move", "url(foo.png) 12 3, url(bar.png), no-drop", "url(foo.png), url(bar.png) 7 2, wait", "url(foo.png) 3 2, url(bar.png) 7 9, pointer" ],
    invalid_values: [ "url(foo.png)", "url(foo.png) 5 5" ]
  },
  "direction": {
    domProp: "direction",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "ltr" ],
    other_values: [ "rtl" ],
    invalid_values: []
  },
  "display": {
    domProp: "display",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "inline" ],
    /* XXX none will really mess with other properties */
    prerequisites: { "float": "none", "position": "static", "contain": "none" },
    other_values: [
      "block",
      "flex",
      "inline-flex",
      "list-item",
      "inline-block",
      "table",
      "inline-table",
      "table-row-group",
      "table-header-group",
      "table-footer-group",
      "table-row",
      "table-column-group",
      "table-column",
      "table-cell",
      "table-caption",
      "ruby",
      "ruby-base",
      "ruby-base-container",
      "ruby-text",
      "ruby-text-container",
      "none"
    ],
    invalid_values: []
  },
  "empty-cells": {
    domProp: "emptyCells",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "show" ],
    other_values: [ "hide" ],
    invalid_values: []
  },
  "float": {
    domProp: "cssFloat",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "left", "right" ],
    invalid_values: []
  },
  "font": {
    domProp: "font",
    inherited: true,
    type: CSS_TYPE_TRUE_SHORTHAND,
    prerequisites: { "writing-mode": "initial" },
    subproperties: [ "font-style", "font-variant", "font-weight", "font-size", "line-height", "font-family", "font-stretch",
                     "font-size-adjust", "font-feature-settings", "font-language-override",
                     "font-kerning", "font-synthesis", "font-variant-alternates", "font-variant-caps", "font-variant-east-asian",
                     "font-variant-ligatures", "font-variant-numeric", "font-variant-position" ],
    initial_values: [ (gInitialFontFamilyIsSansSerif ? "medium sans-serif" : "medium serif") ],
    other_values: [ "large serif", "9px fantasy", "condensed bold italic small-caps 24px/1.4 Times New Roman, serif", "small inherit roman", "small roman inherit",
      // system fonts
      "caption", "icon", "menu", "message-box", "small-caption", "status-bar",
      // Gecko-specific system fonts
      "-moz-window", "-moz-document", "-moz-desktop", "-moz-info", "-moz-dialog", "-moz-button", "-moz-pull-down-menu", "-moz-list", "-moz-field", "-moz-workspace",
      // line-height with calc()
      "condensed bold italic small-caps 24px/calc(2px) Times New Roman, serif",
      "condensed bold italic small-caps 24px/calc(50%) Times New Roman, serif",
      "condensed bold italic small-caps 24px/calc(3*25px) Times New Roman, serif",
      "condensed bold italic small-caps 24px/calc(25px*3) Times New Roman, serif",
      "condensed bold italic small-caps 24px/calc(3*25px + 50%) Times New Roman, serif",
      "condensed bold italic small-caps 24px/calc(1 + 2*3/4) Times New Roman, serif",
    ],
    invalid_values: [ "9 fantasy", "-2px fantasy",
      // line-height with calc()
      "condensed bold italic small-caps 24px/calc(1 + 2px) Times New Roman, serif",
      "condensed bold italic small-caps 24px/calc(100% + 0.1) Times New Roman, serif",
    ]
  },
  "font-family": {
    domProp: "fontFamily",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ (gInitialFontFamilyIsSansSerif ? "sans-serif" : "serif") ],
    other_values: [ (gInitialFontFamilyIsSansSerif ? "serif" : "sans-serif"), "Times New Roman, serif", "'Times New Roman', serif", "cursive", "fantasy", "\\\"Times New Roman", "\"Times New Roman\"", "Times, \\\"Times New Roman", "Times, \"Times New Roman\"", "-no-such-font-installed", "inherit roman", "roman inherit", "Times, inherit roman", "inherit roman, Times", "roman inherit, Times", "Times, roman inherit" ],
    invalid_values: [ "\"Times New\" Roman", "\"Times New Roman\n", "Times, \"Times New Roman\n" ]
  },
  "font-feature-settings": {
    domProp: "fontFeatureSettings",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [
      "'liga' on", "'liga'", "\"liga\" 1", "'liga', 'clig' 1",
      "\"liga\" off", "\"liga\" 0", '"cv01" 3, "cv02" 4',
      '"cswh", "smcp" off, "salt" 4', '"cswh" 1, "smcp" off, "salt" 4',
      '"cswh" 0, \'blah\', "liga", "smcp" off, "salt" 4',
      '"liga"        ,"smcp" 0         , "blah"'
    ],
    invalid_values: [
      'liga', 'liga 1', 'liga normal', '"liga" normal', 'normal liga',
      'normal "liga"', 'normal, "liga"', '"liga=1"', "'foobar' on",
      '"blahblah" 0', '"liga" 3.14', '"liga" 1 3.14', '"liga" 1 normal',
      '"liga" 1 off', '"liga" on off', '"liga" , 0 "smcp"', '"liga" "smcp"'
    ]
  },
  "font-kerning": {
    domProp: "fontKerning",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "normal", "none" ],
    invalid_values: [ "on" ]
  },
  "font-language-override": {
    domProp: "fontLanguageOverride",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "'ENG'", "'TRK'", "\"TRK\"", "'N\\'Ko'" ],
    invalid_values: [ "TRK", "ja" ]
  },
  "font-size": {
    domProp: "fontSize",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "medium",
      "1rem",
      "calc(1rem)",
      "calc(0.75rem + 200% - 125% + 0.25rem - 75%)"
    ],
    other_values: [ "large", "2em", "50%", "xx-small", "36pt", "8px", "larger", "smaller",
      "0px",
      "0%",
      "calc(2em)",
      "calc(36pt + 75% + (30% + 2em + 2px))",
      "calc(-2em)",
      "calc(-50%)",
      "calc(-1px)"
    ],
    invalid_values: [ "-2em", "-50%", "-1px" ],
    quirks_values: { "5": "5px" },
  },
  "font-size-adjust": {
    domProp: "fontSizeAdjust",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "0.3", "0.5", "0.7", "0.0", "0", "3" ],
    invalid_values: [ "-0.3", "-1" ]
  },
  "font-stretch": {
    domProp: "fontStretch",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded" ],
    invalid_values: [ "narrower", "wider" ]
  },
  "font-style": {
    domProp: "fontStyle",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "italic", "oblique" ],
    invalid_values: []
  },
  "font-synthesis": {
    domProp: "fontSynthesis",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "weight style" ],
    other_values: [ "none", "weight", "style" ],
    invalid_values: [ "weight none", "style none", "none style", "weight 10px", "weight weight", "style style" ]
  },
  "font-variant": {
    domProp: "fontVariant",
    inherited: true,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "font-variant-alternates", "font-variant-caps", "font-variant-east-asian", "font-variant-ligatures", "font-variant-numeric", "font-variant-position" ],
    initial_values: [ "normal" ],
    other_values: [ "small-caps", "none", "traditional oldstyle-nums", "all-small-caps", "common-ligatures no-discretionary-ligatures",
                    "proportional-nums oldstyle-nums", "proportional-nums slashed-zero diagonal-fractions oldstyle-nums ordinal",
                    "traditional historical-forms styleset(ok-alt-a, ok-alt-b)", "styleset(potato)" ],
    invalid_values: [ "small-caps normal", "small-caps small-caps", "none common-ligatures", "common-ligatures none", "small-caps potato",
                      "small-caps jis83 all-small-caps", "super historical-ligatures sub", "stacked-fractions diagonal-fractions historical-ligatures",
                      "common-ligatures traditional common-ligatures", "lining-nums traditional slashed-zero ordinal normal",
                      "traditional historical-forms styleset(ok-alt-a, ok-alt-b) historical-forms",
                      "historical-forms styleset(ok-alt-a, ok-alt-b) traditional styleset(potato)", "annotation(a,b,c)" ]
  },
  "font-variant-alternates": {
    domProp: "fontVariantAlternates",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "historical-forms",
                        "styleset(alt-a, alt-b)", "character-variant(a, b, c)", "annotation(circled)",
                        "swash(squishy)", "styleset(complex\\ blob, a)", "annotation(\\62 lah)" ],
    invalid_values: [ "historical-forms normal", "historical-forms historical-forms",
                          "swash", "swash(3)", "annotation(a, b)", "ornaments(a,b)",
                          "styleset(1234blah)", "annotation(a), annotation(b)", "annotation(a) normal" ]
  },
   "font-variant-caps": {
    domProp: "fontVariantCaps",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "small-caps", "all-small-caps", "petite-caps", "all-petite-caps", "titling-caps", "unicase" ],
    invalid_values: [ "normal small-caps", "petite-caps normal", "unicase unicase" ]
  },
  "font-variant-east-asian": {
    domProp: "fontVariantEastAsian",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "jis78", "jis83", "jis90", "jis04", "simplified", "traditional", "full-width", "proportional-width", "ruby",
                    "jis78 full-width", "jis78 full-width ruby", "simplified proportional-width", "ruby simplified" ],
    invalid_values: [ "jis78 normal", "jis90 jis04", "simplified traditional", "full-width proportional-width",
                          "ruby simplified ruby", "jis78 ruby simplified" ]
  },
  "font-variant-ligatures": {
    domProp: "fontVariantLigatures",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "none", "common-ligatures", "no-common-ligatures", "discretionary-ligatures", "no-discretionary-ligatures",
                    "historical-ligatures", "no-historical-ligatures", "contextual", "no-contextual",
                    "common-ligatures no-discretionary-ligatures", "contextual no-discretionary-ligatures",
                    "historical-ligatures no-common-ligatures", "no-historical-ligatures discretionary-ligatures",
                    "common-ligatures no-discretionary-ligatures historical-ligatures no-contextual" ],
    invalid_values: [ "common-ligatures normal", "common-ligatures no-common-ligatures", "common-ligatures common-ligatures",
                      "no-historical-ligatures historical-ligatures", "no-discretionary-ligatures discretionary-ligatures",
                      "no-contextual contextual", "common-ligatures no-discretionary-ligatures no-common-ligatures",
                      "common-ligatures none", "no-discretionary-ligatures none", "none common-ligatures" ]
  },
  "font-variant-numeric": {
    domProp: "fontVariantNumeric",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "lining-nums", "oldstyle-nums", "proportional-nums", "tabular-nums", "diagonal-fractions",
                    "stacked-fractions", "slashed-zero", "ordinal", "lining-nums diagonal-fractions",
                    "tabular-nums stacked-fractions", "tabular-nums slashed-zero stacked-fractions",
                    "proportional-nums slashed-zero diagonal-fractions oldstyle-nums ordinal" ],
    invalid_values: [ "lining-nums normal", "lining-nums oldstyle-nums", "lining-nums normal slashed-zero ordinal",
                      "proportional-nums tabular-nums", "diagonal-fractions stacked-fractions", "slashed-zero diagonal-fractions slashed-zero",
                      "lining-nums slashed-zero diagonal-fractions oldstyle-nums", "diagonal-fractions diagonal-fractions" ]
  },
  "font-variant-position": {
    domProp: "fontVariantPosition",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "super", "sub" ],
    invalid_values: [ "normal sub", "super sub" ]
  },
  "font-weight": {
    domProp: "fontWeight",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal", "400" ],
    other_values: [ "bold", "100", "200", "300", "500", "600", "700", "800", "900", "bolder", "lighter" ],
    invalid_values: [ "0", "100.0", "107", "399", "401", "699", "710", "1000" ]
  },
  "height": {
    domProp: "height",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* FIXME: test zero, and test calc clamping */
    initial_values: [ " auto",
      // these four keywords compute to the initial value when the
      // writing mode is horizontal, and that's the context we're testing in
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
    ],
    /* computed value tests for height test more with display:block */
    prerequisites: { "display": "block" },
    other_values: [ "15px", "3em", "15%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "none" ],
    quirks_values: { "5": "5px" },
  },
  "ime-mode": {
    domProp: "imeMode",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "normal", "disabled", "active", "inactive" ],
    invalid_values: [ "none", "enabled", "1px" ]
  },
  "left": {
    domProp: "left",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [],
    quirks_values: { "5": "5px" },
  },
  "letter-spacing": {
    domProp: "letterSpacing",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "0", "0px", "1em", "2px", "-3px",
      "calc(0px)", "calc(1em)", "calc(1em + 3px)",
      "calc(15px / 2)", "calc(15px/2)", "calc(-3px)"
    ],
    invalid_values: [],
    quirks_values: { "5": "5px" },
  },
  "line-height": {
    domProp: "lineHeight",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    /*
     * Inheritance tests require consistent font size, since
     * getComputedStyle (which uses the CSS2 computed value, or
     * CSS2.1 used value) doesn't match what the CSS2.1 computed
     * value is.  And they even require consistent font metrics for
     * computation of 'normal'.   -moz-block-height requires height
     * on a block.
     */
    prerequisites: { "font-size": "19px", "font-size-adjust": "none", "font-family": "serif", "font-weight": "normal", "font-style": "normal", "height": "18px", "display": "block", "writing-mode": "initial" },
    initial_values: [ "normal" ],
    other_values: [ "1.0", "1", "1em", "47px", "-moz-block-height", "calc(2px)", "calc(50%)", "calc(3*25px)", "calc(25px*3)", "calc(3*25px + 50%)", "calc(1 + 2*3/4)" ],
    invalid_values: [ "calc(1 + 2px)", "calc(100% + 0.1)" ]
  },
  "list-style": {
    domProp: "listStyle",
    inherited: true,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "list-style-type", "list-style-position", "list-style-image" ],
    initial_values: [ "outside", "disc", "disc outside", "outside disc", "disc none", "none disc", "none disc outside", "none outside disc", "disc none outside", "disc outside none", "outside none disc", "outside disc none" ],
    other_values: [ "inside none", "none inside", "none none inside", "square", "none", "none none", "outside none none", "none outside none", "none none outside", "none outside", "outside none", "outside outside", "outside inside", "\\32 style", "\\32 style inside",
      '"-"', "'-'", "inside '-'", "'-' outside", "none '-'", "inside none '-'",
      "symbols(\"*\" \"\\2020\" \"\\2021\" \"\\A7\")",
      "symbols(cyclic \"*\" \"\\2020\" \"\\2021\" \"\\A7\")",
      "inside symbols(\"*\" \"\\2020\" \"\\2021\" \"\\A7\")",
      "symbols(\"*\" \"\\2020\" \"\\2021\" \"\\A7\") outside",
      "none symbols(\"*\" \"\\2020\" \"\\2021\" \"\\A7\")",
      "inside none symbols(\"*\" \"\\2020\" \"\\2021\" \"\\A7\")",
      'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
      'none url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
      'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") none',
      'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") outside',
      'outside url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
      'outside none url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
      'outside url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") none',
      'none url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") outside',
      'none outside url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
      'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") outside none',
      'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") none outside'
    ],
    invalid_values: [ "disc disc", "unknown value", "none none none", "none disc url(404.png)", "none url(404.png) disc", "disc none url(404.png)", "disc url(404.png) none", "url(404.png) none disc", "url(404.png) disc none", "none disc outside url(404.png)" ]
  },
  "list-style-image": {
    domProp: "listStyleImage",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
      // Add some tests for interesting url() values here to test serialization, etc.
      "url(\'data:text/plain,\"\')",
      "url(\"data:text/plain,\'\")",
      "url(\'data:text/plain,\\\'\')",
      "url(\"data:text/plain,\\\"\")",
      "url(\'data:text/plain,\\\"\')",
      "url(\"data:text/plain,\\\'\")",
      "url(data:text/plain,\\\\)",
    ],
    invalid_values: []
  },
  "list-style-position": {
    domProp: "listStylePosition",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "outside" ],
    other_values: [ "inside" ],
    invalid_values: []
  },
  "list-style-type": {
    domProp: "listStyleType",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "disc" ],
    other_values: [ "none", "circle", "square",
      "disclosure-closed", "disclosure-open",
      "decimal", "decimal-leading-zero",
      "lower-roman", "upper-roman", "lower-greek",
      "lower-alpha", "lower-latin", "upper-alpha", "upper-latin",
      "hebrew", "armenian", "georgian",
      "cjk-decimal", "cjk-ideographic",
      "hiragana", "katakana", "hiragana-iroha", "katakana-iroha",
      "japanese-informal", "japanese-formal", "korean-hangul-formal",
      "korean-hanja-informal", "korean-hanja-formal",
      "simp-chinese-informal", "simp-chinese-formal",
      "trad-chinese-informal", "trad-chinese-formal",
      "ethiopic-numeric",
      "-moz-cjk-heavenly-stem", "-moz-cjk-earthly-branch",
      "-moz-trad-chinese-informal", "-moz-trad-chinese-formal",
      "-moz-simp-chinese-informal", "-moz-simp-chinese-formal",
      "-moz-japanese-informal", "-moz-japanese-formal",
      "-moz-arabic-indic", "-moz-persian", "-moz-urdu",
      "-moz-devanagari", "-moz-gurmukhi", "-moz-gujarati",
      "-moz-oriya", "-moz-kannada", "-moz-malayalam", "-moz-bengali",
      "-moz-tamil", "-moz-telugu", "-moz-thai", "-moz-lao",
      "-moz-myanmar", "-moz-khmer",
      "-moz-hangul", "-moz-hangul-consonant",
      "-moz-ethiopic-halehame", "-moz-ethiopic-numeric",
      "-moz-ethiopic-halehame-am",
      "-moz-ethiopic-halehame-ti-er", "-moz-ethiopic-halehame-ti-et",
      "other-style", "inside", "outside", "\\32 style",
      '"-"', "'-'",
      "symbols(\"*\" \"\\2020\" \"\\2021\" \"\\A7\")",
      "symbols(cyclic '*' '\\2020' '\\2021' '\\A7')"
    ],
    invalid_values: []
  },
  "margin": {
    domProp: "margin",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "margin-top", "margin-right", "margin-bottom", "margin-left" ],
    initial_values: [ "0", "0px 0 0em", "0% 0px 0em 0pt" ],
    other_values: [ "3px 0", "2em 4px 2pt", "1em 2em 3px 4px", "1em calc(2em + 3px) 4ex 5cm" ],
    invalid_values: [ "1px calc(nonsense)", "1px red" ],
    unbalanced_values: [ "1px calc(" ],
    quirks_values: { "5": "5px", "3px 6px 2 5px": "3px 6px 2px 5px" },
  },
  "margin-bottom": {
    domProp: "marginBottom",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX testing auto has prerequisites */
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "margin-left": {
    domProp: "marginLeft",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX testing auto has prerequisites */
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "2em", "5%", ".5px", "+32px", "+.789px", "-.328px", "+0.56px", "-0.974px", "237px", "-289px", "-056px", "1987.45px", "-84.32px",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "..25px", ".+5px", ".px", "-.px", "++5px", "-+4px", "+-3px", "--7px", "+-.6px", "-+.5px", "++.7px", "--.4px" ],
    quirks_values: { "5": "5px" },
  },
  "margin-right": {
    domProp: "marginRight",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX testing auto has prerequisites */
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "margin-top": {
    domProp: "marginTop",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX testing auto has prerequisites */
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "max-height": {
    domProp: "maxHeight",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block" },
    initial_values: [ "none",
      // these four keywords compute to the initial value when the
      // writing mode is horizontal, and that's the context we're testing in
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
    ],
    other_values: [ "30px", "50%", "0",
      "calc(2px)",
      "calc(-2px)",
      "calc(0px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "auto" ],
    quirks_values: { "5": "5px" },
  },
  "max-width": {
    domProp: "maxWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block" },
    initial_values: [ "none" ],
    other_values: [ "30px", "50%", "0",
      // these four keywords compute to the initial value only when the
      // writing mode is vertical, and we're testing with a horizontal
      // writing mode
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
      "calc(2px)",
      "calc(-2px)",
      "calc(0px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "auto" ],
    quirks_values: { "5": "5px" },
  },
  "min-height": {
    domProp: "minHeight",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block" },
    initial_values: [ "auto", "0", "calc(0em)", "calc(-2px)",
      // these four keywords compute to the initial value when the
      // writing mode is horizontal, and that's the context we're testing in
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
    ],
    other_values: [ "30px", "50%",
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: ["none"],
    quirks_values: { "5": "5px" },
  },
  "min-width": {
    domProp: "minWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block" },
    initial_values: [ "auto", "0", "calc(0em)", "calc(-2px)" ],
    other_values: [ "30px", "50%",
      // these four keywords compute to the initial value only when the
      // writing mode is vertical, and we're testing with a horizontal
      // writing mode
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "none" ],
    quirks_values: { "5": "5px" },
  },

  "opacity": {
    domProp: "opacity",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1", "17", "397.376", "3e1", "3e+1", "3e0", "3e+0", "3e-0" ],
    other_values: [ "0", "0.4", "0.0000", "-3", "3e-1" ],
    invalid_values: [ "0px", "1px" ]
  },
  "-moz-orient": {
    domProp: "MozOrient",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "inline" ],
    other_values: [ "horizontal", "vertical", "block" ],
    invalid_values: [ "none" ]
  },
  "outline": {
    domProp: "outline",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "outline-color", "outline-style", "outline-width" ],
    initial_values: [
      "none", "medium", "thin",
      // XXX Should be invert, but currently currentcolor.
      //"invert", "none medium invert"
      "currentColor", "none medium currentcolor"
    ],
    other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid" ],
    invalid_values: [ "5%", "5", "5 solid green" ]
  },
  "outline-color": {
    domProp: "outlineColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor" ], // XXX should be invert
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000", "cc00ff" ]
  },
  "outline-offset": {
    domProp: "outlineOffset",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0px", "-0", "calc(0px)", "calc(3em + 2px - 2px - 3em)", "calc(-0em)" ],
    other_values: [ "-3px", "1em", "calc(3em)", "calc(7pt + 3 * 2em)", "calc(-3px)" ],
    invalid_values: [ "5%" ]
  },
  "outline-style": {
    domProp: "outlineStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    // XXX Should 'hidden' be the same as initial?
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge", "auto" ],
    invalid_values: []
  },
  "outline-width": {
    domProp: "outlineWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "outline-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0px)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%", "5" ]
  },
  "overflow": {
    domProp: "overflow",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    prerequisites: { "display": "block", "contain": "none" },
    subproperties: [ "overflow-x", "overflow-y" ],
    initial_values: [ "visible" ],
    other_values: [ "auto", "scroll", "hidden", "-moz-hidden-unscrollable", "-moz-scrollbars-none" ],
    invalid_values: []
  },
  "overflow-x": {
    domProp: "overflowX",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block", "overflow-y": "visible", "contain": "none" },
    initial_values: [ "visible" ],
    other_values: [ "auto", "scroll", "hidden", "-moz-hidden-unscrollable" ],
    invalid_values: []
  },
  "overflow-y": {
    domProp: "overflowY",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "display": "block", "overflow-x": "visible", "contain": "none" },
    initial_values: [ "visible" ],
    other_values: [ "auto", "scroll", "hidden", "-moz-hidden-unscrollable" ],
    invalid_values: []
  },
  "padding": {
    domProp: "padding",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "padding-top", "padding-right", "padding-bottom", "padding-left" ],
    initial_values: [ "0", "0px 0 0em", "0% 0px 0em 0pt", "calc(0px) calc(0em) calc(-2px) calc(-1%)" ],
    other_values: [ "3px 0", "2em 4px 2pt", "1em 2em 3px 4px" ],
    invalid_values: [ "1px calc(nonsense)", "1px red" ],
    unbalanced_values: [ "1px calc(" ],
    quirks_values: { "5": "5px", "3px 6px 2 5px": "3px 6px 2px 5px" },
  },
  "padding-bottom": {
    domProp: "paddingBottom",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "padding-left": {
    domProp: "paddingLeft",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "padding-right": {
    domProp: "paddingRight",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "padding-top": {
    domProp: "paddingTop",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "page-break-after": {
    domProp: "pageBreakAfter",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "always", "avoid", "left", "right" ],
    invalid_values: []
  },
  "page-break-before": {
    domProp: "pageBreakBefore",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "always", "avoid", "left", "right" ],
    invalid_values: []
  },
  "page-break-inside": {
    domProp: "pageBreakInside",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "avoid" ],
    invalid_values: [ "left", "right" ]
  },
  "pointer-events": {
    domProp: "pointerEvents",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "visiblePainted", "visibleFill", "visibleStroke", "visible",
            "painted", "fill", "stroke", "all", "none" ],
    invalid_values: []
  },
  "position": {
    domProp: "position",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "static" ],
    other_values: [ "relative", "absolute", "fixed", "sticky" ],
    invalid_values: []
  },
  "quotes": {
    domProp: "quotes",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ '"\u201C" "\u201D" "\u2018" "\u2019"',
              '"\\201C" "\\201D" "\\2018" "\\2019"' ],
    other_values: [ "none", "'\"' '\"'" ],
    invalid_values: []
  },
  "right": {
    domProp: "right",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [],
    quirks_values: { "5": "5px" },
  },
  "ruby-align": {
    domProp: "rubyAlign",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "space-around" ],
    other_values: [ "start", "center", "space-between" ],
    invalid_values: [
      "end", "1", "10px", "50%", "start center"
    ]
  },
  "ruby-position": {
    domProp: "rubyPosition",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "over" ],
    other_values: [ "under" ],
    invalid_values: [
      "left", "right", "auto", "none", "not_a_position",
      "over left", "right under", "0", "100px", "50%"
    ]
  },
  "table-layout": {
    domProp: "tableLayout",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "fixed" ],
    invalid_values: []
  },
  "text-align": {
    domProp: "textAlign",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    // don't know whether left and right are same as start
    initial_values: [ "start" ],
    other_values: [ "center", "justify", "end", "match-parent" ],
    invalid_values: [ "true", "true true" ]
  },
  "text-align-last": {
    domProp: "textAlignLast",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "center", "justify", "start", "end", "left", "right" ],
    invalid_values: []
  },
  "text-decoration": {
    domProp: "textDecoration",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    subproperties: [ "text-decoration-color", "text-decoration-line", "text-decoration-style" ],
    initial_values: [ "none" ],
    other_values: [ "underline", "overline", "line-through", "blink", "blink line-through underline", "underline overline line-through blink", "-moz-anchor-decoration", "blink -moz-anchor-decoration",
                    "underline red solid", "underline #ff0000", "solid underline", "red underline", "#ff0000 underline", "dotted underline" ],
    invalid_values: [ "none none", "underline none", "none underline", "blink none", "none blink", "line-through blink line-through", "underline overline line-through blink none", "underline overline line-throuh blink blink", "rgb(0, rubbish, 0) underline" ]
  },
  "text-decoration-color": {
    domProp: "textDecorationColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000", "ff00ff" ]
  },
  "text-decoration-line": {
    domProp: "textDecorationLine",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "underline", "overline", "line-through", "blink", "blink line-through underline", "underline overline line-through blink", "-moz-anchor-decoration", "blink -moz-anchor-decoration" ],
    invalid_values: [ "none none", "underline none", "none underline", "line-through blink line-through", "underline overline line-through blink none", "underline overline line-throuh blink blink" ]
  },
  "text-decoration-style": {
    domProp: "textDecorationStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "solid" ],
    other_values: [ "double", "dotted", "dashed", "wavy", "-moz-none" ],
    invalid_values: [ "none", "groove", "ridge", "inset", "outset", "solid dashed", "wave" ]
  },
  "text-emphasis": {
    domProp: "textEmphasis",
    inherited: true,
    type: CSS_TYPE_TRUE_SHORTHAND,
    prerequisites: { "color": "black" },
    subproperties: [ "text-emphasis-style", "text-emphasis-color" ],
    initial_values: [ "none currentColor", "currentColor none", "none", "currentColor", "none black" ],
    other_values: [ "filled dot black", "#f00 circle open", "sesame filled rgba(0,0,255,0.5)", "red", "green none", "currentColor filled", "currentColor open" ],
    invalid_values: [ "filled black dot", "filled filled red", "open open circle #000", "circle dot #f00", "rubbish" ]
  },
  "text-emphasis-color": {
    domProp: "textEmphasisColor",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor", "black", "rgb(0,0,0)" ],
    other_values: [ "red", "rgba(255,255,255,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000", "ff00ff", "rgb(255,xxx,255)" ]
  },
  "text-emphasis-position": {
    domProp: "textEmphasisPosition",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "over right", "right over" ],
    other_values: [ "over left", "left over", "under left", "left under", "under right", "right under" ],
    invalid_values: [ "over over", "left left", "over right left", "rubbish left", "over rubbish" ]
  },
  "text-emphasis-style": {
    domProp: "textEmphasisStyle",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "filled", "open", "dot", "circle", "double-circle", "triangle", "sesame", "'#'",
                    "filled dot", "filled circle", "filled double-circle", "filled triangle", "filled sesame",
                    "dot filled", "circle filled", "double-circle filled", "triangle filled", "sesame filled",
                    "dot open", "circle open", "double-circle open", "triangle open", "sesame open" ],
    invalid_values: [ "rubbish", "dot rubbish", "rubbish dot", "open rubbish", "rubbish open", "open filled", "dot circle",
                      "open '#'", "'#' filled", "dot '#'", "'#' circle", "1", "1 open", "open 1" ]
  },
  "text-indent": {
    domProp: "textIndent",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "calc(3em - 5em + 2px + 2em - 2px)" ],
    other_values: [ "2em", "5%", "-10px",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "text-overflow": {
    domProp: "textOverflow",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "clip" ],
    other_values: [ "ellipsis", '""', "''", '"hello"', 'clip clip', 'ellipsis ellipsis', 'clip ellipsis', 'clip ""', '"hello" ""', '"" ellipsis' ],
    invalid_values: [ "none", "auto", '"hello" inherit', 'inherit "hello"', 'clip initial', 'initial clip', 'initial inherit', 'inherit initial', 'inherit none']
  },
  "text-shadow": {
    domProp: "textShadow",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "blue" },
    initial_values: [ "none" ],
    other_values: [ "2px 2px", "2px 2px 1px", "2px 2px green", "2px 2px 1px green", "green 2px 2px", "green 2px 2px 1px", "green 2px 2px, blue 1px 3px 4px", "currentColor 3px 3px", "blue 2px 2px, currentColor 1px 2px",
      /* calc() values */
      "2px 2px calc(-5px)", /* clamped */
      "calc(3em - 2px) 2px green",
      "green calc(3em - 2px) 2px",
      "2px calc(2px + 0.2em)",
      "blue 2px calc(2px + 0.2em)",
      "2px calc(2px + 0.2em) blue",
      "calc(-2px) calc(-2px)",
      "-2px -2px",
      "calc(2px) calc(2px)",
      "calc(2px) calc(2px) calc(2px)",
    ],
    invalid_values: [ "3% 3%", "2px 2px -5px", "2px 2px 2px 2px", "2px 2px, none", "none, 2px 2px", "inherit, 2px 2px", "2px 2px, inherit", "2 2px", "2px 2", "2px 2px 2", "2px 2px 2px 2",
      "calc(2px) calc(2px) calc(2px) calc(2px)", "3px 3px calc(3px + rubbish)"
    ]
  },
  "text-transform": {
    domProp: "textTransform",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "capitalize", "uppercase", "lowercase", "full-width" ],
    invalid_values: []
  },
  "top": {
    domProp: "top",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [],
    quirks_values: { "5": "5px" },
  },
  "transition": {
    domProp: "transition",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "transition-property", "transition-duration", "transition-timing-function", "transition-delay" ],
    initial_values: [ "all 0s ease 0s", "all", "0s", "0s 0s", "ease" ],
    other_values: [ "all 0s cubic-bezier(0.25, 0.1, 0.25, 1.0) 0s", "width 1s linear 2s", "width 1s 2s linear", "width linear 1s 2s", "linear width 1s 2s", "linear 1s width 2s", "linear 1s 2s width", "1s width linear 2s", "1s width 2s linear", "1s 2s width linear", "1s linear width 2s", "1s linear 2s width", "1s 2s linear width", "width linear 1s", "width 1s linear", "linear width 1s", "linear 1s width", "1s width linear", "1s linear width", "1s 2s width", "1s width 2s", "width 1s 2s", "1s 2s linear", "1s linear 2s", "linear 1s 2s", "width 1s", "1s width", "linear 1s", "1s linear", "1s 2s", "2s 1s", "width", "linear", "1s", "height", "2s", "ease-in-out", "2s ease-in", "opacity linear", "ease-out 2s", "2s color, 1s width, 500ms height linear, 1s opacity 4s cubic-bezier(0.0, 0.1, 1.0, 1.0)", "1s \\32width linear 2s", "1s -width linear 2s", "1s -\\32width linear 2s", "1s \\32 0width linear 2s", "1s -\\32 0width linear 2s", "1s \\2width linear 2s", "1s -\\2width linear 2s", "2s, 1s width", "1s width, 2s", "2s all, 1s width", "1s width, 2s all", "2s all, 1s width", "2s width, 1s all", "3s --my-color" ],
    invalid_values: [ "1s width, 2s none", "2s none, 1s width", "2s inherit", "inherit 2s", "2s width, 1s inherit", "2s inherit, 1s width", "2s initial", "1s width,,2s color", "1s width, ,2s color", "bounce 1s cubic-bezier(0, rubbish) 2s", "bounce 1s steps(rubbish) 2s" ]
  },
  "transition-delay": {
    domProp: "transitionDelay",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0s", "0ms" ],
    other_values: [ "1s", "250ms", "-100ms", "-1s", "1s, 250ms, 2.3s"],
    invalid_values: [ "0", "0px" ]
  },
  "transition-duration": {
    domProp: "transitionDuration",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0s", "0ms" ],
    other_values: [ "1s", "250ms", "1s, 250ms, 2.3s"],
    invalid_values: [ "0", "0px", "-1ms", "-2s" ]
  },
  "transition-property": {
    domProp: "transitionProperty",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "all" ],
    other_values: [ "none", "left", "top", "color", "width, height, opacity", "foobar", "auto", "\\32width", "-width", "-\\32width", "\\32 0width", "-\\32 0width", "\\2width", "-\\2width", "all, all", "all, color", "color, all", "--my-color" ],
    invalid_values: [ "none, none", "color, none", "none, color", "inherit, color", "color, inherit", "initial, color", "color, initial", "none, color", "color, none" ]
  },
  "transition-timing-function": {
    domProp: "transitionTimingFunction",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "ease" ],
    other_values: [ "cubic-bezier(0.25, 0.1, 0.25, 1.0)", "linear", "ease-in", "ease-out", "ease-in-out", "linear, ease-in, cubic-bezier(0.1, 0.2, 0.8, 0.9)", "cubic-bezier(0.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.25, 1.5, 0.75, -0.5)", "step-start", "step-end", "steps(1)", "steps(2, start)", "steps(386)", "steps(3, end)" ],
    invalid_values: [ "none", "auto", "cubic-bezier(0.25, 0.1, 0.25)", "cubic-bezier(0.25, 0.1, 0.25, 0.25, 1.0)", "cubic-bezier(-0.5, 0.5, 0.5, 0.5)", "cubic-bezier(1.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.5, 0.5, -0.5, 0.5)", "cubic-bezier(0.5, 0.5, 1.5, 0.5)", "steps(2, step-end)", "steps(0)", "steps(-2)", "steps(0, step-end, 1)" ]
  },
  "unicode-bidi": {
    domProp: "unicodeBidi",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "embed", "bidi-override", "isolate", "plaintext", "isolate-override", "-moz-isolate", "-moz-plaintext", "-moz-isolate-override" ],
    invalid_values: [ "auto", "none" ]
  },
  "vertical-align": {
    domProp: "verticalAlign",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "baseline" ],
    other_values: [ "sub", "super", "top", "text-top", "middle", "bottom", "text-bottom", "-moz-middle-with-baseline", "15%", "3px", "0.2em", "-5px", "-3%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
    quirks_values: { "5": "5px" },
  },
  "visibility": {
    domProp: "visibility",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "visible" ],
    other_values: [ "hidden", "collapse" ],
    invalid_values: []
  },
  "white-space": {
    domProp: "whiteSpace",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "pre", "nowrap", "pre-wrap", "pre-line", "-moz-pre-space" ],
    invalid_values: []
  },
  "width": {
    domProp: "width",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* computed value tests for width test more with display:block */
    prerequisites: { "display": "block" },
    initial_values: [ " auto" ],
    /* XXX these have prerequisites */
    other_values: [ "15px", "3em", "15%",
      // these three keywords compute to the initial value only when the
      // writing mode is vertical, and we're testing with a horizontal
      // writing mode
      "-moz-max-content", "-moz-min-content", "-moz-fit-content",
      // whether -moz-available computes to the initial value depends on
      // the container size, and for the container size we're testing
      // with, it does
      // "-moz-available",
      "3e1px", "3e+1px", "3e0px", "3e+0px", "3e-0px", "3e-1px",
      "3.2e1px", "3.2e+1px", "3.2e0px", "3.2e+0px", "3.2e-0px", "3.2e-1px",
      "3e1%", "3e+1%", "3e0%", "3e+0%", "3e-0%", "3e-1%",
      "3.2e1%", "3.2e+1%", "3.2e0%", "3.2e+0%", "3.2e-0%", "3.2e-1%",
      /* valid -moz-calc() values */
      "-moz-calc(-2px)",
      "-moz-calc(2px)",
      "-moz-calc(50%)",
      "-moz-calc(50% + 2px)",
      "-moz-calc( 50% + 2px)",
      "-moz-calc(50% + 2px )",
      "-moz-calc( 50% + 2px )",
      "-moz-calc(50% - -2px)",
      "-moz-calc(2px - -50%)",
      "-moz-calc(3*25px)",
      "-moz-calc(3 *25px)",
      "-moz-calc(3 * 25px)",
      "-moz-calc(3* 25px)",
      "-moz-calc(25px*3)",
      "-moz-calc(25px *3)",
      "-moz-calc(25px* 3)",
      "-moz-calc(25px * 3)",
      "-moz-calc(3*25px + 50%)",
      "-moz-calc(50% - 3em + 2px)",
      "-moz-calc(50% - (3em + 2px))",
      "-moz-calc((50% - 3em) + 2px)",
      "-moz-calc(2em)",
      "-moz-calc(50%)",
      "-moz-calc(50px/2)",
      "-moz-calc(50px/(2 - 1))",
      /* valid calc() values */
      "calc(-2px)",
      "calc(2px)",
      "calc(50%)",
      "calc(50% + 2px)",
      "calc( 50% + 2px)",
      "calc(50% + 2px )",
      "calc( 50% + 2px )",
      "calc(50% - -2px)",
      "calc(2px - -50%)",
      "calc(3*25px)",
      "calc(3 *25px)",
      "calc(3 * 25px)",
      "calc(3* 25px)",
      "calc(25px*3)",
      "calc(25px *3)",
      "calc(25px* 3)",
      "calc(25px * 3)",
      "calc(3*25px + 50%)",
      "calc(50% - 3em + 2px)",
      "calc(50% - (3em + 2px))",
      "calc((50% - 3em) + 2px)",
      "calc(2em)",
      "calc(50%)",
      "calc(50px/2)",
      "calc(50px/(2 - 1))",
    ],
    invalid_values: [ "none", "-2px",
      /* invalid -moz-calc() values */
      "-moz-calc(50%+ 2px)",
      "-moz-calc(50% +2px)",
      "-moz-calc(50%+2px)",
      /* invalid calc() values */
      "calc(50%+ 2px)",
      "calc(50% +2px)",
      "calc(50%+2px)",
      "-moz-min()",
      "calc(min())",
      "-moz-max()",
      "calc(max())",
      "-moz-min(5px)",
      "calc(min(5px))",
      "-moz-max(5px)",
      "calc(max(5px))",
      "-moz-min(5px,2em)",
      "calc(min(5px,2em))",
      "-moz-max(5px,2em)",
      "calc(max(5px,2em))",
      "calc(50px/(2 - 2))",
      /* If we ever support division by values, which is
       * complicated for the reasons described in
       * http://lists.w3.org/Archives/Public/www-style/2010Jan/0007.html
       * , we should support all 4 of these as described in
       * http://lists.w3.org/Archives/Public/www-style/2009Dec/0296.html
       */
      "calc((3em / 100%) * 3em)",
      "calc(3em / 100% * 3em)",
      "calc(3em * (3em / 100%))",
      "calc(3em * 3em / 100%)",
    ],
    quirks_values: { "5": "5px" },
  },
  "will-change": {
    domProp: "willChange",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "scroll-position", "contents", "transform", "opacity", "scroll-position, transform", "transform, opacity", "contents, transform", "property-that-doesnt-exist-yet" ],
    invalid_values: [ "none", "all", "default", "auto, scroll-position", "scroll-position, auto", "transform scroll-position", ",", "trailing,", "will-change", "transform, will-change" ]
  },
  "word-break": {
    domProp: "wordBreak",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "break-all", "keep-all" ],
    invalid_values: []
  },
  "word-spacing": {
    domProp: "wordSpacing",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal", "0", "0px", "-0em",
      "calc(-0px)", "calc(0em)"
    ],
    other_values: [ "1em", "2px", "-3px", "0%", "50%", "-120%",
      "calc(1em)", "calc(1em + 3px)",
      "calc(15px / 2)", "calc(15px/2)",
      "calc(-2em)", "calc(0% + 0px)",
      "calc(-10%/2 - 1em)"
    ],
    invalid_values: [],
    quirks_values: { "5": "5px" },
  },
  "overflow-wrap": {
    domProp: "overflowWrap",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "break-word" ],
    invalid_values: []
  },
  "hyphens": {
    domProp: "hyphens",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "manual" ],
    other_values: [ "none", "auto" ],
    invalid_values: []
  },
  "z-index": {
    domProp: "zIndex",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    /* XXX requires position */
    initial_values: [ "auto" ],
    other_values: [ "0", "3", "-7000", "12000" ],
    invalid_values: [ "3.0", "17.5", "3e1" ]
  }
  ,
  "clip-path": {
    domProp: "clipPath",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(#mypath)", "url('404.svg#mypath')" ],
    invalid_values: []
  },
  "clip-rule": {
    domProp: "clipRule",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "nonzero" ],
    other_values: [ "evenodd" ],
    invalid_values: []
  },
  "color-interpolation": {
    domProp: "colorInterpolation",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "sRGB" ],
    other_values: [ "auto", "linearRGB" ],
    invalid_values: []
  },
  "color-interpolation-filters": {
    domProp: "colorInterpolationFilters",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "linearRGB" ],
    other_values: [ "sRGB", "auto" ],
    invalid_values: []
  },
  "dominant-baseline": {
    domProp: "dominantBaseline",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "use-script", "no-change", "reset-size", "ideographic", "alphabetic", "hanging", "mathematical", "central", "middle", "text-after-edge", "text-before-edge" ],
    invalid_values: []
  },
  "fill": {
    domProp: "fill",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "blue" },
    initial_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)" ],
    other_values: [ "green", "#fc3", "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "none", "currentColor", "context-fill", "context-stroke" ],
    invalid_values: [ "000000", "ff00ff", "url('#myserver') rgb(0, rubbish, 0)" ]
  },
  "fill-opacity": {
    domProp: "fillOpacity",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1", "2.8", "1.000", "context-fill-opacity", "context-stroke-opacity" ],
    other_values: [ "0", "0.3", "-7.3" ],
    invalid_values: []
  },
  "fill-rule": {
    domProp: "fillRule",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "nonzero" ],
    other_values: [ "evenodd" ],
    invalid_values: []
  },
  "filter": {
    domProp: "filter",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(#myfilt)" ],
    invalid_values: [ "url(#myfilt) none" ]
  },
  "flood-color": {
    domProp: "floodColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "blue" },
    initial_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)" ],
    other_values: [ "green", "#fc3", "currentColor" ],
    invalid_values: [ "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "000000", "ff00ff" ]
  },
  "flood-opacity": {
    domProp: "floodOpacity",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1", "2.8", "1.000" ],
    other_values: [ "0", "0.3", "-7.3" ],
    invalid_values: []
  },
  "image-rendering": {
    domProp: "imageRendering",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "optimizeSpeed", "optimizeQuality", "-moz-crisp-edges" ],
    invalid_values: []
  },
  "lighting-color": {
    domProp: "lightingColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "blue" },
    initial_values: [ "white", "#fff", "#ffffff", "rgb(255,255,255)", "rgba(255,255,255,1.0)", "rgba(255,255,255,42.0)" ],
    other_values: [ "green", "#fc3", "currentColor" ],
    invalid_values: [ "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "000000", "ff00ff" ]
  },
  "marker": {
    domProp: "marker",
    inherited: true,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "marker-start", "marker-mid", "marker-end" ],
    initial_values: [ "none" ],
    other_values: [ "url(#mysym)" ],
    invalid_values: [ "none none", "url(#mysym) url(#mysym)", "none url(#mysym)", "url(#mysym) none" ]
  },
  "marker-end": {
    domProp: "markerEnd",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(#mysym)" ],
    invalid_values: []
  },
  "marker-mid": {
    domProp: "markerMid",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(#mysym)" ],
    invalid_values: []
  },
  "marker-start": {
    domProp: "markerStart",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(#mysym)" ],
    invalid_values: []
  },
  "shape-rendering": {
    domProp: "shapeRendering",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "optimizeSpeed", "crispEdges", "geometricPrecision" ],
    invalid_values: []
  },
  "stop-color": {
    domProp: "stopColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "blue" },
    initial_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)" ],
    other_values: [ "green", "#fc3", "currentColor" ],
    invalid_values: [ "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "000000", "ff00ff" ]
  },
  "stop-opacity": {
    domProp: "stopOpacity",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1", "2.8", "1.000" ],
    other_values: [ "0", "0.3", "-7.3" ],
    invalid_values: []
  },
  "stroke": {
    domProp: "stroke",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)", "green", "#fc3", "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "currentColor", "context-fill", "context-stroke" ],
    invalid_values: [ "000000", "ff00ff" ]
  },
  "stroke-dasharray": {
    domProp: "strokeDasharray",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none", "context-value" ],
    other_values: [ "5px,3px,2px", "5px 3px 2px", "  5px ,3px\t, 2px ", "1px", "5%", "3em" ],
    invalid_values: [ "-5px,3px,2px", "5px,3px,-2px" ]
  },
  "stroke-dashoffset": {
    domProp: "strokeDashoffset",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "-0px", "0em", "context-value" ],
    other_values: [ "3px", "3%", "1em" ],
    invalid_values: []
  },
  "stroke-linecap": {
    domProp: "strokeLinecap",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "butt" ],
    other_values: [ "round", "square" ],
    invalid_values: []
  },
  "stroke-linejoin": {
    domProp: "strokeLinejoin",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "miter" ],
    other_values: [ "round", "bevel" ],
    invalid_values: []
  },
  "stroke-miterlimit": {
    domProp: "strokeMiterlimit",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "4" ],
    other_values: [ "1", "7", "5000", "1.1" ],
    invalid_values: [ "0.9", "0", "-1", "3px", "-0.3" ]
  },
  "stroke-opacity": {
    domProp: "strokeOpacity",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1", "2.8", "1.000", "context-fill-opacity", "context-stroke-opacity" ],
    other_values: [ "0", "0.3", "-7.3" ],
    invalid_values: []
  },
  "stroke-width": {
    domProp: "strokeWidth",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1px", "context-value" ],
    other_values: [ "0", "0px", "-0em", "17px", "0.2em" ],
    invalid_values: [ "-0.1px", "-3px" ]
  },
  "text-anchor": {
    domProp: "textAnchor",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "start" ],
    other_values: [ "middle", "end" ],
    invalid_values: []
  },
  "text-rendering": {
    domProp: "textRendering",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "optimizeSpeed", "optimizeLegibility", "geometricPrecision" ],
    invalid_values: []
  },
  "vector-effect": {
    domProp: "vectorEffect",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "non-scaling-stroke" ],
    invalid_values: []
  },
  "-moz-window-dragging": {
    domProp: "MozWindowDragging",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "default" ],
    other_values: [ "drag", "no-drag" ],
    invalid_values: [ "none" ]
  },
  "align-content": {
    domProp: "alignContent",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "start", "end", "flex-start", "flex-end", "center", "left",
                    "right", "space-between", "space-around", "space-evenly",
                    "first baseline", "last baseline", "baseline", "stretch", "start safe",
                    "unsafe end", "unsafe end stretch", "end safe space-evenly" ],
    invalid_values: [ "none", "5", "self-end", "safe", "normal unsafe", "unsafe safe",
                      "safe baseline", "baseline unsafe", "baseline end", "end normal",
                      "safe end unsafe start", "safe end unsafe", "normal safe start",
                      "unsafe end start", "end start safe", "space-between unsafe",
                      "stretch safe", "auto", "first", "last" ]
  },
  "align-items": {
    domProp: "alignItems",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    // Can't test 'left'/'right' here since that computes to 'start' for blocks.
    other_values: [ "end", "flex-start", "flex-end", "self-start", "self-end",
                    "center", "stretch", "first baseline", "last baseline", "baseline",
                    "unsafe left", "start", "center unsafe", "safe right", "center safe" ],
    invalid_values: [ "space-between", "abc", "5%", "legacy", "legacy end",
                      "end legacy", "unsafe", "unsafe baseline", "normal unsafe",
                      "safe left unsafe", "safe stretch", "end end", "auto" ]
  },
  "align-self": {
    domProp: "alignSelf",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "normal", "start", "flex-start", "flex-end", "center", "stretch",
                    "first baseline", "last baseline", "baseline", "right safe",
                    "unsafe center", "self-start", "self-end safe" ],
    invalid_values: [ "space-between", "abc", "30px", "stretch safe", "safe" ]
  },
  "justify-content": {
    domProp: "justifyContent",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "start", "end", "flex-start", "flex-end", "center", "left",
                    "right", "space-between", "space-around", "space-evenly",
                    "first baseline", "last baseline", "baseline", "stretch", "start safe",
                    "unsafe end", "unsafe end stretch", "end safe space-evenly" ],
    invalid_values: [ "30px", "5%", "self-end", "safe", "normal unsafe", "unsafe safe",
                      "safe baseline", "baseline unsafe", "baseline end", "normal end",
                      "safe end unsafe start", "safe end unsafe", "normal safe start",
                      "unsafe end start", "end start safe", "space-around unsafe",
                      "safe stretch", "auto", "first", "last" ]
  },
  "justify-items": {
    domProp: "justifyItems",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto", "normal" ],
    other_values: [ "end", "flex-start", "flex-end", "self-start", "self-end",
                    "center", "left", "right", "first baseline", "last baseline",
                    "baseline", "stretch", "start", "legacy left", "right legacy",
                    "legacy center", "unsafe right", "left unsafe", "safe right",
                    "center safe" ],
    invalid_values: [ "space-between", "abc", "30px", "legacy", "legacy start",
                      "end legacy", "legacy baseline", "legacy legacy", "unsafe",
                      "safe legacy left", "legacy left safe", "legacy safe left",
                      "safe left legacy", "legacy left legacy", "baseline unsafe",
                      "safe unsafe", "safe left unsafe", "safe stretch", "last" ]
  },
  "justify-self": {
    domProp: "justifySelf",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "normal", "start", "end", "flex-start", "flex-end", "self-start",
                    "self-end", "center", "left", "right", "baseline", "first baseline",
                    "last baseline", "stretch", "left unsafe", "unsafe right",
                    "safe right", "center safe" ],
    invalid_values: [ "space-between", "abc", "30px", "none", "first", "last",
                      "legacy left", "right legacy", "baseline first", "baseline last" ]
  },
  "place-content": {
    domProp: "placeContent",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "align-content", "justify-content" ],
    initial_values: [ "normal" ],
    other_values: [ "normal start", "end baseline", "end end",
                    "space-between flex-end", "last baseline start",
                    "space-evenly", "flex-start", "end", "left" ],
    invalid_values: [ "none", "center safe", "unsafe start", "right / end" ]
  },
  "place-items": {
    domProp: "placeItems",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "align-items", "justify-items" ],
    initial_values: [ "normal" ],
    other_values: [ "normal center", "end baseline", "end auto",
                    "end", "right", "baseline", "start last baseline",
                    "left flex-end", "last baseline start", "stretch" ],
    invalid_values: [ "space-between", "start space-evenly", "none", "end/end",
                      "center safe", "auto start", "end legacy left" ]
  },
  "place-self": {
    domProp: "placeSelf",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "align-self", "justify-self" ],
    initial_values: [ "auto" ],
    other_values: [ "normal start", "end first baseline", "end auto",
                    "end", "right", "normal", "baseline", "start baseline",
                    "left self-end", "last baseline start", "stretch" ],
    invalid_values: [ "space-between", "start space-evenly", "none", "end safe",
                      "auto legacy left", "legacy left", "auto/auto" ]
  },
  "flex": {
    domProp: "flex",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "flex-grow",
      "flex-shrink",
      "flex-basis"
    ],
    initial_values: [ "0 1 auto", "auto 0 1", "0 auto", "auto 0" ],
    other_values: [
      "none",
      "1",
      "0",
      "0 1",
      "0.5",
      "1.2 3.4",
      "0 0 0",
      "0 0 0px",
      "0px 0 0",
      "5px 0 0",
      "2 auto",
      "auto 4",
      "auto 5.6 7.8",
      "-moz-max-content",
      "1 -moz-max-content",
      "1 2 -moz-max-content",
      "-moz-max-content 1",
      "-moz-max-content 1 2",
      "-0"
    ],
    invalid_values: [
      "1 2px 3",
      "1 auto 3",
      "1px 2 3px",
      "1px 2 3 4px",
      "-1",
      "1 -1",
      "0 1 calc(0px + rubbish)",
    ]
  },
  "flex-basis": {
    domProp: "flexBasis",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ " auto" ],
        // NOTE: This is cribbed directly from the "width" chunk, since this
        // property takes the exact same values as width (albeit with
        // different semantics on 'auto').
        // XXXdholbert (Maybe these should get separated out into
        // a reusable array defined at the top of this file?)
    other_values: [ "15px", "3em", "15%", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
      // valid calc() values
      "calc(-2px)",
      "calc(2px)",
      "calc(50%)",
      "calc(50% + 2px)",
      "calc( 50% + 2px)",
      "calc(50% + 2px )",
      "calc( 50% + 2px )",
      "calc(50% - -2px)",
      "calc(2px - -50%)",
      "calc(3*25px)",
      "calc(3 *25px)",
      "calc(3 * 25px)",
      "calc(3* 25px)",
      "calc(25px*3)",
      "calc(25px *3)",
      "calc(25px* 3)",
      "calc(25px * 3)",
      "calc(3*25px + 50%)",
      "calc(50% - 3em + 2px)",
      "calc(50% - (3em + 2px))",
      "calc((50% - 3em) + 2px)",
      "calc(2em)",
      "calc(50%)",
      "calc(50px/2)",
      "calc(50px/(2 - 1))"
    ],
    invalid_values: [ "none", "-2px",
      // invalid calc() values
      "calc(50%+ 2px)",
      "calc(50% +2px)",
      "calc(50%+2px)",
      "-moz-min()",
      "calc(min())",
      "-moz-max()",
      "calc(max())",
      "-moz-min(5px)",
      "calc(min(5px))",
      "-moz-max(5px)",
      "calc(max(5px))",
      "-moz-min(5px,2em)",
      "calc(min(5px,2em))",
      "-moz-max(5px,2em)",
      "calc(max(5px,2em))",
      "calc(50px/(2 - 2))",
      // If we ever support division by values, which is
      // complicated for the reasons described in
      // http://lists.w3.org/Archives/Public/www-style/2010Jan/0007.html
      // , we should support all 4 of these as described in
      // http://lists.w3.org/Archives/Public/www-style/2009Dec/0296.html
      "calc((3em / 100%) * 3em)",
      "calc(3em / 100% * 3em)",
      "calc(3em * (3em / 100%))",
      "calc(3em * 3em / 100%)"
    ]
  },
  "flex-direction": {
    domProp: "flexDirection",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "row" ],
    other_values: [ "row-reverse", "column", "column-reverse" ],
    invalid_values: [ "10px", "30%", "justify", "column wrap" ]
  },
  "flex-flow": {
    domProp: "flexFlow",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "flex-direction",
      "flex-wrap"
    ],
    initial_values: [ "row nowrap", "nowrap row", "row", "nowrap" ],
    other_values: [
      // only specifying one property:
      "column",
      "wrap",
      "wrap-reverse",
      // specifying both properties, 'flex-direction' first:
      "row wrap",
      "row wrap-reverse",
      "column wrap",
      "column wrap-reverse",
      // specifying both properties, 'flex-wrap' first:
      "wrap row",
      "wrap column",
      "wrap-reverse row",
      "wrap-reverse column",
    ],
    invalid_values: [
      // specifying flex-direction twice (invalid):
      "row column",
      "row column nowrap",
      "row nowrap column",
      "nowrap row column",
      // specifying flex-wrap twice (invalid):
      "nowrap wrap-reverse",
      "nowrap wrap-reverse row",
      "nowrap row wrap-reverse",
      "row nowrap wrap-reverse",
      // Invalid data-type / invalid keyword type:
      "1px", "5%", "justify", "none"
    ]
  },
  "flex-grow": {
    domProp: "flexGrow",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0" ],
    other_values: [ "3", "1", "1.0", "2.5", "123" ],
    invalid_values: [ "0px", "-5", "1%", "3em", "stretch", "auto" ]
  },
  "flex-shrink": {
    domProp: "flexShrink",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "1" ],
    other_values: [ "3", "0", "0.0", "2.5", "123" ],
    invalid_values: [ "0px", "-5", "1%", "3em", "stretch", "auto" ]
  },
  "flex-wrap": {
    domProp: "flexWrap",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "nowrap" ],
    other_values: [ "wrap", "wrap-reverse" ],
    invalid_values: [ "10px", "30%", "justify", "column wrap", "auto" ]
  },
  "order": {
    domProp: "order",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0" ],
    other_values: [ "1", "99999", "-1", "-50" ],
    invalid_values: [ "0px", "1.0", "1.", "1%", "0.2", "3em", "stretch" ]
  },

  // Aliases
  "word-wrap": {
    domProp: "wordWrap",
    inherited: true,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "overflow-wrap",
    subproperties: [ "overflow-wrap" ],
  },
  "-moz-transform": {
    domProp: "MozTransform",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transform",
    subproperties: [ "transform" ],
    // NOTE: We specify other_values & invalid_values explicitly here (instead
    // of deferring to "transform") because we accept some legacy syntax as
    // valid for "-moz-transform" but not for "transform".
    other_values: [ "translatex(1px)", "translatex(4em)",
      "translatex(-4px)", "translatex(3px)",
      "translatex(0px) translatex(1px) translatex(2px) translatex(3px) translatex(4px)",
      "translatey(4em)", "translate(3px)", "translate(10px, -3px)",
      "rotate(45deg)", "rotate(45grad)", "rotate(45rad)",
      "rotate(0.25turn)", "rotate(0)", "scalex(10)", "scaley(10)",
      "scale(10)", "scale(10, 20)", "skewx(30deg)", "skewx(0)",
      "skewy(0)", "skewx(30grad)", "skewx(30rad)", "skewx(0.08turn)",
      "skewy(30deg)", "skewy(30grad)", "skewy(30rad)", "skewy(0.08turn)",
      "rotate(45deg) scale(2, 1)", "skewx(45deg) skewx(-50grad)",
      "translate(0, 0) scale(1, 1) skewx(0) skewy(0) matrix(1, 0, 0, 1, 0, 0)",
      "translatex(50%)", "translatey(50%)", "translate(50%)",
      "translate(3%, 5px)", "translate(5px, 3%)",
      "matrix(1, 2, 3, 4, 5, 6)",
      /* valid calc() values */
      "translatex(calc(5px + 10%))",
      "translatey(calc(0.25 * 5px + 10% / 3))",
      "translate(calc(5px - 10% * 3))",
      "translate(calc(5px - 3 * 10%), 50px)",
      "translate(-50px, calc(5px - 10% * 3))",
      /* valid only when prefixed */
      "matrix(1, 2, 3, 4, 5px, 6%)",
      "matrix(1, 2, 3, 4, 5%, 6px)",
      "matrix(1, 2, 3, 4, 5%, 6%)",
      "matrix(1, 2, 3, 4, 5px, 6em)",
      "matrix(1, 0, 0, 1, calc(5px * 3), calc(10% - 3px))",
      "translatez(1px)", "translatez(4em)", "translatez(-4px)",
      "translatez(0px)", "translatez(2px) translatez(5px)",
      "translate3d(3px, 4px, 5px)", "translate3d(2em, 3px, 1em)",
      "translatex(2px) translate3d(4px, 5px, 6px) translatey(1px)",
      "scale3d(4, 4, 4)", "scale3d(-2, 3, -7)", "scalez(4)",
      "scalez(-6)", "rotate3d(2, 3, 4, 45deg)",
      "rotate3d(-3, 7, 0, 12rad)", "rotatex(15deg)", "rotatey(-12grad)",
      "rotatez(72rad)", "rotatex(0.125turn)",
      "perspective(0px)", "perspective(1000px)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)",
      /* valid only when prefixed */
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13px, 14em, 15px, 16)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 20%, 10%, 15, 16)",
    ],
    invalid_values: ["1px", "#0000ff", "red", "auto",
      "translatex(1)", "translatey(1)", "translate(2)",
      "translate(-3, -4)",
      "translatex(1px 1px)", "translatex(translatex(1px))",
      "translatex(#0000ff)", "translatex(red)", "translatey()",
      "matrix(1px, 2px, 3px, 4px, 5px, 6px)", "scale(150%)",
      "skewx(red)", "matrix(1%, 0, 0, 0, 0px, 0px)",
      "matrix(0, 1%, 2, 3, 4px,5px)", "matrix(0, 1, 2%, 3, 4px, 5px)",
      "matrix(0, 1, 2, 3%, 4%, 5%)",
      /* invalid calc() values */
      "translatey(-moz-min(5px,10%))",
      "translatex(-moz-max(5px,10%))",
      "translate(10px, calc(min(5px,10%)))",
      "translate(calc(max(5px,10%)), 10%)",
      "matrix(1, 0, 0, 1, max(5px * 3), calc(10% - 3px))",
      "perspective(-10px)", "matrix3d(dinosaur)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15%, 16)",
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16px)",
      "rotatey(words)", "rotatex(7)", "translate3d(3px, 4px, 1px, 7px)",
    ],
  },
  "-moz-transform-origin": {
    domProp: "MozTransformOrigin",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transform-origin",
    subproperties: [ "transform-origin" ],
  },
  "-moz-perspective-origin": {
    domProp: "MozPerspectiveOrigin",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "perspective-origin",
    subproperties: [ "perspective-origin" ],
  },
  "-moz-perspective": {
    domProp: "MozPerspective",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "perspective",
    subproperties: [ "perspective" ],
  },
  "-moz-backface-visibility": {
    domProp: "MozBackfaceVisibility",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "backface-visibility",
    subproperties: [ "backface-visibility" ],
  },
  "-moz-transform-style": {
    domProp: "MozTransformStyle",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transform-style",
    subproperties: [ "transform-style" ],
  },
  "-moz-border-image": {
    domProp: "MozBorderImage",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "border-image",
    subproperties: [ "border-image-source", "border-image-slice", "border-image-width", "border-image-outset", "border-image-repeat" ],
  },
  "-moz-transition": {
    domProp: "MozTransition",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "transition",
    subproperties: [ "transition-property", "transition-duration", "transition-timing-function", "transition-delay" ],
  },
  "-moz-transition-delay": {
    domProp: "MozTransitionDelay",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-delay",
    subproperties: [ "transition-delay" ],
  },
  "-moz-transition-duration": {
    domProp: "MozTransitionDuration",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-duration",
    subproperties: [ "transition-duration" ],
  },
  "-moz-transition-property": {
    domProp: "MozTransitionProperty",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-property",
    subproperties: [ "transition-property" ],
  },
  "-moz-transition-timing-function": {
    domProp: "MozTransitionTimingFunction",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-timing-function",
    subproperties: [ "transition-timing-function" ],
  },
  "-moz-animation": {
    domProp: "MozAnimation",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "animation",
    subproperties: [ "animation-name", "animation-duration", "animation-timing-function", "animation-delay", "animation-direction", "animation-fill-mode", "animation-iteration-count", "animation-play-state" ],
  },
  "-moz-animation-delay": {
    domProp: "MozAnimationDelay",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-delay",
    subproperties: [ "animation-delay" ],
  },
  "-moz-animation-direction": {
    domProp: "MozAnimationDirection",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-direction",
    subproperties: [ "animation-direction" ],
  },
  "-moz-animation-duration": {
    domProp: "MozAnimationDuration",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-duration",
    subproperties: [ "animation-duration" ],
  },
  "-moz-animation-fill-mode": {
    domProp: "MozAnimationFillMode",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-fill-mode",
    subproperties: [ "animation-fill-mode" ],
  },
  "-moz-animation-iteration-count": {
    domProp: "MozAnimationIterationCount",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-iteration-count",
    subproperties: [ "animation-iteration-count" ],
  },
  "-moz-animation-name": {
    domProp: "MozAnimationName",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-name",
    subproperties: [ "animation-name" ],
  },
  "-moz-animation-play-state": {
    domProp: "MozAnimationPlayState",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-play-state",
    subproperties: [ "animation-play-state" ],
  },
  "-moz-animation-timing-function": {
    domProp: "MozAnimationTimingFunction",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-timing-function",
    subproperties: [ "animation-timing-function" ],
  },
  "-moz-font-feature-settings": {
    domProp: "MozFontFeatureSettings",
    inherited: true,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "font-feature-settings",
    subproperties: [ "font-feature-settings" ],
  },
  "-moz-font-language-override": {
    domProp: "MozFontLanguageOverride",
    inherited: true,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "font-language-override",
    subproperties: [ "font-language-override" ],
  },
  "-moz-hyphens": {
    domProp: "MozHyphens",
    inherited: true,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "hyphens",
    subproperties: [ "hyphens" ],
  },
  "-moz-text-align-last": {
    domProp: "MozTextAlignLast",
    inherited: true,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "text-align-last",
    subproperties: [ "text-align-last" ],
  },
  // vertical text properties
  "writing-mode": {
    domProp: "writingMode",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "horizontal-tb", "lr", "lr-tb", "rl", "rl-tb" ],
    other_values: [ "vertical-lr", "vertical-rl", "sideways-rl", "sideways-lr", "tb", "tb-rl" ],
    invalid_values: [ "10px", "30%", "justify", "auto", "1em" ]
  },
  "text-orientation": {
    domProp: "textOrientation",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "mixed" ],
    other_values: [ "upright", "sideways", "sideways-right" ], /* sideways-right alias for backward compatibility */
    invalid_values: [ "none", "3em", "sideways-left" ] /* sideways-left removed from CSS Writing Modes */
  },
  "border-block-end": {
    domProp: "borderBlockEnd",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-block-end-color", "border-block-end-style", "border-block-end-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 solid green" ]
  },
  "block-size": {
    domProp: "blockSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    axis: true,
    get_computed: logical_axis_prop_get_computed,
    /* XXX testing auto has prerequisites */
    initial_values: [ "auto" ],
    prerequisites: { "display": "block" },
    other_values: [ "15px", "3em", "15%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "none", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available" ],
  },
  "border-block-end-color": {
    domProp: "borderBlockEndColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000" ]
  },
  "border-block-end-style": {
    domProp: "borderBlockEndStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-block-end-width": {
    domProp: "borderBlockEndWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    prerequisites: { "border-block-end-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%", "5" ]
  },
  "border-block-start": {
    domProp: "borderBlockStart",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "border-block-start-color", "border-block-start-style", "border-block-start-width" ],
    initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
    other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
    invalid_values: [ "5%", "5", "5 solid green" ]
  },
  "border-block-start-color": {
    domProp: "borderBlockStartColor",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    initial_values: [ "currentColor" ],
    other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000" ]
  },
  "border-block-start-style": {
    domProp: "borderBlockStartStyle",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* XXX hidden is sometimes the same as initial */
    initial_values: [ "none" ],
    other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
    invalid_values: []
  },
  "border-block-start-width": {
    domProp: "borderBlockStartWidth",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    prerequisites: { "border-block-start-style": "solid" },
    initial_values: [ "medium", "3px", "calc(4px - 1px)" ],
    other_values: [ "thin", "thick", "1px", "2em",
      "calc(2px)",
      "calc(-2px)",
      "calc(0em)",
      "calc(0px)",
      "calc(5em)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 5em)",
    ],
    invalid_values: [ "5%", "5" ]
  },
  "-moz-border-end": {
    domProp: "MozBorderEnd",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "border-inline-end",
    subproperties: [ "-moz-border-end-color", "-moz-border-end-style", "-moz-border-end-width" ],
  },
  "-moz-border-end-color": {
    domProp: "MozBorderEndColor",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-inline-end-color",
    subproperties: [ "border-inline-end-color" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-border-end-style": {
    domProp: "MozBorderEndStyle",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-inline-end-style",
    subproperties: [ "border-inline-end-style" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-border-end-width": {
    domProp: "MozBorderEndWidth",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-inline-end-width",
    subproperties: [ "border-inline-end-width" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-border-start": {
    domProp: "MozBorderStart",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "border-inline-start",
    subproperties: [ "-moz-border-start-color", "-moz-border-start-style", "-moz-border-start-width" ],
  },
  "-moz-border-start-color": {
    domProp: "MozBorderStartColor",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-inline-start-color",
    subproperties: [ "border-inline-start-color" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-border-start-style": {
    domProp: "MozBorderStartStyle",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-inline-start-style",
    subproperties: [ "border-inline-start-style" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-border-start-width": {
    domProp: "MozBorderStartWidth",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-inline-start-width",
    subproperties: [ "border-inline-start-width" ],
    get_computed: logical_box_prop_get_computed,
  },
  "inline-size": {
    domProp: "inlineSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    axis: true,
    get_computed: logical_axis_prop_get_computed,
    /* XXX testing auto has prerequisites */
    initial_values: [ "auto" ],
    prerequisites: { "display": "block" },
    other_values: [ "15px", "3em", "15%",
      // these three keywords compute to the initial value only when the
      // writing mode is vertical, and we're testing with a horizontal
      // writing mode
      "-moz-max-content", "-moz-min-content", "-moz-fit-content",
      // whether -moz-available computes to the initial value depends on
      // the container size, and for the container size we're testing
      // with, it does
      // "-moz-available",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "none" ],
  },
  "margin-block-end": {
    domProp: "marginBlockEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* XXX testing auto has prerequisites */
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "..25px", ".+5px", ".px", "-.px", "++5px", "-+4px", "+-3px", "--7px", "+-.6px", "-+.5px", "++.7px", "--.4px" ],
  },
  "margin-block-start": {
    domProp: "marginBlockStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* XXX testing auto has prerequisites */
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "..25px", ".+5px", ".px", "-.px", "++5px", "-+4px", "+-3px", "--7px", "+-.6px", "-+.5px", "++.7px", "--.4px" ],
  },
  "-moz-margin-end": {
    domProp: "MozMarginEnd",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "margin-inline-end",
    subproperties: [ "margin-inline-end" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-margin-start": {
    domProp: "MozMarginStart",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "margin-inline-start",
    subproperties: [ "margin-inline-start" ],
    get_computed: logical_box_prop_get_computed,
  },
  "max-block-size": {
    domProp: "maxBlockSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    axis: true,
    get_computed: logical_axis_prop_get_computed,
    prerequisites: { "display": "block" },
    initial_values: [ "none" ],
    other_values: [ "30px", "50%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "auto", "5", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available" ]
  },
  "max-inline-size": {
    domProp: "maxInlineSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    axis: true,
    get_computed: logical_axis_prop_get_computed,
    prerequisites: { "display": "block" },
    initial_values: [ "none" ],
    other_values: [ "30px", "50%",
      // these four keywords compute to the initial value only when the
      // writing mode is vertical, and we're testing with a horizontal
      // writing mode
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "auto", "5" ]
  },
  "min-block-size": {
    domProp: "minBlockSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    axis: true,
    get_computed: logical_axis_prop_get_computed,
    prerequisites: { "display": "block" },
    initial_values: [ "auto", "0", "calc(0em)", "calc(-2px)" ],
    other_values: [ "30px", "50%",
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "none", "5", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available" ]
  },
  "min-inline-size": {
    domProp: "minInlineSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    axis: true,
    get_computed: logical_axis_prop_get_computed,
    prerequisites: { "display": "block" },
    initial_values: [ "auto", "0", "calc(0em)", "calc(-2px)" ],
    other_values: [ "30px", "50%",
      // these four keywords compute to the initial value only when the
      // writing mode is vertical, and we're testing with a horizontal
      // writing mode
      "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
      "calc(-1%)",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ "none", "5" ]
  },
  "offset-block-end": {
    domProp: "offsetBlockEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: []
  },
  "offset-block-start": {
    domProp: "offsetBlockStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: []
  },
  "offset-inline-end": {
    domProp: "offsetInlineEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: []
  },
  "offset-inline-start": {
    domProp: "offsetInlineStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    /* FIXME: run tests with multiple prerequisites */
    prerequisites: { "position": "relative" },
    /* XXX 0 may or may not be equal to auto */
    initial_values: [ "auto" ],
    other_values: [ "32px", "-3em", "12%",
      "calc(2px)",
      "calc(-2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: []
  },
  "padding-block-end": {
    domProp: "paddingBlockEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
  },
  "padding-block-start": {
    domProp: "paddingBlockStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    logical: true,
    get_computed: logical_box_prop_get_computed,
    initial_values: [ "0", "0px", "0%", "calc(0pt)", "calc(0% + 0px)", "calc(-3px)", "calc(-1%)" ],
    other_values: [ "1px", "2em", "5%",
      "calc(2px)",
      "calc(50%)",
      "calc(3*25px)",
      "calc(25px*3)",
      "calc(3*25px + 50%)",
    ],
    invalid_values: [ ],
  },
  "-moz-padding-end": {
    domProp: "MozPaddingEnd",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "padding-inline-end",
    subproperties: [ "padding-inline-end" ],
    get_computed: logical_box_prop_get_computed,
  },
  "-moz-padding-start": {
    domProp: "MozPaddingStart",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "padding-inline-start",
    subproperties: [ "padding-inline-start" ],
    get_computed: logical_box_prop_get_computed,
  },
} // end of gCSSProperties

function logical_axis_prop_get_computed(cs, property)
{
  // Use defaults for these two properties in case the vertical text
  // pref (which they live behind) is turned off.
  var writingMode = cs.getPropertyValue("writing-mode") || "horizontal-tb";
  var orientation = writingMode.substring(0, writingMode.indexOf("-"));

  var mappings = {
    "block-size":      { horizontal: "height",
                         vertical:   "width",
                         sideways:   "width"      },
    "inline-size":     { horizontal: "width",
                         vertical:   "height",
                         sideways:   "height"     },
    "max-block-size":  { horizontal: "max-height",
                         vertical:   "max-width",
                         sideways:   "max-width"  },
    "max-inline-size": { horizontal: "max-width",
                         vertical:   "max-height",
                         sideways:   "max-height" },
    "min-block-size":  { horizontal: "min-height",
                         vertical:   "min-width",
                         sideways:   "min-width"  },
    "min-inline-size": { horizontal: "min-width",
                         vertical:   "min-height",
                         sideways:   "min-height" },
  };

  if (!mappings[property]) {
    throw "unexpected property " + property;
  }

  var prop = mappings[property][orientation];
  if (!prop) {
    throw "unexpected writing mode " + writingMode;
  }

  return cs.getPropertyValue(prop);
}

function logical_box_prop_get_computed(cs, property)
{
  // http://dev.w3.org/csswg/css-writing-modes-3/#logical-to-physical

  // Use default for writing-mode in case the vertical text
  // pref (which it lives behind) is turned off.
  var writingMode = cs.getPropertyValue("writing-mode") || "horizontal-tb";

  var direction = cs.getPropertyValue("direction");

  // keys in blockMappings are writing-mode values
  var blockMappings = {
    "horizontal-tb": { "start": "top",   "end": "bottom" },
    "vertical-rl":   { "start": "right", "end": "left"   },
    "vertical-lr":   { "start": "left",  "end": "right"  },
    "sideways-rl":   { "start": "right", "end": "left"   },
    "sideways-lr":   { "start": "left",  "end": "right"  },
  };

  // keys in inlineMappings are regular expressions that match against
  // a {writing-mode,direction} pair as a space-separated string
  var inlineMappings = {
    "horizontal-tb ltr": { "start": "left",   "end": "right"  },
    "horizontal-tb rtl": { "start": "right",  "end": "left"   },
    "vertical-.. ltr":   { "start": "bottom", "end": "top"    },
    "vertical-.. rtl":   { "start": "top",    "end": "bottom" },
    "vertical-.. ltr":   { "start": "top",    "end": "bottom" },
    "vertical-.. rtl":   { "start": "bottom", "end": "top"    },
    "sideways-lr ltr":   { "start": "bottom", "end": "top"    },
    "sideways-lr rtl":   { "start": "top",    "end": "bottom" },
    "sideways-rl ltr":   { "start": "top",    "end": "bottom" },
    "sideways-rl rtl":   { "start": "bottom", "end": "top"    },
  };

  var blockMapping = blockMappings[writingMode];
  var inlineMapping;

  // test each regular expression in inlineMappings against the
  // {writing-mode,direction} pair
  var key = `${writingMode} ${direction}`;
  for (var k in inlineMappings) {
    if (new RegExp(k).test(key)) {
      inlineMapping = inlineMappings[k];
      break;
    }
  }

  if (!blockMapping || !inlineMapping) {
    throw "Unexpected writing mode property values";
  }

  function physicalize(aProperty, aMapping, aLogicalPrefix) {
    for (var logicalSide in aMapping) {
      var physicalSide = aMapping[logicalSide];
      logicalSide = aLogicalPrefix + logicalSide;
      aProperty = aProperty.replace(logicalSide, physicalSide);
    }
    return aProperty;
  }

  if (/^-moz-/.test(property)) {
    property = physicalize(property.substring(5), inlineMapping, "");
  } else if (/^offset-(block|inline)-(start|end)/.test(property)) {
    property = property.substring(7);  // we want "top" not "offset-top", e.g.
    property = physicalize(property, blockMapping, "block-");
    property = physicalize(property, inlineMapping, "inline-");
  } else if (/-(block|inline)-(start|end)/.test(property)) {
    property = physicalize(property, blockMapping, "block-");
    property = physicalize(property, inlineMapping, "inline-");
  } else {
    throw "Unexpected property";
  }
  return cs.getPropertyValue(property);
}

// Get the computed value for a property.  For shorthands, return the
// computed values of all the subproperties, delimited by " ; ".
function get_computed_value(cs, property)
{
  var info = gCSSProperties[property];
  if (info.type == CSS_TYPE_TRUE_SHORTHAND ||
      (info.type == CSS_TYPE_SHORTHAND_AND_LONGHAND &&
        (property == "text-decoration" || property == "mask"))) {
    var results = [];
    for (var idx in info.subproperties) {
      var subprop = info.subproperties[idx];
      results.push(get_computed_value(cs, subprop));
    }
    return results.join(" ; ");
  }
  if (info.get_computed)
    return info.get_computed(cs, property);
  return cs.getPropertyValue(property);
}

if (IsCSSPropertyPrefEnabled("layout.css.touch_action.enabled")) {
    gCSSProperties["touch-action"] = {
        domProp: "touchAction",
        inherited: false,
        type: CSS_TYPE_LONGHAND,
        initial_values: ["auto"],
        other_values: ["none", "pan-x", "pan-y", "pan-x pan-y", "pan-y pan-x", "manipulation"],
        invalid_values: ["zoom", "pinch", "tap", "10px", "2", "auto pan-x", "pan-x auto", "none pan-x", "pan-x none",
                 "auto pan-y", "pan-y auto", "none pan-y", "pan-y none", "pan-x pan-x", "pan-y pan-y",
                 "pan-x pan-y none", "pan-x none pan-y", "none pan-x pan-y", "pan-y pan-x none", "pan-y none pan-x", "none pan-y pan-x",
                 "pan-x pan-y auto", "pan-x auto pan-y", "auto pan-x pan-y", "pan-y pan-x auto", "pan-y auto pan-x", "auto pan-y pan-x",
                 "pan-x pan-y zoom", "pan-x zoom pan-y", "zoom pan-x pan-y", "pan-y pan-x zoom", "pan-y zoom pan-x", "zoom pan-y pan-x",
                 "pan-x pan-y pan-x", "pan-x pan-x pan-y", "pan-y pan-x pan-x", "pan-y pan-x pan-y", "pan-y pan-y pan-x", "pan-x pan-y pan-y",
                 "manipulation none", "none manipulation", "manipulation auto", "auto manipulation", "manipulation zoom", "zoom manipulation",
                 "manipulation manipulation", "manipulation pan-x", "pan-x manipulation", "manipulation pan-y", "pan-y manipulation",
                 "manipulation pan-x pan-y", "pan-x manipulation pan-y", "pan-x pan-y manipulation",
                 "manipulation pan-y pan-x", "pan-y manipulation pan-x", "pan-y pan-x manipulation"]
    };
}

if (IsCSSPropertyPrefEnabled("layout.css.text-combine-upright.enabled")) {
  gCSSProperties["text-combine-upright"] = {
    domProp: "textCombineUpright",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "all" ],
    invalid_values: [ "auto", "all 2", "none all", "digits -3", "digits 0",
                      "digits 12", "none 3", "digits 3.1415", "digits3", "digits 1",
                      "digits 3 all", "digits foo", "digits all", "digits 3.0" ]
  };
  if (IsCSSPropertyPrefEnabled("layout.css.text-combine-upright-digits.enabled")) {
    gCSSProperties["text-combine-upright"].other_values.push(
      "digits", "digits 2", "digits 3", "digits 4", "digits     3");
  }
}

if (IsCSSPropertyPrefEnabled("svg.paint-order.enabled")) {
  gCSSProperties["paint-order"] = {
    domProp: "paintOrder",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "fill", "fill stroke", "fill stroke markers", "stroke markers fill" ],
    invalid_values: [ "fill stroke markers fill", "fill normal" ]
  };
}

if (IsCSSPropertyPrefEnabled("svg.transform-box.enabled")) {
  gCSSProperties["transform-box"] = {
    domProp: "transformBox",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "border-box" ],
    other_values: [ "fill-box", "view-box" ],
    invalid_values: []
  };
}

var basicShapeOtherValues = [
  "polygon(20px 20px)",
  "polygon(20px 20%)",
  "polygon(20% 20%)",
  "polygon(20rem 20em)",
  "polygon(20cm 20mm)",
  "polygon(20px 20px, 30px 30px)",
  "polygon(20px 20px, 30% 30%, 30px 30px)",
  "polygon(nonzero, 20px 20px, 30% 30%, 30px 30px)",
  "polygon(evenodd, 20px 20px, 30% 30%, 30px 30px)",

  "content-box",
  "padding-box",
  "border-box",
  "margin-box",

  "polygon(0 0) content-box",
  "border-box polygon(0 0)",
  "padding-box    polygon(   0  20px ,  30px    20% )  ",
  "polygon(evenodd, 20% 20em) content-box",
  "polygon(evenodd, 20vh 20em) padding-box",
  "polygon(evenodd, 20vh calc(20% + 20em)) border-box",
  "polygon(evenodd, 20vh 20vw) margin-box",

  "circle()",
  "circle(at center)",
  "circle(at top left 20px)",
  "circle(at bottom right)",
  "circle(20%)",
  "circle(300px)",
  "circle(calc(20px + 30px))",
  "circle(farthest-side)",
  "circle(closest-side)",
  "circle(closest-side at center)",
  "circle(farthest-side at top)",
  "circle(20px at top right)",
  "circle(40% at 50% 100%)",
  "circle(calc(20% + 20%) at right bottom)",
  "circle() padding-box",

  "ellipse()",
  "ellipse(at center)",
  "ellipse(at top left 20px)",
  "ellipse(at bottom right)",
  "ellipse(20% 20%)",
  "ellipse(300px 50%)",
  "ellipse(calc(20px + 30px) 10%)",
  "ellipse(farthest-side closest-side)",
  "ellipse(closest-side farthest-side)",
  "ellipse(farthest-side farthest-side)",
  "ellipse(closest-side closest-side)",
  "ellipse(closest-side closest-side at center)",
  "ellipse(20% farthest-side at top)",
  "ellipse(20px 50% at top right)",
  "ellipse(closest-side 40% at 50% 100%)",
  "ellipse(calc(20% + 20%) calc(20px + 20cm) at right bottom)",

  "inset(1px)",
  "inset(20% -20px)",
  "inset(20em 4rem calc(20% + 20px))",
  "inset(20vh 20vw 20pt 3%)",
  "inset(5px round 3px)",
  "inset(1px 2px round 3px / 3px)",
  "inset(1px 2px 3px round 3px 2em / 20%)",
  "inset(1px 2px 3px 4px round 3px 2vw 20% / 20px 3em 2vh 20%)",
];

var basicShapeInvalidValues = [
  "url(#test) url(#tes2)",
  "polygon (0 0)",
  "polygon(20px, 40px)",
  "border-box content-box",
  "polygon(0 0) polygon(0 0)",
  "polygon(nonzero 0 0)",
  "polygon(evenodd 20px 20px)",
  "polygon(20px 20px, evenodd)",
  "polygon(20px 20px, nonzero)",
  "polygon(0 0) conten-box content-box",
  "content-box polygon(0 0) conten-box",
  "padding-box polygon(0 0) conten-box",
  "polygon(0 0) polygon(0 0) content-box",
  "polygon(0 0) content-box polygon(0 0)",
  "polygon(0 0), content-box",
  "polygon(0 0), polygon(0 0)",
  "content-box polygon(0 0) polygon(0 0)",
  "content-box polygon(0 0) none",
  "none content-box polygon(0 0)",
  "inherit content-box polygon(0 0)",
  "initial polygon(0 0)",
  "polygon(0 0) farthest-side",
  "farthest-corner polygon(0 0)",
  "polygon(0 0) farthest-corner",
  "polygon(0 0) conten-box",
  "polygon(0 0) polygon(0 0) farthest-corner",
  "polygon(0 0) polygon(0 0) polygon(0 0)",
  "border-box polygon(0, 0)",
  "border-box padding-box",
  "margin-box farthest-side",
  "nonsense() border-box",
  "border-box nonsense()",

  "circle(at)",
  "circle(at 20% 20% 30%)",
  "circle(20px 2px at center)",
  "circle(2at center)",
  "circle(closest-corner)",
  "circle(at center top closest-side)",
  "circle(-20px)",
  "circle(farthest-side closest-side)",
  "circle(20% 20%)",
  "circle(at farthest-side)",
  "circle(calc(20px + rubbish))",

  "ellipse(at)",
  "ellipse(at 20% 20% 30%)",
  "ellipse(20px at center)",
  "ellipse(-20px 20px)",
  "ellipse(closest-corner farthest-corner)",
  "ellipse(20px -20px)",
  "ellipse(-20px -20px)",
  "ellipse(farthest-side)",
  "ellipse(20%)",
  "ellipse(at farthest-side farthest-side)",
  "ellipse(at top left calc(20px + rubbish))",

  "polygon(at)",
  "polygon(at 20% 20% 30%)",
  "polygon(20px at center)",
  "polygon(2px 2at center)",
  "polygon(closest-corner farthest-corner)",
  "polygon(at center top closest-side closest-side)",
  "polygon(40% at 50% 100%)",
  "polygon(40% farthest-side 20px at 50% 100%)",

  "inset()",
  "inset(round)",
  "inset(round 3px)",
  "inset(1px round 1px 2px 3px 4px 5px)",
  "inset(1px 2px 3px 4px 5px)",
  "inset(1px, round 3px)",
  "inset(1px, 2px)",
  "inset(1px 2px, 3px)",
  "inset(1px at 3px)",
  "inset(1px round 1px // 2px)",
  "inset(1px round)",
  "inset(1px calc(2px + rubbish))",
  "inset(1px round 2px calc(3px + rubbish))",
];

var basicShapeUnbalancedValues = [
  "polygon(30% 30%",
  "polygon(nonzero, 20% 20px",
  "polygon(evenodd, 20px 20px",

  "circle(",
  "circle(40% at 50% 100%",
  "ellipse(",
  "ellipse(40% at 50% 100%",

  "inset(1px",
  "inset(1px 2px",
  "inset(1px 2px 3px",
  "inset(1px 2px 3px 4px",
  "inset(1px 2px 3px 4px round 5px",
  "inset(1px 2px 3px 4px round 5px / 6px",
];

if (IsCSSPropertyPrefEnabled("layout.css.clip-path-shapes.enabled")) {
  gCSSProperties["clip-path"] = {
    domProp: "clipPath",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
      // SVG reference clip-path
      "url(#my-clip-path)",

      "fill-box",
      "stroke-box",
      "view-box",

      "polygon(evenodd, 20pt 20cm) fill-box",
      "polygon(evenodd, 20ex 20pc) stroke-box",
      "polygon(evenodd, 20rem 20in) view-box",
    ].concat(basicShapeOtherValues),
    invalid_values: basicShapeInvalidValues,
    unbalanced_values: basicShapeUnbalancedValues,
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.shape-outside.enabled")) {
  gCSSProperties["shape-outside"] = {
    domProp: "shapeOutside",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
      "url(#my-shape-outside)",
    ].concat(basicShapeOtherValues),
    invalid_values: basicShapeInvalidValues,
    unbalanced_values: basicShapeUnbalancedValues,
  };
}


if (IsCSSPropertyPrefEnabled("layout.css.filters.enabled")) {
  gCSSProperties["filter"] = {
    domProp: "filter",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
      // SVG reference filters
      "url(#my-filter)",
      "url(#my-filter-1) url(#my-filter-2)",

      // Filter functions
      "opacity(50%) saturate(1.0)",
      "invert(50%) sepia(0.1) brightness(90%)",

      // Mixed SVG reference filters and filter functions
      "grayscale(1) url(#my-filter-1)",
      "url(#my-filter-1) brightness(50%) contrast(0.9)",

      // Bad URLs
      "url('badscheme:badurl')",
      "blur(3px) url('badscheme:badurl') grayscale(50%)",

      "blur(0)",
      "blur(0px)",
      "blur(0.5px)",
      "blur(3px)",
      "blur(100px)",
      "blur(0.1em)",
      "blur(calc(-1px))", // Parses and becomes blur(0px).
      "blur(calc(0px))",
      "blur(calc(5px))",
      "blur(calc(2 * 5px))",

      "brightness(0)",
      "brightness(50%)",
      "brightness(1)",
      "brightness(1.0)",
      "brightness(2)",
      "brightness(350%)",
      "brightness(4.567)",

      "contrast(0)",
      "contrast(50%)",
      "contrast(1)",
      "contrast(1.0)",
      "contrast(2)",
      "contrast(350%)",
      "contrast(4.567)",

      "drop-shadow(2px 2px)",
      "drop-shadow(2px 2px 1px)",
      "drop-shadow(2px 2px green)",
      "drop-shadow(2px 2px 1px green)",
      "drop-shadow(green 2px 2px)",
      "drop-shadow(green 2px 2px 1px)",
      "drop-shadow(currentColor 3px 3px)",
      "drop-shadow(2px 2px calc(-5px))", /* clamped */
      "drop-shadow(calc(3em - 2px) 2px green)",
      "drop-shadow(green calc(3em - 2px) 2px)",
      "drop-shadow(2px calc(2px + 0.2em))",
      "drop-shadow(blue 2px calc(2px + 0.2em))",
      "drop-shadow(2px calc(2px + 0.2em) blue)",
      "drop-shadow(calc(-2px) calc(-2px))",
      "drop-shadow(-2px -2px)",
      "drop-shadow(calc(2px) calc(2px))",
      "drop-shadow(calc(2px) calc(2px) calc(2px))",

      "grayscale(0)",
      "grayscale(50%)",
      "grayscale(1)",
      "grayscale(1.0)",
      "grayscale(2)",
      "grayscale(350%)",
      "grayscale(4.567)",

      "hue-rotate(0deg)",
      "hue-rotate(90deg)",
      "hue-rotate(540deg)",
      "hue-rotate(-90deg)",
      "hue-rotate(10grad)",
      "hue-rotate(1.6rad)",
      "hue-rotate(-1.6rad)",
      "hue-rotate(0.5turn)",
      "hue-rotate(-2turn)",

      "invert(0)",
      "invert(50%)",
      "invert(1)",
      "invert(1.0)",
      "invert(2)",
      "invert(350%)",
      "invert(4.567)",

      "opacity(0)",
      "opacity(50%)",
      "opacity(1)",
      "opacity(1.0)",
      "opacity(2)",
      "opacity(350%)",
      "opacity(4.567)",

      "saturate(0)",
      "saturate(50%)",
      "saturate(1)",
      "saturate(1.0)",
      "saturate(2)",
      "saturate(350%)",
      "saturate(4.567)",

      "sepia(0)",
      "sepia(50%)",
      "sepia(1)",
      "sepia(1.0)",
      "sepia(2)",
      "sepia(350%)",
      "sepia(4.567)",
    ],
    invalid_values: [
      // none
      "none none",
      "url(#my-filter) none",
      "none url(#my-filter)",
      "blur(2px) none url(#my-filter)",

      // Nested filters
      "grayscale(invert(1.0))",

      // Comma delimited filters
      "url(#my-filter),",
      "invert(50%), url(#my-filter), brightness(90%)",

      // Test the following situations for each filter function:
      // - Invalid number of arguments
      // - Comma delimited arguments
      // - Wrong argument type
      // - Argument value out of range
      "blur()",
      "blur(3px 5px)",
      "blur(3px,)",
      "blur(3px, 5px)",
      "blur(#my-filter)",
      "blur(0.5)",
      "blur(50%)",
      "blur(calc(0))", // Unitless zero in calc is not a valid length.
      "blur(calc(0.1))",
      "blur(calc(10%))",
      "blur(calc(20px - 5%))",
      "blur(-3px)",

      "brightness()",
      "brightness(0.5 0.5)",
      "brightness(0.5,)",
      "brightness(0.5, 0.5)",
      "brightness(#my-filter)",
      "brightness(10px)",
      "brightness(-1)",

      "contrast()",
      "contrast(0.5 0.5)",
      "contrast(0.5,)",
      "contrast(0.5, 0.5)",
      "contrast(#my-filter)",
      "contrast(10px)",
      "contrast(-1)",

      "drop-shadow()",
      "drop-shadow(3% 3%)",
      "drop-shadow(2px 2px -5px)",
      "drop-shadow(2px 2px 2px 2px)",
      "drop-shadow(2px 2px, none)",
      "drop-shadow(none, 2px 2px)",
      "drop-shadow(inherit, 2px 2px)",
      "drop-shadow(2px 2px, inherit)",
      "drop-shadow(2 2px)",
      "drop-shadow(2px 2)",
      "drop-shadow(2px 2px 2)",
      "drop-shadow(2px 2px 2px 2)",
      "drop-shadow(calc(2px) calc(2px) calc(2px) calc(2px))",
      "drop-shadow(green 2px 2px, blue 1px 3px 4px)",
      "drop-shadow(blue 2px 2px, currentColor 1px 2px)",

      "grayscale()",
      "grayscale(0.5 0.5)",
      "grayscale(0.5,)",
      "grayscale(0.5, 0.5)",
      "grayscale(#my-filter)",
      "grayscale(10px)",
      "grayscale(-1)",

      "hue-rotate()",
      "hue-rotate(0)",
      "hue-rotate(0.5 0.5)",
      "hue-rotate(0.5,)",
      "hue-rotate(0.5, 0.5)",
      "hue-rotate(#my-filter)",
      "hue-rotate(10px)",
      "hue-rotate(-1)",
      "hue-rotate(45deg,)",

      "invert()",
      "invert(0.5 0.5)",
      "invert(0.5,)",
      "invert(0.5, 0.5)",
      "invert(#my-filter)",
      "invert(10px)",
      "invert(-1)",

      "opacity()",
      "opacity(0.5 0.5)",
      "opacity(0.5,)",
      "opacity(0.5, 0.5)",
      "opacity(#my-filter)",
      "opacity(10px)",
      "opacity(-1)",

      "saturate()",
      "saturate(0.5 0.5)",
      "saturate(0.5,)",
      "saturate(0.5, 0.5)",
      "saturate(#my-filter)",
      "saturate(10px)",
      "saturate(-1)",

      "sepia()",
      "sepia(0.5 0.5)",
      "sepia(0.5,)",
      "sepia(0.5, 0.5)",
      "sepia(#my-filter)",
      "sepia(10px)",
      "sepia(-1)",
    ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.grid.enabled")) {
  var isGridTemplateSubgridValueEnabled =
    IsCSSPropertyPrefEnabled("layout.css.grid-template-subgrid-value.enabled");

  gCSSProperties["display"].other_values.push("grid", "inline-grid");
  gCSSProperties["grid-auto-flow"] = {
    domProp: "gridAutoFlow",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "row" ],
    other_values: [
      "column",
      "column dense",
      "row dense",
      "dense column",
      "dense row",
      "dense",
    ],
    invalid_values: [
      "",
      "auto",
      "none",
      "10px",
      "column row",
      "dense row dense",
    ]
  };

  gCSSProperties["grid-auto-columns"] = {
    domProp: "gridAutoColumns",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [
      "40px",
      "2em",
      "2.5fr",
      "12%",
      "min-content",
      "max-content",
      "calc(2px - 99%)",
      "minmax(20px, max-content)",
      "minmax(min-content, auto)",
      "minmax(auto, max-content)",
      "m\\69nmax(20px, 4Fr)",
      "MinMax(min-content, calc(20px + 10%))",
      "fit-content(1px)",
      "fit-content(calc(1px - 99%))",
      "fit-content(10%)",
    ],
    invalid_values: [
      "",
      "normal",
      "40ms",
      "-40px",
      "-12%",
      "-2em",
      "-2.5fr",
      "minmax()",
      "minmax(20px)",
      "mİnmax(20px, 100px)",
      "minmax(20px, 100px, 200px)",
      "maxmin(100px, 20px)",
      "minmax(min-content, minmax(30px, max-content))",
      "fit-content(-1px)",
      "fit-content(auto)",
      "fit-content(min-content)",
    ]
  };
  gCSSProperties["grid-auto-rows"] = {
    domProp: "gridAutoRows",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: gCSSProperties["grid-auto-columns"].initial_values,
    other_values: gCSSProperties["grid-auto-columns"].other_values,
    invalid_values: gCSSProperties["grid-auto-columns"].invalid_values
  };

  gCSSProperties["grid-template-columns"] = {
    domProp: "gridTemplateColumns",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
      "auto",
      "40px",
      "2.5fr",
      "[normal] 40px [] auto [ ] 12%",
      "[foo] 40px min-content [ bar ] calc(2px - 99%) max-content",
      "40px min-content calc(20px + 10%) max-content",
      "minmax(min-content, auto)",
      "minmax(auto, max-content)",
      "m\\69nmax(20px, 4Fr)",
      "40px MinMax(min-content, calc(20px + 10%)) max-content",
      "40px 2em",
      "[] 40px [-foo] 2em [bar baz This\ is\ one\ ident]",
      // TODO bug 978478: "[a] repeat(3, [b] 20px [c] 40px [d]) [e]",
      "repeat(1, 20px)",
      "repeat(1, [a] 20px)",
      "[a] Repeat(4, [a] 20px [] auto [b c]) [d]",
      "[a] 2.5fr Repeat(4, [a] 20px [] auto [b c]) [d]",
      "[a] 2.5fr [z] Repeat(4, [a] 20px [] auto [b c]) [d]",
      "[a] 2.5fr [z] Repeat(4, [a] 20px [] auto) [d]",
      "[a] 2.5fr [z] Repeat(4, 20px [b c] auto [b c]) [d]",
      "[a] 2.5fr [z] Repeat(4, 20px auto) [d]",
      "repeat(auto-fill, 0)",
      "[a] repeat( Auto-fill,1%)",
      "minmax(auto,0) [a] repeat(Auto-fit, 0) minmax(0,auto)",
      "minmax(calc(1% + 1px),auto) repeat(Auto-fit,[] 1%) minmax(auto,1%)",
      "[a] repeat( auto-fit,[a b] minmax(0,0) )",
      "[a] 40px repeat(auto-fit,[a b] minmax(1px, 0) [])",
      "[a] calc(1px - 99%) [b] repeat(auto-fit,[a b] minmax(1mm, 1%) [c]) [c]",
      "repeat(auto-fill,minmax(1%,auto))",
      "repeat(auto-fill,minmax(1em,min-content)) minmax(min-content,0)",
      "repeat(auto-fill,minmax(max-content,1mm))",
      "fit-content(1px) 1fr",
      "[a] fit-content(calc(1px - 99%)) [b]",
      "[a] fit-content(10%) [b c] fit-content(1em)",
    ],
    invalid_values: [
      "",
      "normal",
      "40ms",
      "-40px",
      "-12%",
      "-2fr",
      "[foo]",
      "[inherit] 40px",
      "[initial] 40px",
      "[unset] 40px",
      "[default] 40px",
      "[span] 40px",
      "[6%] 40px",
      "[5th] 40px",
      "[foo[] bar] 40px",
      "[foo]] 40px",
      "(foo) 40px",
      "[foo] [bar] 40px",
      "40px [foo] [bar]",
      "minmax()",
      "minmax(20px)",
      "mİnmax(20px, 100px)",
      "minmax(20px, 100px, 200px)",
      "maxmin(100px, 20px)",
      "minmax(min-content, minmax(30px, max-content))",
      "repeat(0, 20px)",
      "repeat(-3, 20px)",
      "rêpeat(1, 20px)",
      "repeat(1)",
      "repeat(1, )",
      "repeat(3px, 20px)",
      "repeat(2.0, 20px)",
      "repeat(2.5, 20px)",
      "repeat(2, (foo))",
      "repeat(2, foo)",
      "40px calc(0px + rubbish)",
      "repeat(1, repeat(1, 20px))",
      "repeat(auto-fill, auto)",
      "repeat(auto-fit,auto)",
      "repeat(auto-fit,[])",
      "repeat(auto-fill, 0) repeat(auto-fit, 0) ",
      "repeat(auto-fit, 0) repeat(auto-fill, 0) ",
      "[a] repeat(auto-fit, 0) repeat(auto-fit, 0) ",
      "[a] repeat(auto-fill, 0) [a] repeat(auto-fill, 0) ",
      "repeat(auto-fill, 0 0)",
      "repeat(auto-fill, 0 [] 0)",
      "repeat(auto-fill, min-content)",
      "repeat(auto-fit,max-content)",
      "repeat(auto-fit,1fr)",
      "repeat(auto-fit,minmax(auto,auto))",
      "repeat(auto-fit,minmax(min-content,1fr))",
      "repeat(auto-fit,minmax(1fr,auto))",
      "repeat(auto-fill,minmax(1fr,1em))",
      "repeat(auto-fill, 10px) auto",
      "auto repeat(auto-fit, 10px)",
      "minmax(min-content,max-content) repeat(auto-fit, 0)",
      "10px [a] 10px [b a] 1fr [b] repeat(auto-fill, 0)",
      "fit-content(-1px)",
      "fit-content(auto)",
      "fit-content(min-content)",
    ],
    unbalanced_values: [
      "(foo] 40px",
    ]
  };
  if (isGridTemplateSubgridValueEnabled) {
    gCSSProperties["grid-template-columns"].other_values.push(
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=981300
      "[none auto subgrid min-content max-content foo] 40px",

      "subgrid",
      "subgrid [] [foo bar]",
      "subgrid repeat(1, [])",
      "subgrid Repeat(4, [a] [b c] [] [d])",
      "subgrid repeat(auto-fill, [])",
      "subgrid [x] repeat( Auto-fill, [a b c]) []",
      "subgrid [x] repeat(auto-fill, []) [y z]"
    );
    gCSSProperties["grid-template-columns"].invalid_values.push(
      "subgrid [inherit]",
      "subgrid [initial]",
      "subgrid [unset]",
      "subgrid [default]",
      "subgrid [span]",
      "subgrid [foo] 40px",
      "subgrid [foo 40px]",
      "[foo] subgrid",
      "subgrid rêpeat(1, [])",
      "subgrid repeat(0, [])",
      "subgrid repeat(-3, [])",
      "subgrid repeat(2.0, [])",
      "subgrid repeat(2.5, [])",
      "subgrid repeat(3px, [])",
      "subgrid repeat(1)",
      "subgrid repeat(1, )",
      "subgrid repeat(2, [40px])",
      "subgrid repeat(2, foo)",
      "subgrid repeat(1, repeat(1, []))",
      "subgrid repeat(auto-fit,[])",
      "subgrid [] repeat(auto-fit,[])",
      "subgrid [a] repeat(auto-fit,[])",
      "subgrid repeat(auto-fill, 1px)",
      "subgrid repeat(auto-fill, 1px [])",
      "subgrid repeat(Auto-fill, [a] [b c] [] [d])",
      "subgrid repeat(auto-fill, []) repeat(auto-fill, [])"
    );
  }
  gCSSProperties["grid-template-rows"] = {
    domProp: "gridTemplateRows",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: gCSSProperties["grid-template-columns"].initial_values,
    other_values: gCSSProperties["grid-template-columns"].other_values,
    invalid_values: gCSSProperties["grid-template-columns"].invalid_values
  };
  gCSSProperties["grid-template-areas"] = {
    domProp: "gridTemplateAreas",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
      "''",
      "'' ''",
      "'1a-é_ .' \"b .\"",
      "' Z\t\\aZ' 'Z Z'",
      " '. . a b'  '. .a b' ",
      "'a.b' '. . .'",
      "'.' '..'",
      "'...' '.'",
      "'...-blah' '. .'",
      "'.. ..' '.. ...'",
    ],
    invalid_values: [
      "'a b' 'a/b'",
      "'a . a'",
      "'. a a' 'a a a'",
      "'a a .' 'a a a'",
      "'a a' 'a .'",
      "'a a'\n'..'\n'a a'",
    ]
  };

  gCSSProperties["grid-template"] = {
    domProp: "gridTemplate",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "grid-template-areas",
      "grid-template-rows",
      "grid-template-columns",
    ],
    initial_values: [
      "none",
      "none / none",
    ],
    other_values: [
      // <'grid-template-rows'> / <'grid-template-columns'>
      "40px / 100px",
      "[foo] 40px [bar] / [baz] repeat(auto-fill,100px) [fizz]",
      " none/100px",
      "40px/none",
      // [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <explicit-track-list> ]?
      "'fizz'",
      "[bar] 'fizz'",
      "'fizz' / [foo] 40px",
      "[bar] 'fizz' / [foo] 40px",
      "'fizz' 100px / [foo] 40px",
      "[bar] 'fizz' 100px / [foo] 40px",
      "[bar] 'fizz' 100px [buzz] / [foo] 40px",
      "[bar] 'fizz' 100px [buzz] \n [a] '.' 200px [b] / [foo] 40px",
    ],
    invalid_values: [
      "'fizz' / repeat(1, 100px)",
      "'fizz' repeat(1, 100px) / 0px",
      "[foo] [bar] 40px / 100px",
      "[fizz] [buzz] 100px / 40px",
      "[fizz] [buzz] 'foo' / 40px",
      "'foo' / none"
    ]
  };
  if (isGridTemplateSubgridValueEnabled) {
    gCSSProperties["grid-template"].other_values.push(
      "subgrid",
      "subgrid/40px 20px",
      "subgrid [foo] [] [bar baz] / 40px 20px",
      "40px 20px/subgrid",
      "40px 20px/subgrid  [foo] [] repeat(3, [a] [b]) [bar baz]",
      "subgrid/subgrid",
      "subgrid [foo] [] [bar baz]/subgrid [foo] [] [bar baz]"
    );
    gCSSProperties["grid-template"].invalid_values.push(
      "subgrid []",
      "subgrid [] / 'fizz'",
      "subgrid / 'fizz'"
    );
  }

  gCSSProperties["grid"] = {
    domProp: "grid",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "grid-template-areas",
      "grid-template-rows",
      "grid-template-columns",
      "grid-auto-flow",
      "grid-auto-rows",
      "grid-auto-columns",
      "grid-column-gap",
      "grid-row-gap",
    ],
    initial_values: [
      "none",
      "none / none",
    ],
    other_values: [
      "auto-flow 40px / none",
      "auto-flow / 40px",
      "auto-flow dense auto / auto",
      "dense auto-flow minmax(min-content, 2fr) / auto",
      "dense auto-flow / 100px",
      "none / auto-flow 40px",
      "40px / auto-flow",
      "none / dense auto-flow auto",
    ].concat(
      gCSSProperties["grid-template"].other_values
    ),
    invalid_values: [
      "auto-flow",
      " / auto-flow",
      "dense 0 / 0",
      "dense dense 40px / 0",
      "auto-flow / auto-flow",
      "auto-flow / dense",
      "auto-flow [a] 0 / 0",
      "0 / auto-flow [a] 0",
      "auto-flow -20px / 0",
      "auto-flow 200ms / 0",
      "auto-flow 40px 100px / 0",
    ].concat(
      gCSSProperties["grid-template"].invalid_values,
      gCSSProperties["grid-auto-flow"].other_values,
      gCSSProperties["grid-auto-flow"].invalid_values
        .filter((v) => v != 'none')
    )
  };

  var gridLineOtherValues = [
    "foo",
    "2",
    "2 foo",
    "foo 2",
    "-3",
    "-3 bar",
    "bar -3",
    "span 2",
    "2 span",
    "span foo",
    "foo span",
    "span 2 foo",
    "span foo 2",
    "2 foo span",
    "foo 2 span",
  ];
  var gridLineInvalidValues = [
    "",
    "4th",
    "span",
    "inherit 2",
    "2 inherit",
    "20px",
    "2 3",
    "2.5",
    "2.0",
    "0",
    "0 foo",
    "span 0",
    "2 foo 3",
    "foo 2 foo",
    "2 span foo",
    "foo span 2",
    "span -3",
    "span -3 bar",
    "span 2 span",
    "span foo span",
    "span 2 foo span",
  ];

  gCSSProperties["grid-column-start"] = {
    domProp: "gridColumnStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: gridLineOtherValues,
    invalid_values: gridLineInvalidValues
  };
  gCSSProperties["grid-column-end"] = {
    domProp: "gridColumnEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: gridLineOtherValues,
    invalid_values: gridLineInvalidValues
  };
  gCSSProperties["grid-row-start"] = {
    domProp: "gridRowStart",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: gridLineOtherValues,
    invalid_values: gridLineInvalidValues
  };
  gCSSProperties["grid-row-end"] = {
    domProp: "gridRowEnd",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: gridLineOtherValues,
    invalid_values: gridLineInvalidValues
  };

  // The grid-column and grid-row shorthands take values of the form
  //   <grid-line> [ / <grid-line> ]?
  var gridColumnRowOtherValues = [].concat(gridLineOtherValues);
  gridLineOtherValues.concat([ "auto" ]).forEach(function(val) {
    gridColumnRowOtherValues.push(" foo / " + val);
    gridColumnRowOtherValues.push(val + "/2");
  });
  var gridColumnRowInvalidValues = [
    "foo, bar",
    "foo / bar / baz",
  ].concat(gridLineInvalidValues);
  gridLineInvalidValues.forEach(function(val) {
    gridColumnRowInvalidValues.push("span 3 / " + val);
    gridColumnRowInvalidValues.push(val + " / foo");
  });
  gCSSProperties["grid-column"] = {
    domProp: "gridColumn",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "grid-column-start",
      "grid-column-end"
    ],
    initial_values: [ "auto", "auto / auto" ],
    other_values: gridColumnRowOtherValues,
    invalid_values: gridColumnRowInvalidValues
  };
  gCSSProperties["grid-row"] = {
    domProp: "gridRow",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "grid-row-start",
      "grid-row-end"
    ],
    initial_values: [ "auto", "auto / auto" ],
    other_values: gridColumnRowOtherValues,
    invalid_values: gridColumnRowInvalidValues
  };

  var gridAreaOtherValues = gridLineOtherValues.slice();
  gridLineOtherValues.forEach(function(val) {
    gridAreaOtherValues.push("foo / " + val);
    gridAreaOtherValues.push(val + "/2/3");
    gridAreaOtherValues.push("foo / bar / " + val + " / baz");
  });
  var gridAreaInvalidValues = [
    "foo, bar",
    "foo / bar / baz / fizz / buzz",
    "default / foo / bar / baz",
    "foo / initial / bar / baz",
    "foo / bar / inherit / baz",
    "foo / bar / baz / unset",
  ].concat(gridLineInvalidValues);
  gridLineInvalidValues.forEach(function(val) {
    gridAreaInvalidValues.push("foo / " + val);
    gridAreaInvalidValues.push("foo / bar / " + val);
    gridAreaInvalidValues.push("foo / 4 / bar / " + val);
  });

  gCSSProperties["grid-area"] = {
    domProp: "gridArea",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [
      "grid-row-start",
      "grid-column-start",
      "grid-row-end",
      "grid-column-end"
    ],
    initial_values: [
      "auto",
      "auto / auto",
      "auto / auto / auto",
      "auto / auto / auto / auto"
    ],
    other_values: gridAreaOtherValues,
    invalid_values: gridAreaInvalidValues
  };

  gCSSProperties["grid-column-gap"] = {
    domProp: "gridColumnGap",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0" ],
    other_values: [ "2px", "2%", "1em", "calc(1px + 1em)", "calc(1%)",
                    "calc(1% + 1ch)" , "calc(1px - 99%)" ],
    invalid_values: [ "-1px", "auto", "none", "1px 1px", "-1%", "fit-content(1px)" ],
  };
  gCSSProperties["grid-row-gap"] = {
    domProp: "gridRowGap",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0" ],
    other_values: [ "2px", "2%", "1em", "calc(1px + 1em)", "calc(1%)",
                    "calc(1% + 1ch)" , "calc(1px - 99%)" ],
    invalid_values: [ "-1px", "auto", "none", "1px 1px", "-1%", "min-content" ],
  };
  gCSSProperties["grid-gap"] = {
    domProp: "gridGap",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "grid-column-gap", "grid-row-gap" ],
    initial_values: [ "0", "0 0" ],
    other_values: [ "1ch 0", "1px 1%", "1em 1px", "calc(1px) calc(1%)" ],
    invalid_values: [ "-1px", "1px -1px", "1px 1px 1px", "inherit 1px",
                      "1px auto" ]
  };
}

gCSSProperties["display"].other_values.push("contents");

if (IsCSSPropertyPrefEnabled("layout.css.contain.enabled")) {
  gCSSProperties["contain"] = {
    domProp: "contain",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
      "strict",
      "layout",
      "style",
      "layout style",
      "style layout",
      "paint",
      "layout paint",
      "paint layout",
      "style paint",
      "paint style",
      "layout style paint",
      "layout paint style",
      "style paint layout",
      "paint style layout",
    ],
    invalid_values: [
      "none strict",
      "strict layout",
      "strict layout style",
      "layout strict",
      "layout style strict",
      "layout style paint strict",
      "paint strict",
      "style strict",
      "paint paint",
      "strict strict",
      "auto",
      "10px",
      "0",
    ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.image-orientation.enabled")) {
  gCSSProperties["image-orientation"] = {
    domProp: "imageOrientation",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [
      "0deg",
      "0grad",
      "0rad",
      "0turn",

      // Rounded initial values.
      "-90deg",
      "15deg",
      "360deg",
    ],
    other_values: [
      "0deg flip",
      "90deg",
      "90deg flip",
      "180deg",
      "180deg flip",
      "270deg",
      "270deg flip",
      "flip",
      "from-image",

      // Grad units.
      "0grad flip",
      "100grad",
      "100grad flip",
      "200grad",
      "200grad flip",
      "300grad",
      "300grad flip",

      // Radian units.
      "0rad flip",
      "1.57079633rad",
      "1.57079633rad flip",
      "3.14159265rad",
      "3.14159265rad flip",
      "4.71238898rad",
      "4.71238898rad flip",

      // Turn units.
      "0turn flip",
      "0.25turn",
      "0.25turn flip",
      "0.5turn",
      "0.5turn flip",
      "0.75turn",
      "0.75turn flip",

      // Rounded values.
      "-45deg flip",
      "65deg flip",
      "400deg flip",
    ],
    invalid_values: [
      "none",
      "0deg none",
      "flip 0deg",
      "flip 0deg",
      "0",
      "0 flip",
      "flip 0",
      "0deg from-image",
      "from-image 0deg",
      "flip from-image",
      "from-image flip",
    ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.initial-letter.enabled")) {
  gCSSProperties["initial-letter"] = {
    domProp: "initialLetter",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "2", "2.5", "3.7 2", "4 3" ],
    invalid_values: [ "-3", "3.7 -2", "25%", "16px", "1 0", "0", "0 1" ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.osx-font-smoothing.enabled")) {
  gCSSProperties["-moz-osx-font-smoothing"] = {
    domProp: "MozOsxFontSmoothing",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "grayscale" ],
    invalid_values: [ "none", "subpixel-antialiased", "antialiased" ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.mix-blend-mode.enabled")) {
  gCSSProperties["mix-blend-mode"] = {
    domProp: "mixBlendMode",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: ["multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn",
        "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"],
    invalid_values: []
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.isolation.enabled")) {
  gCSSProperties["isolation"] = {
    domProp: "isolation",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: ["isolate"],
    invalid_values: []
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.background-blend-mode.enabled")) {
  gCSSProperties["background-blend-mode"] = {
    domProp: "backgroundBlendMode",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "normal" ],
    other_values: [ "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn",
      "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity" ],
    invalid_values: ["none", "10px", "multiply multiply"]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.object-fit-and-position.enabled")) {
  gCSSProperties["object-fit"] = {
    domProp: "objectFit",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "fill" ],
    other_values: [ "contain", "cover", "none", "scale-down" ],
    invalid_values: [ "auto", "5px", "100%" ]
  };
  gCSSProperties["object-position"] = {
    domProp: "objectPosition",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "50% 50%", "50%", "center", "center center" ],
    other_values: [
      "calc(20px)",
      "calc(20px) 10px",
      "10px calc(20px)",
      "calc(20px) 25%",
      "25% calc(20px)",
      "calc(20px) calc(20px)",
      "calc(20px + 1em) calc(20px / 2)",
      "calc(20px + 50%) calc(50% - 10px)",
      "calc(-20px) calc(-50%)",
      "calc(-20%) calc(-50%)",
      "0px 0px",
      "right 20px top 60px",
      "right 20px bottom 60px",
      "left 20px top 60px",
      "left 20px bottom 60px",
      "right -50px top -50px",
      "left -50px bottom -50px",
      "right 20px top -50px",
      "right -20px top 50px",
      "right 3em bottom 10px",
      "bottom 3em right 10px",
      "top 3em right 10px",
      "left 15px",
      "10px top",
      "left top 15px",
      "left 10px top",
      "left 20%",
      "right 20%"
    ],
    invalid_values: [ "center 10px center 4px", "center 10px center",
                      "top 20%", "bottom 20%", "50% left", "top 50%",
                      "50% bottom 10%", "right 10% 50%", "left right",
                      "top bottom", "left 10% right",
                      "top 20px bottom 20px", "left left", "20 20" ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.overflow-clip-box.enabled")) {
  gCSSProperties["overflow-clip-box"] = {
    domProp: "overflowClipBox",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "padding-box" ],
    other_values: [ "content-box" ],
    invalid_values: [ "none", "auto", "border-box", "0" ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.box-decoration-break.enabled")) {
  gCSSProperties["box-decoration-break"] = {
    domProp: "boxDecorationBreak",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "slice" ],
    other_values: [ "clone" ],
    invalid_values: [ "auto",  "none",  "1px" ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.scroll-behavior.property-enabled")) {
  gCSSProperties["scroll-behavior"] = {
    domProp: "scrollBehavior",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto" ],
    other_values: [ "smooth" ],
    invalid_values: [ "none",  "1px" ]
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.scroll-snap.enabled")) {
  gCSSProperties["scroll-snap-coordinate"] = {
    domProp: "scrollSnapCoordinate",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "25% 25%", "top", "0px 100px, 10em 50%",
                    "top left, top right, bottom left, bottom right, center",
                    "calc(2px)",
                    "calc(50%)",
                    "calc(3*25px)",
                    "calc(3*25px) 5px",
                    "5px calc(3*25px)",
                    "calc(20%) calc(3*25px)",
                    "calc(25px*3)",
                    "calc(3*25px + 50%)",
                    "calc(20%) calc(3*25px), center"],
    invalid_values: [ "auto", "default" ]
  }
  gCSSProperties["scroll-snap-destination"] = {
    domProp: "scrollSnapDestination",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0px 0px" ],
    other_values: [ "25% 25%", "6px 5px", "20% 3em", "0in 1in",
                    "top", "right", "top left", "top right", "center",
                    "calc(2px)",
                    "calc(50%)",
                    "calc(3*25px)",
                    "calc(3*25px) 5px",
                    "5px calc(3*25px)",
                    "calc(20%) calc(3*25px)",
                    "calc(25px*3)",
                    "calc(3*25px + 50%)"],
    invalid_values: [ "auto", "none", "default" ]
  }
  gCSSProperties["scroll-snap-points-x"] = {
    domProp: "scrollSnapPointsX",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "repeat(100%)", "repeat(120px)", "repeat(calc(3*25px))" ],
    invalid_values: [ "auto", "1px", "left", "rgb(1,2,3)" ]
  }
  gCSSProperties["scroll-snap-points-y"] = {
    domProp: "scrollSnapPointsY",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "repeat(100%)", "repeat(120px)", "repeat(calc(3*25px))" ],
    invalid_values: [ "auto", "1px", "top", "rgb(1,2,3)" ]
  }
  gCSSProperties["scroll-snap-type"] = {
    domProp: "scrollSnapType",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    subproperties: [ "scroll-snap-type-x", "scroll-snap-type-y" ],
    initial_values: [ "none" ],
    other_values: [ "mandatory", "proximity" ],
    invalid_values: [ "auto",  "1px" ]
  };
  gCSSProperties["scroll-snap-type-x"] = {
    domProp: "scrollSnapTypeX",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: ["mandatory", "proximity"],
    invalid_values: [ "auto",  "1px" ]
  };
  gCSSProperties["scroll-snap-type-y"] = {
    domProp: "scrollSnapTypeY",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: ["mandatory", "proximity"],
    invalid_values: [ "auto",  "1px" ]
  };
}

function SupportsMaskShorthand() {
  return "maskImage" in document.documentElement.style;
}

if (SupportsMaskShorthand()) {
  gCSSProperties["mask"] = {
    domProp: "mask",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    /* FIXME: All mask-border-* should be added when we implement them. */
    subproperties: ["mask-clip", "mask-image", "mask-mode", "mask-origin", "mask-position-x", "mask-position-y", "mask-repeat", "mask-size" , "mask-composite"],
    initial_values: [ "match-source", "none", "repeat", "add", "0% 0%", "top left", "0% 0% / auto", "top left / auto", "left top / auto", "0% 0% / auto auto",
      "top left none", "left top none", "none left top", "none top left", "none 0% 0%", "top left / auto none", "left top / auto none",
      "top left / auto auto none",
      "match-source none repeat add top left", "top left repeat none add", "none repeat add top left / auto", "top left / auto repeat none add match-source", "none repeat add 0% 0% / auto auto match-source",
      "border-box", "border-box border-box" ],
    other_values: [
      "none alpha repeat add left top",
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)",
      "no-repeat url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==') alpha left top add",
      "repeat-x",
      "repeat-y",
      "no-repeat",
      "none repeat-y alpha add 0% 0%",
      "subtract",
      "0% top subtract alpha repeat none",
      "top",
      "left",
      "50% 50%",
      "center",
      "top / 100px",
      "left / contain",
      "left / cover",
      "10px / 10%",
      "10em / calc(20px)",
      "top left / 100px 100px",
      "top left / 100px auto",
      "top left / 100px 10%",
      "top left / 100px calc(20px)",
      "bottom right add none alpha repeat",
      "50% alpha",
      "alpha 50%",
      "50%",
      "url(#mymask)",
      "-moz-radial-gradient(10% bottom, #ffffff, black) add no-repeat",
      "-moz-linear-gradient(10px 10px -45deg, red, blue) repeat",
      "-moz-linear-gradient(10px 10px -0.125turn, red, blue) repeat",
      "-moz-repeating-radial-gradient(10% bottom, #ffffff, black) add no-repeat",
      "-moz-repeating-linear-gradient(10px 10px -45deg, red, blue) repeat",
      "-moz-element(#test) alpha",
      /* multiple mask-image */
      "url(404.png), url(404.png)",
      "repeat-x, subtract, none",
      "0% top url(404.png), url(404.png) 50% top",
      "subtract repeat-y top left url(404.png), repeat-x alpha",
      "url(404.png), -moz-linear-gradient(20px 20px -45deg, blue, green), -moz-element(#a) alpha",
      "top left / contain, bottom right / cover",
      /* test cases with clip+origin in the shorthand */
      "url(404.png) alpha padding-box",
      "url(404.png) border-box alpha",
      "content-box url(404.png)",
      "url(404.png) alpha padding-box padding-box",
      "url(404.png) alpha padding-box border-box",
      "content-box border-box url(404.png)",
    ],
    invalid_values: [
      /* mixes with keywords have to be in correct order */
      "50% left", "top 50%",
      /* no quirks mode colors */
      "-moz-radial-gradient(10% bottom, ffffff, black) add no-repeat",
      /* no quirks mode lengths */
      "-moz-linear-gradient(10 10px -45deg, red, blue) repeat",
      "-moz-linear-gradient(10px 10 -45deg, red, blue) repeat",
      "linear-gradient(red -99, yellow, green, blue 120%)",
      /* bug 258080: don't accept background-position separated */
      "left url(404.png) top", "top url(404.png) left",
      "alpha padding-box url(404.png) border-box",
      "alpha padding-box url(404.png) padding-box",
      "-moz-element(#a rubbish)",
      "left top / match-source"
    ]
  };
  gCSSProperties["mask-clip"] = {
    domProp: "maskClip",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "border-box" ],
    other_values: [ "content-box", "padding-box", "border-box, padding-box", "padding-box, padding-box, padding-box", "border-box, border-box" ],
    invalid_values: [ "margin-box", "content-box content-box" ]
  };
  gCSSProperties["mask-image"] = {
    domProp: "maskImage",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [
    "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
    "none, none",
    "none, none, none, none, none",
    "url(#mymask)",
    "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), none",
    "none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), none",
    "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)",
    ].concat(validGradientAndElementValues),
    invalid_values: [
    ].concat(invalidGradientAndElementValues),
    unbalanced_values: [
    ].concat(unbalancedGradientAndElementValues)
  };
  gCSSProperties["mask-mode"] = {
    domProp: "maskMode",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "match-source" ],
    other_values: [ "alpha", "luminance", "match-source, match-source", "match-source, alpha", "alpha, luminance, match-source"],
    invalid_values: [ "match-source match-source", "alpha match-source" ]
  };
  gCSSProperties["mask-composite"] = {
    domProp: "maskComposite",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "add" ],
    other_values: [ "subtract", "intersect", "exclude", "add, add", "subtract, intersect", "subtract, subtract, add"],
    invalid_values: [ "add subtract", "intersect exclude" ]
  };
  gCSSProperties["mask-origin"] = {
    domProp: "maskOrigin",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "border-box" ],
    other_values: [ "padding-box", "content-box", "border-box, padding-box", "padding-box, padding-box, padding-box", "border-box, border-box" ],
    invalid_values: [ "margin-box", "padding-box padding-box" ]
  };
  gCSSProperties["mask-position"] = {
    domProp: "maskPosition",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    initial_values: [ "top 0% left 0%", "top 0% left", "top left", "left top", "0% 0%", "0% top", "left 0%" ],
    other_values: [ "top", "left", "right", "bottom", "center", "center bottom", "bottom center", "center right", "right center", "center top", "top center", "center left", "left center", "right bottom", "bottom right", "50%", "top left, top left", "top left, top right", "top right, top left", "left top, 0% 0%", "10% 20%, 30%, 40%", "top left, bottom right", "right bottom, left top", "0%", "0px", "30px", "0%, 10%, 20%, 30%", "top, top, top, top, top",
      "calc(20px)",
      "calc(20px) 10px",
      "10px calc(20px)",
      "calc(20px) 25%",
      "25% calc(20px)",
      "calc(20px) calc(20px)",
      "calc(20px + 1em) calc(20px / 2)",
      "calc(20px + 50%) calc(50% - 10px)",
      "calc(-20px) calc(-50%)",
      "calc(-20%) calc(-50%)",
      "0px 0px",
      "right 20px top 60px",
      "right 20px bottom 60px",
      "left 20px top 60px",
      "left 20px bottom 60px",
      "right -50px top -50px",
      "left -50px bottom -50px",
      "right 20px top -50px",
      "right -20px top 50px",
      "right 3em bottom 10px",
      "bottom 3em right 10px",
      "top 3em right 10px",
      "left 15px",
      "10px top",
      "left top 15px",
      "left 10px top",
      "left 20%",
      "right 20%"
    ],
    subproperties: [ "mask-position-x", "mask-position-y" ],
    invalid_values: [ "center 10px center 4px", "center 10px center",
                      "top 20%", "bottom 20%", "50% left", "top 50%",
                      "50% bottom 10%", "right 10% 50%", "left right",
                      "top bottom", "left 10% right",
                      "top 20px bottom 20px", "left left",
                      "0px calc(0px + rubbish)"],
  };
  gCSSProperties["mask-position-x"] = {
    domProp: "maskPositionX",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "left", "0%" ],
    other_values: [ "right", "center", "50%", "center, center", "center, right", "right, center", "center, 50%", "10%, 20%, 40%", "1px", "30px", "50%, 10%, 20%, 30%", "center, center, center, center, center",
      "calc(20px)",
      "calc(20px + 1em)",
      "calc(20px / 2)",
      "calc(20px + 50%)",
      "calc(50% - 10px)",
      "calc(-20px)",
      "calc(-50%)",
      "calc(-20%)",
      "right 20px",
      "left 20px",
      "right -50px",
      "left -50px",
      "right 20px",
      "right 3em",
    ],
    invalid_values: [ "center 10px", "right 10% 50%", "left right", "left left",
                      "bottom 20px", "top 10%", "bottom 3em",
                      "top", "bottom", "top, top", "top, bottom", "bottom, top", "top, 0%", "top, top, top, top, top",
                      "calc(0px + rubbish)", "center 0%"],
  };
  gCSSProperties["mask-position-y"] = {
    domProp: "maskPositionY",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "top", "0%" ],
    other_values: [ "bottom", "center", "50%", "center, center", "center, bottom", "bottom, center", "center, 0%", "10%, 20%, 40%", "1px", "30px", "50%, 10%, 20%, 30%", "center, center, center, center, center",
      "calc(20px)",
      "calc(20px + 1em)",
      "calc(20px / 2)",
      "calc(20px + 50%)",
      "calc(50% - 10px)",
      "calc(-20px)",
      "calc(-50%)",
      "calc(-20%)",
      "bottom 20px",
      "top 20px",
      "bottom -50px",
      "top -50px",
      "bottom 20px",
      "bottom 3em",
    ],
    invalid_values: [ "center 10px", "bottom 10% 50%", "top bottom", "top top",
                      "right 20px", "left 10%", "right 3em",
                      "left", "right", "left, left", "left, right", "right, left", "left, 0%", "left, left, left, left, left",
                      "calc(0px + rubbish)", "center 0%"],
  };
  gCSSProperties["mask-repeat"] = {
    domProp: "maskRepeat",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "repeat", "repeat repeat" ],
    other_values: [ "repeat-x", "repeat-y", "no-repeat",
      "repeat-x, repeat-x",
      "repeat, no-repeat",
      "repeat-y, no-repeat, repeat-y",
      "repeat, repeat, repeat",
      "repeat no-repeat",
      "no-repeat repeat",
      "no-repeat no-repeat",
      "repeat no-repeat",
      "no-repeat no-repeat, no-repeat no-repeat",
    ],
    invalid_values: [ "repeat repeat repeat",
                      "repeat-x repeat-y",
                      "repeat repeat-x",
                      "repeat repeat-y",
                      "repeat-x repeat",
                      "repeat-y repeat" ]
  };
  gCSSProperties["mask-size"] = {
    domProp: "maskSize",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "auto", "auto auto" ],
    other_values: [ "contain", "cover", "100px auto", "auto 100px", "100% auto", "auto 100%", "25% 50px", "3em 40%",
      "calc(20px)",
      "calc(20px) 10px",
      "10px calc(20px)",
      "calc(20px) 25%",
      "25% calc(20px)",
      "calc(20px) calc(20px)",
      "calc(20px + 1em) calc(20px / 2)",
      "calc(20px + 50%) calc(50% - 10px)",
      "calc(-20px) calc(-50%)",
      "calc(-20%) calc(-50%)"
    ],
    invalid_values: [ "contain contain", "cover cover", "cover auto", "auto cover", "contain cover", "cover contain", "-5px 3px", "3px -5px", "auto -5px", "-5px auto", "5 3", "10px calc(10px + rubbish)" ]
  };
} else {
  gCSSProperties["mask"] = {
    domProp: "mask",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "none" ],
    other_values: [ "url(#mymask)" ],
    invalid_values: []
  };
}

if (IsCSSPropertyPrefEnabled("layout.css.prefixes.webkit")) {
  gCSSProperties["-webkit-animation"] = {
    domProp: "webkitAnimation",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "animation",
    subproperties: [ "animation-name", "animation-duration", "animation-timing-function", "animation-delay", "animation-direction", "animation-fill-mode", "animation-iteration-count", "animation-play-state" ],
  };
  gCSSProperties["-webkit-animation-delay"] = {
    domProp: "webkitAnimationDelay",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-delay",
    subproperties: [ "animation-delay" ],
  };
  gCSSProperties["-webkit-animation-direction"] = {
    domProp: "webkitAnimationDirection",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-direction",
    subproperties: [ "animation-direction" ],
  };
  gCSSProperties["-webkit-animation-duration"] = {
    domProp: "webkitAnimationDuration",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-duration",
    subproperties: [ "animation-duration" ],
  };
  gCSSProperties["-webkit-animation-fill-mode"] = {
    domProp: "webkitAnimationFillMode",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-fill-mode",
    subproperties: [ "animation-fill-mode" ],
  };
  gCSSProperties["-webkit-animation-iteration-count"] = {
    domProp: "webkitAnimationIterationCount",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-iteration-count",
    subproperties: [ "animation-iteration-count" ],
  };
  gCSSProperties["-webkit-animation-name"] = {
    domProp: "webkitAnimationName",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-name",
    subproperties: [ "animation-name" ],
  };
  gCSSProperties["-webkit-animation-play-state"] = {
    domProp: "webkitAnimationPlayState",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-play-state",
    subproperties: [ "animation-play-state" ],
  };
  gCSSProperties["-webkit-animation-timing-function"] = {
    domProp: "webkitAnimationTimingFunction",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "animation-timing-function",
    subproperties: [ "animation-timing-function" ],
  };
  gCSSProperties["-webkit-filter"] = {
    domProp: "webkitFilter",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "filter",
    subproperties: [ "filter" ],
  };
  gCSSProperties["-webkit-text-fill-color"] = {
    domProp: "webkitTextFillColor",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor", "black", "#000", "#000000", "rgb(0,0,0)" ],
    other_values: [ "red", "rgba(255,255,255,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000", "ff00ff", "rgb(255,xxx,255)" ]
  };
  gCSSProperties["-webkit-text-stroke"] = {
    domProp: "webkitTextStroke",
    inherited: true,
    type: CSS_TYPE_TRUE_SHORTHAND,
    prerequisites: { "color": "black" },
    subproperties: [ "-webkit-text-stroke-width", "-webkit-text-stroke-color" ],
    initial_values: [ "0 currentColor", "currentColor 0px", "0", "currentColor", "0px black" ],
    other_values: [ "thin black", "#f00 medium", "thick rgba(0,0,255,0.5)", "calc(4px - 8px) green", "2px", "green 0", "currentColor 4em", "currentColor calc(5px - 1px)" ],
    invalid_values: [ "-3px black", "calc(50%+ 2px) #000", "30% #f00" ]
  };
  gCSSProperties["-webkit-text-stroke-color"] = {
    domProp: "webkitTextStrokeColor",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    prerequisites: { "color": "black" },
    initial_values: [ "currentColor", "black", "#000", "#000000", "rgb(0,0,0)" ],
    other_values: [ "red", "rgba(255,255,255,0.5)", "transparent" ],
    invalid_values: [ "#0", "#00", "#00000", "#0000000", "#000000000", "000000", "ff00ff", "rgb(255,xxx,255)" ]
  };
  gCSSProperties["-webkit-text-stroke-width"] = {
    domProp: "webkitTextStrokeWidth",
    inherited: true,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "0", "0px", "0em", "0ex", "calc(0pt)", "calc(4px - 8px)" ],
    other_values: [ "thin", "medium", "thick", "17px", "0.2em", "calc(3*25px + 5em)", "calc(5px - 1px)" ],
    invalid_values: [ "5%", "1px calc(nonsense)", "1px red", "-0.1px", "-3px", "30%" ]
  },
  gCSSProperties["-webkit-text-size-adjust"] = {
    domProp: "webkitTextSizeAdjust",
    inherited: true,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-text-size-adjust",
    subproperties: [ "-moz-text-size-adjust" ],
  };
  gCSSProperties["-webkit-transform"] = {
    domProp: "webkitTransform",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transform",
    subproperties: [ "transform" ],
  };
  gCSSProperties["-webkit-transform-origin"] = {
    domProp: "webkitTransformOrigin",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transform-origin",
    subproperties: [ "transform-origin" ],
  };
  gCSSProperties["-webkit-transform-style"] = {
    domProp: "webkitTransformStyle",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transform-style",
    subproperties: [ "transform-style" ],
  };
  gCSSProperties["-webkit-backface-visibility"] = {
    domProp: "webkitBackfaceVisibility",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "backface-visibility",
    subproperties: [ "backface-visibility" ],
  };
  gCSSProperties["-webkit-perspective"] = {
    domProp: "webkitPerspective",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "perspective",
    subproperties: [ "perspective" ],
  };
  gCSSProperties["-webkit-perspective-origin"] = {
    domProp: "webkitPerspectiveOrigin",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "perspective-origin",
    subproperties: [ "perspective-origin" ],
  };
  gCSSProperties["-webkit-transition"] = {
    domProp: "webkitTransition",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "transition",
    subproperties: [ "transition-property", "transition-duration", "transition-timing-function", "transition-delay" ],
  };
  gCSSProperties["-webkit-transition-delay"] = {
    domProp: "webkitTransitionDelay",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-delay",
    subproperties: [ "transition-delay" ],
  };
  gCSSProperties["-webkit-transition-duration"] = {
    domProp: "webkitTransitionDuration",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-duration",
    subproperties: [ "transition-duration" ],
  };
  gCSSProperties["-webkit-transition-property"] = {
    domProp: "webkitTransitionProperty",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-property",
    subproperties: [ "transition-property" ],
  };
  gCSSProperties["-webkit-transition-timing-function"] = {
    domProp: "webkitTransitionTimingFunction",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "transition-timing-function",
    subproperties: [ "transition-timing-function" ],
  };
  gCSSProperties["-webkit-border-radius"] = {
    domProp: "webkitBorderRadius",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "border-radius",
    subproperties: [ "border-bottom-left-radius", "border-bottom-right-radius", "border-top-left-radius", "border-top-right-radius" ],
  };
  gCSSProperties["-webkit-border-top-left-radius"] = {
    domProp: "webkitBorderTopLeftRadius",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-top-left-radius",
    subproperties: [ "border-top-left-radius" ],
  };
  gCSSProperties["-webkit-border-top-right-radius"] = {
    domProp: "webkitBorderTopRightRadius",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-top-right-radius",
    subproperties: [ "border-top-right-radius" ],
  };
  gCSSProperties["-webkit-border-bottom-left-radius"] = {
    domProp: "webkitBorderBottomLeftRadius",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-bottom-left-radius",
    subproperties: [ "border-bottom-left-radius" ],
  };
  gCSSProperties["-webkit-border-bottom-right-radius"] = {
    domProp: "webkitBorderBottomRightRadius",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "border-bottom-right-radius",
    subproperties: [ "border-bottom-right-radius" ],
  };
  gCSSProperties["-webkit-background-clip"] = {
    domProp: "webkitBackgroundClip",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "background-clip",
    subproperties: [ "background-clip" ],
  };
  gCSSProperties["-webkit-background-origin"] = {
    domProp: "webkitBackgroundOrigin",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "background-origin",
    subproperties: [ "background-origin" ],
  };
  gCSSProperties["-webkit-background-size"] = {
    domProp: "webkitBackgroundSize",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "background-size",
    subproperties: [ "background-size" ],
  };
  gCSSProperties["-webkit-border-image"] = {
    domProp: "webkitBorderImage",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "border-image",
    subproperties: [ "border-image-source", "border-image-slice", "border-image-width",  "border-image-outset", "border-image-repeat" ],
  };
  gCSSProperties["-webkit-box-shadow"] = {
    domProp: "webkitBoxShadow",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "box-shadow",
    subproperties: [ "box-shadow" ],
  };
  gCSSProperties["-webkit-box-sizing"] = {
    domProp: "webkitBoxSizing",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "box-sizing",
    subproperties: [ "box-sizing" ],
  };
  gCSSProperties["-webkit-box-flex"] = {
    domProp: "webkitBoxFlex",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-box-flex",
    subproperties: [ "-moz-box-flex" ],
  };
  gCSSProperties["-webkit-box-ordinal-group"] = {
    domProp: "webkitBoxOrdinalGroup",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-box-ordinal-group",
    subproperties: [ "-moz-box-ordinal-group" ],
  };
  gCSSProperties["-webkit-box-orient"] = {
    domProp: "webkitBoxOrient",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-box-orient",
    subproperties: [ "-moz-box-orient" ],
  };
  gCSSProperties["-webkit-box-direction"] = {
    domProp: "webkitBoxDirection",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-box-direction",
    subproperties: [ "-moz-box-direction" ],
  };
  gCSSProperties["-webkit-box-align"] = {
    domProp: "webkitBoxAlign",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-box-align",
    subproperties: [ "-moz-box-align" ],
  };
  gCSSProperties["-webkit-box-pack"] = {
    domProp: "webkitBoxPack",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-box-pack",
    subproperties: [ "-moz-box-pack" ],
  };
  gCSSProperties["-webkit-flex-direction"] = {
    domProp: "webkitFlexDirection",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "flex-direction",
    subproperties: [ "flex-direction" ],
  };
  gCSSProperties["-webkit-flex-wrap"] = {
    domProp: "webkitFlexWrap",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "flex-wrap",
    subproperties: [ "flex-wrap" ],
  };
  gCSSProperties["-webkit-flex-flow"] = {
    domProp: "webkitFlexFlow",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "flex-flow",
    subproperties: [ "flex-direction", "flex-wrap" ],
  };
  gCSSProperties["-webkit-order"] = {
    domProp: "webkitOrder",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "order",
    subproperties: [ "order" ],
  };
  gCSSProperties["-webkit-flex"] = {
    domProp: "webkitFlex",
    inherited: false,
    type: CSS_TYPE_TRUE_SHORTHAND,
    alias_for: "flex",
    subproperties: [ "flex-grow", "flex-shrink", "flex-basis" ],
  };
  gCSSProperties["-webkit-flex-grow"] = {
    domProp: "webkitFlexGrow",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "flex-grow",
    subproperties: [ "flex-grow" ],
  };
  gCSSProperties["-webkit-flex-shrink"] = {
    domProp: "webkitFlexShrink",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "flex-shrink",
    subproperties: [ "flex-shrink" ],
  };
  gCSSProperties["-webkit-flex-basis"] = {
    domProp: "webkitFlexBasis",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "flex-basis",
    subproperties: [ "flex-basis" ],
  };
  gCSSProperties["-webkit-justify-content"] = {
    domProp: "webkitJustifyContent",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "justify-content",
    subproperties: [ "justify-content" ],
  };
  gCSSProperties["-webkit-align-items"] = {
    domProp: "webkitAlignItems",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "align-items",
    subproperties: [ "align-items" ],
  };
  gCSSProperties["-webkit-align-self"] = {
    domProp: "webkitAlignSelf",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "align-self",
    subproperties: [ "align-self" ],
  };
  gCSSProperties["-webkit-align-content"] = {
    domProp: "webkitAlignContent",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "align-content",
    subproperties: [ "align-content" ],
  };
  gCSSProperties["-webkit-user-select"] = {
    domProp: "webkitUserSelect",
    inherited: false,
    type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
    alias_for: "-moz-user-select",
    subproperties: [ "-moz-user-select" ],
  };

  if (SupportsMaskShorthand()) {
    gCSSProperties["-webkit-mask"] = {
      domProp: "webkitMask",
      inherited: false,
      type: CSS_TYPE_TRUE_SHORTHAND,
      alias_for: "mask",
      subproperties: [ "mask-clip", "mask-image", "mask-mode", "mask-origin", "mask-position", "mask-repeat", "mask-size" , "mask-composite" ],
    };
    gCSSProperties["-webkit-mask-clip"] = {
      domProp: "webkitMaskClip",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-clip",
      subproperties: [ "mask-clip" ],
    };

    gCSSProperties["-webkit-mask-composite"] = {
      domProp: "webkitMaskComposite",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-composite",
      subproperties: [ "mask-composite" ],
    };

    gCSSProperties["-webkit-mask-image"] = {
      domProp: "webkitMaskImage",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-image",
      subproperties: [ "mask-image" ],
    };
    gCSSProperties["-webkit-mask-origin"] = {
      domProp: "webkitMaskOrigin",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-origin",
      subproperties: [ "mask-origin" ],
    };
    gCSSProperties["-webkit-mask-position"] = {
      domProp: "webkitMaskPosition",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-position",
      subproperties: [ "mask-position" ],
    };
    gCSSProperties["-webkit-mask-position-x"] = {
      domProp: "webkitMaskPositionX",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-position-x",
      subproperties: [ "mask-position-x" ],
    };
    gCSSProperties["-webkit-mask-position-y"] = {
      domProp: "webkitMaskPositionY",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-position-y",
      subproperties: [ "mask-position-y" ],
    };
    gCSSProperties["-webkit-mask-repeat"] = {
      domProp: "webkitMaskRepeat",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-repeat",
      subproperties: [ "mask-repeat" ],
    };
    gCSSProperties["-webkit-mask-size"] = {
      domProp: "webkitMaskSize",
      inherited: false,
      type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
      alias_for: "mask-size",
      subproperties: [ "mask-size" ],
    };
  }
}

if (IsCSSPropertyPrefEnabled("layout.css.unset-value.enabled")) {
  gCSSProperties["animation"].invalid_values.push("2s unset");
  gCSSProperties["animation-direction"].invalid_values.push("normal, unset", "unset, normal");
  gCSSProperties["animation-name"].invalid_values.push("bounce, unset", "unset, bounce");
  gCSSProperties["-moz-border-bottom-colors"].invalid_values.push("red unset", "unset red");
  gCSSProperties["-moz-border-left-colors"].invalid_values.push("red unset", "unset red");
  gCSSProperties["border-radius"].invalid_values.push("unset 2px", "unset / 2px", "2px unset", "2px / unset");
  gCSSProperties["border-bottom-left-radius"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["border-bottom-right-radius"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["border-top-left-radius"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["border-top-right-radius"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["-moz-border-right-colors"].invalid_values.push("red unset", "unset red");
  gCSSProperties["-moz-border-top-colors"].invalid_values.push("red unset", "unset red");
  gCSSProperties["-moz-outline-radius"].invalid_values.push("unset 2px", "unset / 2px", "2px unset", "2px / unset");
  gCSSProperties["-moz-outline-radius-bottomleft"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["-moz-outline-radius-bottomright"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["-moz-outline-radius-topleft"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["-moz-outline-radius-topright"].invalid_values.push("unset 2px", "2px unset");
  gCSSProperties["background-image"].invalid_values.push("-moz-linear-gradient(unset, 10px 10px, from(blue))", "-moz-linear-gradient(unset, 10px 10px, blue 0)", "-moz-repeating-linear-gradient(unset, 10px 10px, blue 0)");
  gCSSProperties["box-shadow"].invalid_values.push("unset, 2px 2px", "2px 2px, unset", "inset unset");
  gCSSProperties["text-overflow"].invalid_values.push('"hello" unset', 'unset "hello"', 'clip unset', 'unset clip', 'unset inherit', 'unset none', 'initial unset');
  gCSSProperties["text-shadow"].invalid_values.push("unset, 2px 2px", "2px 2px, unset");
  gCSSProperties["transition"].invalid_values.push("2s unset");
  gCSSProperties["transition-property"].invalid_values.push("unset, color", "color, unset");
  if (IsCSSPropertyPrefEnabled("layout.css.filters.enabled")) {
    gCSSProperties["filter"].invalid_values.push("drop-shadow(unset, 2px 2px)", "drop-shadow(2px 2px, unset)");
  }
  if (IsCSSPropertyPrefEnabled("layout.css.text-align-unsafe-value.enabled")) {
    gCSSProperties["text-align"].other_values.push("true left");
  } else {
    gCSSProperties["text-align"].invalid_values.push("true left");
  }
}

if (IsCSSPropertyPrefEnabled("layout.css.float-logical-values.enabled")) {
  gCSSProperties["float"].other_values.push("inline-start");
  gCSSProperties["float"].other_values.push("inline-end");
  gCSSProperties["clear"].other_values.push("inline-start");
  gCSSProperties["clear"].other_values.push("inline-end");
} else {
  gCSSProperties["float"].invalid_values.push("inline-start");
  gCSSProperties["float"].invalid_values.push("inline-end");
  gCSSProperties["clear"].invalid_values.push("inline-start");
  gCSSProperties["clear"].invalid_values.push("inline-end");
}

if (IsCSSPropertyPrefEnabled("layout.css.background-clip-text.enabled")) {
  gCSSProperties["background-clip"].other_values.push(
    "text",
    "content-box, text",
    "text, border-box",
    "text, text"
  );
  gCSSProperties["background"].other_values.push(
    "url(404.png) green padding-box text",
    "content-box text url(404.png) blue"
  );
} else {
  gCSSProperties["background-clip"].invalid_values.push(
    "text",
    "content-box, text",
    "text, border-box",
    "text, text"
  );
  gCSSProperties["background"].invalid_values.push(
    "url(404.png) green padding-box text",
    "content-box text url(404.png) blue"
  );
}

// Copy aliased properties' fields from their alias targets.
for (var prop in gCSSProperties) {
  var entry = gCSSProperties[prop];
  if (entry.alias_for) {
    var aliasTargetEntry = gCSSProperties[entry.alias_for];
    if (!aliasTargetEntry) {
      ok(false,
         "Alias '" + prop + "' alias_for field, '" + entry.alias_for + "', " +
         "must be set to a recognized CSS property in gCSSProperties");
    } else {
      // Copy 'values' fields & 'prerequisites' field from aliasTargetEntry:
      var fieldsToCopy =
          ["initial_values", "other_values", "invalid_values",
           "quirks_values", "unbalanced_values",
           "prerequisites"];

      fieldsToCopy.forEach(function(fieldName) {
        // (Don't copy the field if the alias already has something there,
        // or if the aliased property doesn't have anything to copy.)
        if (!(fieldName in entry) &&
            fieldName in aliasTargetEntry) {
          entry[fieldName] = aliasTargetEntry[fieldName]
        }
      });
    }
  }
}

if (false) {
  // TODO These properties are chrome-only, and are not exposed via CSSOM.
  // We may still want to find a way to test them. See bug 1206999.
  gCSSProperties["-moz-window-shadow"] = {
    //domProp: "MozWindowShadow",
    inherited: false,
    type: CSS_TYPE_LONGHAND,
    initial_values: [ "default" ],
    other_values: [ "none", "menu", "tooltip", "sheet" ],
    invalid_values: []
  };
}

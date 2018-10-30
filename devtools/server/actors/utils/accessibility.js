/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "Ci", "chrome", true);
loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);
loader.lazyRequireGetter(this, "InspectorActorUtils", "devtools/server/actors/inspector/utils");
loader.lazyRequireGetter(this, "Services");

/**
 * Calculates the contrast ratio of the referenced DOM node.
 *
 * @param  {DOMNode} node
 *         The node for which we want to calculate the contrast ratio.
 *
 * @return {Number|null} Contrast ratio value.
*/
function getContrastRatioFor(node) {
  const backgroundColor = InspectorActorUtils.getClosestBackgroundColor(node);
  const backgroundImage = InspectorActorUtils.getClosestBackgroundImage(node);
  const computedStyles = CssLogic.getComputedStyle(node);
  if (!computedStyles) {
    return null;
  }

  const { color, "font-size": fontSize, "font-weight": fontWeight } = computedStyles;
  const isBoldText = parseInt(fontWeight, 10) >= 600;
  const backgroundRgbaColor = new colorUtils.CssColor(backgroundColor, true);
  const textRgbaColor = new colorUtils.CssColor(color, true);

  // TODO: For cases where text color is transparent, it likely comes from the color of
  // the background that is underneath it (commonly from background-clip: text property).
  // With some additional investigation it might be possible to calculate the color
  // contrast where the color of the background is used as text color and the color of
  // the ancestor's background is used as its background.
  if (textRgbaColor.isTransparent()) {
    return null;
  }

  // TODO: these cases include handling gradient backgrounds and the actual image
  // backgrounds. Each one needs to be handled individually.
  if (backgroundImage !== "none") {
    return null;
  }

  let { r: bgR, g: bgG, b: bgB, a: bgA} = backgroundRgbaColor.getRGBATuple();
  let { r: textR, g: textG, b: textB, a: textA } = textRgbaColor.getRGBATuple();

  // If the element has opacity in addition to text and background alpha values, take it
  // into account.
  const opacity = parseFloat(computedStyles.opacity);
  if (opacity < 1) {
    bgA = opacity * bgA;
    textA = opacity * textA;
  }

  return {
    ratio: colorUtils.calculateContrastRatio([ bgR, bgG, bgB, bgA ],
                                             [ textR, textG, textB, textA ]),
    largeText: Math.ceil(parseFloat(fontSize) * 72) / 96 >= (isBoldText ? 14 : 18),
  };
}

/**
 * Helper function that determines if nsIAccessible object is in defunct state.
 *
 * @param  {nsIAccessible}  accessible
 *         object to be tested.
 * @return {Boolean}
 *         True if accessible object is defunct, false otherwise.
 */
function isDefunct(accessible) {
  // If accessibility is disabled, safely assume that the accessible object is
  // now dead.
  if (!Services.appinfo.accessibilityEnabled) {
    return true;
  }

  let defunct = false;

  try {
    const extraState = {};
    accessible.getState({}, extraState);
    // extraState.value is a bitmask. We are applying bitwise AND to mask out
    // irrelevant states.
    defunct = !!(extraState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT);
  } catch (e) {
    defunct = true;
  }

  return defunct;
}

exports.getContrastRatioFor = getContrastRatioFor;
exports.isDefunct = isDefunct;

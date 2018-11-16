/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "Ci", "chrome", true);
loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);
loader.lazyRequireGetter(this, "getBounds", "devtools/server/actors/highlighters/utils/accessibility", true);
loader.lazyRequireGetter(this, "getCurrentZoom", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "addPseudoClassLock", "devtools/server/actors/highlighters/utils/markup", true);
loader.lazyRequireGetter(this, "removePseudoClassLock", "devtools/server/actors/highlighters/utils/markup", true);

const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";

/**
 * Get text style properties for a given node, if possible.
 * @param  {DOMNode} node
 *         DOM node for which text styling information is to be calculated.
 * @return {Object}
 *         Color and text size information for a given DOM node.
 */
function getTextProperties(node) {
  const computedStyles = CssLogic.getComputedStyle(node);
  if (!computedStyles) {
    return null;
  }

  const { color, "font-size": fontSize, "font-weight": fontWeight } = computedStyles;
  const opacity = parseFloat(computedStyles.opacity);

  let { r, g, b, a } = colorUtils.colorToRGBA(color, true);
  a = opacity * a;
  const textRgbaColor = new colorUtils.CssColor(`rgba(${r}, ${g}, ${b}, ${a})`, true);
  // TODO: For cases where text color is transparent, it likely comes from the color of
  // the background that is underneath it (commonly from background-clip: text
  // property). With some additional investigation it might be possible to calculate the
  // color contrast where the color of the background is used as text color and the
  // color of the ancestor's background is used as its background.
  if (textRgbaColor.isTransparent()) {
    return null;
  }

  const isBoldText = parseInt(fontWeight, 10) >= 600;
  const isLargeText = Math.ceil(parseFloat(fontSize) * 72) / 96 >= (isBoldText ? 14 : 18);

  return {
    // Blend text color taking its alpha into account asuming white background.
    color: colorUtils.blendColors([r, g, b, a]),
    isLargeText,
  };
}

/**
 * Get canvas rendering context for the current target window bound by the bounds of the
 * accessible objects.
 * @param  {Object}  win
 *         Current target window.
 * @param  {Object}  bounds
 *         Bounds for the accessible object.
 * @param  {null|DOMNode} node
 *         If not null, a node that corresponds to the accessible object to be used to
 *         make its text color transparent.
 * @return {CanvasRenderingContext2D}
 *         Canvas rendering context for the current window.
 */
function getImageCtx(win, bounds, node) {
  const doc = win.document;
  const canvas = doc.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  const scale = getCurrentZoom(win);

  const { left, top, width, height } = bounds;
  canvas.width = width / scale;
  canvas.height = height / scale;
  const ctx = canvas.getContext("2d", { alpha: false });

  // If node is passed, make its color related text properties invisible.
  if (node) {
    addPseudoClassLock(node, HIGHLIGHTED_PSEUDO_CLASS);
  }

  ctx.drawWindow(win, left / scale, top / scale, width / scale, height / scale, "#fff",
                 ctx.DRAWWINDOW_USE_WIDGET_LAYERS);

  // Restore all inline styling.
  if (node) {
    removePseudoClassLock(node, HIGHLIGHTED_PSEUDO_CLASS);
  }

  return ctx;
}

/**
 * Get RGBA or a range of RGBAs for the background pixels under the text. If luminance is
 * uniform, only return one value of RGBA, otherwise return values that correspond to the
 * min and max luminances.
 * @param  {ImageData} dataText
 *         pixel data for the accessible object with text visible.
 * @param  {ImageData} dataBackground
 *         pixel data for the accessible object with transparent text.
 * @return {Object}
 *         RGBA or a range of RGBAs with min and max values.
 */
function getBgRGBA(dataText, dataBackground) {
  let min = [0, 0, 0, 1];
  let max = [255, 255, 255, 1];
  let minLuminance = 1;
  let maxLuminance = 0;
  const luminances = {};

  let foundDistinctColor = false;
  for (let i = 0; i < dataText.length; i = i + 4) {
    const tR = dataText[i];
    const bgR = dataBackground[i];
    const tG = dataText[i + 1];
    const bgG = dataBackground[i + 1];
    const tB = dataText[i + 2];
    const bgB = dataBackground[i + 2];

    // Ignore pixels that are the same where pixels that are different between the two
    // images are assumed to belong to the text within the node.
    if (tR === bgR && tG === bgG && tB === bgB) {
      continue;
    }

    foundDistinctColor = true;

    const bgColor = `rgb(${bgR}, ${bgG}, ${bgB})`;
    let luminance = luminances[bgColor];

    if (!luminance) {
      // Calculate luminance for the RGB value and store it to only measure once.
      luminance = colorUtils.calculateLuminance([bgR, bgG, bgB]);
      luminances[bgColor] = luminance;
    }

    if (minLuminance >= luminance) {
      minLuminance = luminance;
      min = [bgR, bgG, bgB, 1];
    }

    if (maxLuminance <= luminance) {
      maxLuminance = luminance;
      max = [bgR, bgG, bgB, 1];
    }
  }

  if (!foundDistinctColor) {
    return null;
  }

  return minLuminance === maxLuminance ? { value: max } : { min, max };
}

/**
 * Calculates the contrast ratio of the referenced DOM node.
 *
 * @param  {DOMNode} node
 *         The node for which we want to calculate the contrast ratio.
 * @param  {Object}  options
 *         - bounds   {Object}
 *                    Bounds for the accessible object.
 *         - contexts {null|Object}
 *                    Canvas rendering contexts that have a window drawn as is and also
 *                    with the all text made transparent for contrast comparison.
 *         - win      {Object}
 *                    Target window.
 *
 * @return {Object}
 *         An object that may contain one or more of the following fields: error,
 *         isLargeText, value, min, max values for contrast.
*/
function getContrastRatioFor(node, options = {}) {
  const props = getTextProperties(node);
  if (!props) {
    return {
      error: true,
    };
  }

  const bounds = getBounds(options.win, options.bounds);
  const textContext = getImageCtx(options.win, bounds);
  const backgroundContext = getImageCtx(options.win, bounds, node);

  const { data: dataText } = textContext.getImageData(0, 0, bounds.width, bounds.height);
  const { data: dataBackground } = backgroundContext.getImageData(
    0, 0, bounds.width, bounds.height);

  const rgba = getBgRGBA(dataText, dataBackground);
  if (!rgba) {
    return {
      error: true,
    };
  }

  const { color, isLargeText } = props;
  if (rgba.value) {
    return {
      value: colorUtils.calculateContrastRatio(rgba.value, color),
      isLargeText,
    };
  }

  // calculateContrastRatio modifies the array, since we need to use color array twice,
  // pass its copy to the method.
  const min = colorUtils.calculateContrastRatio(rgba.min, Array.from(color));
  const max = colorUtils.calculateContrastRatio(rgba.max, Array.from(color));

  return {
    min: min < max ? min : max,
    max: min < max ? max : min,
    isLargeText,
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

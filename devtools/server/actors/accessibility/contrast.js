/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);
loader.lazyRequireGetter(this, "getBounds", "devtools/server/actors/highlighters/utils/accessibility", true);
loader.lazyRequireGetter(this, "getCurrentZoom", "devtools/shared/layout/utils", true);
loader.lazyRequireGetter(this, "addPseudoClassLock", "devtools/server/actors/highlighters/utils/markup", true);
loader.lazyRequireGetter(this, "removePseudoClassLock", "devtools/server/actors/highlighters/utils/markup", true);
loader.lazyRequireGetter(this, "DevToolsWorker", "devtools/shared/worker/worker", true);

const WORKER_URL = "resource://devtools/server/actors/accessibility/worker.js";
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";

loader.lazyGetter(this, "worker", () => new DevToolsWorker(WORKER_URL));

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
 * Calculates the contrast ratio of the referenced DOM node.
 *
 * @param  {DOMNode} node
 *         The node for which we want to calculate the contrast ratio.
 * @param  {Object}  options
 *         - bounds   {Object}
 *                    Bounds for the accessible object.
 *         - win      {Object}
 *                    Target window.
 *
 * @return {Object}
 *         An object that may contain one or more of the following fields: error,
 *         isLargeText, value, min, max values for contrast.
*/
async function getContrastRatioFor(node, options = {}) {
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

  const rgba = await worker.performTask("getBgRGBA", {
    dataTextBuf: dataText.buffer,
    dataBackgroundBuf: dataBackground.buffer,
  }, [ dataText.buffer, dataBackground.buffer ]);

  if (!rgba) {
    return {
      error: true,
    };
  }

  const { color, isLargeText } = props;
  if (rgba.value) {
    return {
      value: colorUtils.calculateContrastRatio(rgba.value, color),
      color,
      backgroundColor: rgba.value,
      isLargeText,
    };
  }

  let min = colorUtils.calculateContrastRatio(rgba.min, color);
  let max = colorUtils.calculateContrastRatio(rgba.max, color);

  // Flip minimum and maximum contrast ratios if necessary.
  if (min > max) {
    [min, max] = [max, min];
    [rgba.min, rgba.max] = [rgba.max, rgba.min];
  }

  return {
    min,
    max,
    color,
    backgroundColorMin: rgba.min,
    backgroundColorMax: rgba.max,
    isLargeText,
  };
}

exports.getContrastRatioFor = getContrastRatioFor;

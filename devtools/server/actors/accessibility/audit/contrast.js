/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
loader.lazyRequireGetter(
  this,
  "CssLogic",
  "devtools/server/actors/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "getCurrentZoom",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "addPseudoClassLock",
  "devtools/server/actors/highlighters/utils/markup",
  true
);
loader.lazyRequireGetter(
  this,
  "removePseudoClassLock",
  "devtools/server/actors/highlighters/utils/markup",
  true
);
loader.lazyRequireGetter(
  this,
  "getContrastRatioAgainstBackground",
  "devtools/shared/accessibility",
  true
);
loader.lazyRequireGetter(
  this,
  "getTextProperties",
  "devtools/shared/accessibility",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsWorker",
  "devtools/shared/worker/worker",
  true
);
loader.lazyRequireGetter(
  this,
  "InspectorActorUtils",
  "devtools/server/actors/inspector/utils"
);

const WORKER_URL = "resource://devtools/server/actors/accessibility/worker.js";
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";
const {
  LARGE_TEXT: { BOLD_LARGE_TEXT_MIN_PIXELS, LARGE_TEXT_MIN_PIXELS },
} = require("devtools/shared/accessibility");

loader.lazyGetter(this, "worker", () => new DevToolsWorker(WORKER_URL));

/**
 * Get canvas rendering context for the current target window bound by the bounds of the
 * accessible objects.
 * @param  {Object}  win
 *         Current target window.
 * @param  {Object}  bounds
 *         Bounds for the accessible object.
 * @param  {Object}  zoom
 *         Current zoom level for the window.
 * @param  {Object}  scale
 *         Scale value to scale down the drawn image.
 * @param  {null|DOMNode} node
 *         If not null, a node that corresponds to the accessible object to be used to
 *         make its text color transparent.
 * @return {CanvasRenderingContext2D}
 *         Canvas rendering context for the current window.
 */
function getImageCtx(win, bounds, zoom, scale, node) {
  const doc = win.document;
  const canvas = doc.createElementNS("http://www.w3.org/1999/xhtml", "canvas");

  const { left, top, width, height } = bounds;
  canvas.width = width * zoom * scale;
  canvas.height = height * zoom * scale;
  const ctx = canvas.getContext("2d", { alpha: false });
  ctx.imageSmoothingEnabled = false;
  ctx.scale(scale, scale);

  // If node is passed, make its color related text properties invisible.
  if (node) {
    addPseudoClassLock(node, HIGHLIGHTED_PSEUDO_CLASS);
  }

  ctx.drawWindow(
    win,
    left * zoom,
    top * zoom,
    width * zoom,
    height * zoom,
    "#fff",
    ctx.DRAWWINDOW_USE_WIDGET_LAYERS
  );

  // Restore all inline styling.
  if (node) {
    removePseudoClassLock(node, HIGHLIGHTED_PSEUDO_CLASS);
  }

  return ctx;
}

/**
 * Find RGBA or a range of RGBAs for the background pixels under the text.
 *
 * @param  {DOMNode}  node
 *         Node for which we want to get the background color data.
 * @param  {Object}  options
 *         - bounds       {Object}
 *                        Bounds for the accessible object.
 *         - win          {Object}
 *                        Target window.
 *         - size         {Number}
 *                        Font size of the selected text node
 *         - isBoldText   {Boolean}
 *                        True if selected text node is bold
 * @return {Object}
 *         Object with one or more of the following RGBA fields: value, min, max
 */
function getBackgroundFor(node, { win, bounds, size, isBoldText }) {
  const zoom = 1 / getCurrentZoom(win);
  // When calculating colour contrast, we traverse image data for text nodes that are
  // drawn both with and without transparent text. Image data arrays are typically really
  // big. In cases when the font size is fairly large or when the page is zoomed in image
  // data is especially large (retrieving it and/or traversing it takes significant amount
  // of time). Here we optimize the size of the image data by scaling down the drawn nodes
  // to a size where their text size equals either BOLD_LARGE_TEXT_MIN_PIXELS or
  // LARGE_TEXT_MIN_PIXELS (lower threshold for large text size) depending on the font
  // weight.
  //
  // IMPORTANT: this optimization, in some cases where background colour is non-uniform
  // (gradient or image), can result in small (not noticeable) blending of the background
  // colours. In turn this might affect the reported values of the contrast ratio. The
  // delta is fairly small (<0.1) to noticeably skew the results.
  //
  // NOTE: this optimization does not help in cases where contrast is being calculated for
  // nodes with a lot of text.
  let scale =
    ((isBoldText ? BOLD_LARGE_TEXT_MIN_PIXELS : LARGE_TEXT_MIN_PIXELS) / size) *
    zoom;
  // We do not need to scale the images if the font is smaller than large or if the page
  // is zoomed out (scaling in this case would've been scaling up).
  scale = scale > 1 ? 1 : scale;

  const textContext = getImageCtx(win, bounds, zoom, scale);
  const backgroundContext = getImageCtx(win, bounds, zoom, scale, node);

  const { data: dataText } = textContext.getImageData(
    0,
    0,
    bounds.width * scale,
    bounds.height * scale
  );
  const { data: dataBackground } = backgroundContext.getImageData(
    0,
    0,
    bounds.width * scale,
    bounds.height * scale
  );

  return worker.performTask(
    "getBgRGBA",
    {
      dataTextBuf: dataText.buffer,
      dataBackgroundBuf: dataBackground.buffer,
    },
    [dataText.buffer, dataBackground.buffer]
  );
}

/**
 * Calculates the contrast ratio of the referenced DOM node.
 *
 * @param  {DOMNode} node
 *         The node for which we want to calculate the contrast ratio.
 * @param  {Object}  options
 *         - bounds                           {Object}
 *                                            Bounds for the accessible object.
 *         - win                              {Object}
 *                                            Target window.
 * @return {Object}
 *         An object that may contain one or more of the following fields: error,
 *         isLargeText, value, min, max values for contrast.
 */
async function getContrastRatioFor(node, options = {}) {
  const computedStyle = CssLogic.getComputedStyle(node);
  const props = computedStyle ? getTextProperties(computedStyle) : null;

  if (!props) {
    return {
      error: true,
    };
  }

  const { color, isLargeText, isBoldText, size, opacity } = props;

  const rgba = await getBackgroundFor(node, {
    ...options,
    isBoldText,
    size,
  });

  if (!rgba) {
    // Fallback (original) contrast calculation algorithm. It tries to get the
    // closest background colour for the node and use it to calculate contrast.
    const backgroundColor = InspectorActorUtils.getClosestBackgroundColor(node);
    const backgroundImage = InspectorActorUtils.getClosestBackgroundImage(node);

    if (backgroundImage !== "none") {
      // Both approaches failed, at this point we don't have a better one yet.
      return {
        error: true,
      };
    }

    let { r, g, b, a } = colorUtils.colorToRGBA(backgroundColor, true);
    // If the element has opacity in addition to background alpha value, take it
    // into account. TODO: this does not handle opacity set on ancestor
    // elements (see bug https://bugzilla.mozilla.org/show_bug.cgi?id=1544721).
    if (opacity < 1) {
      a = opacity * a;
    }

    return getContrastRatioAgainstBackground(
      {
        value: [r, g, b, a],
      },
      {
        color,
        isLargeText,
      }
    );
  }

  return getContrastRatioAgainstBackground(rgba, {
    color,
    isLargeText,
  });
}

exports.getContrastRatioFor = getContrastRatioFor;
exports.getBackgroundFor = getBackgroundFor;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "CssLogic",
  "resource://devtools/server/actors/inspector/css-logic.js",
  true
);
loader.lazyRequireGetter(
  this,
  "getCurrentZoom",
  "resource://devtools/shared/layout/utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  "addPseudoClassLock",
  "resource://devtools/server/actors/highlighters/utils/markup.js",
  true
);
loader.lazyRequireGetter(
  this,
  "removePseudoClassLock",
  "resource://devtools/server/actors/highlighters/utils/markup.js",
  true
);
loader.lazyRequireGetter(
  this,
  "getContrastRatioAgainstBackground",
  "resource://devtools/shared/accessibility.js",
  true
);
loader.lazyRequireGetter(
  this,
  "getTextProperties",
  "resource://devtools/shared/accessibility.js",
  true
);
loader.lazyRequireGetter(
  this,
  "InspectorActorUtils",
  "resource://devtools/server/actors/inspector/utils.js"
);
const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    DevToolsWorker: "resource://devtools/shared/worker/worker.sys.mjs",
  },
  { global: "contextual" }
);

const WORKER_URL = "resource://devtools/server/actors/accessibility/worker.js";
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";
const {
  LARGE_TEXT: { BOLD_LARGE_TEXT_MIN_PIXELS, LARGE_TEXT_MIN_PIXELS },
} = require("resource://devtools/shared/accessibility.js");

loader.lazyGetter(this, "worker", () => new lazy.DevToolsWorker(WORKER_URL));

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
 * Calculate the transformed RGBA when a color matrix is set in docShell by
 * multiplying the color matrix with the RGBA vector.
 *
 * @param  {Array}  rgba
 *         Original RGBA array which we want to transform.
 * @param  {Array}  colorMatrix
 *         Flattened 4x5 color matrix that is set in docShell.
 *         A 4x5 matrix of the form:
 *           1  2  3  4  5
 *           6  7  8  9  10
 *           11 12 13 14 15
 *           16 17 18 19 20
 *         will be set in docShell as:
 *           [1, 6, 11, 16, 2, 7, 12, 17, 3, 8, 13, 18, 4, 9, 14, 19, 5, 10, 15, 20]
 * @return {Array}
 *         Transformed RGBA after the color matrix is multiplied with the original RGBA.
 */
function getTransformedRGBA(rgba, colorMatrix) {
  const transformedRGBA = [0, 0, 0, 0];

  // Only use the first four columns of the color matrix corresponding to R, G, B and A
  // color channels respectively. The fifth column is a fixed offset that does not need
  // to be considered for the matrix multiplication. We end up multiplying a 4x4 color
  // matrix with a 4x1 RGBA vector.
  for (let i = 0; i < 16; i++) {
    const row = i % 4;
    const col = Math.floor(i / 4);
    transformedRGBA[row] += colorMatrix[i] * rgba[col];
  }

  return transformedRGBA;
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
 *         - appliedColorMatrix               {Array|null}
 *                                            Simulation color matrix applied to
 *                                            to the viewport, if it exists.
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

  const { isLargeText, isBoldText, size, opacity } = props;
  const { appliedColorMatrix } = options;
  const color = appliedColorMatrix
    ? getTransformedRGBA(props.color, appliedColorMatrix)
    : props.color;
  let rgba = await getBackgroundFor(node, {
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

    let { r, g, b, a } = InspectorUtils.colorToRGBA(backgroundColor);
    // If the element has opacity in addition to background alpha value, take it
    // into account. TODO: this does not handle opacity set on ancestor
    // elements (see bug https://bugzilla.mozilla.org/show_bug.cgi?id=1544721).
    if (opacity < 1) {
      a = opacity * a;
    }

    return getContrastRatioAgainstBackground(
      {
        value: appliedColorMatrix
          ? getTransformedRGBA([r, g, b, a], appliedColorMatrix)
          : [r, g, b, a],
      },
      {
        color,
        isLargeText,
      }
    );
  }

  if (appliedColorMatrix) {
    rgba = rgba.value
      ? {
          value: getTransformedRGBA(rgba.value, appliedColorMatrix),
        }
      : {
          min: getTransformedRGBA(rgba.min, appliedColorMatrix),
          max: getTransformedRGBA(rgba.max, appliedColorMatrix),
        };
  }

  return getContrastRatioAgainstBackground(rgba, {
    color,
    isLargeText,
  });
}

exports.getContrastRatioFor = getContrastRatioFor;
exports.getBackgroundFor = getBackgroundFor;

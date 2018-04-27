/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  apply,
  getNodeTransformationMatrix,
  getWritingModeMatrix,
  identity,
  isIdentity,
  multiply,
  scale,
  translate,
} = require("devtools/shared/layout/dom-matrix-2d");
const { getViewportDimensions } = require("devtools/shared/layout/utils");
const { getComputedStyle } = require("./markup");

// A set of utility functions for highlighters that render their content to a <canvas>
// element.

// We create a <canvas> element that has always 4096x4096 physical pixels, to displays
// our grid's overlay.
// Then, we move the element around when needed, to give the perception that it always
// covers the screen (See bug 1345434).
//
// This canvas size value is the safest we can use because most GPUs can handle it.
// It's also far from the maximum canvas memory allocation limit (4096x4096x4 is
// 67.108.864 bytes, where the limit is 500.000.000 bytes, see:
// http://searchfox.org/mozilla-central/source/gfx/thebes/gfxPrefs.h#401).
//
// Note:
// Once bug 1232491 lands, we could try to refactor this code to use the values from
// the displayport API instead.
//
// Using a fixed value should also solve bug 1348293.
const CANVAS_SIZE = 4096;

// The default color used for the canvas' font, fill and stroke colors.
const DEFAULT_COLOR = "#9400FF";

/**
 * Draws a rect to the context given and applies a transformation matrix if passed.
 * The coordinates are the start and end points of the rectangle's diagonal.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2D canvas context.
 * @param  {Number} x1
 *         The x-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} y1
 *         The y-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} x2
 *         The x-axis coordinate of the rectangle's diagonal end point.
 * @param  {Number} y2
 *         The y-axis coordinate of the rectangle's diagonal end point.
 * @param  {Array} [matrix=identity()]
 *         The transformation matrix to apply.
 */
function clearRect(ctx, x1, y1, x2, y2, matrix = identity()) {
  let p = getPointsFromDiagonal(x1, y1, x2, y2, matrix);
  ctx.clearRect(p[0].x, p[0].y, p[1].x - p[0].x, p[3].y - p[0].y);
}

/**
 * Draws an arrow-bubble rectangle in the provided canvas context.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2D canvas context.
 * @param  {Number} x
 *         The x-axis origin of the rectangle.
 * @param  {Number} y
 *         The y-axis origin of the rectangle.
 * @param  {Number} width
 *         The width of the rectangle.
 * @param  {Number} height
 *         The height of the rectangle.
 * @param  {Number} radius
 *         The radius of the rounding.
 * @param  {Number} margin
 *         The distance of the origin point from the pointer.
 * @param  {Number} arrowSize
 *         The size of the arrow.
 * @param  {String} alignment
 *         The alignment of the rectangle in relation to its position to the grid.
 */
function drawBubbleRect(ctx, x, y, width, height, radius, margin, arrowSize, alignment) {
  let angle = 0;

  if (alignment === "bottom") {
    angle = 180;
  } else if (alignment === "right") {
    angle = 90;
    [width, height] = [height, width];
  } else if (alignment === "left") {
    [width, height] = [height, width];
    angle = 270;
  }

  let originX = x;
  let originY = y;

  ctx.save();
  ctx.translate(originX, originY);
  ctx.rotate(angle * (Math.PI / 180));
  ctx.translate(-originX, -originY);
  ctx.translate(-width / 2, -height - arrowSize - margin);

  ctx.beginPath();
  ctx.moveTo(x, y + radius);
  ctx.lineTo(x, y + height - radius);
  ctx.arcTo(x, y + height, x + radius, y + height, radius);
  ctx.lineTo(x + width / 2 - arrowSize, y + height);
  ctx.lineTo(x + width / 2, y + height + arrowSize);
  ctx.lineTo(x + width / 2 + arrowSize, y + height);
  ctx.arcTo(x + width, y + height, x + width, y + height - radius, radius);
  ctx.lineTo(x + width, y + radius);
  ctx.arcTo(x + width, y, x + width - radius, y, radius);
  ctx.lineTo(x + radius, y);
  ctx.arcTo(x, y, x, y + radius, radius);

  ctx.stroke();
  ctx.fill();

  ctx.restore();
}

/**
 * Draws a line to the context given and applies a transformation matrix if passed.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2D canvas context.
 * @param  {Number} x1
 *         The x-axis of the coordinate for the begin of the line.
 * @param  {Number} y1
 *         The y-axis of the coordinate for the begin of the line.
 * @param  {Number} x2
 *         The x-axis of the coordinate for the end of the line.
 * @param  {Number} y2
 *         The y-axis of the coordinate for the end of the line.
 * @param  {Object} [options]
 *         The options object.
 * @param  {Array} [options.matrix=identity()]
 *         The transformation matrix to apply.
 * @param  {Array} [options.extendToBoundaries]
 *         If set, the line will be extended to reach the boundaries specified.
 */
function drawLine(ctx, x1, y1, x2, y2, options) {
  let matrix = options.matrix || identity();

  let p1 = apply(matrix, [x1, y1]);
  let p2 = apply(matrix, [x2, y2]);

  x1 = p1[0];
  y1 = p1[1];
  x2 = p2[0];
  y2 = p2[1];

  if (options.extendToBoundaries) {
    if (p1[1] === p2[1]) {
      x1 = options.extendToBoundaries[0];
      x2 = options.extendToBoundaries[2];
    } else {
      y1 = options.extendToBoundaries[1];
      x1 = (p2[0] - p1[0]) * (y1 - p1[1]) / (p2[1] - p1[1]) + p1[0];
      y2 = options.extendToBoundaries[3];
      x2 = (p2[0] - p1[0]) * (y2 - p1[1]) / (p2[1] - p1[1]) + p1[0];
    }
  }

  ctx.beginPath();
  ctx.moveTo(Math.round(x1), Math.round(y1));
  ctx.lineTo(Math.round(x2), Math.round(y2));
}

/**
 * Draws a rect to the context given and applies a transformation matrix if passed.
 * The coordinates are the start and end points of the rectangle's diagonal.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2D canvas context.
 * @param  {Number} x1
 *         The x-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} y1
 *         The y-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} x2
 *         The x-axis coordinate of the rectangle's diagonal end point.
 * @param  {Number} y2
 *         The y-axis coordinate of the rectangle's diagonal end point.
 * @param  {Array} [matrix=identity()]
 *         The transformation matrix to apply.
 */
function drawRect(ctx, x1, y1, x2, y2, matrix = identity()) {
  let p = getPointsFromDiagonal(x1, y1, x2, y2, matrix);

  ctx.beginPath();
  ctx.moveTo(Math.round(p[0].x), Math.round(p[0].y));
  ctx.lineTo(Math.round(p[1].x), Math.round(p[1].y));
  ctx.lineTo(Math.round(p[2].x), Math.round(p[2].y));
  ctx.lineTo(Math.round(p[3].x), Math.round(p[3].y));
  ctx.closePath();
}

/**
 * Draws a rounded rectangle in the provided canvas context.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2D canvas context.
 * @param  {Number} x
 *         The x-axis origin of the rectangle.
 * @param  {Number} y
 *         The y-axis origin of the rectangle.
 * @param  {Number} width
 *         The width of the rectangle.
 * @param  {Number} height
 *         The height of the rectangle.
 * @param  {Number} radius
 *         The radius of the rounding.
 */
function drawRoundedRect(ctx, x, y, width, height, radius) {
  ctx.beginPath();
  ctx.moveTo(x, y + radius);
  ctx.lineTo(x, y + height - radius);
  ctx.arcTo(x, y + height, x + radius, y + height, radius);
  ctx.lineTo(x + width - radius, y + height);
  ctx.arcTo(x + width, y + height, x + width, y + height - radius, radius);
  ctx.lineTo(x + width, y + radius);
  ctx.arcTo(x + width, y, x + width - radius, y, radius);
  ctx.lineTo(x + radius, y);
  ctx.arcTo(x, y, x, y + radius, radius);
  ctx.stroke();
  ctx.fill();
}

/**
 * Given an array of four points and returns a DOMRect-like object representing the
 * boundaries defined by the four points.
 *
 * @param  {Array} points
 *         An array with 4 pointer objects {x, y} representing the box quads.
 * @return {Object} DOMRect-like object of the 4 points.
 */
function getBoundsFromPoints(points) {
  let bounds = {};

  bounds.left = Math.min(points[0].x, points[1].x, points[2].x, points[3].x);
  bounds.right = Math.max(points[0].x, points[1].x, points[2].x, points[3].x);
  bounds.top = Math.min(points[0].y, points[1].y, points[2].y, points[3].y);
  bounds.bottom = Math.max(points[0].y, points[1].y, points[2].y, points[3].y);

  bounds.x = bounds.left;
  bounds.y = bounds.top;
  bounds.width = bounds.right - bounds.left;
  bounds.height = bounds.bottom - bounds.top;

  return bounds;
}

/**
 * Returns the current matrices for both canvas drawing and SVG taking into account the
 * following transformations, in this order:
 *   1. The scale given by the display pixel ratio.
 *   2. The translation to the top left corner of the element.
 *   3. The scale given by the current zoom.
 *   4. The translation given by the top and left padding of the element.
 *   5. Any CSS transformation applied directly to the element (only 2D
 *      transformation; the 3D transformation are flattened, see `dom-matrix-2d` module
 *      for further details.)
 *   6. Rotate, translate, and reflect as needed to match the writing mode and text
 *      direction of the element.
 *
 *  The transformations of the element's ancestors are not currently computed (see
 *  bug 1355675).
 *
 * @param  {Element} element
 *         The current element.
 * @param  {Window} window
 *         The window object.
 * @return {Object} An object with the following properties:
 *         - {Array} currentMatrix
 *           The current matrix.
 *         - {Boolean} hasNodeTransformations
 *           true if the node has transformed and false otherwise.
 */
function getCurrentMatrix(element, window) {
  let computedStyle = getComputedStyle(element);

  let paddingTop = parseFloat(computedStyle.paddingTop);
  let paddingLeft = parseFloat(computedStyle.paddingLeft);
  let borderTop = parseFloat(computedStyle.borderTopWidth);
  let borderLeft = parseFloat(computedStyle.borderLeftWidth);

  let nodeMatrix = getNodeTransformationMatrix(element, window.document.documentElement);

  let currentMatrix = identity();
  let hasNodeTransformations = false;

  // Scale based on the device pixel ratio.
  currentMatrix = multiply(currentMatrix, scale(window.devicePixelRatio));

  // Apply the current node's transformation matrix, relative to the inspected window's
  // root element, but only if it's not a identity matrix.
  if (isIdentity(nodeMatrix)) {
    hasNodeTransformations = false;
  } else {
    currentMatrix = multiply(currentMatrix, nodeMatrix);
    hasNodeTransformations = true;
  }

  // Translate the origin based on the node's padding and border values.
  currentMatrix = multiply(currentMatrix,
    translate(paddingLeft + borderLeft, paddingTop + borderTop));

  // Adjust as needed to match the writing mode and text direction of the element.
  let size = {
    width: element.offsetWidth,
    height: element.offsetHeight,
  };
  let writingModeMatrix = getWritingModeMatrix(size, computedStyle);
  if (!isIdentity(writingModeMatrix)) {
    currentMatrix = multiply(currentMatrix, writingModeMatrix);
  }

  return { currentMatrix, hasNodeTransformations };
}

/**
 * Given an array of four points, returns a string represent a path description.
 *
 * @param  {Array} points
 *         An array with 4 pointer objects {x, y} representing the box quads.
 * @return {String} a Path Description that can be used in svg's <path> element.
 */
function getPathDescriptionFromPoints(points) {
  return "M" + points[0].x + "," + points[0].y + " " +
         "L" + points[1].x + "," + points[1].y + " " +
         "L" + points[2].x + "," + points[2].y + " " +
         "L" + points[3].x + "," + points[3].y;
}

/**
 * Given the rectangle's diagonal start and end coordinates, returns an array containing
 * the four coordinates of a rectangle. If a matrix is provided, applies the matrix
 * function to each of the coordinates' value.
 *
 * @param  {Number} x1
 *         The x-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} y1
 *         The y-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} x2
 *         The x-axis coordinate of the rectangle's diagonal end point.
 * @param  {Number} y2
 *         The y-axis coordinate of the rectangle's diagonal end point.
 * @param  {Array} [matrix=identity()]
 *         A transformation matrix to apply.
 * @return {Array} the four coordinate points of the given rectangle transformed by the
 * matrix given.
 */
function getPointsFromDiagonal(x1, y1, x2, y2, matrix = identity()) {
  return [
    [x1, y1],
    [x2, y1],
    [x2, y2],
    [x1, y2]
  ].map(point => {
    let transformedPoint = apply(matrix, point);

    return { x: transformedPoint[0], y: transformedPoint[1] };
  });
}

/**
 * Updates the <canvas> element's style in accordance with the current window's
 * device pixel ratio, and the position calculated in `getCanvasPosition`. It also
 * clears the drawing context. This is called on canvas update after a scroll event where
 * `getCanvasPosition` updates the new canvasPosition.
 *
 * @param  {Canvas} canvas
 *         The <canvas> element.
 * @param  {Object} canvasPosition
 *         A pointer object {x, y} representing the <canvas> position to the top left
 *         corner of the page.
 * @param  {Number} devicePixelRatio
 *         The device pixel ratio.
 */
function updateCanvasElement(canvas, canvasPosition, devicePixelRatio) {
  let { x, y } = canvasPosition;
  let size = CANVAS_SIZE / devicePixelRatio;

  // Resize the canvas taking the dpr into account so as to have crisp lines, and
  // translating it to give the perception that it always covers the viewport.
  canvas.setAttribute("style",
    `width: ${size}px; height: ${size}px; transform: translate(${x}px, ${y}px);`);
  canvas.getCanvasContext("2d").clearRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
}

/**
 * Calculates and returns the <canvas>'s position in accordance with the page's scroll,
 * document's size, canvas size, and viewport's size. This is called when a page's scroll
 * is detected.
 *
 * @param  {Object} canvasPosition
 *         A pointer object {x, y} representing the <canvas> position to the top left
 *         corner of the page.
 * @param  {Object} scrollPosition
 *         A pointer object {x, y} representing the window's pageXOffset and pageYOffset.
 * @param  {Window} window
 *         The window object.
 * @param  {Object} windowDimensions
 *         An object {width, height} representing the window's dimensions for the
 *         `window` given.
 * @return {Boolean} true if the <canvas> position was updated and false otherwise.
 */
function updateCanvasPosition(canvasPosition, scrollPosition, window, windowDimensions) {
  let { x: canvasX, y: canvasY } = canvasPosition;
  let { x: scrollX, y: scrollY } = scrollPosition;
  let cssCanvasSize = CANVAS_SIZE / window.devicePixelRatio;
  let viewportSize = getViewportDimensions(window);
  let { height, width } = windowDimensions;
  let canvasWidth = cssCanvasSize;
  let canvasHeight = cssCanvasSize;
  let hasUpdated = false;

  // Those values indicates the relative horizontal and vertical space the page can
  // scroll before we have to reposition the <canvas>; they're 1/4 of the delta between
  // the canvas' size and the viewport's size: that's because we want to consider both
  // sides (top/bottom, left/right; so 1/2 for each side) and also we don't want to
  // shown the edges of the canvas in case of fast scrolling (to avoid showing undraw
  // areas, therefore another 1/2 here).
  let bufferSizeX = (canvasWidth - viewportSize.width) >> 2;
  let bufferSizeY = (canvasHeight - viewportSize.height) >> 2;

  // Defines the boundaries for the canvas.
  let leftBoundary = 0;
  let rightBoundary = width - canvasWidth;
  let topBoundary = 0;
  let bottomBoundary = height - canvasHeight;

  // Defines the thresholds that triggers the canvas' position to be updated.
  let leftThreshold = scrollX - bufferSizeX;
  let rightThreshold = scrollX - canvasWidth + viewportSize.width + bufferSizeX;
  let topThreshold = scrollY - bufferSizeY;
  let bottomThreshold = scrollY - canvasHeight + viewportSize.height + bufferSizeY;

  if (canvasX < rightBoundary && canvasX < rightThreshold) {
    canvasX = Math.min(leftThreshold, rightBoundary);
    hasUpdated = true;
  } else if (canvasX > leftBoundary && canvasX > leftThreshold) {
    canvasX = Math.max(rightThreshold, leftBoundary);
    hasUpdated = true;
  }

  if (canvasY < bottomBoundary && canvasY < bottomThreshold) {
    canvasY = Math.min(topThreshold, bottomBoundary);
    hasUpdated = true;
  } else if (canvasY > topBoundary && canvasY > topThreshold) {
    canvasY = Math.max(bottomThreshold, topBoundary);
    hasUpdated = true;
  }

  // Update the canvas position with the calculated canvasX and canvasY positions.
  canvasPosition.x = canvasX;
  canvasPosition.y = canvasY;

  return hasUpdated;
}

exports.CANVAS_SIZE = CANVAS_SIZE;
exports.DEFAULT_COLOR = DEFAULT_COLOR;
exports.clearRect = clearRect;
exports.drawBubbleRect = drawBubbleRect;
exports.drawLine = drawLine;
exports.drawRect = drawRect;
exports.drawRoundedRect = drawRoundedRect;
exports.getBoundsFromPoints = getBoundsFromPoints;
exports.getCurrentMatrix = getCurrentMatrix;
exports.getPathDescriptionFromPoints = getPathDescriptionFromPoints;
exports.getPointsFromDiagonal = getPointsFromDiagonal;
exports.updateCanvasElement = updateCanvasElement;
exports.updateCanvasPosition = updateCanvasPosition;

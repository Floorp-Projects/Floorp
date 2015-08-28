/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// How many times, maximum, can we loop before we find the optimal time
// interval in the timeline graph.
const OPTIMAL_TIME_INTERVAL_MAX_ITERS = 100;
// Background time graduations should be multiple of this number of millis.
const TIME_INTERVAL_MULTIPLE = 25;
const TIME_INTERVAL_SCALES = 3;
// The default minimum spacing between time graduations in px.
const TIME_GRADUATION_MIN_SPACING = 10;
// RGB color for the time interval background.
const TIME_INTERVAL_COLOR = [128, 136, 144];
// byte
const TIME_INTERVAL_OPACITY_MIN = 32;
// byte
const TIME_INTERVAL_OPACITY_ADD = 32;

/**
 * DOM node creation helper function.
 * @param {Object} Options to customize the node to be created.
 * - nodeType {String} Optional, defaults to "div",
 * - attributes {Object} Optional attributes object like
 *   {attrName1:value1, attrName2: value2, ...}
 * - parent {DOMNode} Mandatory node to append the newly created node to.
 * - textContent {String} Optional text for the node.
 * @return {DOMNode} The newly created node.
 */
function createNode(options) {
  if (!options.parent) {
    throw new Error("Missing parent DOMNode to create new node");
  }

  let type = options.nodeType || "div";
  let node = options.parent.ownerDocument.createElement(type);

  for (let name in options.attributes || {}) {
    let value = options.attributes[name];
    node.setAttribute(name, value);
  }

  if (options.textContent) {
    node.textContent = options.textContent;
  }

  options.parent.appendChild(node);
  return node;
}

exports.createNode = createNode;

/**
 * Given a data-scale, draw the background for a graph (vertical lines) into a
 * canvas and set that canvas as an image-element with an ID that can be used
 * from CSS.
 * @param {Document} document The document where the image-element should be set.
 * @param {String} id The ID for the image-element.
 * @param {Number} graphWidth The width of the graph.
 * @param {Number} timeScale How many px is 1ms in the graph.
 */
function drawGraphElementBackground(document, id, graphWidth, timeScale) {
  let canvas = document.createElement("canvas");
  let ctx = canvas.getContext("2d");

  // Set the canvas width (as requested) and height (1px, repeated along the Y
  // axis).
  canvas.width = graphWidth;
  canvas.height = 1;

  // Create the image data array which will receive the pixels.
  let imageData = ctx.createImageData(canvas.width, canvas.height);
  let pixelArray = imageData.data;

  let buf = new ArrayBuffer(pixelArray.length);
  let view8bit = new Uint8ClampedArray(buf);
  let view32bit = new Uint32Array(buf);

  // Build new millisecond tick lines...
  let [r, g, b] = TIME_INTERVAL_COLOR;
  let alphaComponent = TIME_INTERVAL_OPACITY_MIN;
  let interval = findOptimalTimeInterval(timeScale);

  // Insert one pixel for each division on each scale.
  for (let i = 1; i <= TIME_INTERVAL_SCALES; i++) {
    let increment = interval * Math.pow(2, i);
    for (let x = 0; x < canvas.width; x += increment) {
      let position = x | 0;
      view32bit[position] = (alphaComponent << 24) | (b << 16) | (g << 8) | r;
    }
    alphaComponent += TIME_INTERVAL_OPACITY_ADD;
  }

  // Flush the image data and cache the waterfall background.
  pixelArray.set(view8bit);
  ctx.putImageData(imageData, 0, 0);
  document.mozSetImageElement(id, canvas);
}

exports.drawGraphElementBackground = drawGraphElementBackground;

/**
 * Find the optimal interval between time graduations in the animation timeline
 * graph based on a time scale and a minimum spacing.
 * @param {Number} timeScale How many px is 1ms in the graph.
 * @param {Number} minSpacing The minimum spacing between 2 graduations,
 * defaults to TIME_GRADUATION_MIN_SPACING.
 * @return {Number} The optimal interval, in pixels.
 */
function findOptimalTimeInterval(timeScale,
                                 minSpacing=TIME_GRADUATION_MIN_SPACING) {
  let timingStep = TIME_INTERVAL_MULTIPLE;
  let numIters = 0;

  if (timeScale > minSpacing) {
    return timeScale;
  }

  while (true) {
    let scaledStep = timeScale * timingStep;
    if (++numIters > OPTIMAL_TIME_INTERVAL_MAX_ITERS) {
      return scaledStep;
    }
    if (scaledStep < minSpacing) {
      timingStep *= 2;
      continue;
    }
    return scaledStep;
  }
}

exports.findOptimalTimeInterval = findOptimalTimeInterval;

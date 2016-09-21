/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const HTML_NS = "http://www.w3.org/1999/xhtml";

// ms
const WATERFALL_BACKGROUND_TICKS_MULTIPLE = 5;
const WATERFALL_BACKGROUND_TICKS_SCALES = 3;
// px
const WATERFALL_BACKGROUND_TICKS_SPACING_MIN = 10;
const WATERFALL_BACKGROUND_TICKS_COLOR_RGB = [128, 136, 144];
// byte
const WATERFALL_BACKGROUND_TICKS_OPACITY_MIN = 32;
// byte
const WATERFALL_BACKGROUND_TICKS_OPACITY_ADD = 32;

const FIND_OPTIMAL_TICK_INTERVAL_MAX_ITERS = 100;

/**
 * Creates the background displayed on the marker's waterfall.
 */
function drawWaterfallBackground(doc, dataScale, waterfallWidth) {
  let canvas = doc.createElementNS(HTML_NS, "canvas");
  let ctx = canvas.getContext("2d");

  // Nuke the context.
  let canvasWidth = canvas.width = waterfallWidth;
  // Awww yeah, 1px, repeats on Y axis.
  let canvasHeight = canvas.height = 1;

  // Start over.
  let imageData = ctx.createImageData(canvasWidth, canvasHeight);
  let pixelArray = imageData.data;

  let buf = new ArrayBuffer(pixelArray.length);
  let view8bit = new Uint8ClampedArray(buf);
  let view32bit = new Uint32Array(buf);

  // Build new millisecond tick lines...
  let [r, g, b] = WATERFALL_BACKGROUND_TICKS_COLOR_RGB;
  let alphaComponent = WATERFALL_BACKGROUND_TICKS_OPACITY_MIN;
  let tickInterval = findOptimalTickInterval({
    ticksMultiple: WATERFALL_BACKGROUND_TICKS_MULTIPLE,
    ticksSpacingMin: WATERFALL_BACKGROUND_TICKS_SPACING_MIN,
    dataScale: dataScale
  });

  // Insert one pixel for each division on each scale.
  for (let i = 1; i <= WATERFALL_BACKGROUND_TICKS_SCALES; i++) {
    let increment = tickInterval * Math.pow(2, i);
    for (let x = 0; x < canvasWidth; x += increment) {
      let position = x | 0;
      view32bit[position] = (alphaComponent << 24) | (b << 16) | (g << 8) | r;
    }
    alphaComponent += WATERFALL_BACKGROUND_TICKS_OPACITY_ADD;
  }

  // Flush the image data and cache the waterfall background.
  pixelArray.set(view8bit);
  ctx.putImageData(imageData, 0, 0);
  doc.mozSetImageElement("waterfall-background", canvas);

  return canvas;
}

/**
 * Finds the optimal tick interval between time markers in this timeline.
 *
 * @param number ticksMultiple
 * @param number ticksSpacingMin
 * @param number dataScale
 * @return number
 */
function findOptimalTickInterval({ ticksMultiple, ticksSpacingMin, dataScale }) {
  let timingStep = ticksMultiple;
  let maxIters = FIND_OPTIMAL_TICK_INTERVAL_MAX_ITERS;
  let numIters = 0;

  if (dataScale > ticksSpacingMin) {
    return dataScale;
  }

  while (true) {
    let scaledStep = dataScale * timingStep;
    if (++numIters > maxIters) {
      return scaledStep;
    }
    if (scaledStep < ticksSpacingMin) {
      timingStep <<= 1;
      continue;
    }
    return scaledStep;
  }
}

exports.TickUtils = { findOptimalTickInterval, drawWaterfallBackground };

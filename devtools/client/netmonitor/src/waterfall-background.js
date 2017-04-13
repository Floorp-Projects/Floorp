/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HTML_NS = "http://www.w3.org/1999/xhtml";
const REQUESTS_WATERFALL_BACKGROUND_TICKS_MULTIPLE = 5; // ms
const REQUESTS_WATERFALL_BACKGROUND_TICKS_SCALES = 3;
const REQUESTS_WATERFALL_BACKGROUND_TICKS_SPACING_MIN = 10; // px
const REQUESTS_WATERFALL_BACKGROUND_TICKS_COLOR_RGB = [128, 136, 144];
// 8-bit value of the alpha component of the tick color
const REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_MIN = 32;
const REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_ADD = 32;
// RGBA colors for the timing markers
const REQUESTS_WATERFALL_DOMCONTENTLOADED_TICKS_COLOR_RGBA = [0, 0, 255, 128];
const REQUESTS_WATERFALL_LOAD_TICKS_COLOR_RGBA = [255, 0, 0, 128];

const STATE_KEYS = [
  "scale",
  "waterfallWidth",
  "firstRequestStartedMillis",
  "timingMarkers",
];

/**
 * Creates the background displayed on each waterfall view in this container.
 */
function WaterfallBackground() {
  this.canvas = document.createElementNS(HTML_NS, "canvas");
  this.ctx = this.canvas.getContext("2d");
  this.prevState = {};
}

/**
 * Changes the element being used as the CSS background for a background
 * with a given background element ID.
 *
 * The funtion wrap the Firefox only API. Waterfall Will not draw the
 * vertical line when running on non-firefox browser.
 * Could be fixed by Bug 1308695
 */
function setImageElement(imageElementId, imageElement) {
  if (document.mozSetImageElement) {
    document.mozSetImageElement(imageElementId, imageElement);
  }
}

WaterfallBackground.prototype = {
  draw(state) {
    // Do a shallow compare of the previous and the new state
    const shouldUpdate = STATE_KEYS.some(key => this.prevState[key] !== state[key]);
    if (!shouldUpdate) {
      return;
    }

    this.prevState = state;

    if (state.waterfallWidth === null || state.scale === null) {
      setImageElement("waterfall-background", null);
      return;
    }

    // Nuke the context.
    let canvasWidth = this.canvas.width = state.waterfallWidth;
    // Awww yeah, 1px, repeats on Y axis.
    let canvasHeight = this.canvas.height = 1;

    // Start over.
    let imageData = this.ctx.createImageData(canvasWidth, canvasHeight);
    let pixelArray = imageData.data;

    let buf = new ArrayBuffer(pixelArray.length);
    let view8bit = new Uint8ClampedArray(buf);
    let view32bit = new Uint32Array(buf);

    // Build new millisecond tick lines...
    let timingStep = REQUESTS_WATERFALL_BACKGROUND_TICKS_MULTIPLE;
    let optimalTickIntervalFound = false;
    let scaledStep;

    while (!optimalTickIntervalFound) {
      // Ignore any divisions that would end up being too close to each other.
      scaledStep = state.scale * timingStep;
      if (scaledStep < REQUESTS_WATERFALL_BACKGROUND_TICKS_SPACING_MIN) {
        timingStep <<= 1;
        continue;
      }
      optimalTickIntervalFound = true;
    }

    const isRTL = isDocumentRTL(document);
    const [r, g, b] = REQUESTS_WATERFALL_BACKGROUND_TICKS_COLOR_RGB;
    let alphaComponent = REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_MIN;

    function drawPixelAt(offset, color) {
      let position = (isRTL ? canvasWidth - offset : offset - 1) | 0;
      let [rc, gc, bc, ac] = color;
      view32bit[position] = (ac << 24) | (bc << 16) | (gc << 8) | rc;
    }

    // Insert one pixel for each division on each scale.
    for (let i = 1; i <= REQUESTS_WATERFALL_BACKGROUND_TICKS_SCALES; i++) {
      let increment = scaledStep * Math.pow(2, i);
      for (let x = 0; x < canvasWidth; x += increment) {
        drawPixelAt(x, [r, g, b, alphaComponent]);
      }
      alphaComponent += REQUESTS_WATERFALL_BACKGROUND_TICKS_OPACITY_ADD;
    }

    function drawTimestamp(timestamp, color) {
      if (timestamp === -1) {
        return;
      }

      let delta = Math.floor((timestamp - state.firstRequestStartedMillis) * state.scale);
      drawPixelAt(delta, color);
    }

    drawTimestamp(state.timingMarkers.firstDocumentDOMContentLoadedTimestamp,
                  REQUESTS_WATERFALL_DOMCONTENTLOADED_TICKS_COLOR_RGBA);

    drawTimestamp(state.timingMarkers.firstDocumentLoadTimestamp,
                  REQUESTS_WATERFALL_LOAD_TICKS_COLOR_RGBA);

    // Flush the image data and cache the waterfall background.
    pixelArray.set(view8bit);
    this.ctx.putImageData(imageData, 0, 0);

    setImageElement("waterfall-background", this.canvas);
  },

  destroy() {
    setImageElement("waterfall-background", null);
  }
};

/**
 * Returns true if this is document is in RTL mode.
 * @return boolean
 */
function isDocumentRTL(doc) {
  return doc.defaultView.getComputedStyle(doc.documentElement).direction === "rtl";
}

module.exports = WaterfallBackground;

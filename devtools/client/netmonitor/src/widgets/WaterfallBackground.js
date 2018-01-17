/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getColor } = require("devtools/client/shared/theme");
const { colorUtils } = require("devtools/shared/css/color");
const { REQUESTS_WATERFALL } = require("../constants");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const STATE_KEYS = [
  "firstRequestStartedMillis",
  "scale",
  "timingMarkers",
  "waterfallWidth",
];

/**
 * Creates the background displayed on each waterfall view in this container.
 */
class WaterfallBackground {
  constructor() {
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
  setImageElement(imageElementId, imageElement) {
    if (document.mozSetImageElement) {
      document.mozSetImageElement(imageElementId, imageElement);
    }
  }

  draw(state) {
    // Do a shallow compare of the previous and the new state
    const shouldUpdate = STATE_KEYS.some(key => this.prevState[key] !== state[key]);
    if (!shouldUpdate) {
      return;
    }

    this.prevState = state;

    if (state.waterfallWidth === null || state.scale === null) {
      this.setImageElement("waterfall-background", null);
      return;
    }

    // Nuke the context.
    let canvasWidth = this.canvas.width =
      state.waterfallWidth - REQUESTS_WATERFALL.LABEL_WIDTH;
    // Awww yeah, 1px, repeats on Y axis.
    let canvasHeight = this.canvas.height = 1;

    // Start over.
    let imageData = this.ctx.createImageData(canvasWidth, canvasHeight);
    let pixelArray = imageData.data;

    let buf = new ArrayBuffer(pixelArray.length);
    let view8bit = new Uint8ClampedArray(buf);
    let view32bit = new Uint32Array(buf);

    // Build new millisecond tick lines...
    let timingStep = REQUESTS_WATERFALL.BACKGROUND_TICKS_MULTIPLE;
    let optimalTickIntervalFound = false;
    let scaledStep;

    while (!optimalTickIntervalFound) {
      // Ignore any divisions that would end up being too close to each other.
      scaledStep = state.scale * timingStep;
      if (scaledStep < REQUESTS_WATERFALL.BACKGROUND_TICKS_SPACING_MIN) {
        timingStep <<= 1;
        continue;
      }
      optimalTickIntervalFound = true;
    }

    const isRTL = document.dir === "rtl";
    const [r, g, b] = REQUESTS_WATERFALL.BACKGROUND_TICKS_COLOR_RGB;
    let alphaComponent = REQUESTS_WATERFALL.BACKGROUND_TICKS_OPACITY_MIN;

    function drawPixelAt(offset, color) {
      let position = (isRTL ? canvasWidth - offset : offset - 1) | 0;
      let [rc, gc, bc, ac] = color;
      view32bit[position] = (ac << 24) | (bc << 16) | (gc << 8) | rc;
    }

    // Insert one pixel for each division on each scale.
    for (let i = 1; i <= REQUESTS_WATERFALL.BACKGROUND_TICKS_SCALES; i++) {
      let increment = scaledStep * Math.pow(2, i);
      for (let x = 0; x < canvasWidth; x += increment) {
        drawPixelAt(x, [r, g, b, alphaComponent]);
      }
      alphaComponent += REQUESTS_WATERFALL.BACKGROUND_TICKS_OPACITY_ADD;
    }

    function drawTimestamp(timestamp, color) {
      if (timestamp === -1) {
        return;
      }

      let delta = Math.floor((timestamp - state.firstRequestStartedMillis) * state.scale);
      drawPixelAt(delta, color);
    }

    let { DOMCONTENTLOADED_TICKS_COLOR, LOAD_TICKS_COLOR } = REQUESTS_WATERFALL;
    drawTimestamp(state.timingMarkers.firstDocumentDOMContentLoadedTimestamp,
      this.getThemeColorAsRgba(DOMCONTENTLOADED_TICKS_COLOR, state.theme));

    drawTimestamp(state.timingMarkers.firstDocumentLoadTimestamp,
      this.getThemeColorAsRgba(LOAD_TICKS_COLOR, state.theme));

    // Flush the image data and cache the waterfall background.
    pixelArray.set(view8bit);
    this.ctx.putImageData(imageData, 0, 0);

    this.setImageElement("waterfall-background", this.canvas);
  }

  /**
   * Retrieve a color defined for the provided theme as a rgba array. The alpha channel is
   * forced to the waterfall constant TICKS_COLOR_OPACITY.
   *
   * @param {String} colorName
   *        The name of the theme color
   * @param {String} theme
   *        The name of the theme
   * @return {Array} RGBA array for the color.
   */
  getThemeColorAsRgba(colorName, theme) {
    let colorStr = getColor(colorName, theme);
    let color = new colorUtils.CssColor(colorStr);
    let { r, g, b } = color.getRGBATuple();
    return [r, g, b, REQUESTS_WATERFALL.TICKS_COLOR_OPACITY];
  }

  destroy() {
    this.setImageElement("waterfall-background", null);
  }
}

module.exports = WaterfallBackground;

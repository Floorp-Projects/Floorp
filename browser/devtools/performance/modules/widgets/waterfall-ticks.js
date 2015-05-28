/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the "waterfall ticks" view, a header for the
 * markers displayed in the waterfall.
 */

loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "WATERFALL_MARKER_SIDEBAR_WIDTH",
  "devtools/performance/marker-view", true);

const HTML_NS = "http://www.w3.org/1999/xhtml";

const FIND_OPTIMAL_TICK_INTERVAL_MAX_ITERS = 100;

const WATERFALL_HEADER_TICKS_MULTIPLE = 5; // ms
const WATERFALL_HEADER_TICKS_SPACING_MIN = 50; // px
const WATERFALL_HEADER_TEXT_PADDING = 3; // px

const WATERFALL_BACKGROUND_TICKS_MULTIPLE = 5; // ms
const WATERFALL_BACKGROUND_TICKS_SCALES = 3;
const WATERFALL_BACKGROUND_TICKS_SPACING_MIN = 10; // px
const WATERFALL_BACKGROUND_TICKS_COLOR_RGB = [128, 136, 144];
const WATERFALL_BACKGROUND_TICKS_OPACITY_MIN = 32; // byte
const WATERFALL_BACKGROUND_TICKS_OPACITY_ADD = 32; // byte

/**
 * A header for a markers waterfall.
 *
 * @param MarkerView root
 *        The root item of the waterfall tree.
 */
function WaterfallHeader(root) {
  this.root = root;
}

WaterfallHeader.prototype = {
  /**
   * Creates and appends this header as the first element of the specified
   * parent element.
   *
   * @param nsIDOMNode parentNode
   *        The parent element for this header.
   */
  attachTo: function(parentNode) {
    let document = parentNode.ownerDocument;
    let startTime = this.root.interval.startTime;
    let dataScale = this.root.getDataScale();
    let waterfallWidth = this.root.getWaterfallWidth();

    let header = this._buildNode(document, startTime, dataScale, waterfallWidth);
    parentNode.insertBefore(header, parentNode.firstChild);

    this._drawWaterfallBackground(document, dataScale, waterfallWidth);
  },

  /**
   * Creates the node displaying this view.
   */
  _buildNode: function(doc, startTime, dataScale, waterfallWidth) {
    let container = doc.createElement("hbox");
    container.className = "waterfall-header-container";
    container.setAttribute("flex", "1");

    let sidebar = doc.createElement("hbox");
    sidebar.className = "waterfall-sidebar theme-sidebar";
    sidebar.setAttribute("width", WATERFALL_MARKER_SIDEBAR_WIDTH);
    sidebar.setAttribute("align", "center");
    container.appendChild(sidebar);

    let name = doc.createElement("description");
    name.className = "plain waterfall-header-name";
    name.setAttribute("value", L10N.getStr("timeline.records"));
    sidebar.appendChild(name);

    let ticks = doc.createElement("hbox");
    ticks.className = "waterfall-header-ticks waterfall-background-ticks";
    ticks.setAttribute("align", "center");
    ticks.setAttribute("flex", "1");
    container.appendChild(ticks);

    let tickInterval = findOptimalTickInterval({
      ticksMultiple: WATERFALL_HEADER_TICKS_MULTIPLE,
      ticksSpacingMin: WATERFALL_HEADER_TICKS_SPACING_MIN,
      dataScale: dataScale
    });

    for (let x = 0; x < waterfallWidth; x += tickInterval) {
      let left = x + WATERFALL_HEADER_TEXT_PADDING;
      let time = Math.round(x / dataScale + startTime);
      let label = L10N.getFormatStr("timeline.tick", time);

      let node = doc.createElement("description");
      node.className = "plain waterfall-header-tick";
      node.style.transform = "translateX(" + left + "px)";
      node.setAttribute("value", label);
      ticks.appendChild(node);
    }

    return container;
  },

  /**
   * Creates the background displayed on the marker's waterfall.
   */
  _drawWaterfallBackground: function(doc, dataScale, waterfallWidth) {
    if (!this._canvas || !this._ctx) {
      this._canvas = doc.createElementNS(HTML_NS, "canvas");
      this._ctx = this._canvas.getContext("2d");
    }
    let canvas = this._canvas;
    let ctx = this._ctx;

    // Nuke the context.
    let canvasWidth = canvas.width = waterfallWidth;
    let canvasHeight = canvas.height = 1; // Awww yeah, 1px, repeats on Y axis.

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
  }
};

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

exports.WaterfallHeader = WaterfallHeader;
exports.TickUtils = { findOptimalTickInterval };

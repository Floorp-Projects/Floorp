/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the "waterfall" view, essentially a detailed list
 * of all the markers in the timeline data.
 */

const {Cc, Ci, Cu, Cr} = require("chrome");

loader.lazyRequireGetter(this, "L10N",
  "devtools/timeline/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/timeline/global", true);

loader.lazyImporter(this, "setNamedTimeout",
  "resource:///modules/devtools/ViewHelpers.jsm");
loader.lazyImporter(this, "clearNamedTimeout",
  "resource:///modules/devtools/ViewHelpers.jsm");

const HTML_NS = "http://www.w3.org/1999/xhtml";

const WATERFALL_SIDEBAR_WIDTH = 150; // px

const WATERFALL_IMMEDIATE_DRAW_MARKERS_COUNT = 30;
const WATERFALL_FLUSH_OUTSTANDING_MARKERS_DELAY = 75; // ms

const WATERFALL_HEADER_TICKS_MULTIPLE = 5; // ms
const WATERFALL_HEADER_TICKS_SPACING_MIN = 50; // px
const WATERFALL_HEADER_TEXT_PADDING = 3; // px

const WATERFALL_BACKGROUND_TICKS_MULTIPLE = 5; // ms
const WATERFALL_BACKGROUND_TICKS_SCALES = 3;
const WATERFALL_BACKGROUND_TICKS_SPACING_MIN = 10; // px
const WATERFALL_BACKGROUND_TICKS_COLOR_RGB = [128, 136, 144];
const WATERFALL_BACKGROUND_TICKS_OPACITY_MIN = 32; // byte
const WATERFALL_BACKGROUND_TICKS_OPACITY_ADD = 32; // byte
const WATERFALL_MARKER_BAR_WIDTH_MIN = 5; // px

/**
 * A detailed waterfall view for the timeline data.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the waterfall.
 */
function Waterfall(parent) {
  this._parent = parent;
  this._document = parent.ownerDocument;
  this._fragment = this._document.createDocumentFragment();
  this._outstandingMarkers = [];

  this._headerContents = this._document.createElement("hbox");
  this._headerContents.className = "waterfall-header-contents";
  this._parent.appendChild(this._headerContents);

  this._listContents = this._document.createElement("vbox");
  this._listContents.className = "waterfall-list-contents";
  this._listContents.setAttribute("flex", "1");
  this._parent.appendChild(this._listContents);

  this._isRTL = this._getRTL();

  // Lazy require is a bit slow, and these are hot objects.
  this._l10n = L10N;
  this._blueprint = TIMELINE_BLUEPRINT;
  this._setNamedTimeout = setNamedTimeout;
  this._clearNamedTimeout = clearNamedTimeout;
}

Waterfall.prototype = {
  /**
   * Populates this view with the provided data source.
   *
   * @param array markers
   *        A list of markers received from the controller.
   * @param number timeEpoch
   *        The absolute time (in milliseconds) when the recording started.
   * @param number timeStart
   *        The time (in milliseconds) to start drawing from.
   * @param number timeEnd
   *        The time (in milliseconds) to end drawing at.
   */
  setData: function(markers, timeEpoch, timeStart, timeEnd) {
    this.clearView();

    let dataScale = this._waterfallWidth / (timeEnd - timeStart);
    this._drawWaterfallBackground(dataScale);

    // Label the header as if the first possible marker was at T=0.
    this._buildHeader(this._headerContents, timeStart - timeEpoch, dataScale);
    this._buildMarkers(this._listContents, markers, timeStart, timeEnd, dataScale);
  },

  /**
   * Depopulates this view.
   */
  clearView: function() {
    while (this._headerContents.hasChildNodes()) {
      this._headerContents.firstChild.remove();
    }
    while (this._listContents.hasChildNodes()) {
      this._listContents.firstChild.remove();
    }
    this._listContents.scrollTop = 0;
    this._outstandingMarkers.length = 0;
    this._clearNamedTimeout("flush-outstanding-markers");
  },

  /**
   * Calculates and stores the available width for the waterfall.
   * This should be invoked every time the container window is resized.
   */
  recalculateBounds: function() {
    let bounds = this._parent.getBoundingClientRect();
    this._waterfallWidth = bounds.width - WATERFALL_SIDEBAR_WIDTH;
  },

  /**
   * Creates the header part of this view.
   *
   * @param nsIDOMNode parent
   *        The parent node holding the header.
   * @param number timeStart
   *        @see Waterfall.prototype.setData
   * @param number dataScale
   *        The time scale of the data source.
   */
  _buildHeader: function(parent, timeStart, dataScale) {
    let container = this._document.createElement("hbox");
    container.className = "waterfall-header-container";
    container.setAttribute("flex", "1");

    let sidebar = this._document.createElement("hbox");
    sidebar.className = "waterfall-sidebar theme-sidebar";
    sidebar.setAttribute("width", WATERFALL_SIDEBAR_WIDTH);
    sidebar.setAttribute("align", "center");
    container.appendChild(sidebar);

    let name = this._document.createElement("label");
    name.className = "plain waterfall-header-name";
    name.setAttribute("value", this._l10n.getStr("timeline.records"));
    sidebar.appendChild(name);

    let ticks = this._document.createElement("hbox");
    ticks.className = "waterfall-header-ticks waterfall-background-ticks";
    ticks.setAttribute("align", "center");
    ticks.setAttribute("flex", "1");
    container.appendChild(ticks);

    let offset = this._isRTL ? this._waterfallWidth : 0;
    let direction = this._isRTL ? -1 : 1;
    let tickInterval = this._findOptimalTickInterval({
      ticksMultiple: WATERFALL_HEADER_TICKS_MULTIPLE,
      ticksSpacingMin: WATERFALL_HEADER_TICKS_SPACING_MIN,
      dataScale: dataScale
    });

    for (let x = 0; x < this._waterfallWidth; x += tickInterval) {
      let start = x + direction * WATERFALL_HEADER_TEXT_PADDING;
      let time = Math.round(timeStart + x / dataScale);
      let label = this._l10n.getFormatStr("timeline.tick", time);

      let node = this._document.createElement("label");
      node.className = "plain waterfall-header-tick";
      node.style.transform = "translateX(" + (start - offset) + "px)";
      node.setAttribute("value", label);
      ticks.appendChild(node);
    }

    parent.appendChild(container);
  },

  /**
   * Creates the markers part of this view.
   *
   * @param nsIDOMNode parent
   *        The parent node holding the markers.
   * @param number timeStart
   *        @see Waterfall.prototype.setData
   * @param number dataScale
   *        The time scale of the data source.
   */
  _buildMarkers: function(parent, markers, timeStart, timeEnd, dataScale) {
    let processed = 0;

    for (let marker of markers) {
      if (!isMarkerInRange(marker, timeStart, timeEnd)) {
        continue;
      }
      // Only build and display a finite number of markers initially, to
      // preserve a snappy UI. After a certain delay, continue building the
      // outstanding markers while there's (hopefully) no user interaction.
      let arguments_ = [this._fragment, marker, timeStart, dataScale];
      if (processed++ < WATERFALL_IMMEDIATE_DRAW_MARKERS_COUNT) {
        this._buildMarker.apply(this, arguments_);
      } else {
        this._outstandingMarkers.push(arguments_);
      }
    }

    // If there are no outstanding markers, add a dummy "spacer" at the end
    // to fill up any remaining available space in the UI.
    if (!this._outstandingMarkers.length) {
      this._buildMarker(this._fragment, null);
    }
    // Otherwise prepare flushing the outstanding markers after a small delay.
    else {
      this._setNamedTimeout("flush-outstanding-markers",
        WATERFALL_FLUSH_OUTSTANDING_MARKERS_DELAY,
        () => this._buildOutstandingMarkers(parent));
    }

    parent.appendChild(this._fragment);
  },

  /**
   * Finishes building the outstanding markers in this view.
   * @see Waterfall.prototype._buildMarkers
   */
  _buildOutstandingMarkers: function(parent) {
    if (!this._outstandingMarkers.length) {
      return;
    }
    for (let args of this._outstandingMarkers) {
      this._buildMarker.apply(this, args);
    }
    this._outstandingMarkers.length = 0;
    parent.appendChild(this._fragment);
  },

  /**
   * Creates a single marker in this view.
   *
   * @param nsIDOMNode parent
   *        The parent node holding the marker.
   * @param object marker
   *        The { name, start, end } marker in the data source.
   * @param timeStart
   *        @see Waterfall.prototype.setData
   * @param number dataScale
   *        @see Waterfall.prototype._buildMarkers
   */
  _buildMarker: function(parent, marker, timeStart, dataScale) {
    let container = this._document.createElement("hbox");
    container.className = "waterfall-marker-container";

    if (marker) {
      this._buildMarkerSidebar(container, marker);
      this._buildMarkerWaterfall(container, marker, timeStart, dataScale);
    } else {
      this._buildMarkerSpacer(container);
      container.setAttribute("flex", "1");
      container.setAttribute("is-spacer", "");
    }

    parent.appendChild(container);
  },

  /**
   * Creates the sidebar part of a marker in this view.
   *
   * @param nsIDOMNode container
   *        The container node representing the marker in this view.
   * @param object marker
   *        @see Waterfall.prototype._buildMarker
   */
  _buildMarkerSidebar: function(container, marker) {
    let blueprint = this._blueprint[marker.name];

    let sidebar = this._document.createElement("hbox");
    sidebar.className = "waterfall-sidebar theme-sidebar";
    sidebar.setAttribute("width", WATERFALL_SIDEBAR_WIDTH);
    sidebar.setAttribute("align", "center");

    let bullet = this._document.createElement("hbox");
    bullet.className = "waterfall-marker-bullet";
    bullet.style.backgroundColor = blueprint.fill;
    bullet.style.borderColor = blueprint.stroke;
    bullet.setAttribute("type", marker.name);
    sidebar.appendChild(bullet);

    let name = this._document.createElement("label");
    name.setAttribute("crop", "end");
    name.setAttribute("flex", "1");
    name.className = "plain waterfall-marker-name";

    let label;
    if (marker.detail && marker.detail.causeName) {
      label = this._l10n.getFormatStr("timeline.markerDetailFormat",
                                      blueprint.label,
                                      marker.detail.causeName);
    } else {
      label = blueprint.label;
    }
    name.setAttribute("value", label);
    name.setAttribute("tooltiptext", label);
    sidebar.appendChild(name);

    container.appendChild(sidebar);
  },

  /**
   * Creates the waterfall part of a marker in this view.
   *
   * @param nsIDOMNode container
   *        The container node representing the marker.
   * @param object marker
   *        @see Waterfall.prototype._buildMarker
   * @param timeStart
   *        @see Waterfall.prototype.setData
   * @param number dataScale
   *        @see Waterfall.prototype._buildMarkers
   */
  _buildMarkerWaterfall: function(container, marker, timeStart, dataScale) {
    let blueprint = this._blueprint[marker.name];

    let waterfall = this._document.createElement("hbox");
    waterfall.className = "waterfall-marker-item waterfall-background-ticks";
    waterfall.setAttribute("align", "center");
    waterfall.setAttribute("flex", "1");

    let start = (marker.start - timeStart) * dataScale;
    let width = (marker.end - marker.start) * dataScale;
    let offset = this._isRTL ? this._waterfallWidth : 0;

    let bar = this._document.createElement("hbox");
    bar.className = "waterfall-marker-bar";
    bar.style.backgroundColor = blueprint.fill;
    bar.style.borderColor = blueprint.stroke;
    bar.style.transform = "translateX(" + (start - offset) + "px)";
    bar.setAttribute("type", marker.name);
    bar.setAttribute("width", Math.max(width, WATERFALL_MARKER_BAR_WIDTH_MIN));
    waterfall.appendChild(bar);

    container.appendChild(waterfall);
  },

  /**
   * Creates a dummy spacer as an empty marker.
   *
   * @param nsIDOMNode container
   *        The container node representing the marker.
   */
  _buildMarkerSpacer: function(container) {
    let sidebarSpacer = this._document.createElement("spacer");
    sidebarSpacer.className = "waterfall-sidebar theme-sidebar";
    sidebarSpacer.setAttribute("width", WATERFALL_SIDEBAR_WIDTH);

    let waterfallSpacer = this._document.createElement("spacer");
    waterfallSpacer.className = "waterfall-marker-item waterfall-background-ticks";
    waterfallSpacer.setAttribute("flex", "1");

    container.appendChild(sidebarSpacer);
    container.appendChild(waterfallSpacer);
  },

  /**
   * Creates the background displayed on the marker's waterfall.
   *
   * @param number dataScale
   *        @see Waterfall.prototype._buildMarkers
   */
  _drawWaterfallBackground: function(dataScale) {
    if (!this._canvas || !this._ctx) {
      this._canvas = this._document.createElementNS(HTML_NS, "canvas");
      this._ctx = this._canvas.getContext("2d");
    }
    let canvas = this._canvas;
    let ctx = this._ctx;

    // Nuke the context.
    let canvasWidth = canvas.width = this._waterfallWidth;
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
    let tickInterval = this._findOptimalTickInterval({
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
    this._document.mozSetImageElement("waterfall-background", canvas);
  },

  /**
   * Finds the optimal tick interval between time markers in this timeline.
   *
   * @param number ticksMultiple
   * @param number ticksSpacingMin
   * @param number dataScale
   * @return number
   */
  _findOptimalTickInterval: function({ ticksMultiple, ticksSpacingMin, dataScale }) {
    let timingStep = ticksMultiple;

    while (true) {
      let scaledStep = dataScale * timingStep;
      if (scaledStep < ticksSpacingMin) {
        timingStep <<= 1;
        continue;
      }
      return scaledStep;
    }
  },

  /**
   * Returns true if this is document is in RTL mode.
   * @return boolean
   */
  _getRTL: function() {
    let win = this._document.defaultView;
    let doc = this._document.documentElement;
    return win.getComputedStyle(doc, null).direction == "rtl";
  }
};

/**
 * Checks if a given marker is in the specified time range.
 *
 * @param object e
 *        The marker containing the { start, end } timestamps.
 * @param number start
 *        The earliest allowed time.
 * @param number end
 *        The latest allowed time.
 * @return boolean
 *         True if the marker fits inside the specified time range.
 */
function isMarkerInRange(e, start, end) {
  return (e.start >= start && e.end <= end) || // bounds inside
         (e.start < start && e.end > end) || // bounds outside
         (e.start < start && e.end >= start && e.end <= end) || // overlap start
         (e.end > end && e.start >= start && e.start <= end); // overlap end
}

exports.Waterfall = Waterfall;

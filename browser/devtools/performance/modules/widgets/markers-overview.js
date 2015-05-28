/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the "markers overview" graph, which is a minimap of all
 * the timeline data. Regions inside it may be selected, determining which
 * markers are visible in the "waterfall".
 */

const { Cc, Ci, Cu, Cr } = require("chrome");
const { AbstractCanvasGraph } = require("resource:///modules/devtools/Graphs.jsm");
const { Heritage } = require("resource:///modules/devtools/ViewHelpers.jsm");

loader.lazyRequireGetter(this, "colorUtils",
  "devtools/css-color", true);
loader.lazyRequireGetter(this, "getColor",
  "devtools/shared/theme", true);
loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "TickUtils",
  "devtools/performance/waterfall-ticks", true);

const OVERVIEW_HEADER_HEIGHT = 14; // px
const OVERVIEW_ROW_HEIGHT = 11; // px

const OVERVIEW_SELECTION_LINE_COLOR = "#666";
const OVERVIEW_CLIPHEAD_LINE_COLOR = "#555";

const FIND_OPTIMAL_TICK_INTERVAL_MAX_ITERS = 100;
const OVERVIEW_HEADER_TICKS_MULTIPLE = 100; // ms
const OVERVIEW_HEADER_TICKS_SPACING_MIN = 75; // px
const OVERVIEW_HEADER_TEXT_FONT_SIZE = 9; // px
const OVERVIEW_HEADER_TEXT_FONT_FAMILY = "sans-serif";
const OVERVIEW_HEADER_TEXT_PADDING_LEFT = 6; // px
const OVERVIEW_HEADER_TEXT_PADDING_TOP = 1; // px
const OVERVIEW_MARKERS_COLOR_STOPS = [0, 0.1, 0.75, 1];
const OVERVIEW_MARKER_WIDTH_MIN = 4; // px
const OVERVIEW_GROUP_VERTICAL_PADDING = 5; // px

/**
 * An overview for the markers data.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the overview.
 * @param Object blueprint
 *        List of names and colors defining markers.
 */
function MarkersOverview(parent, blueprint, ...args) {
  AbstractCanvasGraph.apply(this, [parent, "markers-overview", ...args]);
  this.setTheme();
  this.setBlueprint(blueprint);
}

MarkersOverview.prototype = Heritage.extend(AbstractCanvasGraph.prototype, {
  clipheadLineColor: OVERVIEW_CLIPHEAD_LINE_COLOR,
  selectionLineColor: OVERVIEW_SELECTION_LINE_COLOR,
  headerHeight: OVERVIEW_HEADER_HEIGHT,
  rowHeight: OVERVIEW_ROW_HEIGHT,
  groupPadding: OVERVIEW_GROUP_VERTICAL_PADDING,

  /**
   * Compute the height of the overview.
   */
  get fixedHeight() {
    return this.headerHeight + this.rowHeight * (this._lastGroup + 1);
  },

  /**
   * List of names and colors used to paint this overview.
   * @see TIMELINE_BLUEPRINT in timeline/widgets/global.js
   */
  setBlueprint: function(blueprint) {
    this._paintBatches = new Map();
    this._lastGroup = 0;

    for (let type in blueprint) {
      this._paintBatches.set(type, { style: blueprint[type], batch: [] });
      this._lastGroup = Math.max(this._lastGroup, blueprint[type].group || 0);
    }
  },

  /**
   * Disables selection and empties this graph.
   */
  clearView: function() {
    this.selectionEnabled = false;
    this.dropSelection();
    this.setData({ duration: 0, markers: [] });
  },

  /**
   * Renders the graph's data source.
   * @see AbstractCanvasGraph.prototype.buildGraphImage
   */
  buildGraphImage: function() {
    let { markers, duration } = this._data;

    let { canvas, ctx } = this._getNamedCanvas("markers-overview-data");
    let canvasWidth = this._width;
    let canvasHeight = this._height;

    // Group markers into separate paint batches. This is necessary to
    // draw all markers sharing the same style at once.

    for (let marker of markers) {
      let markerType = this._paintBatches.get(marker.name);
      if (markerType) {
        markerType.batch.push(marker);
      }
    }

    // Calculate each row's height, and the time-based scaling.

    let totalGroups = this._lastGroup + 1;
    let groupHeight = this.rowHeight * this._pixelRatio;
    let groupPadding = this.groupPadding * this._pixelRatio;
    let headerHeight = this.headerHeight * this._pixelRatio;
    let dataScale = this.dataScaleX = canvasWidth / duration;

    // Draw the header and overview background.

    ctx.fillStyle = this.headerBackgroundColor;
    ctx.fillRect(0, 0, canvasWidth, headerHeight);

    ctx.fillStyle = this.backgroundColor;
    ctx.fillRect(0, headerHeight, canvasWidth, canvasHeight);

    // Draw the alternating odd/even group backgrounds.

    ctx.fillStyle = this.alternatingBackgroundColor;
    ctx.beginPath();

    for (let i = 0; i < totalGroups; i += 2) {
      let top = headerHeight + i * groupHeight;
      ctx.rect(0, top, canvasWidth, groupHeight);
    }

    ctx.fill();

    // Draw the timeline header ticks.

    let fontSize = OVERVIEW_HEADER_TEXT_FONT_SIZE * this._pixelRatio;
    let fontFamily = OVERVIEW_HEADER_TEXT_FONT_FAMILY;
    let textPaddingLeft = OVERVIEW_HEADER_TEXT_PADDING_LEFT * this._pixelRatio;
    let textPaddingTop = OVERVIEW_HEADER_TEXT_PADDING_TOP * this._pixelRatio;

    let tickInterval = TickUtils.findOptimalTickInterval({
      ticksMultiple: OVERVIEW_HEADER_TICKS_MULTIPLE,
      ticksSpacingMin: OVERVIEW_HEADER_TICKS_SPACING_MIN,
      dataScale: dataScale
    });

    ctx.textBaseline = "middle";
    ctx.font = fontSize + "px " + fontFamily;
    ctx.fillStyle = this.headerTextColor;
    ctx.strokeStyle = this.headerTimelineStrokeColor;
    ctx.beginPath();

    for (let x = 0; x < canvasWidth; x += tickInterval) {
      let lineLeft = x;
      let textLeft = lineLeft + textPaddingLeft;
      let time = Math.round(x / dataScale);
      let label = L10N.getFormatStr("timeline.tick", time);
      ctx.fillText(label, textLeft, headerHeight / 2 + textPaddingTop);
      ctx.moveTo(lineLeft, 0);
      ctx.lineTo(lineLeft, canvasHeight);
    }

    ctx.stroke();

    // Draw the timeline markers.

    for (let [, { style, batch }] of this._paintBatches) {
      let top = headerHeight + style.group * groupHeight + groupPadding / 2;
      let height = groupHeight - groupPadding;

      let color = getColor(style.colorName, this.theme);
      ctx.fillStyle = color;
      ctx.beginPath();

      for (let { start, end } of batch) {
        let left = start * dataScale;
        let width = Math.max((end - start) * dataScale, OVERVIEW_MARKER_WIDTH_MIN);
        ctx.rect(left, top, width, height);
      }

      ctx.fill();

      // Since all the markers in this batch (thus sharing the same style) have
      // been drawn, empty it. The next time new markers will be available,
      // they will be sorted and drawn again.
      batch.length = 0;
    }

    return canvas;
  },

  /**
   * Sets the theme via `theme` to either "light" or "dark",
   * and updates the internal styling to match. Requires a redraw
   * to see the effects.
   */
  setTheme: function (theme) {
    this.theme = theme = theme || "light";
    this.backgroundColor = getColor("body-background", theme);
    this.selectionBackgroundColor = colorUtils.setAlpha(getColor("selection-background", theme), 0.25);
    this.selectionStripesColor = colorUtils.setAlpha("#fff", 0.1);
    this.headerBackgroundColor = getColor("body-background", theme);
    this.headerTextColor = getColor("body-color", theme);
    this.headerTimelineStrokeColor = colorUtils.setAlpha(getColor("body-color-alt", theme), 0.25);
    this.alternatingBackgroundColor = colorUtils.setAlpha(getColor("body-color", theme), 0.05);
  }
});

exports.MarkersOverview = MarkersOverview;

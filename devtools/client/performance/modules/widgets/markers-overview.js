/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the "markers overview" graph, which is a minimap of all
 * the timeline data. Regions inside it may be selected, determining which
 * markers are visible in the "waterfall".
 */

const { extend } = require("devtools/shared/extend");
const { AbstractCanvasGraph } = require("devtools/client/shared/widgets/Graphs");

const { colorUtils } = require("devtools/shared/css/color");
const { getColor } = require("devtools/client/shared/theme");
const ProfilerGlobal = require("devtools/client/performance/modules/global");
const { MarkerBlueprintUtils } = require("devtools/client/performance/modules/marker-blueprint-utils");
const { TickUtils } = require("devtools/client/performance/modules/waterfall-ticks");
const { TIMELINE_BLUEPRINT } = require("devtools/client/performance/modules/markers");

const OVERVIEW_HEADER_HEIGHT = 14; // px
const OVERVIEW_ROW_HEIGHT = 11; // px

const OVERVIEW_SELECTION_LINE_COLOR = "#666";
const OVERVIEW_CLIPHEAD_LINE_COLOR = "#555";

const OVERVIEW_HEADER_TICKS_MULTIPLE = 100; // ms
const OVERVIEW_HEADER_TICKS_SPACING_MIN = 75; // px
const OVERVIEW_HEADER_TEXT_FONT_SIZE = 9; // px
const OVERVIEW_HEADER_TEXT_FONT_FAMILY = "sans-serif";
const OVERVIEW_HEADER_TEXT_PADDING_LEFT = 6; // px
const OVERVIEW_HEADER_TEXT_PADDING_TOP = 1; // px
const OVERVIEW_MARKER_WIDTH_MIN = 4; // px
const OVERVIEW_GROUP_VERTICAL_PADDING = 5; // px

/**
 * An overview for the markers data.
 *
 * @param Node parent
 *        The parent node holding the overview.
 * @param Array<String> filter
 *        List of names of marker types that should not be shown.
 */
function MarkersOverview(parent, filter = [], ...args) {
  AbstractCanvasGraph.apply(this, [parent, "markers-overview", ...args]);
  this.setTheme();
  this.setFilter(filter);
}

MarkersOverview.prototype = extend(AbstractCanvasGraph.prototype, {
  clipheadLineColor: OVERVIEW_CLIPHEAD_LINE_COLOR,
  selectionLineColor: OVERVIEW_SELECTION_LINE_COLOR,
  headerHeight: OVERVIEW_HEADER_HEIGHT,
  rowHeight: OVERVIEW_ROW_HEIGHT,
  groupPadding: OVERVIEW_GROUP_VERTICAL_PADDING,

  /**
   * Compute the height of the overview.
   */
  get fixedHeight() {
    return this.headerHeight + this.rowHeight * this._numberOfGroups;
  },

  /**
   * List of marker types that should not be shown in the graph.
   */
  setFilter: function(filter) {
    this._paintBatches = new Map();
    this._filter = filter;
    this._groupMap = Object.create(null);

    const observedGroups = new Set();

    for (const type in TIMELINE_BLUEPRINT) {
      if (filter.includes(type)) {
        continue;
      }
      this._paintBatches.set(type, { definition: TIMELINE_BLUEPRINT[type], batch: [] });
      observedGroups.add(TIMELINE_BLUEPRINT[type].group);
    }

    // Take our set of observed groups and order them and map
    // the group numbers to fill in the holes via `_groupMap`.
    // This normalizes our rows by removing rows that aren't used
    // if filters are enabled.
    let actualPosition = 0;
    for (const groupNumber of Array.from(observedGroups).sort()) {
      this._groupMap[groupNumber] = actualPosition++;
    }
    this._numberOfGroups = Object.keys(this._groupMap).length;
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
    const { markers, duration } = this._data;

    const { canvas, ctx } = this._getNamedCanvas("markers-overview-data");
    const canvasWidth = this._width;
    const canvasHeight = this._height;

    // Group markers into separate paint batches. This is necessary to
    // draw all markers sharing the same style at once.
    for (const marker of markers) {
      // Again skip over markers that we're filtering -- we don't want them
      // to be labeled as "Unknown"
      if (!MarkerBlueprintUtils.shouldDisplayMarker(marker, this._filter)) {
        continue;
      }

      const markerType = this._paintBatches.get(marker.name) ||
                                              this._paintBatches.get("UNKNOWN");
      markerType.batch.push(marker);
    }

    // Calculate each row's height, and the time-based scaling.

    const groupHeight = this.rowHeight * this._pixelRatio;
    const groupPadding = this.groupPadding * this._pixelRatio;
    const headerHeight = this.headerHeight * this._pixelRatio;
    const dataScale = this.dataScaleX = canvasWidth / duration;

    // Draw the header and overview background.

    ctx.fillStyle = this.headerBackgroundColor;
    ctx.fillRect(0, 0, canvasWidth, headerHeight);

    ctx.fillStyle = this.backgroundColor;
    ctx.fillRect(0, headerHeight, canvasWidth, canvasHeight);

    // Draw the alternating odd/even group backgrounds.

    ctx.fillStyle = this.alternatingBackgroundColor;
    ctx.beginPath();

    for (let i = 0; i < this._numberOfGroups; i += 2) {
      const top = headerHeight + i * groupHeight;
      ctx.rect(0, top, canvasWidth, groupHeight);
    }

    ctx.fill();

    // Draw the timeline header ticks.

    const fontSize = OVERVIEW_HEADER_TEXT_FONT_SIZE * this._pixelRatio;
    const fontFamily = OVERVIEW_HEADER_TEXT_FONT_FAMILY;
    const textPaddingLeft = OVERVIEW_HEADER_TEXT_PADDING_LEFT * this._pixelRatio;
    const textPaddingTop = OVERVIEW_HEADER_TEXT_PADDING_TOP * this._pixelRatio;

    const tickInterval = TickUtils.findOptimalTickInterval({
      ticksMultiple: OVERVIEW_HEADER_TICKS_MULTIPLE,
      ticksSpacingMin: OVERVIEW_HEADER_TICKS_SPACING_MIN,
      dataScale: dataScale,
    });

    ctx.textBaseline = "middle";
    ctx.font = fontSize + "px " + fontFamily;
    ctx.fillStyle = this.headerTextColor;
    ctx.strokeStyle = this.headerTimelineStrokeColor;
    ctx.beginPath();

    for (let x = 0; x < canvasWidth; x += tickInterval) {
      const lineLeft = x;
      const textLeft = lineLeft + textPaddingLeft;
      const time = Math.round(x / dataScale);
      const label = ProfilerGlobal.L10N.getFormatStr("timeline.tick", time);
      ctx.fillText(label, textLeft, headerHeight / 2 + textPaddingTop);
      ctx.moveTo(lineLeft, 0);
      ctx.lineTo(lineLeft, canvasHeight);
    }

    ctx.stroke();

    // Draw the timeline markers.

    for (const [, { definition, batch }] of this._paintBatches) {
      const group = this._groupMap[definition.group];
      const top = headerHeight + group * groupHeight + groupPadding / 2;
      const height = groupHeight - groupPadding;

      const color = getColor(definition.colorName, this.theme);
      ctx.fillStyle = color;
      ctx.beginPath();

      for (const { start, end } of batch) {
        const left = start * dataScale;
        const width = Math.max((end - start) * dataScale, OVERVIEW_MARKER_WIDTH_MIN);
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
  setTheme: function(theme) {
    this.theme = theme = theme || "light";
    this.backgroundColor = getColor("body-background", theme);
    this.selectionBackgroundColor = colorUtils.setAlpha(
      getColor("selection-background", theme), 0.25);
    this.selectionStripesColor = colorUtils.setAlpha("#fff", 0.1);
    this.headerBackgroundColor = getColor("body-background", theme);
    this.headerTextColor = getColor("body-color", theme);
    this.headerTimelineStrokeColor = colorUtils.setAlpha(
      getColor("text-color-alt", theme), 0.25);
    this.alternatingBackgroundColor = colorUtils.setAlpha(
      getColor("body-color", theme), 0.05);
  },
});

exports.MarkersOverview = MarkersOverview;

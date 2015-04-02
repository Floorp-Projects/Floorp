/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the base line graph that all Performance line graphs use.
 */

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource:///modules/devtools/Graphs.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

const { colorUtils: { setAlpha }} = require("devtools/css-color");
const { getColor } = require("devtools/shared/theme");

loader.lazyRequireGetter(this, "ProfilerGlobal",
  "devtools/shared/profiler/global");
loader.lazyRequireGetter(this, "TimelineGlobal",
  "devtools/shared/timeline/global");

const HEIGHT = 35; // px
const STROKE_WIDTH = 1; // px
const DAMPEN_VALUES = 0.95;
const CLIPHEAD_LINE_COLOR = "#666";
const SELECTION_LINE_COLOR = "#555";
const SELECTION_BACKGROUND_COLOR_NAME = "highlight-blue";
const FRAMERATE_GRAPH_COLOR_NAME = "highlight-green";
const MEMORY_GRAPH_COLOR_NAME = "highlight-blue";

/**
 * A base class for performance graphs to inherit from.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the overview.
 * @param string metric
 *        The unit of measurement for this graph.
 */
function PerformanceGraph(parent, metric) {
  LineGraphWidget.call(this, parent, { metric });
  this.setTheme();
}

PerformanceGraph.prototype = Heritage.extend(LineGraphWidget.prototype, {
  strokeWidth: STROKE_WIDTH,
  dampenValuesFactor: DAMPEN_VALUES,
  fixedHeight: HEIGHT,
  clipheadLineColor: CLIPHEAD_LINE_COLOR,
  selectionLineColor: SELECTION_LINE_COLOR,
  withTooltipArrows: false,
  withFixedTooltipPositions: true,

  /**
   * Disables selection and empties this graph.
   */
  clearView: function() {
    this.selectionEnabled = false;
    this.dropSelection();
    this.setData([]);
  },

  /**
   * Sets the theme via `theme` to either "light" or "dark",
   * and updates the internal styling to match. Requires a redraw
   * to see the effects.
   */
  setTheme: function (theme) {
    theme = theme || "light";
    let mainColor = getColor(this.mainColor || "highlight-blue", theme);
    this.backgroundColor = getColor("body-background", theme);
    this.strokeColor = mainColor;
    this.backgroundGradientStart = setAlpha(mainColor, 0.2);
    this.backgroundGradientEnd = setAlpha(mainColor, 0.2);
    this.selectionBackgroundColor = setAlpha(getColor(SELECTION_BACKGROUND_COLOR_NAME, theme), 0.25);
    this.selectionStripesColor = "rgba(255, 255, 255, 0.1)";
    this.maximumLineColor = setAlpha(mainColor, 0.4);
    this.averageLineColor = setAlpha(mainColor, 0.7);
    this.minimumLineColor = setAlpha(mainColor, 0.9);
  }
});

/**
 * Constructor for the framerate graph. Inherits from PerformanceGraph.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the overview.
 */
function FramerateGraph(parent) {
  PerformanceGraph.call(this, parent, ProfilerGlobal.L10N.getStr("graphs.fps"));
}

FramerateGraph.prototype = Heritage.extend(PerformanceGraph.prototype, {
  mainColor: FRAMERATE_GRAPH_COLOR_NAME
});

exports.FramerateGraph = FramerateGraph;

/**
 * Constructor for the memory graph. Inherits from PerformanceGraph.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the overview.
 */
function MemoryGraph(parent) {
  PerformanceGraph.call(this, parent, TimelineGlobal.L10N.getStr("graphs.memory"));
}

MemoryGraph.prototype = Heritage.extend(PerformanceGraph.prototype, {
  mainColor: MEMORY_GRAPH_COLOR_NAME
});

exports.MemoryGraph = MemoryGraph;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the base line graph that all Performance line graphs use.
 */

const { Cc, Ci, Cu, Cr } = require("chrome");
const { Task } = require("resource://gre/modules/Task.jsm");
const { Heritage } = require("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");
const BarGraphWidget = require("devtools/client/shared/widgets/BarGraphWidget");
const MountainGraphWidget = require("devtools/client/shared/widgets/MountainGraphWidget");
const { CanvasGraphUtils } = require("devtools/client/shared/widgets/Graphs");

loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "colorUtils",
  "devtools/shared/css-color", true);
loader.lazyRequireGetter(this, "getColor",
  "devtools/client/shared/theme", true);
loader.lazyRequireGetter(this, "ProfilerGlobal",
  "devtools/client/performance/modules/global");
loader.lazyRequireGetter(this, "L10N",
  "devtools/client/performance/modules/global", true);
loader.lazyRequireGetter(this, "MarkersOverview",
  "devtools/client/performance/modules/widgets/markers-overview", true);
loader.lazyRequireGetter(this, "createTierGraphDataFromFrameNode",
  "devtools/client/performance/modules/logic/jit", true);

/**
 * For line graphs
 */
const HEIGHT = 35; // px
const STROKE_WIDTH = 1; // px
const DAMPEN_VALUES = 0.95;
const CLIPHEAD_LINE_COLOR = "#666";
const SELECTION_LINE_COLOR = "#555";
const SELECTION_BACKGROUND_COLOR_NAME = "graphs-blue";
const FRAMERATE_GRAPH_COLOR_NAME = "graphs-green";
const MEMORY_GRAPH_COLOR_NAME = "graphs-blue";

/**
 * For timeline overview
 */
const MARKERS_GRAPH_HEADER_HEIGHT = 14; // px
const MARKERS_GRAPH_ROW_HEIGHT = 10; // px
const MARKERS_GROUP_VERTICAL_PADDING = 4; // px

/**
 * For optimization graph
 */
const OPTIMIZATIONS_GRAPH_RESOLUTION = 100;

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
    let mainColor = getColor(this.mainColor || "graphs-blue", theme);
    this.backgroundColor = getColor("body-background", theme);
    this.strokeColor = mainColor;
    this.backgroundGradientStart = colorUtils.setAlpha(mainColor, 0.2);
    this.backgroundGradientEnd = colorUtils.setAlpha(mainColor, 0.2);
    this.selectionBackgroundColor = colorUtils.setAlpha(getColor(SELECTION_BACKGROUND_COLOR_NAME, theme), 0.25);
    this.selectionStripesColor = "rgba(255, 255, 255, 0.1)";
    this.maximumLineColor = colorUtils.setAlpha(mainColor, 0.4);
    this.averageLineColor = colorUtils.setAlpha(mainColor, 0.7);
    this.minimumLineColor = colorUtils.setAlpha(mainColor, 0.9);
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
  mainColor: FRAMERATE_GRAPH_COLOR_NAME,
  setPerformanceData: function ({ duration, ticks }, resolution) {
    this.dataDuration = duration;
    return this.setDataFromTimestamps(ticks, resolution, duration);
  }
});

/**
 * Constructor for the memory graph. Inherits from PerformanceGraph.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the overview.
 */
function MemoryGraph(parent) {
  PerformanceGraph.call(this, parent, L10N.getStr("graphs.memory"));
}

MemoryGraph.prototype = Heritage.extend(PerformanceGraph.prototype, {
  mainColor: MEMORY_GRAPH_COLOR_NAME,
  setPerformanceData: function ({ duration, memory }) {
    this.dataDuration = duration;
    return this.setData(memory);
  }
});

function TimelineGraph(parent, filter) {
  MarkersOverview.call(this, parent, filter);
}

TimelineGraph.prototype = Heritage.extend(MarkersOverview.prototype, {
  headerHeight: MARKERS_GRAPH_HEADER_HEIGHT,
  rowHeight: MARKERS_GRAPH_ROW_HEIGHT,
  groupPadding: MARKERS_GROUP_VERTICAL_PADDING,
  setPerformanceData: MarkersOverview.prototype.setData
});

/**
 * Definitions file for GraphsController, indicating the constructor,
 * selector and other meta for each of the graphs controller by
 * GraphsController.
 */
const GRAPH_DEFINITIONS = {
  memory: {
    constructor: MemoryGraph,
    selector: "#memory-overview",
  },
  framerate: {
    constructor: FramerateGraph,
    selector: "#time-framerate",
  },
  timeline: {
    constructor: TimelineGraph,
    selector: "#markers-overview",
    primaryLink: true
  }
};

/**
 * A controller for orchestrating the performance's tool overview graphs. Constructs,
 * syncs, toggles displays and defines the memory, framerate and timeline view.
 *
 * @param {object} definition
 * @param {DOMElement} root
 * @param {function} getFilter
 * @param {function} getTheme
 */
function GraphsController ({ definition, root, getFilter, getTheme }) {
  this._graphs = {};
  this._enabled = new Set();
  this._definition = definition || GRAPH_DEFINITIONS;
  this._root = root;
  this._getFilter = getFilter;
  this._getTheme = getTheme;
  this._primaryLink = Object.keys(this._definition).filter(name => this._definition[name].primaryLink)[0];
  this.$ = root.ownerDocument.querySelector.bind(root.ownerDocument);

  EventEmitter.decorate(this);
  this._onSelecting = this._onSelecting.bind(this);
}

GraphsController.prototype = {

  /**
   * Returns the corresponding graph by `graphName`.
   */
  get: function (graphName) {
    return this._graphs[graphName];
  },

  /**
   * Iterates through all graphs and renders the data
   * from a RecordingModel. Takes a resolution value used in
   * some graphs.
   * Saves rendering progress as a promise to be consumed by `destroy`,
   * to wait for cleaning up rendering during destruction.
   */
  render: Task.async(function *(recordingData, resolution) {
    // Get the previous render promise so we don't start rendering
    // until the previous render cycle completes, which can occur
    // especially when a recording is finished, and triggers a
    // fresh rendering at a higher rate
    yield (this._rendering && this._rendering.promise);

    // Check after yielding to ensure we're not tearing down,
    // as this can create a race condition in tests
    if (this._destroyed) {
      return;
    }

    this._rendering = promise.defer();
    for (let graph of (yield this._getEnabled())) {
      yield graph.setPerformanceData(recordingData, resolution);
      this.emit("rendered", graph.graphName);
    }
    this._rendering.resolve();
  }),

  /**
   * Destroys the underlying graphs.
   */
  destroy: Task.async(function *() {
    let primary = this._getPrimaryLink();

    this._destroyed = true;

    if (primary) {
      primary.off("selecting", this._onSelecting);
    }

    // If there was rendering, wait until the most recent render cycle
    // has finished
    if (this._rendering) {
      yield this._rendering.promise;
    }

    for (let graph of this.getWidgets()) {
      yield graph.destroy();
    }
  }),

  /**
   * Applies the theme to the underlying graphs. Optionally takes
   * a `redraw` boolean in the options to force redraw.
   */
  setTheme: function (options={}) {
    let theme = options.theme || this._getTheme();
    for (let graph of this.getWidgets()) {
      graph.setTheme(theme);
      graph.refresh({ force: options.redraw });
    }
  },

  /**
   * Sets up the graph, if needed. Returns a promise resolving
   * to the graph if it is enabled once it's ready, or otherwise returns
   * null if disabled.
   */
  isAvailable: Task.async(function *(graphName) {
    if (!this._enabled.has(graphName)) {
      return null;
    }

    let graph = this.get(graphName);

    if (!graph) {
      graph = yield this._construct(graphName);
    }

    yield graph.ready();
    return graph;
  }),

  /**
   * Enable or disable a subgraph controlled by GraphsController.
   * This determines what graphs are visible and get rendered.
   */
  enable: function (graphName, isEnabled) {
    let el = this.$(this._definition[graphName].selector);
    el.classList[isEnabled ? "remove" : "add"]("hidden");

    // If no status change, just return
    if (this._enabled.has(graphName) === isEnabled) {
      return;
    }
    if (isEnabled) {
      this._enabled.add(graphName);
    } else {
      this._enabled.delete(graphName);
    }

    // Invalidate our cache of ready-to-go graphs
    this._enabledGraphs = null;
  },

  /**
   * Disables all graphs controller by the GraphsController, and
   * also hides the root element. This is a one way switch, and used
   * when older platforms do not have any timeline data.
   */
  disableAll: function () {
    this._root.classList.add("hidden");
    // Hide all the subelements
    Object.keys(this._definition).forEach(graphName => this.enable(graphName, false));
  },

  /**
   * Sets a mapped selection on the graph that is the main controller
   * for keeping the graphs' selections in sync.
   */
  setMappedSelection: function (selection, { mapStart, mapEnd }) {
    return this._getPrimaryLink().setMappedSelection(selection, { mapStart, mapEnd });
  },

  /**
   * Fetches the currently mapped selection. If graphs are not yet rendered,
   * (which throws in Graphs.js), return null.
   */
  getMappedSelection: function ({ mapStart, mapEnd }) {
    let primary = this._getPrimaryLink();
    if (primary && primary.hasData()) {
      return primary.getMappedSelection({ mapStart, mapEnd });
    } else {
      return null;
    }
  },

  /**
   * Returns an array of graphs that have been created, not necessarily
   * enabled currently.
   */
  getWidgets: function () {
    return Object.keys(this._graphs).map(name => this._graphs[name]);
  },

  /**
   * Drops the selection.
   */
  dropSelection: function () {
    if (this._getPrimaryLink()) {
      return this._getPrimaryLink().dropSelection();
    }
  },

  /**
   * Makes sure the selection is enabled or disabled in all the graphs.
   */
  selectionEnabled: Task.async(function *(enabled) {
    for (let graph of (yield this._getEnabled())) {
      graph.selectionEnabled = enabled;
    }
  }),

  /**
   * Creates the graph `graphName` and initializes it.
   */
  _construct: Task.async(function *(graphName) {
    let def = this._definition[graphName];
    let el = this.$(def.selector);
    let filter = this._getFilter();
    let graph = this._graphs[graphName] = new def.constructor(el, filter);
    graph.graphName = graphName;

    yield graph.ready();

    // Sync the graphs' animations and selections together
    if (def.primaryLink) {
      graph.on("selecting", this._onSelecting);
    } else {
      CanvasGraphUtils.linkAnimation(this._getPrimaryLink(), graph);
      CanvasGraphUtils.linkSelection(this._getPrimaryLink(), graph);
    }

    // Sets the container element's visibility based off of enabled status
    el.classList[this._enabled.has(graphName) ? "remove" : "add"]("hidden");

    this.setTheme();
    return graph;
  }),

  /**
   * Returns the main graph for this collection, that all graphs
   * are bound to for syncing and selection.
   */
  _getPrimaryLink: function () {
    return this.get(this._primaryLink);
  },

  /**
   * Emitted when a selection occurs.
   */
  _onSelecting: function () {
    this.emit("selecting");
  },

  /**
   * Resolves to an array with all graphs that are enabled, and
   * creates them if needed. Different than just iterating over `this._graphs`,
   * as those could be enabled. Uses caching, as rendering happens many times per second,
   * compared to how often which graphs/features are changed (rarely).
   */
  _getEnabled: Task.async(function *() {
    if (this._enabledGraphs) {
      return this._enabledGraphs;
    }
    let enabled = [];
    for (let graphName of this._enabled) {
      let graph;
      if (graph = yield this.isAvailable(graphName)) {
        enabled.push(graph);
      }
    }
    return this._enabledGraphs = enabled;
  }),
};

/**
 * A base class for performance graphs to inherit from.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the overview.
 * @param string metric
 *        The unit of measurement for this graph.
 */
function OptimizationsGraph(parent) {
  MountainGraphWidget.call(this, parent);
  this.setTheme();
}

OptimizationsGraph.prototype = Heritage.extend(MountainGraphWidget.prototype, {

  render: Task.async(function *(threadNode, frameNode) {
    // Regardless if we draw or clear the graph, wait
    // until it's ready.
    yield this.ready();

    if (!threadNode || !frameNode) {
      this.setData([]);
      return;
    }

    let { sampleTimes } = threadNode;

    if (!sampleTimes.length) {
      this.setData([]);
      return;
    }

    // Take startTime/endTime from samples recorded, rather than
    // using duration directly from threadNode, as the first sample that
    // equals the startTime does not get recorded.
    let startTime = sampleTimes[0];
    let endTime = sampleTimes[sampleTimes.length - 1];

    let bucketSize = (endTime - startTime) / OPTIMIZATIONS_GRAPH_RESOLUTION;
    let data = createTierGraphDataFromFrameNode(frameNode, sampleTimes, bucketSize);

    // If for some reason we don't have data (like the frameNode doesn't
    // have optimizations, but it shouldn't be at this point if it doesn't),
    // log an error.
    if (!data) {
      Cu.reportError(`FrameNode#${frameNode.location} does not have optimizations data to render.`);
      return;
    }

    this.dataOffsetX = startTime;
    yield this.setData(data);
  }),

  /**
   * Sets the theme via `theme` to either "light" or "dark",
   * and updates the internal styling to match. Requires a redraw
   * to see the effects.
   */
  setTheme: function (theme) {
    theme = theme || "light";

    let interpreterColor = getColor("graphs-red", theme);
    let baselineColor = getColor("graphs-blue", theme);
    let ionColor = getColor("graphs-green", theme);

    this.format = [
      { color: interpreterColor },
      { color: baselineColor },
      { color: ionColor },
    ];

    this.backgroundColor = getColor("sidebar-background", theme);
  }
});

exports.OptimizationsGraph = OptimizationsGraph;
exports.FramerateGraph = FramerateGraph;
exports.MemoryGraph = MemoryGraph;
exports.TimelineGraph = TimelineGraph;
exports.GraphsController = GraphsController;

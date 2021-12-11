/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains the base line graph that all Performance line graphs use.
 */

const { extend } = require("devtools/shared/extend");
const LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");
const MountainGraphWidget = require("devtools/client/shared/widgets/MountainGraphWidget");
const { CanvasGraphUtils } = require("devtools/client/shared/widgets/Graphs");

const EventEmitter = require("devtools/shared/event-emitter");

const { colorUtils } = require("devtools/shared/css/color");
const { getColor } = require("devtools/client/shared/theme");
const ProfilerGlobal = require("devtools/client/performance/modules/global");
const {
  MarkersOverview,
} = require("devtools/client/performance/modules/widgets/markers-overview");
const {
  createTierGraphDataFromFrameNode,
} = require("devtools/client/performance/modules/logic/jit");

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
 * @param Node parent
 *        The parent node holding the overview.
 * @param string metric
 *        The unit of measurement for this graph.
 */
function PerformanceGraph(parent, metric) {
  LineGraphWidget.call(this, parent, { metric });
  this.setTheme();
}

PerformanceGraph.prototype = extend(LineGraphWidget.prototype, {
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
  setTheme: function(theme) {
    theme = theme || "light";
    const mainColor = getColor(this.mainColor || "graphs-blue", theme);
    this.backgroundColor = getColor("body-background", theme);
    this.strokeColor = mainColor;
    this.backgroundGradientStart = colorUtils.setAlpha(mainColor, 0.2);
    this.backgroundGradientEnd = colorUtils.setAlpha(mainColor, 0.2);
    this.selectionBackgroundColor = colorUtils.setAlpha(
      getColor(SELECTION_BACKGROUND_COLOR_NAME, theme),
      0.25
    );
    this.selectionStripesColor = "rgba(255, 255, 255, 0.1)";
    this.maximumLineColor = colorUtils.setAlpha(mainColor, 0.4);
    this.averageLineColor = colorUtils.setAlpha(mainColor, 0.7);
    this.minimumLineColor = colorUtils.setAlpha(mainColor, 0.9);
  },
});

/**
 * Constructor for the framerate graph. Inherits from PerformanceGraph.
 *
 * @param Node parent
 *        The parent node holding the overview.
 */
function FramerateGraph(parent) {
  PerformanceGraph.call(this, parent, ProfilerGlobal.L10N.getStr("graphs.fps"));
}

FramerateGraph.prototype = extend(PerformanceGraph.prototype, {
  mainColor: FRAMERATE_GRAPH_COLOR_NAME,
  setPerformanceData: function({ duration, ticks }, resolution) {
    this.dataDuration = duration;
    return this.setDataFromTimestamps(ticks, resolution, duration);
  },
});

/**
 * Constructor for the memory graph. Inherits from PerformanceGraph.
 *
 * @param Node parent
 *        The parent node holding the overview.
 */
function MemoryGraph(parent) {
  PerformanceGraph.call(
    this,
    parent,
    ProfilerGlobal.L10N.getStr("graphs.memory")
  );
}

MemoryGraph.prototype = extend(PerformanceGraph.prototype, {
  mainColor: MEMORY_GRAPH_COLOR_NAME,
  setPerformanceData: function({ duration, memory }) {
    this.dataDuration = duration;
    return this.setData(memory);
  },
});

function TimelineGraph(parent, filter) {
  MarkersOverview.call(this, parent, filter);
}

TimelineGraph.prototype = extend(MarkersOverview.prototype, {
  headerHeight: MARKERS_GRAPH_HEADER_HEIGHT,
  rowHeight: MARKERS_GRAPH_ROW_HEIGHT,
  groupPadding: MARKERS_GROUP_VERTICAL_PADDING,
  setPerformanceData: MarkersOverview.prototype.setData,
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
    primaryLink: true,
  },
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
function GraphsController({ definition, root, getFilter, getTheme }) {
  this._graphs = {};
  this._enabled = new Set();
  this._definition = definition || GRAPH_DEFINITIONS;
  this._root = root;
  this._getFilter = getFilter;
  this._getTheme = getTheme;
  this._primaryLink = Object.keys(this._definition).filter(
    name => this._definition[name].primaryLink
  )[0];
  this.$ = root.ownerDocument.querySelector.bind(root.ownerDocument);

  EventEmitter.decorate(this);
  this._onSelecting = this._onSelecting.bind(this);
}

GraphsController.prototype = {
  /**
   * Returns the corresponding graph by `graphName`.
   */
  get: function(graphName) {
    return this._graphs[graphName];
  },

  /**
   * Iterates through all graphs and renders the data
   * from a RecordingModel. Takes a resolution value used in
   * some graphs.
   * Saves rendering progress as a promise to be consumed by `destroy`,
   * to wait for cleaning up rendering during destruction.
   */
  async render(recordingData, resolution) {
    // Get the previous render promise so we don't start rendering
    // until the previous render cycle completes, which can occur
    // especially when a recording is finished, and triggers a
    // fresh rendering at a higher rate
    await this._rendering;

    // Check after yielding to ensure we're not tearing down,
    // as this can create a race condition in tests
    if (this._destroyed) {
      return;
    }

    this._rendering = (async () => {
      for (const graph of await this._getEnabled()) {
        await graph.setPerformanceData(recordingData, resolution);
        this.emit("rendered", graph.graphName);
      }
    })();
    await this._rendering;
  },

  /**
   * Destroys the underlying graphs.
   */
  async destroy() {
    const primary = this._getPrimaryLink();

    this._destroyed = true;

    if (primary) {
      primary.off("selecting", this._onSelecting);
    }

    // If there was rendering, wait until the most recent render cycle
    // has finished
    if (this._rendering) {
      await this._rendering;
    }

    for (const graph of this.getWidgets()) {
      await graph.destroy();
    }
  },

  /**
   * Applies the theme to the underlying graphs. Optionally takes
   * a `redraw` boolean in the options to force redraw.
   */
  setTheme: function(options = {}) {
    const theme = options.theme || this._getTheme();
    for (const graph of this.getWidgets()) {
      graph.setTheme(theme);
      graph.refresh({ force: options.redraw });
    }
  },

  /**
   * Sets up the graph, if needed. Returns a promise resolving
   * to the graph if it is enabled once it's ready, or otherwise returns
   * null if disabled.
   */
  async isAvailable(graphName) {
    if (!this._enabled.has(graphName)) {
      return null;
    }

    let graph = this.get(graphName);

    if (!graph) {
      graph = await this._construct(graphName);
    }

    await graph.ready();
    return graph;
  },

  /**
   * Enable or disable a subgraph controlled by GraphsController.
   * This determines what graphs are visible and get rendered.
   */
  enable: function(graphName, isEnabled) {
    const el = this.$(this._definition[graphName].selector);
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
  disableAll: function() {
    this._root.classList.add("hidden");
    // Hide all the subelements
    Object.keys(this._definition).forEach(graphName =>
      this.enable(graphName, false)
    );
  },

  /**
   * Sets a mapped selection on the graph that is the main controller
   * for keeping the graphs' selections in sync.
   */
  setMappedSelection: function(selection, { mapStart, mapEnd }) {
    return this._getPrimaryLink().setMappedSelection(selection, {
      mapStart,
      mapEnd,
    });
  },

  /**
   * Fetches the currently mapped selection. If graphs are not yet rendered,
   * (which throws in Graphs.js), return null.
   */
  getMappedSelection: function({ mapStart, mapEnd }) {
    const primary = this._getPrimaryLink();
    if (primary && primary.hasData()) {
      return primary.getMappedSelection({ mapStart, mapEnd });
    }
    return null;
  },

  /**
   * Returns an array of graphs that have been created, not necessarily
   * enabled currently.
   */
  getWidgets: function() {
    return Object.keys(this._graphs).map(name => this._graphs[name]);
  },

  /**
   * Drops the selection.
   */
  dropSelection: function() {
    if (this._getPrimaryLink()) {
      return this._getPrimaryLink().dropSelection();
    }
    return null;
  },

  /**
   * Makes sure the selection is enabled or disabled in all the graphs.
   */
  async selectionEnabled(enabled) {
    for (const graph of await this._getEnabled()) {
      graph.selectionEnabled = enabled;
    }
  },

  /**
   * Creates the graph `graphName` and initializes it.
   */
  async _construct(graphName) {
    const def = this._definition[graphName];
    const el = this.$(def.selector);
    const filter = this._getFilter();
    const graph = (this._graphs[graphName] = new def.constructor(el, filter));
    graph.graphName = graphName;

    await graph.ready();

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
  },

  /**
   * Returns the main graph for this collection, that all graphs
   * are bound to for syncing and selection.
   */
  _getPrimaryLink: function() {
    return this.get(this._primaryLink);
  },

  /**
   * Emitted when a selection occurs.
   */
  _onSelecting: function() {
    this.emit("selecting");
  },

  /**
   * Resolves to an array with all graphs that are enabled, and
   * creates them if needed. Different than just iterating over `this._graphs`,
   * as those could be enabled. Uses caching, as rendering happens many times per second,
   * compared to how often which graphs/features are changed (rarely).
   */
  async _getEnabled() {
    if (this._enabledGraphs) {
      return this._enabledGraphs;
    }
    const enabled = [];
    for (const graphName of this._enabled) {
      const graph = await this.isAvailable(graphName);
      if (graph) {
        enabled.push(graph);
      }
    }
    this._enabledGraphs = enabled;
    return this._enabledGraphs;
  },
};

/**
 * A base class for performance graphs to inherit from.
 *
 * @param Node parent
 *        The parent node holding the overview.
 * @param string metric
 *        The unit of measurement for this graph.
 */
function OptimizationsGraph(parent) {
  MountainGraphWidget.call(this, parent);
  this.setTheme();
}

OptimizationsGraph.prototype = extend(MountainGraphWidget.prototype, {
  async render(threadNode, frameNode) {
    // Regardless if we draw or clear the graph, wait
    // until it's ready.
    await this.ready();

    if (!threadNode || !frameNode) {
      this.setData([]);
      return;
    }

    const { sampleTimes } = threadNode;

    if (!sampleTimes.length) {
      this.setData([]);
      return;
    }

    // Take startTime/endTime from samples recorded, rather than
    // using duration directly from threadNode, as the first sample that
    // equals the startTime does not get recorded.
    const startTime = sampleTimes[0];
    const endTime = sampleTimes[sampleTimes.length - 1];

    const bucketSize = (endTime - startTime) / OPTIMIZATIONS_GRAPH_RESOLUTION;
    const data = createTierGraphDataFromFrameNode(
      frameNode,
      sampleTimes,
      bucketSize
    );

    // If for some reason we don't have data (like the frameNode doesn't
    // have optimizations, but it shouldn't be at this point if it doesn't),
    // log an error.
    if (!data) {
      console.error(
        `FrameNode#${frameNode.location} does not have optimizations data to render.`
      );
      return;
    }

    this.dataOffsetX = startTime;
    await this.setData(data);
  },

  /**
   * Sets the theme via `theme` to either "light" or "dark",
   * and updates the internal styling to match. Requires a redraw
   * to see the effects.
   */
  setTheme: function(theme) {
    theme = theme || "light";

    const interpreterColor = getColor("graphs-red", theme);
    const baselineColor = getColor("graphs-blue", theme);
    const ionColor = getColor("graphs-green", theme);

    this.format = [
      { color: interpreterColor },
      { color: baselineColor },
      { color: ionColor },
    ];

    this.backgroundColor = getColor("sidebar-background", theme);
  },
});

exports.OptimizationsGraph = OptimizationsGraph;
exports.FramerateGraph = FramerateGraph;
exports.MemoryGraph = MemoryGraph;
exports.TimelineGraph = TimelineGraph;
exports.GraphsController = GraphsController;

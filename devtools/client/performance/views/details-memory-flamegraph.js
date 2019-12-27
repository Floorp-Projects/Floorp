/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals $, PerformanceController, OverviewView */
"use strict";

const {
  FlameGraph,
  FlameGraphUtils,
} = require("devtools/client/shared/widgets/FlameGraph");
const { extend } = require("devtools/shared/extend");
const RecordingUtils = require("devtools/shared/performance/recording-utils");
const EventEmitter = require("devtools/shared/event-emitter");

const EVENTS = require("../events");
const { DetailsSubview } = require("./details-abstract-subview");
const { L10N } = require("../modules/global");

/**
 * FlameGraph view containing a pyramid-like visualization of memory allocation
 * sites, controlled by DetailsView.
 */
const MemoryFlameGraphView = extend(DetailsSubview, {
  shouldUpdateWhileMouseIsActive: true,

  rerenderPrefs: [
    "invert-flame-graph",
    "flatten-tree-recursion",
    "show-idle-blocks",
  ],

  /**
   * Sets up the view with event binding.
   */
  async initialize() {
    DetailsSubview.initialize.call(this);

    this.graph = new FlameGraph($("#memory-flamegraph-view"));
    this.graph.timelineTickUnits = L10N.getStr("graphs.ms");
    this.graph.setTheme(PerformanceController.getTheme());
    await this.graph.ready();

    this._onRangeChangeInGraph = this._onRangeChangeInGraph.bind(this);
    this._onThemeChanged = this._onThemeChanged.bind(this);

    PerformanceController.on(EVENTS.THEME_CHANGED, this._onThemeChanged);
    this.graph.on("selecting", this._onRangeChangeInGraph);
  },

  /**
   * Unbinds events.
   */
  async destroy() {
    DetailsSubview.destroy.call(this);

    PerformanceController.off(EVENTS.THEME_CHANGED, this._onThemeChanged);
    this.graph.off("selecting", this._onRangeChangeInGraph);

    await this.graph.destroy();
  },

  /**
   * Method for handling all the set up for rendering a new flamegraph.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function(interval = {}) {
    const recording = PerformanceController.getCurrentRecording();
    const duration = recording.getDuration();
    const allocations = recording.getAllocations();

    const thread = RecordingUtils.getProfileThreadFromAllocations(allocations);
    const data = FlameGraphUtils.createFlameGraphDataFromThread(thread, {
      invertStack: PerformanceController.getOption("invert-flame-graph"),
      flattenRecursion: PerformanceController.getOption(
        "flatten-tree-recursion"
      ),
      showIdleBlocks:
        PerformanceController.getOption("show-idle-blocks") &&
        L10N.getStr("table.idle"),
    });

    this.graph.setData({
      data,
      bounds: {
        startTime: 0,
        endTime: duration,
      },
      visible: {
        startTime: interval.startTime || 0,
        endTime: interval.endTime || duration,
      },
    });

    this.emit(EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  },

  /**
   * Fired when a range is selected or cleared in the FlameGraph.
   */
  _onRangeChangeInGraph: function() {
    const interval = this.graph.getViewRange();

    // Squelch rerendering this view when we update the range here
    // to avoid recursion, as our FlameGraph handles rerendering itself
    // when originating from within the graph.
    this.requiresUpdateOnRangeChange = false;
    OverviewView.setTimeInterval(interval);
    this.requiresUpdateOnRangeChange = true;
  },

  /**
   * Called whenever a pref is changed and this view needs to be rerendered.
   */
  _onRerenderPrefChanged: function() {
    const recording = PerformanceController.getCurrentRecording();
    const allocations = recording.getAllocations();
    const thread = RecordingUtils.getProfileThreadFromAllocations(allocations);
    FlameGraphUtils.removeFromCache(thread);
  },

  /**
   * Called when `devtools.theme` changes.
   */
  _onThemeChanged: function(theme) {
    this.graph.setTheme(theme);
    this.graph.refresh({ force: true });
  },

  toString: () => "[object MemoryFlameGraphView]",
});

EventEmitter.decorate(MemoryFlameGraphView);

exports.MemoryFlameGraphView = MemoryFlameGraphView;

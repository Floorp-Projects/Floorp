/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * FlameGraph view containing a pyramid-like visualization of memory allocation
 * sites, controlled by DetailsView.
 */
let MemoryFlameGraphView = Heritage.extend(DetailsSubview, {

  shouldUpdateWhileMouseIsActive: true,

  rerenderPrefs: [
    "invert-flame-graph",
    "flatten-tree-recursion",
    "show-idle-blocks"
  ],

  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function* () {
    DetailsSubview.initialize.call(this);

    this.graph = new FlameGraph($("#memory-flamegraph-view"));
    this.graph.timelineTickUnits = L10N.getStr("graphs.ms");
    this.graph.setTheme(PerformanceController.getTheme());
    yield this.graph.ready();

    this._onRangeChangeInGraph = this._onRangeChangeInGraph.bind(this);
    this._onThemeChanged = this._onThemeChanged.bind(this);

    PerformanceController.on(EVENTS.THEME_CHANGED, this._onThemeChanged);
    this.graph.on("selecting", this._onRangeChangeInGraph);
  }),

  /**
   * Unbinds events.
   */
  destroy: Task.async(function* () {
    DetailsSubview.destroy.call(this);

    PerformanceController.off(EVENTS.THEME_CHANGED, this._onThemeChanged);
    this.graph.off("selecting", this._onRangeChangeInGraph);

    yield this.graph.destroy();
  }),

  /**
   * Method for handling all the set up for rendering a new flamegraph.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function (interval={}) {
    let recording = PerformanceController.getCurrentRecording();
    let duration = recording.getDuration();
    let allocations = recording.getAllocations();

    let samples = RecordingUtils.getSamplesFromAllocations(allocations);
    let data = FlameGraphUtils.createFlameGraphDataFromSamples(samples, {
      invertStack: PerformanceController.getOption("invert-flame-graph"),
      flattenRecursion: PerformanceController.getOption("flatten-tree-recursion"),
      showIdleBlocks: PerformanceController.getOption("show-idle-blocks") && L10N.getStr("table.idle")
    });

    this.graph.setData({ data,
      bounds: {
        startTime: 0,
        endTime: duration
      },
      visible: {
        startTime: interval.startTime || 0,
        endTime: interval.endTime || duration
      }
    });

    this.emit(EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  },

  /**
   * Fired when a range is selected or cleared in the FlameGraph.
   */
  _onRangeChangeInGraph: function () {
    let interval = this.graph.getViewRange();
    OverviewView.setTimeInterval(interval, { stopPropagation: true });
  },

  /**
   * Called whenever a pref is changed and this view needs to be rerendered.
   */
  _onRerenderPrefChanged: function() {
    let recording = PerformanceController.getCurrentRecording();
    let allocations = recording.getAllocations();
    let samples = RecordingUtils.getSamplesFromAllocations(allocations);
    FlameGraphUtils.removeFromCache(samples);
  },

  /**
   * Called when `devtools.theme` changes.
   */
  _onThemeChanged: function (_, theme) {
    this.graph.setTheme(theme);
    this.graph.refresh({ force: true });
  },

  toString: () => "[object MemoryFlameGraphView]"
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * FlameGraph view containing a pyramid-like visualization of a profile,
 * controlled by DetailsView.
 */
let JsFlameGraphView = Heritage.extend(DetailsSubview, {

  rerenderPrefs: [
    "invert-flame-graph",
    "flatten-tree-recursion",
    "show-platform-data",
    "show-idle-blocks"
  ],

  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function* () {
    DetailsSubview.initialize.call(this);

    this.graph = new FlameGraph($("#js-flamegraph-view"));
    this.graph.timelineTickUnits = L10N.getStr("graphs.ms");
    yield this.graph.ready();

    this._onRangeChangeInGraph = this._onRangeChangeInGraph.bind(this);

    this.graph.on("selecting", this._onRangeChangeInGraph);
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    DetailsSubview.destroy.call(this);

    this.graph.off("selecting", this._onRangeChangeInGraph);
  },

  /**
   * Method for handling all the set up for rendering a new flamegraph.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function (interval={}) {
    let recording = PerformanceController.getCurrentRecording();
    let duration = recording.getDuration();
    let profile = recording.getProfile();
    let samples = profile.threads[0].samples;

    let data = FlameGraphUtils.createFlameGraphDataFromSamples(samples, {
      invertStack: PerformanceController.getPref("invert-flame-graph"),
      flattenRecursion: PerformanceController.getPref("flatten-tree-recursion"),
      filterFrames: !PerformanceController.getPref("show-platform-data") && FrameNode.isContent,
      showIdleBlocks: PerformanceController.getPref("show-idle-blocks") && L10N.getStr("table.idle")
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

    this.emit(EVENTS.JS_FLAMEGRAPH_RENDERED);
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
    let profile = recording.getProfile();
    let samples = profile.threads[0].samples;
    FlameGraphUtils.removeFromCache(samples);
  },

  toString: () => "[object JsFlameGraphView]"
});

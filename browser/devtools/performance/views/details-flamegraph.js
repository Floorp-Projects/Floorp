/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * FlameGraph view containing a pyramid-like visualization of a profile,
 * controlled by DetailsView.
 */
let FlameGraphView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function* () {
    this._onRecordingStoppedOrSelected = this._onRecordingStoppedOrSelected.bind(this);
    this._onRangeChange = this._onRangeChange.bind(this);

    this.graph = new FlameGraph($("#flamegraph-view"));
    this.graph.timelineTickUnits = L10N.getStr("graphs.ms");
    yield this.graph.ready();

    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
  },

  /**
   * Method for handling all the set up for rendering a new flamegraph.
   */
  render: function (profilerData) {
    // Empty recordings might yield no profiler data.
    if (profilerData.profile == null) {
      return;
    }
    let samples = profilerData.profile.threads[0].samples;
    let dataSrc = FlameGraphUtils.createFlameGraphDataFromSamples(samples, {
      flattenRecursion: Prefs.flattenTreeRecursion,
      filterFrames: !Prefs.showPlatformData && FrameNode.isContent,
      showIdleBlocks: Prefs.showIdleBlocks && L10N.getStr("table.idle")
    });
    this.graph.setData(dataSrc);
    this.emit(EVENTS.FLAMEGRAPH_RENDERED);
  },

  /**
   * Called when recording is stopped or selected.
   */
  _onRecordingStoppedOrSelected: function (_, recording) {
    // If not recording, then this recording is done and we can render all of it
    if (!recording.isRecording()) {
      let profilerData = recording.getProfilerData();
      this.render(profilerData);
    }
  },

  /**
   * Fired when a range is selected or cleared in the OverviewView.
   */
  _onRangeChange: function (_, params) {
    // TODO bug 1105014
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(FlameGraphView);

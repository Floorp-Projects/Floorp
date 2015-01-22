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
    this._onRangeChangeInGraph = this._onRangeChangeInGraph.bind(this);
    this._onDetailsViewSelected = this._onDetailsViewSelected.bind(this);

    this.graph = new FlameGraph($("#flamegraph-view"));
    this.graph.timelineTickUnits = L10N.getStr("graphs.ms");
    yield this.graph.ready();

    this.graph.on("selecting", this._onRangeChangeInGraph);

    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    DetailsView.on(EVENTS.DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    this.graph.off("selecting", this._onRangeChangeInGraph);

    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    DetailsView.off(EVENTS.DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);
  },

  /**
   * Method for handling all the set up for rendering a new flamegraph.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function (interval={}) {
    let recording = PerformanceController.getCurrentRecording();
    let startTime = interval.startTime || 0;
    let endTime = interval.endTime || recording.getDuration();
    this.graph.setViewRange({ startTime, endTime });
    this.emit(EVENTS.FLAMEGRAPH_RENDERED);
  },

  /**
   * Called when recording is stopped or selected.
   */
  _onRecordingStoppedOrSelected: function (_, recording) {
    if (recording.isRecording()) {
      return;
    }
    let profile = recording.getProfile();
    let samples = profile.threads[0].samples;
    let data = FlameGraphUtils.createFlameGraphDataFromSamples(samples, {
      flattenRecursion: Prefs.flattenTreeRecursion,
      filterFrames: !Prefs.showPlatformData && FrameNode.isContent,
      showIdleBlocks: Prefs.showIdleBlocks && L10N.getStr("table.idle")
    });
    let startTime = 0;
    let endTime = recording.getDuration();
    this.graph.setData({ data, bounds: { startTime, endTime } });
    this.render();
  },

  /**
   * Fired when a range is selected or cleared in the OverviewView.
   */
  _onRangeChange: function (_, interval) {
    if (DetailsView.isViewSelected(this)) {
      this.render(interval);
    } else {
      this._dirty = true;
      this._interval = interval;
    }
  },

  /**
   * Fired when a range is selected or cleared in the FlameGraph.
   */
  _onRangeChangeInGraph: function () {
    let interval = this.graph.getViewRange();
    OverviewView.setTimeInterval(interval, { stopPropagation: true });
  },

  /**
   * Fired when a view is selected in the DetailsView.
   */
  _onDetailsViewSelected: function() {
    if (DetailsView.isViewSelected(this) && this._dirty) {
      this.render(this._interval);
      this._dirty = false;
    }
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(FlameGraphView);

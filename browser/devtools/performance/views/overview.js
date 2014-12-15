/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// No sense updating the overview more often than receiving data from the
// backend. Make sure this isn't lower than DEFAULT_TIMELINE_DATA_PULL_TIMEOUT
// in toolkit/devtools/server/actors/timeline.js
const OVERVIEW_UPDATE_INTERVAL = 200; // ms

const FRAMERATE_GRAPH_LOW_RES_INTERVAL = 100; // ms
const FRAMERATE_GRAPH_HIGH_RES_INTERVAL = 16; // ms

const FRAMERATE_GRAPH_HEIGHT = 45; // px
const MARKERS_GRAPH_HEADER_HEIGHT = 12; // px
const MARKERS_GRAPH_BODY_HEIGHT = 45; // 9px * 5 groups
const MARKERS_GROUP_VERTICAL_PADDING = 3.5; // px
const MEMORY_GRAPH_HEIGHT = 30; // px

const GRAPH_SCROLL_EVENTS_DRAIN = 50; // ms

/**
 * View handler for the overview panel's time view, displaying
 * framerate over time.
 */
let OverviewView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function *() {
    this._start = this._start.bind(this);
    this._stop = this._stop.bind(this);
    this._onRecordingTick = this._onRecordingTick.bind(this);
    this._onGraphMouseUp = this._onGraphMouseUp.bind(this);
    this._onGraphScroll = this._onGraphScroll.bind(this);

    yield this._showFramerateGraph();
    yield this._showMarkersGraph();
    yield this._showMemoryGraph();

    this.framerateGraph.on("mouseup", this._onGraphMouseUp);
    this.framerateGraph.on("scroll", this._onGraphScroll);
    this.markersOverview.on("mouseup", this._onGraphMouseUp);
    this.markersOverview.on("scroll", this._onGraphScroll);
    this.memoryOverview.on("mouseup", this._onGraphMouseUp);
    this.memoryOverview.on("scroll", this._onGraphScroll);

    PerformanceController.on(EVENTS.RECORDING_STARTED, this._start);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._stop);
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    this.framerateGraph.off("mouseup", this._onGraphMouseUp);
    this.framerateGraph.off("scroll", this._onGraphScroll);
    this.markersOverview.off("mouseup", this._onGraphMouseUp);
    this.markersOverview.off("scroll", this._onGraphScroll);
    this.memoryOverview.off("mouseup", this._onGraphMouseUp);
    this.memoryOverview.off("scroll", this._onGraphScroll);

    clearNamedTimeout("graph-scroll");
    PerformanceController.off(EVENTS.RECORDING_STARTED, this._start);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._stop);
  },

  /**
   * Sets up the framerate graph.
   */
  _showFramerateGraph: Task.async(function *() {
    this.framerateGraph = new LineGraphWidget($("#time-framerate"), L10N.getStr("graphs.fps"));
    this.framerateGraph.fixedHeight = FRAMERATE_GRAPH_HEIGHT;
    yield this.framerateGraph.ready();
  }),

  /**
   * Sets up the markers overivew graph.
   */
  _showMarkersGraph: Task.async(function *() {
    this.markersOverview = new MarkersOverview($("#markers-overview"));
    this.markersOverview.headerHeight = MARKERS_GRAPH_HEADER_HEIGHT;
    this.markersOverview.bodyHeight = MARKERS_GRAPH_BODY_HEIGHT;
    this.markersOverview.groupPadding = MARKERS_GROUP_VERTICAL_PADDING;
    yield this.markersOverview.ready();

    CanvasGraphUtils.linkAnimation(this.framerateGraph, this.markersOverview);
    CanvasGraphUtils.linkSelection(this.framerateGraph, this.markersOverview);
  }),

  /**
   * Sets up the memory overview graph.
   */
  _showMemoryGraph: Task.async(function *() {
    this.memoryOverview = new MemoryOverview($("#memory-overview"));
    this.memoryOverview.fixedHeight = MEMORY_GRAPH_HEIGHT;
    yield this.memoryOverview.ready();

    CanvasGraphUtils.linkAnimation(this.framerateGraph, this.memoryOverview);
    CanvasGraphUtils.linkSelection(this.framerateGraph, this.memoryOverview);
  }),

  /**
   * Method for handling all the set up for rendering the overview graphs.
   *
   * @param number resolution
   *        The fps graph resolution. @see Graphs.jsm
   */
  render: Task.async(function *(resolution) {
    let interval = PerformanceController.getInterval();
    let markers = PerformanceController.getMarkers();
    let memory = PerformanceController.getMemory();
    let timestamps = PerformanceController.getTicks();

    // Compute an approximate ending time for the view. This is
    // needed to ensure that the view updates even when new data is
    // not being generated.
    let fakeTime = interval.startTime + interval.localElapsedTime;
    interval.endTime = fakeTime;

    this.markersOverview.setData({ interval, markers });
    this.emit(EVENTS.MARKERS_GRAPH_RENDERED);

    this.memoryOverview.setData({ interval, memory });
    this.emit(EVENTS.MEMORY_GRAPH_RENDERED);

    yield this.framerateGraph.setDataFromTimestamps(timestamps, resolution);
    this.emit(EVENTS.FRAMERATE_GRAPH_RENDERED);

    // Finished rendering all graphs in this overview.
    this.emit(EVENTS.OVERVIEW_RENDERED);
  }),

  /**
   * Called at most every OVERVIEW_UPDATE_INTERVAL milliseconds
   * and uses data fetched from `_onTimelineData` to render
   * data into all the corresponding overview graphs.
   */
  _onRecordingTick: Task.async(function *() {
    yield this.render(FRAMERATE_GRAPH_LOW_RES_INTERVAL);
    this._prepareNextTick();
  }),

  /**
   * Fired when the graph selection has changed. Called by
   * mouseup and scroll events.
   */
  _onSelectionChange: function () {
    if (this.framerateGraph.hasSelection()) {
      let { min: beginAt, max: endAt } = this.framerateGraph.getMappedSelection();
      this.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { beginAt, endAt });
    } else {
      this.emit(EVENTS.OVERVIEW_RANGE_CLEARED);
    }
  },

  /**
   * Listener handling the "mouseup" event for the framerate graph.
   * Fires an event to be handled elsewhere.
   */
  _onGraphMouseUp: function () {
    // Only fire a selection change event if the selection is actually enabled.
    if (this.framerateGraph.selectionEnabled) {
      this._onSelectionChange();
    }
  },

  /**
   * Listener handling the "scroll" event for the framerate graph.
   * Fires an event to be handled elsewhere.
   */
  _onGraphScroll: function () {
    setNamedTimeout("graph-scroll", GRAPH_SCROLL_EVENTS_DRAIN, () => {
      this._onSelectionChange();
    });
  },

  /**
   * Called to refresh the timer to keep firing _onRecordingTick.
   */
  _prepareNextTick: function () {
    // Check here to see if there's still a _timeoutId, incase
    // `stop` was called before the _prepareNextTick call was executed.
    if (this._timeoutId) {
      this._timeoutId = setTimeout(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);
    }
  },

  /**
   * Called when recording starts.
   */
  _start: function () {
    this._timeoutId = setTimeout(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);

    this.framerateGraph.dropSelection();
    this.framerateGraph.selectionEnabled = false;
    this.markersOverview.selectionEnabled = false;
    this.memoryOverview.selectionEnabled = false;
  },

  /**
   * Called when recording stops.
   */
  _stop: function () {
    clearTimeout(this._timeoutId);
    this._timeoutId = null;

    this.render(FRAMERATE_GRAPH_HIGH_RES_INTERVAL);

    this.framerateGraph.selectionEnabled = true;
    this.markersOverview.selectionEnabled = true;
    this.memoryOverview.selectionEnabled = true;
  }
};

// Decorates the OverviewView as an EventEmitter
EventEmitter.decorate(OverviewView);

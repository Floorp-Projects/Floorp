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

const FRAMERATE_GRAPH_HEIGHT = 40; // px
const MARKERS_GRAPH_HEADER_HEIGHT = 14; // px
const MARKERS_GRAPH_ROW_HEIGHT = 10; // px
const MARKERS_GROUP_VERTICAL_PADDING = 4; // px
const MEMORY_GRAPH_HEIGHT = 30; // px

/**
 * View handler for the overview panel's time view, displaying
 * framerate, markers and memory over time.
 */
let OverviewView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function *() {
    this._onRecordingWillStart = this._onRecordingWillStart.bind(this);
    this._onRecordingStarted = this._onRecordingStarted.bind(this);
    this._onRecordingWillStop = this._onRecordingWillStop.bind(this);
    this._onRecordingStopped = this._onRecordingStopped.bind(this);
    this._onRecordingSelected = this._onRecordingSelected.bind(this);
    this._onRecordingTick = this._onRecordingTick.bind(this);
    this._onGraphSelecting = this._onGraphSelecting.bind(this);

    yield this._showMarkersGraph();
    yield this._showMemoryGraph();
    yield this._showFramerateGraph();

    this.markersOverview.on("selecting", this._onGraphSelecting);

    PerformanceController.on(EVENTS.RECORDING_WILL_START, this._onRecordingWillStart);
    PerformanceController.on(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.on(EVENTS.RECORDING_WILL_STOP, this._onRecordingWillStop);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelected);

    // Populate this overview with some dummy initial data.
    this.markersOverview.setData({ duration: 1000, markers: [] });
    this.memoryOverview.setData([]);
    this.framerateGraph.setData([]);
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    this.markersOverview.off("selecting", this._onGraphSelecting);

    PerformanceController.off(EVENTS.RECORDING_WILL_START, this._onRecordingWillStart);
    PerformanceController.off(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.off(EVENTS.RECORDING_WILL_STOP, this._onRecordingWillStop);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingSelected);
  },

  /**
   * Sets the time interval selection for all graphs in this overview.
   *
   * @param object interval
   *        The { starTime, endTime }, in milliseconds.
   */
  setTimeInterval: function(interval, options = {}) {
    let recording = PerformanceController.getCurrentRecording();
    if (recording == null) {
      throw "A recording should be available in order to set the selection."
    }
    let mapStart = () => 0;
    let mapEnd = () => recording.getDuration();
    let selection = { start: interval.startTime, end: interval.endTime };
    this._stopSelectionChangeEventPropagation = options.stopPropagation;
    this.markersOverview.setMappedSelection(selection, { mapStart, mapEnd });
    this._stopSelectionChangeEventPropagation = false;
  },

  /**
   * Gets the time interval selection for all graphs in this overview.
   *
   * @return object
   *         The { starTime, endTime }, in milliseconds.
   */
  getTimeInterval: function() {
    let recording = PerformanceController.getCurrentRecording();
    if (recording == null) {
      throw "A recording should be available in order to get the selection."
    }
    let mapStart = () => 0;
    let mapEnd = () => recording.getDuration();
    let selection = this.markersOverview.getMappedSelection({ mapStart, mapEnd });
    return { startTime: selection.min, endTime: selection.max };
  },

  /**
   * Sets up the framerate graph.
   */
  _showFramerateGraph: Task.async(function *() {
    this.framerateGraph = new LineGraphWidget($("#time-framerate"), {
      metric: L10N.getStr("graphs.fps")
    });
    this.framerateGraph.fixedHeight = FRAMERATE_GRAPH_HEIGHT;
    yield this.framerateGraph.ready();
  }),

  /**
   * Sets up the markers overivew graph.
   */
  _showMarkersGraph: Task.async(function *() {
    this.markersOverview = new MarkersOverview($("#markers-overview"), TIMELINE_BLUEPRINT);
    this.markersOverview.headerHeight = MARKERS_GRAPH_HEADER_HEIGHT;
    this.markersOverview.rowHeight = MARKERS_GRAPH_ROW_HEIGHT;
    this.markersOverview.groupPadding = MARKERS_GROUP_VERTICAL_PADDING;
    yield this.markersOverview.ready();
  }),

  /**
   * Sets up the memory overview graph.
   */
  _showMemoryGraph: Task.async(function *() {
    this.memoryOverview = new MemoryOverview($("#memory-overview"));
    this.memoryOverview.fixedHeight = MEMORY_GRAPH_HEIGHT;
    yield this.memoryOverview.ready();

    CanvasGraphUtils.linkAnimation(this.markersOverview, this.memoryOverview);
    CanvasGraphUtils.linkSelection(this.markersOverview, this.memoryOverview);
  }),

  /**
   * Sets up the framerate graph.
   */
  _showFramerateGraph: Task.async(function *() {
    let metric = L10N.getStr("graphs.fps");
    this.framerateGraph = new LineGraphWidget($("#time-framerate"), { metric });
    this.framerateGraph.fixedHeight = FRAMERATE_GRAPH_HEIGHT;
    yield this.framerateGraph.ready();

    CanvasGraphUtils.linkAnimation(this.markersOverview, this.framerateGraph);
    CanvasGraphUtils.linkSelection(this.markersOverview, this.framerateGraph);
  }),

  /**
   * Method for handling all the set up for rendering the overview graphs.
   *
   * @param number resolution
   *        The fps graph resolution. @see Graphs.jsm
   */
  render: Task.async(function *(resolution) {
    let recording = PerformanceController.getCurrentRecording();
    let duration = recording.getDuration();
    let markers = recording.getMarkers();
    let memory = recording.getMemory();
    let timestamps = recording.getTicks();

    // Empty or older recordings might yield no markers, memory or timestamps.
    if (markers) {
      this.markersOverview.setData({ markers, duration });
      this.emit(EVENTS.MARKERS_GRAPH_RENDERED);
    }
    if (memory) {
      this.memoryOverview.dataDuration = duration;
      this.memoryOverview.setData(memory);
      this.emit(EVENTS.MEMORY_GRAPH_RENDERED);
    }
    if (timestamps) {
      this.framerateGraph.dataDuration = duration;
      yield this.framerateGraph.setDataFromTimestamps(timestamps, resolution);
      this.emit(EVENTS.FRAMERATE_GRAPH_RENDERED);
    }

    // Finished rendering all graphs in this overview.
    this.emit(EVENTS.OVERVIEW_RENDERED);
  }),

  /**
   * Called at most every OVERVIEW_UPDATE_INTERVAL milliseconds
   * and uses data fetched from the controller to render
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
  _onGraphSelecting: function () {
    let recording = PerformanceController.getCurrentRecording();
    if (recording == null || this._stopSelectionChangeEventPropagation) {
      return;
    }
    // If the range is smaller than a pixel (which can happen when performing
    // a click on the graphs), treat this as a cleared selection.
    let interval = this.getTimeInterval();
    if (interval.endTime - interval.startTime < 1) {
      this.emit(EVENTS.OVERVIEW_RANGE_CLEARED);
    } else {
      this.emit(EVENTS.OVERVIEW_RANGE_SELECTED, interval);
    }
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
   * Called when recording will start.
   */
  _onRecordingWillStart: function (_, recording) {
    this._checkSelection(recording);
    this.framerateGraph.dropSelection();
  },

  /**
   * Called when recording actually starts.
   */
  _onRecordingStarted: function (_, recording) {
    this._timeoutId = setTimeout(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);
  },

  /**
   * Called when recording will stop.
   */
  _onRecordingWillStop: function(_, recording) {
    clearTimeout(this._timeoutId);
    this._timeoutId = null;
  },

  /**
   * Called when recording actually stops.
   */
  _onRecordingStopped: function (_, recording) {
    this._checkSelection(recording);
    this.render(FRAMERATE_GRAPH_HIGH_RES_INTERVAL);
  },

  /**
   * Called when a new recording is selected.
   */
  _onRecordingSelected: function (_, recording) {
    this.markersOverview.dropSelection();
    this._checkSelection(recording);

    // If timeout exists, we have something recording, so
    // this will still tick away at rendering. Otherwise, force a render.
    if (!this._timeoutId) {
      this.render(FRAMERATE_GRAPH_HIGH_RES_INTERVAL);
    }
  },

  _checkSelection: function (recording) {
    let selectionEnabled = !recording.isRecording();
    this.markersOverview.selectionEnabled = selectionEnabled;
    this.memoryOverview.selectionEnabled = selectionEnabled;
    this.framerateGraph.selectionEnabled = selectionEnabled;
  }
};

// Decorates the OverviewView as an EventEmitter
EventEmitter.decorate(OverviewView);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");

devtools.lazyRequireGetter(this, "promise");
devtools.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

devtools.lazyRequireGetter(this, "Overview",
  "devtools/timeline/overview", true);
devtools.lazyRequireGetter(this, "Waterfall",
  "devtools/timeline/waterfall", true);

devtools.lazyImporter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

const OVERVIEW_UPDATE_INTERVAL = 200;
const OVERVIEW_INITIAL_SELECTION_RATIO = 0.15;

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When a recording is started or stopped, via the `stopwatch` button.
  RECORDING_STARTED: "Timeline:RecordingStarted",
  RECORDING_ENDED: "Timeline:RecordingEnded",

  // When the overview graph is populated with new markers.
  OVERVIEW_UPDATED: "Timeline:OverviewUpdated",

  // When the waterfall view is populated with new markers.
  WATERFALL_UPDATED: "Timeline:WaterfallUpdated"
};

/**
 * The current target and the timeline front, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Initializes the timeline controller and views.
 */
let startupTimeline = Task.async(function*() {
  yield TimelineView.initialize();
  yield TimelineController.initialize();
});

/**
 * Destroys the timeline controller and views.
 */
let shutdownTimeline = Task.async(function*() {
  yield TimelineView.destroy();
  yield TimelineController.destroy();
  yield gFront.stop();
});

/**
 * Functions handling the timeline frontend controller.
 */
let TimelineController = {
  /**
   * Permanent storage for the markers streamed by the backend.
   */
  _markers: [],

  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this._onRecordingTick = this._onRecordingTick.bind(this);
    this._onMarkers = this._onMarkers.bind(this);
    gFront.on("markers", this._onMarkers);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    gFront.off("markers", this._onMarkers);
  },

  /**
   * Gets the accumulated markers in this recording.
   * @return array.
   */
  getMarkers: function() {
    return this._markers;
  },

  /**
   * Starts/stops the timeline recording and streaming.
   */
  toggleRecording: Task.async(function*() {
    let isRecording = yield gFront.isRecording();
    if (isRecording == false) {
      yield this._startRecording();
    } else {
      yield this._stopRecording();
    }
  }),

  /**
   * Starts the recording, updating the UI as needed.
   */
  _startRecording: function*() {
    TimelineView.handleRecordingStarted();
    let startTime = yield gFront.start();
    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data.  To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    // See _onRecordingTick.
    this._localStartTime = performance.now();

    this._markers = [];
    this._markers.startTime = startTime;
    this._markers.endTime = startTime;
    this._updateId = setInterval(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);
  },

  /**
   * Stops the recording, updating the UI as needed.
   */
  _stopRecording: function*() {
    clearInterval(this._updateId);

    // Sorting markers is only important when displayed in the waterfall.
    this._markers = this._markers.sort((a,b) => (a.start > b.start));

    TimelineView.handleMarkersUpdate(this._markers);
    TimelineView.handleRecordingEnded();
    yield gFront.stop();
  },

  /**
   * Used in tests. Stops the recording, discarding the accumulated markers and
   * updating the UI as needed.
   */
  _stopRecordingAndDiscardData: function*() {
    yield this._stopRecording();
    this._markers.length = 0;
  },

  /**
   * Callback handling the "markers" event on the timeline front.
   *
   * @param array markers
   *        A list of new markers collected since the last time this
   *        function was invoked.
   * @param number endTime
   *        A time after the last marker in markers was collected.
   */
  _onMarkers: function(markers, endTime) {
    Array.prototype.push.apply(this._markers, markers);
    this._markers.endTime = endTime;
  },

  /**
   * Callback invoked at a fixed interval while recording.
   * Updates the markers store with the current time and the timeline overview.
   */
  _onRecordingTick: function() {
    // Compute an approximate ending time for the view.  This is
    // needed to ensure that the view updates even when new data is
    // not being generated.
    let fakeTime = this._markers.startTime + (performance.now() - this._localStartTime);
    if (fakeTime > this._markers.endTime) {
      this._markers.endTime = fakeTime;
    }
    TimelineView.handleMarkersUpdate(this._markers);
  }
};

/**
 * Functions handling the timeline frontend view.
 */
let TimelineView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: Task.async(function*() {
    this.overview = new Overview($("#timeline-overview"));
    this.waterfall = new Waterfall($("#timeline-waterfall"));

    this._onSelecting = this._onSelecting.bind(this);
    this._onRefresh = this._onRefresh.bind(this);
    this.overview.on("selecting", this._onSelecting);
    this.overview.on("refresh", this._onRefresh);

    yield this.overview.ready();
    yield this.waterfall.recalculateBounds();
  }),

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this.overview.off("selecting", this._onSelecting);
    this.overview.off("refresh", this._onRefresh);
    this.overview.destroy();
  },

  /**
   * Signals that a recording session has started and triggers the appropriate
   * changes in the UI.
   */
  handleRecordingStarted: function() {
    $("#record-button").setAttribute("checked", "true");
    $("#timeline-pane").selectedPanel = $("#recording-notice");

    this.overview.selectionEnabled = false;
    this.overview.dropSelection();
    this.overview.setData([]);
    this.waterfall.clearView();

    window.emit(EVENTS.RECORDING_STARTED);
  },

  /**
   * Signals that a recording session has ended and triggers the appropriate
   * changes in the UI.
   */
  handleRecordingEnded: function() {
    $("#record-button").removeAttribute("checked");
    $("#timeline-pane").selectedPanel = $("#timeline-waterfall");

    this.overview.selectionEnabled = true;

    let markers = TimelineController.getMarkers();
    if (markers.length) {
      let start = (markers[0].start - markers.startTime) * this.overview.dataScaleX;
      let end = start + this.overview.width * OVERVIEW_INITIAL_SELECTION_RATIO;
      this.overview.setSelection({ start, end });
    } else {
      let duration = markers.endTime - markers.startTime;
      this.waterfall.setData(markers, markers.startTime, markers.endTime);
    }

    window.emit(EVENTS.RECORDING_ENDED);
  },

  /**
   * Signals that a new set of markers was made available by the controller,
   * or that the overview graph needs to be updated.
   *
   * @param array markers
   *        A list of new markers collected since the recording has started.
   */
  handleMarkersUpdate: function(markers) {
    this.overview.setData(markers);
    window.emit(EVENTS.OVERVIEW_UPDATED);
  },

  /**
   * Callback handling the "selecting" event on the timeline overview.
   */
  _onSelecting: function() {
    if (!this.overview.hasSelection() &&
        !this.overview.hasSelectionInProgress()) {
      this.waterfall.clearView();
      return;
    }
    let selection = this.overview.getSelection();
    let start = selection.start / this.overview.dataScaleX;
    let end = selection.end / this.overview.dataScaleX;

    let markers = TimelineController.getMarkers();
    let timeStart = markers.startTime + Math.min(start, end);
    let timeEnd = markers.startTime + Math.max(start, end);
    this.waterfall.setData(markers, timeStart, timeEnd);
  },

  /**
   * Callback handling the "refresh" event on the timeline overview.
   */
  _onRefresh: function() {
    this.waterfall.recalculateBounds();
    this._onSelecting();
  }
};

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * DOM query helpers.
 */
function $(selector, target = document) {
  return target.querySelector(selector);
}
function $$(selector, target = document) {
  return target.querySelectorAll(selector);
}

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
    this._markers = [];
    this._markers.startTime = performance.now();
    this._markers.endTime = performance.now();
    this._updateId = setInterval(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);

    TimelineView.handleRecordingStarted();
    yield gFront.start();
  },

  /**
   * Stops the recording, updating the UI as needed.
   */
  _stopRecording: function*() {
    clearInterval(this._updateId);

    TimelineView.handleMarkersUpdate(this._markers);
    TimelineView.handleRecordingEnded();
    yield gFront.stop();
  },

  /**
   * Used in tests. Stops the recording, discarding the accumulated markers and
   * updating the UI as needed.
   */
  _stopRecordingAndDiscardData: function*() {
    // Clear the markers before calling async method _stopRecording to properly
    // reset the selection if markers were already received. Bug 1092452.
    this._markers.length = 0;

    yield this._stopRecording();

    // Clear the markers after _stopRecording has finished. It's possible that
    // server sent new markers before it received the request to stop sending
    // them and client received them while we were waiting for _stopRecording
    // to finish. Bug 1067287.
    this._markers.length = 0;
  },

  /**
   * Callback handling the "markers" event on the timeline front.
   *
   * @param array markers
   *        A list of new markers collected since the last time this
   *        function was invoked.
   */
  _onMarkers: function(markers) {
    Array.prototype.push.apply(this._markers, markers);
  },

  /**
   * Callback invoked at a fixed interval while recording.
   * Updates the markers store with the current time and the timeline overview.
   */
  _onRecordingTick: function() {
    this._markers.endTime = performance.now();
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
      let start = markers[0].start * this.overview.dataScaleX;
      let end = start + this.overview.width * OVERVIEW_INITIAL_SELECTION_RATIO;
      this.overview.setSelection({ start, end });
    } else {
      let duration = markers.endTime - markers.startTime;
      this.waterfall.setData(markers, 0, duration);
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
    let timeStart = Math.min(start, end);
    let timeEnd = Math.max(start, end);
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

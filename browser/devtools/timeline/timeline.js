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

devtools.lazyRequireGetter(this, "MarkersOverview",
  "devtools/timeline/markers-overview", true);
devtools.lazyRequireGetter(this, "MemoryOverview",
  "devtools/timeline/memory-overview", true);
devtools.lazyRequireGetter(this, "Waterfall",
  "devtools/timeline/waterfall", true);

devtools.lazyImporter(this, "CanvasGraphUtils",
  "resource:///modules/devtools/Graphs.jsm");

devtools.lazyImporter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

const OVERVIEW_UPDATE_INTERVAL = 200;
const OVERVIEW_INITIAL_SELECTION_RATIO = 0.15;

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When a recording is started or stopped, via the `stopwatch` button.
  RECORDING_STARTED: "Timeline:RecordingStarted",
  RECORDING_ENDED: "Timeline:RecordingEnded",

  // When the overview graphs are populated with new markers.
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
   * Permanent storage for the markers and the memory measurements streamed by
   * the backend, along with the start and end timestamps.
   */
  _starTime: 0,
  _endTime: 0,
  _markers: [],
  _memory: [],

  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this._onRecordingTick = this._onRecordingTick.bind(this);
    this._onMarkers = this._onMarkers.bind(this);
    this._onMemory = this._onMemory.bind(this);
    gFront.on("markers", this._onMarkers);
    gFront.on("memory", this._onMemory);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    gFront.off("markers", this._onMarkers);
    gFront.off("memory", this._onMemory);
  },

  /**
   * Gets the { stat, end } time interval for this recording.
   * @return object
   */
  getInterval: function() {
    return { startTime: this._startTime, endTime: this._endTime };
  },

  /**
   * Gets the accumulated markers in this recording.
   * @return array
   */
  getMarkers: function() {
    return this._markers;
  },

  /**
   * Gets the accumulated memory measurements in this recording.
   * @return array
   */
  getMemory: function() {
    return this._memory;
  },

  /**
   * Updates the views to show or hide the memory recording data.
   */
  updateMemoryRecording: Task.async(function*() {
    if ($("#memory-checkbox").checked) {
      yield TimelineView.showMemoryOverview();
    } else {
      yield TimelineView.hideMemoryOverview();
    }
  }),

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

    let withMemory = $("#memory-checkbox").checked;
    let startTime = yield gFront.start({ withMemory });

    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data.  To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    // See _onRecordingTick.
    this._localStartTime = performance.now();
    this._startTime = startTime;
    this._endTime = startTime;
    this._markers = [];
    this._memory = [];
    this._updateId = setInterval(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);
  },

  /**
   * Stops the recording, updating the UI as needed.
   */
  _stopRecording: function*() {
    clearInterval(this._updateId);

    // Sorting markers is only important when displayed in the waterfall.
    this._markers = this._markers.sort((a,b) => (a.start > b.start));

    TimelineView.handleRecordingUpdate();
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
    this._memory.length = 0;
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
    this._endTime = endTime;
  },

  /**
   * Callback handling the "memory" event on the timeline front.
   *
   * @param number delta
   *        The number of milliseconds elapsed since epoch.
   * @param object measurement
   *        A detailed breakdown of the current memory usage.
   */
  _onMemory: function(delta, measurement) {
    this._memory.push({ delta, value: measurement.total / 1024 / 1024 });
  },

  /**
   * Callback invoked at a fixed interval while recording.
   * Updates the current time and the timeline overview.
   */
  _onRecordingTick: function() {
    // Compute an approximate ending time for the view.  This is
    // needed to ensure that the view updates even when new data is
    // not being generated.
    let fakeTime = this._startTime + (performance.now() - this._localStartTime);
    if (fakeTime > this._endTime) {
      this._endTime = fakeTime;
    }
    TimelineView.handleRecordingUpdate();
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
    this.markersOverview = new MarkersOverview($("#markers-overview"));
    this.waterfall = new Waterfall($("#timeline-waterfall"));

    this._onSelecting = this._onSelecting.bind(this);
    this._onRefresh = this._onRefresh.bind(this);
    this.markersOverview.on("selecting", this._onSelecting);
    this.markersOverview.on("refresh", this._onRefresh);

    yield this.markersOverview.ready();
    yield this.waterfall.recalculateBounds();
  }),

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this.markersOverview.off("selecting", this._onSelecting);
    this.markersOverview.off("refresh", this._onRefresh);
    this.markersOverview.destroy();

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.destroy();
    }
  },

  /**
   * Shows the memory overview graph.
   */
  showMemoryOverview: Task.async(function*() {
    this.memoryOverview = new MemoryOverview($("#memory-overview"));
    yield this.memoryOverview.ready();

    let interval = TimelineController.getInterval();
    let memory = TimelineController.getMemory();
    this.memoryOverview.setData({ interval, memory });

    CanvasGraphUtils.linkAnimation(this.markersOverview, this.memoryOverview);
    CanvasGraphUtils.linkSelection(this.markersOverview, this.memoryOverview);
  }),

  /**
   * Hides the memory overview graph.
   */
  hideMemoryOverview: function() {
    if (!this.memoryOverview) {
      return;
    }
    this.memoryOverview.destroy();
    this.memoryOverview = null;
  },

  /**
   * Signals that a recording session has started and triggers the appropriate
   * changes in the UI.
   */
  handleRecordingStarted: function() {
    $("#record-button").setAttribute("checked", "true");
    $("#memory-checkbox").setAttribute("disabled", "true");
    $("#timeline-pane").selectedPanel = $("#recording-notice");

    this.markersOverview.clearView();

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.clearView();
    }

    this.waterfall.clearView();

    window.emit(EVENTS.RECORDING_STARTED);
  },

  /**
   * Signals that a recording session has ended and triggers the appropriate
   * changes in the UI.
   */
  handleRecordingEnded: function() {
    $("#record-button").removeAttribute("checked");
    $("#memory-checkbox").removeAttribute("disabled");
    $("#timeline-pane").selectedPanel = $("#timeline-waterfall");

    this.markersOverview.selectionEnabled = true;

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.selectionEnabled = true;
    }

    let interval = TimelineController.getInterval();
    let markers = TimelineController.getMarkers();
    let memory = TimelineController.getMemory();

    if (markers.length) {
      let start = (markers[0].start - interval.startTime) * this.markersOverview.dataScaleX;
      let end = start + this.markersOverview.width * OVERVIEW_INITIAL_SELECTION_RATIO;
      this.markersOverview.setSelection({ start, end });
    } else {
      let timeStart = interval.startTime;
      let timeEnd = interval.endTime;
      this.waterfall.setData(markers, timeStart, timeStart, timeEnd);
    }

    window.emit(EVENTS.RECORDING_ENDED);
  },

  /**
   * Signals that a new set of markers was made available by the controller,
   * or that the overview graph needs to be updated.
   */
  handleRecordingUpdate: function() {
    let interval = TimelineController.getInterval();
    let markers = TimelineController.getMarkers();
    let memory = TimelineController.getMemory();

    this.markersOverview.setData({ interval, markers });

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.setData({ interval, memory });
    }

    window.emit(EVENTS.OVERVIEW_UPDATED);
  },

  /**
   * Callback handling the "selecting" event on the timeline overview.
   */
  _onSelecting: function() {
    if (!this.markersOverview.hasSelection() &&
        !this.markersOverview.hasSelectionInProgress()) {
      this.waterfall.clearView();
      return;
    }
    let selection = this.markersOverview.getSelection();
    let start = selection.start / this.markersOverview.dataScaleX;
    let end = selection.end / this.markersOverview.dataScaleX;

    let markers = TimelineController.getMarkers();
    let interval = TimelineController.getInterval();

    let timeStart = interval.startTime + Math.min(start, end);
    let timeEnd = interval.startTime + Math.max(start, end);
    this.waterfall.setData(markers, interval.startTime, timeStart, timeEnd);
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

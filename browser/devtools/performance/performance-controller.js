/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");
Cu.import("resource://gre/modules/devtools/Console.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

devtools.lazyRequireGetter(this, "Services");
devtools.lazyRequireGetter(this, "promise");
devtools.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
devtools.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");
devtools.lazyRequireGetter(this, "L10N",
  "devtools/profiler/global", true);

devtools.lazyRequireGetter(this, "MarkersOverview",
  "devtools/timeline/markers-overview", true);
devtools.lazyRequireGetter(this, "MemoryOverview",
  "devtools/timeline/memory-overview", true);
devtools.lazyRequireGetter(this, "Waterfall",
  "devtools/timeline/waterfall", true);
devtools.lazyRequireGetter(this, "MarkerDetails",
  "devtools/timeline/marker-details", true);
devtools.lazyRequireGetter(this, "CallView",
  "devtools/profiler/tree-view", true);
devtools.lazyRequireGetter(this, "ThreadNode",
  "devtools/profiler/tree-model", true);

devtools.lazyImporter(this, "CanvasGraphUtils",
  "resource:///modules/devtools/Graphs.jsm");
devtools.lazyImporter(this, "LineGraphWidget",
  "resource:///modules/devtools/Graphs.jsm");

// Events emitted by various objects in the panel.
const EVENTS = {
  // When a recording is started or stopped via the PerformanceController
  RECORDING_STARTED: "Performance:RecordingStarted",
  RECORDING_STOPPED: "Performance:RecordingStopped",

  // When the PerformanceController has new recording data.
  TIMELINE_DATA: "Performance:TimelineData",

  // Emitted by the PerformanceView on record button click
  UI_START_RECORDING: "Performance:UI:StartRecording",
  UI_STOP_RECORDING: "Performance:UI:StopRecording",

  // Emitted by the OverviewView when more data has been rendered
  OVERVIEW_RENDERED: "Performance:UI:OverviewRendered",
  FRAMERATE_GRAPH_RENDERED: "Performance:UI:OverviewFramerateRendered",
  MARKERS_GRAPH_RENDERED: "Performance:UI:OverviewMarkersRendered",
  MEMORY_GRAPH_RENDERED: "Performance:UI:OverviewMemoryRendered",

  // Emitted by the OverviewView when a range has been selected in the graphs
  OVERVIEW_RANGE_SELECTED: "Performance:UI:OverviewRangeSelected",
  // Emitted by the OverviewView when a selection range has been removed
  OVERVIEW_RANGE_CLEARED: "Performance:UI:OverviewRangeCleared",

  // Emitted by the DetailsView when a subview is selected
  DETAILS_VIEW_SELECTED: "Performance:UI:DetailsViewSelected",

  // Emitted by the CallTreeView when a call tree has been rendered
  CALL_TREE_RENDERED: "Performance:UI:CallTreeRendered",

  // When a source is shown in the JavaScript Debugger at a specific location.
  SOURCE_SHOWN_IN_JS_DEBUGGER: "Performance:UI:SourceShownInJsDebugger",
  SOURCE_NOT_FOUND_IN_JS_DEBUGGER: "Performance:UI:SourceNotFoundInJsDebugger",

  // Emitted by the WaterfallView when it has been rendered
  WATERFALL_RENDERED: "Performance:UI:WaterfallRendered"
};

/**
 * The current target and the profiler connection, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Initializes the profiler controller and views.
 */
let startupPerformance = Task.async(function*() {
  yield promise.all([
    PrefObserver.register(),
    PerformanceController.initialize(),
    PerformanceView.initialize()
  ]);
});

/**
 * Destroys the profiler controller and views.
 */
let shutdownPerformance = Task.async(function*() {
  yield promise.all([
    PrefObserver.unregister(),
    PerformanceController.destroy(),
    PerformanceView.destroy()
  ]);
});

/**
 * Observes pref changes on the devtools.profiler branch and triggers the
 * required frontend modifications.
 */
let PrefObserver = {
  register: function() {
    this.branch = Services.prefs.getBranch("devtools.profiler.");
    this.branch.addObserver("", this, false);
  },
  unregister: function() {
    this.branch.removeObserver("", this);
  },
  observe: function(subject, topic, pref) {
    Prefs.refresh();
  }
};

/**
 * Functions handling target-related lifetime events and
 * UI interaction.
 */
let PerformanceController = {
  /**
   * Permanent storage for the markers and the memory measurements streamed by
   * the backend, along with the start and end timestamps.
   */
  _startTime: 0,
  _endTime: 0,
  _markers: [],
  _memory: [],
  _ticks: [],

  /**
   * Listen for events emitted by the current tab target and
   * main UI events.
   */
  initialize: function() {
    this.startRecording = this.startRecording.bind(this);
    this.stopRecording = this.stopRecording.bind(this);
    this._onTimelineData = this._onTimelineData.bind(this);

    PerformanceView.on(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.on(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    gFront.on("ticks", this._onTimelineData); // framerate
    gFront.on("markers", this._onTimelineData); // timeline markers
    gFront.on("memory", this._onTimelineData); // timeline memory
  },

  /**
   * Remove events handled by the PerformanceController
   */
  destroy: function() {
    PerformanceView.off(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.off(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    gFront.off("ticks", this._onTimelineData);
    gFront.off("markers", this._onTimelineData);
    gFront.off("memory", this._onTimelineData);
  },

  /**
   * Starts recording with the PerformanceFront. Emits `EVENTS.RECORDING_STARTED`
   * when the front has started to record.
   */
  startRecording: Task.async(function *() {
    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data. To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    this._localStartTime = performance.now();

    let { startTime } = yield gFront.startRecording({
      withTicks: true,
      withMemory: true
    });

    this._startTime = startTime;
    this._endTime = startTime;
    this._markers = [];
    this._memory = [];
    this._ticks = [];

    this.emit(EVENTS.RECORDING_STARTED, this._startTime);
  }),

  /**
   * Stops recording with the PerformanceFront. Emits `EVENTS.RECORDING_STOPPED`
   * when the front has stopped recording.
   */
  stopRecording: Task.async(function *() {
    let results = yield gFront.stopRecording();

    // If `endTime` is not yielded from timeline actor (< Fx36), fake it.
    if (!results.endTime) {
      results.endTime = this._startTime + this.getInterval().localElapsedTime;
    }

    this._endTime = results.endTime;
    this._markers = this._markers.sort((a,b) => (a.start > b.start));

    this.emit(EVENTS.RECORDING_STOPPED, results);
  }),

  /**
   * Gets the time interval for the current recording.
   * @return object
   */
  getInterval: function() {
    let localElapsedTime = performance.now() - this._localStartTime;
    let startTime = this._startTime;
    let endTime = this._endTime;
    return { localElapsedTime, startTime, endTime };
  },

  /**
   * Gets the accumulated markers in the current recording.
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
   * Gets the accumulated refresh driver ticks in this recording.
   * @return array
   */
  getTicks: function() {
    return this._ticks;
  },

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  _onTimelineData: function (eventName, ...data) {
    // Accumulate markers into an array.
    if (eventName == "markers") {
      let [markers] = data;
      Array.prototype.push.apply(this._markers, markers);
    }
    // Accumulate memory measurements into an array.
    else if (eventName == "memory") {
      let [delta, measurement] = data;
      this._memory.push({ delta, value: measurement.total / 1024 / 1024 });
    }
    // Save the accumulated refresh driver ticks.
    else if (eventName == "ticks") {
      let [delta, timestamps] = data;
      this._ticks = timestamps;
    }

    this.emit(EVENTS.TIMELINE_DATA, eventName, ...data);
  }
};

/**
 * Convenient way of emitting events from the controller.
 */
EventEmitter.decorate(PerformanceController);

/**
 * Shortcuts for accessing various profiler preferences.
 */
const Prefs = new ViewHelpers.Prefs("devtools.profiler", {
  showPlatformData: ["Bool", "ui.show-platform-data"]
});

/**
 * DOM query helpers.
 */
function $(selector, target = document) {
  return target.querySelector(selector);
}
function $$(selector, target = document) {
  return target.querySelectorAll(selector);
}

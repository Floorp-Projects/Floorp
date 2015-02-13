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

devtools.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/shared/timeline/global", true);
devtools.lazyRequireGetter(this, "L10N",
  "devtools/shared/profiler/global", true);
devtools.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils", true);
devtools.lazyRequireGetter(this, "RecordingModel",
  "devtools/performance/recording-model", true);
devtools.lazyRequireGetter(this, "MarkersOverview",
  "devtools/shared/timeline/markers-overview", true);
devtools.lazyRequireGetter(this, "MemoryOverview",
  "devtools/shared/timeline/memory-overview", true);
devtools.lazyRequireGetter(this, "Waterfall",
  "devtools/shared/timeline/waterfall", true);
devtools.lazyRequireGetter(this, "MarkerDetails",
  "devtools/shared/timeline/marker-details", true);
devtools.lazyRequireGetter(this, "CallView",
  "devtools/shared/profiler/tree-view", true);
devtools.lazyRequireGetter(this, "ThreadNode",
  "devtools/shared/profiler/tree-model", true);
devtools.lazyRequireGetter(this, "FrameNode",
  "devtools/shared/profiler/tree-model", true);
devtools.lazyRequireGetter(this, "OptionsView",
  "devtools/shared/options-view", true);

devtools.lazyImporter(this, "CanvasGraphUtils",
  "resource:///modules/devtools/Graphs.jsm");
devtools.lazyImporter(this, "LineGraphWidget",
  "resource:///modules/devtools/Graphs.jsm");
devtools.lazyImporter(this, "FlameGraphUtils",
  "resource:///modules/devtools/FlameGraph.jsm");
devtools.lazyImporter(this, "FlameGraph",
  "resource:///modules/devtools/FlameGraph.jsm");
devtools.lazyImporter(this, "SideMenuWidget",
  "resource:///modules/devtools/SideMenuWidget.jsm");

const BRANCH_NAME = "devtools.performance.ui.";

// Events emitted by various objects in the panel.
const EVENTS = {
  // Fired by the OptionsView when a preference changes.
  PREF_CHANGED: "Performance:PrefChanged",

  // Emitted by the PerformanceView when the state (display mode) changes.
  UI_STATE_CHANGED: "Performance:UI:StateChanged",

  // Emitted by the PerformanceView on clear button click
  UI_CLEAR_RECORDINGS: "Performance:UI:ClearRecordings",

  // Emitted by the PerformanceView on record button click
  UI_START_RECORDING: "Performance:UI:StartRecording",
  UI_STOP_RECORDING: "Performance:UI:StopRecording",

  // Emitted by the PerformanceView on import button click
  UI_IMPORT_RECORDING: "Performance:UI:ImportRecording",
  // Emitted by the RecordingsView on export button click
  UI_EXPORT_RECORDING: "Performance:UI:ExportRecording",

  // When a recording is started or stopped via the PerformanceController
  RECORDING_STARTED: "Performance:RecordingStarted",
  RECORDING_STOPPED: "Performance:RecordingStopped",
  RECORDING_WILL_START: "Performance:RecordingWillStart",
  RECORDING_WILL_STOP: "Performance:RecordingWillStop",

  // Emitted by the PerformanceController or RecordingView
  // when a recording model is selected
  RECORDING_SELECTED: "Performance:RecordingSelected",

  // When recordings have been cleared out
  RECORDINGS_CLEARED: "Performance:RecordingsCleared",

  // When a recording is imported or exported via the PerformanceController
  RECORDING_IMPORTED: "Performance:RecordingImported",
  RECORDING_EXPORTED: "Performance:RecordingExported",

  // When the PerformanceController has new recording data
  TIMELINE_DATA: "Performance:TimelineData",

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

  // Emitted by the WaterfallView when it has been rendered
  WATERFALL_RENDERED: "Performance:UI:WaterfallRendered",

  // Emitted by the JsCallTreeView when a call tree has been rendered
  JS_CALL_TREE_RENDERED: "Performance:UI:JsCallTreeRendered",

  // Emitted by the JsFlameGraphView when it has been rendered
  JS_FLAMEGRAPH_RENDERED: "Performance:UI:JsFlameGraphRendered",

  // Emitted by the MemoryCallTreeView when a call tree has been rendered
  MEMORY_CALL_TREE_RENDERED: "Performance:UI:MemoryCallTreeRendered",

  // Emitted by the MemoryFlameGraphView when it has been rendered
  MEMORY_FLAMEGRAPH_RENDERED: "Performance:UI:MemoryFlameGraphRendered",

  // When a source is shown in the JavaScript Debugger at a specific location.
  SOURCE_SHOWN_IN_JS_DEBUGGER: "Performance:UI:SourceShownInJsDebugger",
  SOURCE_NOT_FOUND_IN_JS_DEBUGGER: "Performance:UI:SourceNotFoundInJsDebugger"
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
    PerformanceController.initialize(),
    PerformanceView.initialize()
  ]);
});

/**
 * Destroys the profiler controller and views.
 */
let shutdownPerformance = Task.async(function*() {
  yield promise.all([
    PerformanceController.destroy(),
    PerformanceView.destroy()
  ]);
});

/**
 * Functions handling target-related lifetime events and
 * UI interaction.
 */
let PerformanceController = {
  _recordings: [],
  _currentRecording: null,

  /**
   * Listen for events emitted by the current tab target and
   * main UI events.
   */
  initialize: Task.async(function* () {
    this.startRecording = this.startRecording.bind(this);
    this.stopRecording = this.stopRecording.bind(this);
    this.importRecording = this.importRecording.bind(this);
    this.exportRecording = this.exportRecording.bind(this);
    this.clearRecordings = this.clearRecordings.bind(this);
    this._onTimelineData = this._onTimelineData.bind(this);
    this._onRecordingSelectFromView = this._onRecordingSelectFromView.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);

    ToolbarView.on(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.on(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.on(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.on(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.on(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.on(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);

    gFront.on("markers", this._onTimelineData); // timeline markers
    gFront.on("frames", this._onTimelineData); // stack frames
    gFront.on("memory", this._onTimelineData); // memory measurements
    gFront.on("ticks", this._onTimelineData); // framerate
    gFront.on("allocations", this._onTimelineData); // memory allocations
  }),

  /**
   * Remove events handled by the PerformanceController
   */
  destroy: function() {
    ToolbarView.off(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.off(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.off(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.off(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.off(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.off(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.off(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);

    gFront.off("markers", this._onTimelineData);
    gFront.off("frames", this._onTimelineData);
    gFront.off("memory", this._onTimelineData);
    gFront.off("ticks", this._onTimelineData);
    gFront.off("allocations", this._onTimelineData);
  },

  /**
   * Get a preference setting from `prefName` via the underlying
   * OptionsView in the ToolbarView.
   */
  getPref: function (prefName) {
    return ToolbarView.optionsView.getPref(prefName);
  },

  /**
   * Starts recording with the PerformanceFront. Emits `EVENTS.RECORDING_STARTED`
   * when the front has started to record.
   */
  startRecording: Task.async(function *() {
    let withMemory = this.getPref("enable-memory");
    let withTicks = this.getPref("enable-framerate");
    let withAllocations = this.getPref("enable-memory");

    let recording = this._createRecording({ withMemory, withTicks, withAllocations });

    this.emit(EVENTS.RECORDING_WILL_START, recording);
    yield recording.startRecording({ withTicks, withMemory, withAllocations });
    this.emit(EVENTS.RECORDING_STARTED, recording);

    this.setCurrentRecording(recording);
  }),

  /**
   * Stops recording with the PerformanceFront. Emits `EVENTS.RECORDING_STOPPED`
   * when the front has stopped recording.
   */
  stopRecording: Task.async(function *() {
    let recording = this._getLatestRecording();

    this.emit(EVENTS.RECORDING_WILL_STOP, recording);
    yield recording.stopRecording();
    this.emit(EVENTS.RECORDING_STOPPED, recording);
  }),

  /**
   * Saves the given recording to a file. Emits `EVENTS.RECORDING_EXPORTED`
   * when the file was saved.
   *
   * @param RecordingModel recording
   *        The model that holds the recording data.
   * @param nsILocalFile file
   *        The file to stream the data into.
   */
  exportRecording: Task.async(function*(_, recording, file) {
    yield recording.exportRecording(file);
    this.emit(EVENTS.RECORDING_EXPORTED, recording);
  }),

  /**
   * Clears all recordings from the list as well as the current recording.
   * Emits `EVENTS.RECORDINGS_CLEARED` when complete so other components can clean up.
   */
  clearRecordings: Task.async(function* () {
    let latest = this._getLatestRecording();

    if (latest && latest.isRecording()) {
      yield this.stopRecording();
    }

    this._recordings.length = 0;
    this.setCurrentRecording(null);
    this.emit(EVENTS.RECORDINGS_CLEARED);
  }),

  /**
   * Loads a recording from a file, adding it to the recordings list. Emits
   * `EVENTS.RECORDING_IMPORTED` when the file was loaded.
   *
   * @param nsILocalFile file
   *        The file to import the data from.
   */
  importRecording: Task.async(function*(_, file) {
    let recording = this._createRecording();
    yield recording.importRecording(file);

    this.emit(EVENTS.RECORDING_IMPORTED, recording);
  }),

  /**
   * Creates a new RecordingModel, fires events and stores it
   * internally in the controller.
   *
   * @param object options
   *        @see PerformanceFront.prototype.startRecording
   * @return RecordingModel
   *         The newly created recording model.
   */
  _createRecording: function (options={}) {
    let { withMemory, withTicks, withAllocations } = options;
    let front = gFront;

    let recording = new RecordingModel(
      { front, performance, withMemory, withTicks, withAllocations });

    this._recordings.push(recording);
    return recording;
  },

  /**
   * Sets the currently active RecordingModel.
   * @param RecordingModel recording
   */
  setCurrentRecording: function (recording) {
    if (this._currentRecording !== recording) {
      this._currentRecording = recording;
      this.emit(EVENTS.RECORDING_SELECTED, recording);
    }
  },

  /**
   * Gets the currently active RecordingModel.
   * @return RecordingModel
   */
  getCurrentRecording: function () {
    return this._currentRecording;
  },

  /**
   * Get most recently added recording that was triggered manually (via UI).
   * @return RecordingModel
   */
  _getLatestRecording: function () {
    for (let i = this._recordings.length - 1; i >= 0; i--) {
      return this._recordings[i];
    }
    return null;
  },

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  _onTimelineData: function (...data) {
    this._recordings.forEach(e => e.addTimelineData.apply(e, data));
    this.emit(EVENTS.TIMELINE_DATA, ...data);
  },

  /**
   * Fired from RecordingsView, we listen on the PerformanceController so we can
   * set it here and re-emit on the controller, where all views can listen.
   */
  _onRecordingSelectFromView: function (_, recording) {
    this.setCurrentRecording(recording);
  },

  /**
   * Fired when the ToolbarView fires a PREF_CHANGED event.
   * with the value.
   */
  _onPrefChanged: function (_, prefName, value) {
    this.emit(EVENTS.PREF_CHANGED, prefName, value);
  },

  toString: () => "[object PerformanceController]"
};

/**
 * Convenient way of emitting events from the controller.
 */
EventEmitter.decorate(PerformanceController);

/**
 * DOM query helpers.
 */
function $(selector, target = document) {
  return target.querySelector(selector);
}
function $$(selector, target = document) {
  return target.querySelectorAll(selector);
}

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
  "devtools/timeline/global", true);
devtools.lazyRequireGetter(this, "L10N",
  "devtools/profiler/global", true);
devtools.lazyRequireGetter(this, "RecordingModel",
  "devtools/performance/recording-model", true);
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
devtools.lazyRequireGetter(this, "FrameNode",
  "devtools/profiler/tree-model", true);
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
  PREF_CHANGED: "Preformance:PrefChanged",

  // Emitted by the PerformanceController or RecordingView
  // when a recording model is selected
  RECORDING_SELECTED: "Performance:RecordingSelected",

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

  // Emitted by the CallTreeView when a call tree has been rendered
  CALL_TREE_RENDERED: "Performance:UI:CallTreeRendered",

  // Emitted by the WaterfallView when it has been rendered
  WATERFALL_RENDERED: "Performance:UI:WaterfallRendered",

  // Emitted by the FlameGraphView when it has been rendered
  FLAMEGRAPH_RENDERED: "Performance:UI:FlameGraphRendered",

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
    this._onTimelineData = this._onTimelineData.bind(this);
    this._onRecordingSelectFromView = this._onRecordingSelectFromView.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);

    ToolbarView.on(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.on(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.on(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.on(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    RecordingsView.on(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);

    gFront.on("ticks", this._onTimelineData); // framerate
    gFront.on("markers", this._onTimelineData); // timeline markers
    gFront.on("frames", this._onTimelineData); // stack frames
    gFront.on("memory", this._onTimelineData); // memory measurements
  }),

  /**
   * Remove events handled by the PerformanceController
   */
  destroy: function() {
    ToolbarView.off(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.off(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.off(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.off(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    RecordingsView.off(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.off(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);

    gFront.off("ticks", this._onTimelineData);
    gFront.off("markers", this._onTimelineData);
    gFront.off("frames", this._onTimelineData);
    gFront.off("memory", this._onTimelineData);
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
    let recording = this._createRecording();

    this.emit(EVENTS.RECORDING_WILL_START, recording);
    yield recording.startRecording({ withTicks: true, withMemory: true });
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
   * @return RecordingModel
   *         The newly created recording model.
   */
  _createRecording: function () {
    let recording = new RecordingModel({ front: gFront, performance });
    this._recordings.push(recording);

    this.emit(EVENTS.RECORDING_CREATED, recording);
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
    this._recordings.forEach(profile => profile.addTimelineData.apply(profile, data));
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
  flattenTreeRecursion: ["Bool", "ui.flatten-tree-recursion"],
  showPlatformData: ["Bool", "ui.show-platform-data"],
  showIdleBlocks: ["Bool", "ui.show-idle-blocks"],
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

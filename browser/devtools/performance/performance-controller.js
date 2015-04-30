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

devtools.lazyRequireGetter(this, "TreeWidget",
  "devtools/shared/widgets/TreeWidget", true);
devtools.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/shared/timeline/global", true);
devtools.lazyRequireGetter(this, "L10N",
  "devtools/shared/profiler/global", true);
devtools.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils", true);
devtools.lazyRequireGetter(this, "RecordingModel",
  "devtools/performance/recording-model", true);
devtools.lazyRequireGetter(this, "GraphsController",
  "devtools/performance/graphs", true);
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
devtools.lazyRequireGetter(this, "JITOptimizations",
  "devtools/shared/profiler/jit", true);
devtools.lazyRequireGetter(this, "OptionsView",
  "devtools/shared/options-view", true);
devtools.lazyRequireGetter(this, "FlameGraphUtils",
  "devtools/shared/widgets/FlameGraph", true);
devtools.lazyRequireGetter(this, "FlameGraph",
  "devtools/shared/widgets/FlameGraph", true);

devtools.lazyImporter(this, "SideMenuWidget",
  "resource:///modules/devtools/SideMenuWidget.jsm");
devtools.lazyImporter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

const BRANCH_NAME = "devtools.performance.ui.";

// Events emitted by various objects in the panel.
const EVENTS = {
  // Fired by the PerformanceController and OptionsView when a pref changes.
  PREF_CHANGED: "Performance:PrefChanged",

  // Fired by the PerformanceController when the devtools theme changes.
  THEME_CHANGED: "Performance:ThemeChanged",

  // When the SharedPerformanceConnection handles profiles created via `console.profile()`,
  // the controller handles those events and emits the below events for consumption
  // by other views.
  CONSOLE_RECORDING_STARTED: "Performance:ConsoleRecordingStarted",
  CONSOLE_RECORDING_WILL_STOP: "Performance:ConsoleRecordingWillStop",
  CONSOLE_RECORDING_STOPPED: "Performance:ConsoleRecordingStopped",

  // Emitted by the PerformanceView when the state (display mode) changes,
  // for example when switching between "empty", "recording" or "recorded".
  // This causes certain panels to be hidden or visible.
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


  // Emitted by the JITOptimizationsView when it renders new optimization
  // data and clears the optimization data
  OPTIMIZATIONS_RESET: "Performance:UI:OptimizationsReset",
  OPTIMIZATIONS_RENDERED: "Performance:UI:OptimizationsRendered",

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
    this._onRecordingSelectFromView = this._onRecordingSelectFromView.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);
    this._onThemeChanged = this._onThemeChanged.bind(this);
    this._onConsoleProfileStart = this._onConsoleProfileStart.bind(this);
    this._onConsoleProfileEnd = this._onConsoleProfileEnd.bind(this);
    this._onConsoleProfileEnding = this._onConsoleProfileEnding.bind(this);

    // All boolean prefs should be handled via the OptionsView in the
    // ToolbarView, so that they may be accessible via the "gear" menu.
    // Every other pref should be registered here.
    this._nonBooleanPrefs = new ViewHelpers.Prefs("devtools.performance", {
      "hidden-markers": ["Json", "timeline.hidden-markers"],
      "memory-sample-probability": ["Float", "memory.sample-probability"],
      "memory-max-log-length": ["Int", "memory.max-log-length"],
      "profiler-buffer-size": ["Int", "profiler.buffer-size"],
      "profiler-sample-frequency": ["Int", "profiler.sample-frequency-khz"],
    });

    this._nonBooleanPrefs.registerObserver();
    this._nonBooleanPrefs.on("pref-changed", this._onPrefChanged);

    gFront.on("console-profile-start", this._onConsoleProfileStart);
    gFront.on("console-profile-ending", this._onConsoleProfileEnding);
    gFront.on("console-profile-end", this._onConsoleProfileEnd);
    ToolbarView.on(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.on(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.on(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.on(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.on(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.on(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);

    gDevTools.on("pref-changed", this._onThemeChanged);
  }),

  /**
   * Remove events handled by the PerformanceController
   */
  destroy: function() {
    this._nonBooleanPrefs.unregisterObserver();
    this._nonBooleanPrefs.off("pref-changed", this._onPrefChanged);

    gFront.off("console-profile-start", this._onConsoleProfileStart);
    gFront.off("console-profile-ending", this._onConsoleProfileEnding);
    gFront.off("console-profile-end", this._onConsoleProfileEnd);
    ToolbarView.off(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.off(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.off(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.off(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.off(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.off(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.off(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);

    gDevTools.off("pref-changed", this._onThemeChanged);
  },

  /**
   * Returns the current devtools theme.
   */
  getTheme: function () {
    return Services.prefs.getCharPref("devtools.theme");
  },

  /**
   * Get a boolean preference setting from `prefName` via the underlying
   * OptionsView in the ToolbarView.
   *
   * @param string prefName
   * @return boolean
   */
  getOption: function (prefName) {
    return ToolbarView.optionsView.getPref(prefName);
  },

  /**
   * Get a preference setting from `prefName`.
   * @param string prefName
   * @return object
   */
  getPref: function (prefName) {
    return this._nonBooleanPrefs[prefName];
  },

  /**
   * Set a preference setting from `prefName`.
   * @param string prefName
   * @param object prefValue
   */
  setPref: function (prefName, prefValue) {
    this._nonBooleanPrefs[prefName] = prefValue;
  },

  /**
   * Starts recording with the PerformanceFront. Emits `EVENTS.RECORDING_STARTED`
   * when the front has started to record.
   */
  startRecording: Task.async(function *() {
    // Store retro-mode here so we can easily list true/false
    // values for reverting.
    // TODO bug 1160313
    let superMode = !this.getOption("retro-mode");

    let options = {
      withMarkers: superMode ? true : false,
      withMemory: superMode ? this.getOption("enable-memory") : false,
      withTicks: this.getOption("enable-framerate"),
      withAllocations: superMode ? this.getOption("enable-memory") : false,
      allocationsSampleProbability: this.getPref("memory-sample-probability"),
      allocationsMaxLogLength: this.getPref("memory-max-log-length"),
      bufferSize: this.getPref("profiler-buffer-size"),
      sampleFrequency: this.getPref("profiler-sample-frequency")
    };

    this.emit(EVENTS.RECORDING_WILL_START);

    let recording = yield gFront.startRecording(options);
    this._recordings.push(recording);

    this.emit(EVENTS.RECORDING_STARTED, recording);
  }),

  /**
   * Stops recording with the PerformanceFront. Emits `EVENTS.RECORDING_STOPPED`
   * when the front has stopped recording.
   */
  stopRecording: Task.async(function *() {
    let recording = this.getLatestManualRecording();

    this.emit(EVENTS.RECORDING_WILL_STOP, recording);
    yield gFront.stopRecording(recording);
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
    let latest = this.getLatestManualRecording();

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
    let recording = new RecordingModel();
    this._recordings.push(recording);
    yield recording.importRecording(file);

    this.emit(EVENTS.RECORDING_IMPORTED, recording);
  }),

  /**
   * Sets the currently active RecordingModel. Should rarely be called directly,
   * as RecordingsView handles this when manually selected a recording item. Exceptions
   * are when clearing the view.
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
  getLatestManualRecording: function () {
    for (let i = this._recordings.length - 1; i >= 0; i--) {
      let model = this._recordings[i];
      if (!model.isConsole() && !model.isImported()) {
        return this._recordings[i];
      }
    }
    return null;
  },

  /**
   * Gets the current timeline blueprint without the hidden markers.
   * @return object
   */
  getTimelineBlueprint: function() {
    let blueprint = TIMELINE_BLUEPRINT;
    let hiddenMarkers = this.getPref("hidden-markers");
    return RecordingUtils.getFilteredBlueprint({ blueprint, hiddenMarkers });
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
  _onPrefChanged: function (_, prefName, prefValue) {
    this.emit(EVENTS.PREF_CHANGED, prefName, prefValue);
  },

  /*
   * Called when the developer tools theme changes.
   */
  _onThemeChanged: function (_, data) {
    // Right now, gDevTools only emits `pref-changed` for the theme,
    // but this could change in the future.
    if (data.pref !== "devtools.theme") {
      return;
    }

    this.emit(EVENTS.THEME_CHANGED, data.newValue);
  },

  /**
   * Fired when `console.profile()` is executed.
   */
  _onConsoleProfileStart: function (_, recording) {
    this._recordings.push(recording);
    this.emit(EVENTS.CONSOLE_RECORDING_STARTED, recording);
  },

  /**
   * Fired when `console.profileEnd()` is executed, and the profile
   * is stopping soon, as it fetches profiler data.
   */
  _onConsoleProfileEnding: function (_, recording) {
    this.emit(EVENTS.CONSOLE_RECORDING_WILL_STOP, recording);
  },

  /**
   * Fired when `console.profileEnd()` is executed, and
   * has a corresponding `console.profile()` session.
   */
  _onConsoleProfileEnd: function (_, recording) {
    this.emit(EVENTS.CONSOLE_RECORDING_STOPPED, recording);
  },

  /**
   * Returns the internal store of recording models.
   */
  getRecordings: function () {
    return this._recordings;
  },

  /**
   * Utility method taking the currently selected recording item's features, or optionally passed
   * in recording item, as well as the actor support on the server, returning a boolean
   * indicating if the requirements pass or not. Used to toggle features' visibility mostly.
   *
   * @option {Array<string>} features
   *         An array of strings indicating what configuration is needed on the recording
   *         model, like `withTicks`, or `withMemory`.
   * @option {Array<string>} actors
   *         An array of strings indicating what actors must exist.
   * @option {boolean} isRecording
   *         A boolean indicating whether the recording must be either recording or not
   *         recording. Setting to undefined will allow either state.
   * @param {RecordingModel} recording
   *        An optional recording model to use instead of the currently selected.
   *
   * @return boolean
   */
  isFeatureSupported: function ({ features, actors, isRecording: shouldBeRecording }, recording) {
    recording = recording || this.getCurrentRecording();
    let recordingConfig = recording ? recording.getConfiguration() : {};
    let currentRecordingState = recording ? recording.isRecording() : void 0;
    let actorsSupported = gFront.getActorSupport();

    if (shouldBeRecording != null && shouldBeRecording !== currentRecordingState) {
      return false;
    }
    if (actors && !actors.every(a => actorsSupported[a])) {
      return false;
    }
    if (features && !features.every(f => recordingConfig[f])) {
      return false;
    }
    return true;
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

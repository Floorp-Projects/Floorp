/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
const { Heritage, ViewHelpers, WidgetMethods } = require("resource:///modules/devtools/ViewHelpers.jsm");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");

// Logic modules

loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/performance/recording-utils");
loader.lazyRequireGetter(this, "RecordingModel",
  "devtools/performance/recording-model", true);
loader.lazyRequireGetter(this, "GraphsController",
  "devtools/performance/graphs", true);
loader.lazyRequireGetter(this, "WaterfallHeader",
  "devtools/performance/waterfall-ticks", true);
loader.lazyRequireGetter(this, "MarkerView",
  "devtools/performance/marker-view", true);
loader.lazyRequireGetter(this, "MarkerDetails",
  "devtools/performance/marker-details", true);
loader.lazyRequireGetter(this, "MarkerUtils",
  "devtools/performance/marker-utils");
loader.lazyRequireGetter(this, "WaterfallUtils",
  "devtools/performance/waterfall-utils");
loader.lazyRequireGetter(this, "CallView",
  "devtools/performance/tree-view", true);
loader.lazyRequireGetter(this, "ThreadNode",
  "devtools/performance/tree-model", true);
loader.lazyRequireGetter(this, "FrameNode",
  "devtools/performance/tree-model", true);
loader.lazyRequireGetter(this, "JITOptimizations",
  "devtools/performance/jit", true);

// Widgets modules

loader.lazyRequireGetter(this, "OptionsView",
  "devtools/shared/options-view", true);
loader.lazyRequireGetter(this, "FlameGraphUtils",
  "devtools/shared/widgets/FlameGraph", true);
loader.lazyRequireGetter(this, "FlameGraph",
  "devtools/shared/widgets/FlameGraph", true);
loader.lazyRequireGetter(this, "TreeWidget",
  "devtools/shared/widgets/TreeWidget", true);

loader.lazyImporter(this, "SideMenuWidget",
  "resource:///modules/devtools/SideMenuWidget.jsm");
loader.lazyImporter(this, "setNamedTimeout",
  "resource:///modules/devtools/ViewHelpers.jsm");
loader.lazyImporter(this, "clearNamedTimeout",
  "resource:///modules/devtools/ViewHelpers.jsm");
loader.lazyImporter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

const BRANCH_NAME = "devtools.performance.ui.";

// Events emitted by various objects in the panel.
const EVENTS = {
  // Fired by the PerformanceController and OptionsView when a pref changes.
  PREF_CHANGED: "Performance:PrefChanged",

  // Fired by the PerformanceController when the devtools theme changes.
  THEME_CHANGED: "Performance:ThemeChanged",

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

  // When the front has updated information on the profiler's circular buffer
  PROFILER_STATUS_UPDATED: "Performance:BufferUpdated",

  // When the PerformanceView updates the display of the buffer status
  UI_BUFFER_STATUS_UPDATED: "Performance:UI:BufferUpdated",

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
    this._onRecordingStateChange = this._onRecordingStateChange.bind(this);
    this._onProfilerStatusUpdated = this._onProfilerStatusUpdated.bind(this);

    // Store data regarding if e10s is enabled.
    this._e10s = Services.appinfo.browserTabsRemoteAutostart;
    this._setMultiprocessAttributes();

    this._prefs = require("devtools/performance/global").PREFS;
    this._prefs.on("pref-changed", this._onPrefChanged);

    gFront.on("recording-starting", this._onRecordingStateChange);
    gFront.on("recording-started", this._onRecordingStateChange);
    gFront.on("recording-stopping", this._onRecordingStateChange);
    gFront.on("recording-stopped", this._onRecordingStateChange);
    gFront.on("profiler-status", this._onProfilerStatusUpdated);
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
    this._prefs.off("pref-changed", this._onPrefChanged);

    gFront.off("recording-starting", this._onRecordingStateChange);
    gFront.off("recording-started", this._onRecordingStateChange);
    gFront.off("recording-stopping", this._onRecordingStateChange);
    gFront.off("recording-stopped", this._onRecordingStateChange);
    gFront.off("profiler-status", this._onProfilerStatusUpdated);
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
   * OptionsView in the ToolbarView. This preference is guaranteed to be
   * displayed in the UI.
   *
   * @param string prefName
   * @return boolean
   */
  getOption: function (prefName) {
    return ToolbarView.optionsView.getPref(prefName);
  },

  /**
   * Get a preference setting from `prefName`. This preference can be of
   * any type and might not be displayed in the UI.
   *
   * @param string prefName
   * @return any
   */
  getPref: function (prefName) {
    return this._prefs[prefName];
  },

  /**
   * Set a preference setting from `prefName`. This preference can be of
   * any type and might not be displayed in the UI.
   *
   * @param string prefName
   * @param any prefValue
   */
  setPref: function (prefName, prefValue) {
    this._prefs[prefName] = prefValue;
  },

  /**
   * Starts recording with the PerformanceFront. Emits `EVENTS.RECORDING_STARTED`
   * when the front has started to record.
   */
  startRecording: Task.async(function *() {
    let options = {
      withMarkers: true,
      withMemory: this.getOption("enable-memory"),
      withTicks: this.getOption("enable-framerate"),
      withAllocations: this.getOption("enable-memory"),
      allocationsSampleProbability: this.getPref("memory-sample-probability"),
      allocationsMaxLogLength: this.getPref("memory-max-log-length"),
      bufferSize: this.getPref("profiler-buffer-size"),
      sampleFrequency: this.getPref("profiler-sample-frequency")
    };

    yield gFront.startRecording(options);
  }),

  /**
   * Stops recording with the PerformanceFront. Emits `EVENTS.RECORDING_STOPPED`
   * when the front has stopped recording.
   */
  stopRecording: Task.async(function *() {
    let recording = this.getLatestManualRecording();
    yield gFront.stopRecording(recording);
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
    // If last recording is not recording, but finalizing itself,
    // wait for that to finish
    if (latest && !latest.isCompleted()) {
      yield this.once(EVENTS.RECORDING_STOPPED);
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
   * Emitted when the front updates RecordingModel's buffer status.
   */
  _onProfilerStatusUpdated: function (_, data) {
    this.emit(EVENTS.PROFILER_STATUS_UPDATED, data);
  },

  /**
   * Fired when a recording model changes state.
   *
   * @param {string} state
   * @param {RecordingModel} model
   */
  _onRecordingStateChange: function (state, model) {
    // If we get a state change for a recording that isn't being tracked in the front,
    // just ignore it. This can occur when stopping a profile via console that was cleared.
    if (state !== "recording-starting" && this.getRecordings().indexOf(model) === -1) {
      return;
    }
    switch (state) {
      // Fired when a RecordingModel was just created from the front
      case "recording-starting":
        // When a recording is just starting, store it internally
        this._recordings.push(model);
        this.emit(EVENTS.RECORDING_WILL_START, model);
        break;
      // Fired when a RecordingModel has started recording
      case "recording-started":
        this.emit(EVENTS.RECORDING_STARTED, model);
        break;
      // Fired when a RecordingModel is no longer recording, and
      // starting to fetch all the profiler data
      case "recording-stopping":
        this.emit(EVENTS.RECORDING_WILL_STOP, model);
        break;
      // Fired when a RecordingModel is finished fetching all of its data
      case "recording-stopped":
        this.emit(EVENTS.RECORDING_STOPPED, model);
        break;
    }
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
   * @option {boolean} mustBeCompleted
   *         A boolean indicating whether the recording must be either completed or not.
   *         Setting to undefined will allow either state.
   * @param {RecordingModel} recording
   *        An optional recording model to use instead of the currently selected.
   *
   * @return boolean
   */
  isFeatureSupported: function ({ features, actors, mustBeCompleted }, recording) {
    recording = recording || this.getCurrentRecording();
    let recordingConfig = recording ? recording.getConfiguration() : {};
    let currentCompletedState = recording ? recording.isCompleted() : void 0;
    let actorsSupported = gFront.getActorSupport();

    if (mustBeCompleted != null && mustBeCompleted !== currentCompletedState) {
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

  /**
   * Returns an object with `supported` and `enabled` properties indicating
   * whether or not the platform is capable of turning on e10s and whether or not
   * it's already enabled, respectively.
   *
   * @return {object}
   */
  getMultiprocessStatus: function () {
    // If testing, set both supported and enabled to true so we
    // have realtime rendering tests in non-e10s. This function is
    // overridden wholesale in tests when we want to test multiprocess support
    // specifically.
    if (gDevTools.testing) {
      return { supported: true, enabled: true };
    }
    let supported = SYSTEM.MULTIPROCESS_SUPPORTED;
    // This is only checked on tool startup -- requires a restart if
    // e10s subsequently enabled.
    let enabled = this._e10s;
    return { supported, enabled };
  },

  /**
   * Called on init, sets an `e10s` attribute on the main view container with
   * "disabled" if e10s is possible on the platform and just not on, or "unsupported"
   * if e10s is not possible on the platform. If e10s is on, no attribute is set.
   */
  _setMultiprocessAttributes: function () {
    let { enabled, supported } = this.getMultiprocessStatus();
    if (!enabled && supported) {
      $("#performance-view").setAttribute("e10s", "disabled");
    }
    // Could be a chance where the directive goes away yet e10s is still on
    else if (!enabled && !supported) {
      $("#performance-view").setAttribute("e10s", "unsupported");
    }
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

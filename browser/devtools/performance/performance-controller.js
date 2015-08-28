/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const { loader, require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

const { Task } = require("resource://gre/modules/Task.jsm");
const { Heritage, ViewHelpers, WidgetMethods } = require("resource:///modules/devtools/ViewHelpers.jsm");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/toolkit/DevToolsUtils");
loader.lazyRequireGetter(this, "system",
  "devtools/toolkit/shared/system");

// Logic modules

loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/performance/markers", true);
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/toolkit/performance/utils");
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

  // When a new recording is being tracked in the panel.
  NEW_RECORDING: "Performance:NewRecording",

  // When a recording is started or stopped or stopping via the PerformanceController
  RECORDING_STATE_CHANGE: "Performance:RecordingStateChange",

  // Emitted by the PerformanceController or RecordingView
  // when a recording model is selected
  RECORDING_SELECTED: "Performance:RecordingSelected",

  // When recordings have been cleared out
  RECORDINGS_CLEARED: "Performance:RecordingsCleared",

  // When a recording is exported via the PerformanceController
  RECORDING_EXPORTED: "Performance:RecordingExported",

  // When the front has updated information on the profiler's circular buffer
  PROFILER_STATUS_UPDATED: "Performance:BufferUpdated",

  // When the PerformanceView updates the display of the buffer status
  UI_BUFFER_STATUS_UPDATED: "Performance:UI:BufferUpdated",

  // Emitted by the OptimizationsListView when it renders new optimization
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
  SOURCE_NOT_FOUND_IN_JS_DEBUGGER: "Performance:UI:SourceNotFoundInJsDebugger",

  // These are short hands for the RECORDING_STATE_CHANGE event to make refactoring
  // tests easier. UI components should use RECORDING_STATE_CHANGE, and these are
  // deprecated for test usage only.
  RECORDING_STARTED: "Performance:RecordingStarted",
  RECORDING_WILL_STOP: "Performance:RecordingWillStop",
  RECORDING_STOPPED: "Performance:RecordingStopped",

  // Fired by the PerformanceController when `populateWithRecordings` is finished.
  RECORDINGS_SEEDED: "Performance:RecordingsSeeded",

  // Emitted by the PerformanceController when `PerformanceController.stopRecording()`
  // is completed; used in tests, to know when a manual UI click is finished.
  CONTROLLER_STOPPED_RECORDING: "Performance:Controller:StoppedRecording",

  // Emitted by the PerformanceController when a recording is imported. Used
  // only in tests. Should use the normal RECORDING_STATE_CHANGE in the UI.
  RECORDING_IMPORTED: "Performance:ImportedRecording",
};

/**
 * The current target, toolbox and PerformanceFront, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Initializes the profiler controller and views.
 */
let startupPerformance = Task.async(function*() {
  yield PerformanceController.initialize();
  yield PerformanceView.initialize();
});

/**
 * Destroys the profiler controller and views.
 */
let shutdownPerformance = Task.async(function*() {
  yield PerformanceController.destroy();
  yield PerformanceView.destroy();
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
    this._onFrontEvent = this._onFrontEvent.bind(this);

    // Store data regarding if e10s is enabled.
    this._e10s = Services.appinfo.browserTabsRemoteAutostart;
    this._setMultiprocessAttributes();

    this._prefs = require("devtools/performance/global").PREFS;
    this._prefs.on("pref-changed", this._onPrefChanged);

    gFront.on("*", this._onFrontEvent);
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

    gFront.off("*", this._onFrontEvent);
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
   * Starts recording with the PerformanceFront.
   */
  startRecording: Task.async(function *() {
    let options = {
      withMarkers: true,
      withMemory: this.getOption("enable-memory"),
      withTicks: this.getOption("enable-framerate"),
      withJITOptimizations: this.getOption("enable-jit-optimizations"),
      withAllocations: this.getOption("enable-allocations"),
      allocationsSampleProbability: this.getPref("memory-sample-probability"),
      allocationsMaxLogLength: this.getPref("memory-max-log-length"),
      bufferSize: this.getPref("profiler-buffer-size"),
      sampleFrequency: this.getPref("profiler-sample-frequency")
    };

    yield gFront.startRecording(options);
  }),

  /**
   * Stops recording with the PerformanceFront.
   */
  stopRecording: Task.async(function *() {
    let recording = this.getLatestManualRecording();
    yield gFront.stopRecording(recording);

    // Emit another stop event here, as a lot of tests use
    // the RECORDING_STOPPED event, but in the case of a UI click on a button,
    // the RECORDING_STOPPED event happens from the server, where this request may
    // not have yet finished, so listen to this in tests that fail because the `stopRecording`
    // request is not yet completed. Should only be used in that scenario.
    this.emit(EVENTS.CONTROLLER_STOPPED_RECORDING);
  }),

  /**
   * Saves the given recording to a file. Emits `EVENTS.RECORDING_EXPORTED`
   * when the file was saved.
   *
   * @param PerformanceRecording recording
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
      yield this.waitForStateChangeOnRecording(latest, "recording-stopped");
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
    let recording = yield gFront.importRecording(file);
    this._addNewRecording(recording);

    // Only emit in tests for legacy purposes for shorthand --
    // other things in UI should handle the generic NEW_RECORDING
    // event to handle lazy recordings.
    if (DevToolsUtils.testing) {
      this.emit(EVENTS.RECORDING_IMPORTED, recording);
    }
  }),

  /**
   * Sets the currently active PerformanceRecording. Should rarely be called directly,
   * as RecordingsView handles this when manually selected a recording item. Exceptions
   * are when clearing the view.
   * @param PerformanceRecording recording
   */
  setCurrentRecording: function (recording) {
    if (this._currentRecording !== recording) {
      this._currentRecording = recording;
      this.emit(EVENTS.RECORDING_SELECTED, recording);
    }
  },

  /**
   * Gets the currently active PerformanceRecording.
   * @return PerformanceRecording
   */
  getCurrentRecording: function () {
    return this._currentRecording;
  },

  /**
   * Get most recently added recording that was triggered manually (via UI).
   * @return PerformanceRecording
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
   * Fired from the front on any event. Propagates to other handlers from here.
   */
  _onFrontEvent: function (eventName, ...data) {
    if (eventName === "profiler-status") {
      this._onProfilerStatusUpdated(...data);
      return;
    }

    if (["recording-started", "recording-stopped", "recording-stopping"].indexOf(eventName) !== -1) {
      this._onRecordingStateChange(eventName, ...data);
    }
  },

  /**
   * Emitted when the front updates PerformanceRecording's buffer status.
   */
  _onProfilerStatusUpdated: function (data) {
    this.emit(EVENTS.PROFILER_STATUS_UPDATED, data);
  },

  /**
   * Stores a recording internally.
   *
   * @param {PerformanceRecordingFront} recording
   */
  _addNewRecording: function (recording) {
    if (this._recordings.indexOf(recording) === -1) {
      this._recordings.push(recording);
      this.emit(EVENTS.NEW_RECORDING, recording);
    }
  },

  /**
   * Fired when a recording model changes state.
   *
   * @param {string} state
   *        Can be "recording-started", "recording-stopped" or "recording-stopping".
   * @param {PerformanceRecording} model
   */
  _onRecordingStateChange: function (state, model) {
    this._addNewRecording(model);

    this.emit(EVENTS.RECORDING_STATE_CHANGE, state, model);

    // Emit the state specific events for tests that I'm too
    // lazy and frusterated to change right now. These events
    // should only be used in tests, as the rest of the UI should
    // react to general RECORDING_STATE_CHANGE events and NEW_RECORDING
    // events to handle lazy recordings.
    if (DevToolsUtils.testing) {
      switch (state) {
        case "recording-started":
          this.emit(EVENTS.RECORDING_STARTED, model);
          break;
        case "recording-stopping":
          this.emit(EVENTS.RECORDING_WILL_STOP, model);
          break;
        case "recording-stopped":
          this.emit(EVENTS.RECORDING_STOPPED, model);
          break;
      }
    }
  },

  /**
   * Takes a recording and returns a value between 0 and 1 indicating how much
   * of the buffer is used.
   */
  getBufferUsageForRecording: function (recording) {
    return gFront.getBufferUsageForRecording(recording);
  },

  /**
   * Returns a boolean indicating if any recordings are currently in progress or not.
   */
  isRecording: function () {
    return this._recordings.some(r => r.isRecording());
  },

  /**
   * Returns the internal store of recording models.
   */
  getRecordings: function () {
    return this._recordings;
  },

  /**
   * Returns traits from the front.
   */
  getTraits: function () {
    return gFront.traits;
  },

  /**
   * Utility method taking a string or an array of strings of feature names (like
   * "withAllocations" or "withMarkers"), and returns whether or not the current
   * recording supports that feature, based off of UI preferences and server support.
   *
   * @option {Array<string>|string} features
   *         A string or array of strings indicating what configuration is needed on the recording
   *         model, like `withTicks`, or `withMemory`.
   *
   * @return boolean
   */
  isFeatureSupported: function (features) {
    if (!features) {
      return true;
    }

    let recording = this.getCurrentRecording();
    if (!recording) {
      return false;
    }

    let config = recording.getConfiguration();
    return [].concat(features).every(f => config[f]);
  },

  /**
   * Takes an array of PerformanceRecordingFronts and adds them to the internal
   * store of the UI. Used by the toolbox to lazily seed recordings that
   * were observed before the panel was loaded in the scenario where `console.profile()`
   * is used before the tool is loaded.
   *
   * @param {Array<PerformanceRecordingFront>} recordings
   */
  populateWithRecordings: function (recordings=[]) {
    for (let recording of recordings) {
      PerformanceController._addNewRecording(recording);
    }
    this.emit(EVENTS.RECORDINGS_SEEDED);
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
    if (DevToolsUtils.testing) {
      return { supported: true, enabled: true };
    }
    let supported = system.constants.E10S_TESTING_ONLY;
    // This is only checked on tool startup -- requires a restart if
    // e10s subsequently enabled.
    let enabled = this._e10s;
    return { supported, enabled };
  },

  /**
   * Takes a PerformanceRecording and a state, and waits for
   * the event to be emitted from the front for that recording.
   *
   * @param {PerformanceRecordingFront} recording
   * @param {string} expectedState
   * @return {Promise}
   */
  waitForStateChangeOnRecording: Task.async(function *(recording, expectedState) {
    let deferred = promise.defer();
    this.on(EVENTS.RECORDING_STATE_CHANGE, function handler (state, model) {
      if (state === expectedState && model === recording) {
        this.off(EVENTS.RECORDING_STATE_CHANGE, handler);
        deferred.resolve();
      }
    });
    yield deferred.promise;
  }),

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

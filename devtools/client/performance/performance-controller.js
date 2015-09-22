/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const { loader, require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

const { Task } = require("resource://gre/modules/Task.jsm");
const { Heritage, ViewHelpers, WidgetMethods } = require("resource:///modules/devtools/client/shared/widgets/ViewHelpers.jsm");

// Events emitted by various objects in the panel.
const EVENTS = require("devtools/client/performance/events");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "system",
  "devtools/shared/shared/system");

// Logic modules

loader.lazyRequireGetter(this, "L10N",
  "devtools/client/performance/modules/global", true);
loader.lazyRequireGetter(this, "PerformanceTelemetry",
  "devtools/client/performance/modules/logic/telemetry", true);
loader.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/client/performance/modules/markers", true);
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/shared/performance/utils");
loader.lazyRequireGetter(this, "GraphsController",
  "devtools/client/performance/modules/widgets/graphs", true);
loader.lazyRequireGetter(this, "OptimizationsGraph",
  "devtools/client/performance/modules/widgets/graphs", true);
loader.lazyRequireGetter(this, "WaterfallHeader",
  "devtools/client/performance/modules/widgets/waterfall-ticks", true);
loader.lazyRequireGetter(this, "MarkerView",
  "devtools/client/performance/modules/widgets/marker-view", true);
loader.lazyRequireGetter(this, "MarkerDetails",
  "devtools/client/performance/modules/widgets/marker-details", true);
loader.lazyRequireGetter(this, "MarkerUtils",
  "devtools/client/performance/modules/logic/marker-utils");
loader.lazyRequireGetter(this, "WaterfallUtils",
  "devtools/client/performance/modules/logic/waterfall-utils");
loader.lazyRequireGetter(this, "FrameUtils",
  "devtools/client/performance/modules/logic/frame-utils");
loader.lazyRequireGetter(this, "CallView",
  "devtools/client/performance/modules/widgets/tree-view", true);
loader.lazyRequireGetter(this, "ThreadNode",
  "devtools/client/performance/modules/logic/tree-model", true);
loader.lazyRequireGetter(this, "FrameNode",
  "devtools/client/performance/modules/logic/tree-model", true);
loader.lazyRequireGetter(this, "JITOptimizations",
  "devtools/client/performance/modules/logic/jit", true);

// Widgets modules

loader.lazyRequireGetter(this, "OptionsView",
  "devtools/client/shared/options-view", true);
loader.lazyRequireGetter(this, "FlameGraphUtils",
  "devtools/client/shared/widgets/FlameGraph", true);
loader.lazyRequireGetter(this, "FlameGraph",
  "devtools/client/shared/widgets/FlameGraph", true);
loader.lazyRequireGetter(this, "TreeWidget",
  "devtools/client/shared/widgets/TreeWidget", true);

loader.lazyImporter(this, "SideMenuWidget",
  "resource:///modules/devtools/client/shared/widgets/SideMenuWidget.jsm");
loader.lazyImporter(this, "setNamedTimeout",
  "resource:///modules/devtools/client/shared/widgets/ViewHelpers.jsm");
loader.lazyImporter(this, "clearNamedTimeout",
  "resource:///modules/devtools/client/shared/widgets/ViewHelpers.jsm");
loader.lazyImporter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

const BRANCH_NAME = "devtools.performance.ui.";

/**
 * The current target, toolbox and PerformanceFront, set by this tool's host.
 */
var gToolbox, gTarget, gFront;

/**
 * Initializes the profiler controller and views.
 */
var startupPerformance = Task.async(function*() {
  yield PerformanceController.initialize();
  yield PerformanceView.initialize();
});

/**
 * Destroys the profiler controller and views.
 */
var shutdownPerformance = Task.async(function*() {
  yield PerformanceController.destroy();
  yield PerformanceView.destroy();
});

/**
 * Functions handling target-related lifetime events and
 * UI interaction.
 */
var PerformanceController = {
  _recordings: [],
  _currentRecording: null,

  /**
   * Listen for events emitted by the current tab target and
   * main UI events.
   */
  initialize: Task.async(function* () {
    this._telemetry = new PerformanceTelemetry(this);
    this.startRecording = this.startRecording.bind(this);
    this.stopRecording = this.stopRecording.bind(this);
    this.importRecording = this.importRecording.bind(this);
    this.exportRecording = this.exportRecording.bind(this);
    this.clearRecordings = this.clearRecordings.bind(this);
    this._onRecordingSelectFromView = this._onRecordingSelectFromView.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);
    this._onThemeChanged = this._onThemeChanged.bind(this);
    this._onFrontEvent = this._onFrontEvent.bind(this);
    this._pipe = this._pipe.bind(this);

    // Store data regarding if e10s is enabled.
    this._e10s = Services.appinfo.browserTabsRemoteAutostart;
    this._setMultiprocessAttributes();

    this._prefs = require("devtools/client/performance/modules/global").PREFS;
    this._prefs.on("pref-changed", this._onPrefChanged);

    gFront.on("*", this._onFrontEvent);
    ToolbarView.on(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.on(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.on(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.on(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.on(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.on(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);
    DetailsView.on(EVENTS.DETAILS_VIEW_SELECTED, this._pipe);

    gDevTools.on("pref-changed", this._onThemeChanged);
  }),

  /**
   * Remove events handled by the PerformanceController
   */
  destroy: function() {
    this._telemetry.destroy();
    this._prefs.off("pref-changed", this._onPrefChanged);

    gFront.off("*", this._onFrontEvent);
    ToolbarView.off(EVENTS.PREF_CHANGED, this._onPrefChanged);
    PerformanceView.off(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.off(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.off(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.off(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.off(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.off(EVENTS.RECORDING_SELECTED, this._onRecordingSelectFromView);
    DetailsView.off(EVENTS.DETAILS_VIEW_SELECTED, this._pipe);

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
    this.emit(EVENTS.RECORDING_EXPORTED, recording, file);
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

    this.emit(EVENTS.RECORDING_IMPORTED, recording);
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
    // should only be used in tests and specific rare cases (telemetry),
    // as the rest of the UI should react to general RECORDING_STATE_CHANGE
    // events and NEW_RECORDING events to handle lazy recordings.
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

  /**
   * Pipes an event from some source to the PerformanceController.
   */
  _pipe: function (eventName, ...data) {
    this.emit(eventName, ...data);
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* globals window, document, PerformanceView, ToolbarView, RecordingsView, DetailsView */

/* exported Cc, Ci, Cu, Cr, loader, Promise */
var BrowserLoaderModule = {};
ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", BrowserLoaderModule);
var { loader, require } = BrowserLoaderModule.BrowserLoader({
  baseURI: "resource://devtools/client/performance/",
  window
});
/* exported ViewHelpers, WidgetMethods, setNamedTimeout, clearNamedTimeout */
var { ViewHelpers, WidgetMethods, setNamedTimeout, clearNamedTimeout } = require("devtools/client/shared/widgets/view-helpers");
var { PrefObserver } = require("devtools/client/shared/prefs");
/* exported extend */
const { extend } = require("devtools/shared/extend");
// Use privileged promise in panel documents to prevent having them to freeze
// during toolbox destruction. See bug 1402779.
var Promise = require("Promise");

// Events emitted by various objects in the panel.
var EVENTS = require("devtools/client/performance/events");
Object.defineProperty(this, "EVENTS", {
  value: EVENTS,
  enumerable: true,
  writable: false
});

/* exported React, ReactDOM, JITOptimizationsView, RecordingControls, RecordingButton,
   RecordingList, RecordingListItem, Services, Waterfall, promise, EventEmitter,
   DevToolsUtils, system */
var React = require("devtools/client/shared/vendor/react");
var ReactDOM = require("devtools/client/shared/vendor/react-dom");
var Waterfall = React.createFactory(require("devtools/client/performance/components/waterfall"));
var JITOptimizationsView = React.createFactory(require("devtools/client/performance/components/jit-optimizations"));
var RecordingControls = React.createFactory(require("devtools/client/performance/components/recording-controls"));
var RecordingButton = React.createFactory(require("devtools/client/performance/components/recording-button"));
var RecordingList = React.createFactory(require("devtools/client/performance/components/recording-list"));
var RecordingListItem = React.createFactory(require("devtools/client/performance/components/recording-list-item"));

var Services = require("Services");
var promise = require("promise");
const defer = require("devtools/shared/defer");
var EventEmitter = require("devtools/shared/event-emitter");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var flags = require("devtools/shared/flags");
var system = require("devtools/shared/system");

// Logic modules
/* exported L10N, PerformanceTelemetry, TIMELINE_BLUEPRINT, RecordingUtils,
   PerformanceUtils, OptimizationsGraph, GraphsController,
   MarkerDetails, MarkerBlueprintUtils, WaterfallUtils, FrameUtils, CallView, ThreadNode,
   FrameNode */
var { L10N } = require("devtools/client/performance/modules/global");
var { PerformanceTelemetry } = require("devtools/client/performance/modules/logic/telemetry");
var { TIMELINE_BLUEPRINT } = require("devtools/client/performance/modules/markers");
var RecordingUtils = require("devtools/shared/performance/recording-utils");
var PerformanceUtils = require("devtools/client/performance/modules/utils");
var { OptimizationsGraph, GraphsController } = require("devtools/client/performance/modules/widgets/graphs");
var { MarkerDetails } = require("devtools/client/performance/modules/widgets/marker-details");
var { MarkerBlueprintUtils } = require("devtools/client/performance/modules/marker-blueprint-utils");
var WaterfallUtils = require("devtools/client/performance/modules/logic/waterfall-utils");
var FrameUtils = require("devtools/client/performance/modules/logic/frame-utils");
var { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
var { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
var { FrameNode } = require("devtools/client/performance/modules/logic/tree-model");

// Widgets modules

/* exported OptionsView, FlameGraph, FlameGraphUtils, TreeWidget, SideMenuWidget */
var { OptionsView } = require("devtools/client/shared/options-view");
var { FlameGraph, FlameGraphUtils } = require("devtools/client/shared/widgets/FlameGraph");
var { TreeWidget } = require("devtools/client/shared/widgets/TreeWidget");
var { SideMenuWidget } = require("resource://devtools/client/shared/widgets/SideMenuWidget.jsm");

/* exported BRANCH_NAME */
var BRANCH_NAME = "devtools.performance.ui.";

/**
 * The current target, toolbox and PerformanceFront, set by this tool's host.
 */
/* exported gToolbox, gTarget, gFront */
var gToolbox, gTarget, gFront;

/* exported startupPerformance, shutdownPerformance, PerformanceController */

/**
 * Initializes the profiler controller and views.
 */
var startupPerformance = async function() {
  await PerformanceController.initialize();
  await PerformanceView.initialize();
  PerformanceController.enableFrontEventListeners();
};

/**
 * Destroys the profiler controller and views.
 */
var shutdownPerformance = async function() {
  await PerformanceController.destroy();
  await PerformanceView.destroy();
  PerformanceController.disableFrontEventListeners();
};

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
  async initialize() {
    this._telemetry = new PerformanceTelemetry(this);
    this.startRecording = this.startRecording.bind(this);
    this.stopRecording = this.stopRecording.bind(this);
    this.importRecording = this.importRecording.bind(this);
    this.exportRecording = this.exportRecording.bind(this);
    this.clearRecordings = this.clearRecordings.bind(this);
    this._onRecordingSelectFromView = this._onRecordingSelectFromView.bind(this);
    this._onPrefChanged = this._onPrefChanged.bind(this);
    this._onThemeChanged = this._onThemeChanged.bind(this);
    this._onDetailsViewSelected = this._onDetailsViewSelected.bind(this);
    this._onProfilerStatus = this._onProfilerStatus.bind(this);
    this._onRecordingStarted =
      this._emitRecordingStateChange.bind(this, "recording-started");
    this._onRecordingStopping =
      this._emitRecordingStateChange.bind(this, "recording-stopping");
    this._onRecordingStopped =
      this._emitRecordingStateChange.bind(this, "recording-stopped");

    // Store data regarding if e10s is enabled.
    this._e10s = Services.appinfo.browserTabsRemoteAutostart;
    this._setMultiprocessAttributes();

    this._prefs = require("devtools/client/performance/modules/global").PREFS;
    this._prefs.registerObserver();
    this._prefs.on("pref-changed", this._onPrefChanged);

    ToolbarView.on(EVENTS.UI_PREF_CHANGED, this._onPrefChanged);
    PerformanceView.on(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.on(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.on(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.on(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.on(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.on(EVENTS.UI_RECORDING_SELECTED, this._onRecordingSelectFromView);
    DetailsView.on(EVENTS.UI_DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);

    this._prefObserver = new PrefObserver("devtools.");
    this._prefObserver.on("devtools.theme", this._onThemeChanged);
  },

  /**
   * Remove events handled by the PerformanceController
   */
  destroy: function() {
    this._prefs.off("pref-changed", this._onPrefChanged);
    this._prefs.unregisterObserver();

    ToolbarView.off(EVENTS.UI_PREF_CHANGED, this._onPrefChanged);
    PerformanceView.off(EVENTS.UI_START_RECORDING, this.startRecording);
    PerformanceView.off(EVENTS.UI_STOP_RECORDING, this.stopRecording);
    PerformanceView.off(EVENTS.UI_IMPORT_RECORDING, this.importRecording);
    PerformanceView.off(EVENTS.UI_CLEAR_RECORDINGS, this.clearRecordings);
    RecordingsView.off(EVENTS.UI_EXPORT_RECORDING, this.exportRecording);
    RecordingsView.off(EVENTS.UI_RECORDING_SELECTED, this._onRecordingSelectFromView);
    DetailsView.off(EVENTS.UI_DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);

    this._prefObserver.off("devtools.theme", this._onThemeChanged);
    this._prefObserver.destroy();

    this._telemetry.destroy();
  },

  /**
   * Enables front event listeners.
   *
   * The rationale behind this is given by the async intialization of all the
   * frontend components. Even though the panel is considered "open" only after
   * both the controller and the view are created, and even though their
   * initialization is sequential (controller, then view), the controller might
   * start handling backend events before the view finishes if the event
   * listeners are added too soon.
   */
  enableFrontEventListeners: function() {
    gFront.on("profiler-status", this._onProfilerStatus);
    gFront.on("recording-started", this._onRecordingStarted);
    gFront.on("recording-stopping", this._onRecordingStopping);
    gFront.on("recording-stopped", this._onRecordingStopped);
  },

  /**
   * Disables front event listeners.
   */
  disableFrontEventListeners: function() {
    gFront.off("profiler-status", this._onProfilerStatus);
    gFront.off("recording-started", this._onRecordingStarted);
    gFront.off("recording-stopping", this._onRecordingStopping);
    gFront.off("recording-stopped", this._onRecordingStopped);
  },

  /**
   * Returns the current devtools theme.
   */
  getTheme: function() {
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
  getOption: function(prefName) {
    return ToolbarView.optionsView.getPref(prefName);
  },

  /**
   * Get a preference setting from `prefName`. This preference can be of
   * any type and might not be displayed in the UI.
   *
   * @param string prefName
   * @return any
   */
  getPref: function(prefName) {
    return this._prefs[prefName];
  },

  /**
   * Set a preference setting from `prefName`. This preference can be of
   * any type and might not be displayed in the UI.
   *
   * @param string prefName
   * @param any prefValue
   */
  setPref: function(prefName, prefValue) {
    this._prefs[prefName] = prefValue;
  },

  /**
   * Checks whether or not a new recording is supported by the PerformanceFront.
   * @return Promise:boolean
   */
  async canCurrentlyRecord() {
    const hasActor = await gTarget.hasActor("performance");
    if (!hasActor) {
      return true;
    }
    const actorCanCheck =
      await gTarget.actorHasMethod("performance", "canCurrentlyRecord");
    if (!actorCanCheck) {
      return true;
    }
    return (await gFront.canCurrentlyRecord()).success;
  },

  /**
   * Starts recording with the PerformanceFront.
   */
  async startRecording() {
    const options = {
      withMarkers: true,
      withTicks: this.getOption("enable-framerate"),
      withMemory: this.getOption("enable-memory"),
      withFrames: true,
      withGCEvents: true,
      withAllocations: this.getOption("enable-allocations"),
      allocationsSampleProbability: this.getPref("memory-sample-probability"),
      allocationsMaxLogLength: this.getPref("memory-max-log-length"),
      bufferSize: this.getPref("profiler-buffer-size"),
      sampleFrequency: this.getPref("profiler-sample-frequency")
    };

    const recordingStarted = await gFront.startRecording(options);

    // In some cases, like when the target has a private browsing tab,
    // recording is not currently supported because of the profiler module.
    // Present a notification in this case alerting the user of this issue.
    if (!recordingStarted) {
      this.emit(EVENTS.BACKEND_FAILED_AFTER_RECORDING_START);
      PerformanceView.setState("unavailable");
    } else {
      this.emit(EVENTS.BACKEND_READY_AFTER_RECORDING_START);
    }
  },

  /**
   * Stops recording with the PerformanceFront.
   */
  async stopRecording() {
    const recording = this.getLatestManualRecording();
    await gFront.stopRecording(recording);
    this.emit(EVENTS.BACKEND_READY_AFTER_RECORDING_STOP);
  },

  /**
   * Saves the given recording to a file. Emits `EVENTS.RECORDING_EXPORTED`
   * when the file was saved.
   *
   * @param PerformanceRecording recording
   *        The model that holds the recording data.
   * @param nsIFile file
   *        The file to stream the data into.
   */
  async exportRecording(recording, file) {
    await recording.exportRecording(file);
    this.emit(EVENTS.RECORDING_EXPORTED, recording, file);
  },

   /**
   * Clears all completed recordings from the list as well as the current non-console
   * recording. Emits `EVENTS.RECORDING_DELETED` when complete so other components can
   * clean up.
   */
  async clearRecordings() {
    for (let i = this._recordings.length - 1; i >= 0; i--) {
      const model = this._recordings[i];
      if (!model.isConsole() && model.isRecording()) {
        await this.stopRecording();
      }
      // If last recording is not recording, but finalizing itself,
      // wait for that to finish
      if (!model.isRecording() && !model.isCompleted()) {
        await this.waitForStateChangeOnRecording(model, "recording-stopped");
      }
      // If recording is completed,
      // clean it up from UI and remove it from the _recordings array.
      if (model.isCompleted()) {
        this.emit(EVENTS.RECORDING_DELETED, model);
        this._recordings.splice(i, 1);
      }
    }
    if (this._recordings.length > 0) {
      if (!this._recordings.includes(this.getCurrentRecording())) {
        this.setCurrentRecording(this._recordings[0]);
      }
    } else {
      this.setCurrentRecording(null);
    }
  },

  /**
   * Loads a recording from a file, adding it to the recordings list. Emits
   * `EVENTS.RECORDING_IMPORTED` when the file was loaded.
   *
   * @param nsIFile file
   *        The file to import the data from.
   */
  async importRecording(file) {
    const recording = await gFront.importRecording(file);
    this._addRecordingIfUnknown(recording);

    this.emit(EVENTS.RECORDING_IMPORTED, recording);
  },

  /**
   * Sets the currently active PerformanceRecording. Should rarely be called directly,
   * as RecordingsView handles this when manually selected a recording item. Exceptions
   * are when clearing the view.
   * @param PerformanceRecording recording
   */
  setCurrentRecording: function(recording) {
    if (this._currentRecording !== recording) {
      this._currentRecording = recording;
      this.emit(EVENTS.RECORDING_SELECTED, recording);
    }
  },

  /**
   * Gets the currently active PerformanceRecording.
   * @return PerformanceRecording
   */
  getCurrentRecording: function() {
    return this._currentRecording;
  },

  /**
   * Get most recently added recording that was triggered manually (via UI).
   * @return PerformanceRecording
   */
  getLatestManualRecording: function() {
    for (let i = this._recordings.length - 1; i >= 0; i--) {
      const model = this._recordings[i];
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
  _onRecordingSelectFromView: function(recording) {
    this.setCurrentRecording(recording);
  },

  /**
   * Fired when the ToolbarView fires a PREF_CHANGED event.
   * with the value.
   */
  _onPrefChanged: function(prefName, prefValue) {
    this.emit(EVENTS.PREF_CHANGED, prefName, prefValue);
  },

  /*
   * Called when the developer tools theme changes.
   */
  _onThemeChanged: function() {
    const newValue = Services.prefs.getCharPref("devtools.theme");
    this.emit(EVENTS.THEME_CHANGED, newValue);
  },

  _onProfilerStatus: function(status) {
    this.emit(EVENTS.RECORDING_PROFILER_STATUS_UPDATE, status);
  },

  _emitRecordingStateChange(eventName, recordingModel) {
    this._addRecordingIfUnknown(recordingModel);
    this.emit(EVENTS.RECORDING_STATE_CHANGE, eventName, recordingModel);
  },

  /**
   * Stores a recording internally.
   *
   * @param {PerformanceRecordingFront} recording
   */
  _addRecordingIfUnknown: function(recording) {
    if (!this._recordings.includes(recording)) {
      this._recordings.push(recording);
      this.emit(EVENTS.RECORDING_ADDED, recording);
    }
  },

  /**
   * Takes a recording and returns a value between 0 and 1 indicating how much
   * of the buffer is used.
   */
  getBufferUsageForRecording: function(recording) {
    return gFront.getBufferUsageForRecording(recording);
  },

  /**
   * Returns a boolean indicating if any recordings are currently in progress or not.
   */
  isRecording: function() {
    return this._recordings.some(r => r.isRecording());
  },

  /**
   * Returns the internal store of recording models.
   */
  getRecordings: function() {
    return this._recordings;
  },

  /**
   * Returns traits from the front.
   */
  getTraits: function() {
    return gFront.traits;
  },

  /**
   * Utility method taking a string or an array of strings of feature names (like
   * "withAllocations" or "withMarkers"), and returns whether or not the current
   * recording supports that feature, based off of UI preferences and server support.
   *
   * @option {Array<string>|string} features
   *         A string or array of strings indicating what configuration is needed on the
   *         recording model, like `withTicks`, or `withMemory`.
   *
   * @return boolean
   */
  isFeatureSupported: function(features) {
    if (!features) {
      return true;
    }

    const recording = this.getCurrentRecording();
    if (!recording) {
      return false;
    }

    const config = recording.getConfiguration();
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
  populateWithRecordings: function(recordings = []) {
    for (const recording of recordings) {
      PerformanceController._addRecordingIfUnknown(recording);
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
  getMultiprocessStatus: function() {
    // If testing, set enabled to true so we have realtime rendering tests
    // in non-e10s. This function is overridden wholesale in tests
    // when we want to test multiprocess support
    // specifically.
    if (flags.testing) {
      return { enabled: true };
    }
    // This is only checked on tool startup -- requires a restart if
    // e10s subsequently enabled.
    const enabled = this._e10s;
    return { enabled };
  },

  /**
   * Takes a PerformanceRecording and a state, and waits for
   * the event to be emitted from the front for that recording.
   *
   * @param {PerformanceRecordingFront} recording
   * @param {string} expectedState
   * @return {Promise}
   */
  async waitForStateChangeOnRecording(recording, expectedState) {
    const deferred = defer();
    this.on(EVENTS.RECORDING_STATE_CHANGE, function handler(state, model) {
      if (state === expectedState && model === recording) {
        this.off(EVENTS.RECORDING_STATE_CHANGE, handler);
        deferred.resolve();
      }
    });
    await deferred.promise;
  },

  /**
   * Called on init, sets an `e10s` attribute on the main view container with
   * "disabled" if e10s is possible on the platform and just not on, or "unsupported"
   * if e10s is not possible on the platform. If e10s is on, no attribute is set.
   */
  _setMultiprocessAttributes: function() {
    const { enabled } = this.getMultiprocessStatus();
    if (!enabled) {
      $("#performance-view").setAttribute("e10s", "disabled");
    }
  },

  /**
   * Pipes EVENTS.UI_DETAILS_VIEW_SELECTED to the PerformanceController.
   */
  _onDetailsViewSelected: function(...data) {
    this.emit(EVENTS.UI_DETAILS_VIEW_SELECTED, ...data);
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
/* exported $, $$ */
function $(selector, target = document) {
  return target.querySelector(selector);
}
function $$(selector, target = document) {
  return target.querySelectorAll(selector);
}

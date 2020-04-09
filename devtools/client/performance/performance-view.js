/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals $, $$, PerformanceController */
"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const RecordingControls = React.createFactory(
  require("devtools/client/performance/components/RecordingControls")
);
const RecordingButton = React.createFactory(
  require("devtools/client/performance/components/RecordingButton")
);

const EVENTS = require("devtools/client/performance/events");
const PerformanceUtils = require("devtools/client/performance/modules/utils");
const { DetailsView } = require("devtools/client/performance/views/details");
const { OverviewView } = require("devtools/client/performance/views/overview");
const {
  RecordingsView,
} = require("devtools/client/performance/views/recordings");
const { ToolbarView } = require("devtools/client/performance/views/toolbar");

const { L10N } = require("devtools/client/performance/modules/global");
/**
 * Master view handler for the performance tool.
 */
var PerformanceView = {
  _state: null,

  // Set to true if the front emits a "buffer-status" event, indicating
  // that the server has support for determining buffer status.
  _bufferStatusSupported: false,

  // Mapping of state to selectors for different properties and their values,
  // from the main profiler view. Used in `PerformanceView.setState()`
  states: {
    unavailable: [
      {
        sel: "#performance-view",
        opt: "selectedPanel",
        val: () => $("#unavailable-notice"),
      },
      {
        sel: "#performance-view-content",
        opt: "hidden",
        val: () => true,
      },
    ],
    empty: [
      {
        sel: "#performance-view",
        opt: "selectedPanel",
        val: () => $("#empty-notice"),
      },
      {
        sel: "#performance-view-content",
        opt: "hidden",
        val: () => true,
      },
    ],
    recording: [
      {
        sel: "#performance-view",
        opt: "selectedPanel",
        val: () => $("#performance-view-content"),
      },
      {
        sel: "#performance-view-content",
        opt: "hidden",
        val: () => false,
      },
      {
        sel: "#details-pane-container",
        opt: "selectedPanel",
        val: () => $("#recording-notice"),
      },
    ],
    "console-recording": [
      {
        sel: "#performance-view",
        opt: "selectedPanel",
        val: () => $("#performance-view-content"),
      },
      {
        sel: "#performance-view-content",
        opt: "hidden",
        val: () => false,
      },
      {
        sel: "#details-pane-container",
        opt: "selectedPanel",
        val: () => $("#console-recording-notice"),
      },
    ],
    recorded: [
      {
        sel: "#performance-view",
        opt: "selectedPanel",
        val: () => $("#performance-view-content"),
      },
      {
        sel: "#performance-view-content",
        opt: "hidden",
        val: () => false,
      },
      {
        sel: "#details-pane-container",
        opt: "selectedPanel",
        val: () => $("#details-pane"),
      },
    ],
    loading: [
      {
        sel: "#performance-view",
        opt: "selectedPanel",
        val: () => $("#performance-view-content"),
      },
      {
        sel: "#performance-view-content",
        opt: "hidden",
        val: () => false,
      },
      {
        sel: "#details-pane-container",
        opt: "selectedPanel",
        val: () => $("#loading-notice"),
      },
    ],
  },

  /**
   * Sets up the view with event binding and main subviews.
   */
  async initialize() {
    this._onRecordButtonClick = this._onRecordButtonClick.bind(this);
    this._onImportButtonClick = this._onImportButtonClick.bind(this);
    this._onClearButtonClick = this._onClearButtonClick.bind(this);
    this._onRecordingSelected = this._onRecordingSelected.bind(this);
    this._onProfilerStatusUpdated = this._onProfilerStatusUpdated.bind(this);
    this._onRecordingStateChange = this._onRecordingStateChange.bind(this);
    this._onNewRecordingFailed = this._onNewRecordingFailed.bind(this);

    // Bind to controller events to unlock the record button
    PerformanceController.on(
      EVENTS.RECORDING_SELECTED,
      this._onRecordingSelected
    );
    PerformanceController.on(
      EVENTS.RECORDING_PROFILER_STATUS_UPDATE,
      this._onProfilerStatusUpdated
    );
    PerformanceController.on(
      EVENTS.RECORDING_STATE_CHANGE,
      this._onRecordingStateChange
    );
    PerformanceController.on(
      EVENTS.RECORDING_ADDED,
      this._onRecordingStateChange
    );
    PerformanceController.on(
      EVENTS.BACKEND_FAILED_AFTER_RECORDING_START,
      this._onNewRecordingFailed
    );

    if (await PerformanceController.canCurrentlyRecord()) {
      this.setState("empty");
    } else {
      this.setState("unavailable");
    }

    // Initialize the ToolbarView first, because other views may need access
    // to the OptionsView via the controller, to read prefs.
    await ToolbarView.initialize();
    await RecordingsView.initialize();
    await OverviewView.initialize();
    await DetailsView.initialize();

    // DE-XUL: Begin migrating the toolbar to React. Temporarily hold state here.
    this._recordingControlsState = {
      onRecordButtonClick: this._onRecordButtonClick,
      onImportButtonClick: this._onImportButtonClick,
      onClearButtonClick: this._onClearButtonClick,
      isRecording: false,
      isDisabled: false,
    };
    // Mount to an HTML element.
    const { createHtmlMount } = PerformanceUtils;
    this._recordingControlsMount = createHtmlMount(
      $("#recording-controls-mount")
    );
    this._recordingButtonsMounts = Array.from(
      $$(".recording-button-mount")
    ).map(createHtmlMount);

    this._renderRecordingControls();
  },

  /**
   * DE-XUL: Render the recording controls and buttons using React.
   */
  _renderRecordingControls: function() {
    ReactDOM.render(
      RecordingControls(this._recordingControlsState),
      this._recordingControlsMount
    );
    for (const button of this._recordingButtonsMounts) {
      ReactDOM.render(RecordingButton(this._recordingControlsState), button);
    }
  },

  /**
   * Unbinds events and destroys subviews.
   */
  async destroy() {
    PerformanceController.off(
      EVENTS.RECORDING_SELECTED,
      this._onRecordingSelected
    );
    PerformanceController.off(
      EVENTS.RECORDING_PROFILER_STATUS_UPDATE,
      this._onProfilerStatusUpdated
    );
    PerformanceController.off(
      EVENTS.RECORDING_STATE_CHANGE,
      this._onRecordingStateChange
    );
    PerformanceController.off(
      EVENTS.RECORDING_ADDED,
      this._onRecordingStateChange
    );
    PerformanceController.off(
      EVENTS.BACKEND_FAILED_AFTER_RECORDING_START,
      this._onNewRecordingFailed
    );

    await ToolbarView.destroy();
    await RecordingsView.destroy();
    await OverviewView.destroy();
    await DetailsView.destroy();
  },

  /**
   * Sets the state of the profiler view. Possible options are "unavailable",
   * "empty", "recording", "console-recording", "recorded".
   */
  setState: function(state) {
    // Make sure that the focus isn't captured on a hidden iframe. This fixes a
    // XUL bug where shortcuts stop working.
    const iframes = window.document.querySelectorAll("iframe");
    for (const iframe of iframes) {
      iframe.blur();
    }
    window.focus();

    const viewConfig = this.states[state];
    if (!viewConfig) {
      throw new Error(`Invalid state for PerformanceView: ${state}`);
    }
    for (const { sel, opt, val } of viewConfig) {
      for (const el of $$(sel)) {
        el[opt] = val();
      }
    }

    this._state = state;

    if (state === "console-recording") {
      const recording = PerformanceController.getCurrentRecording();
      let label = recording.getLabel() || "";

      // Wrap the label in quotes if it exists for the commands.
      label = label ? `"${label}"` : "";

      const startCommand = $(
        ".console-profile-recording-notice .console-profile-command"
      );
      const stopCommand = $(
        ".console-profile-stop-notice .console-profile-command"
      );

      startCommand.value = `console.profile(${label})`;
      stopCommand.value = `console.profileEnd(${label})`;
    }

    this.updateBufferStatus();
    this.emit(EVENTS.UI_STATE_CHANGED, state);
  },

  /**
   * Returns the state of the PerformanceView.
   */
  getState: function() {
    return this._state;
  },

  /**
   * Reset the displayed buffer status.
   * Called for every target-switching.
   */
  resetBufferStatus() {
    this._bufferStatusSupported = false;
    $("#details-pane-container").removeAttribute("buffer-status");
  },

  /**
   * Updates the displayed buffer status.
   */
  updateBufferStatus: function() {
    // If we've never seen a "buffer-status" event from the front, ignore
    // and keep the buffer elements hidden.
    if (!this._bufferStatusSupported) {
      return;
    }

    const recording = PerformanceController.getCurrentRecording();
    if (!recording || !recording.isRecording()) {
      return;
    }

    const bufferUsage =
      PerformanceController.getBufferUsageForRecording(recording) || 0;

    // Normalize to a percentage value
    const percent = Math.floor(bufferUsage * 100);

    const $container = $("#details-pane-container");
    const $bufferLabel = $(".buffer-status-message", $container.selectedPanel);

    // Be a little flexible on the buffer status, although not sure how
    // this could happen, as RecordingModel clamps.
    if (percent >= 99) {
      $container.setAttribute("buffer-status", "full");
    } else {
      $container.setAttribute("buffer-status", "in-progress");
    }

    $bufferLabel.value = L10N.getFormatStr("profiler.bufferFull", percent);
    this.emit(EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED, percent);
  },

  /**
   * Toggles the `locked` attribute on the record buttons based
   * on `lock`.
   *
   * @param {boolean} lock
   */
  _lockRecordButtons: function(lock) {
    this._recordingControlsState.isLocked = lock;
    this._renderRecordingControls();
  },

  /*
   * Toggles the `checked` attribute on the record buttons based
   * on `activate`.
   *
   * @param {boolean} activate
   */
  _toggleRecordButtons: function(activate) {
    this._recordingControlsState.isRecording = activate;
    this._renderRecordingControls();
  },

  /**
   * When a recording has started.
   */
  _onRecordingStateChange: function() {
    const currentRecording = PerformanceController.getCurrentRecording();
    const recordings = PerformanceController.getRecordings();

    this._toggleRecordButtons(
      !!recordings.find(r => !r.isConsole() && r.isRecording())
    );
    this._lockRecordButtons(
      !!recordings.find(r => !r.isConsole() && r.isFinalizing())
    );

    if (currentRecording && currentRecording.isFinalizing()) {
      this.setState("loading");
    }
    if (currentRecording && currentRecording.isCompleted()) {
      this.setState("recorded");
    }
    if (currentRecording && currentRecording.isRecording()) {
      this.updateBufferStatus();
    }
  },

  /**
   * When starting a recording has failed.
   */
  _onNewRecordingFailed: function() {
    this._lockRecordButtons(false);
    this._toggleRecordButtons(false);
  },

  /**
   * Handler for clicking the clear button.
   */
  _onClearButtonClick: function(e) {
    this.emit(EVENTS.UI_CLEAR_RECORDINGS);
  },

  /**
   * Handler for clicking the record button.
   */
  _onRecordButtonClick: function(e) {
    if (this._recordingControlsState.isRecording) {
      this.emit(EVENTS.UI_STOP_RECORDING);
    } else {
      this._lockRecordButtons(true);
      this._toggleRecordButtons(true);
      this.emit(EVENTS.UI_START_RECORDING);
    }
  },

  /**
   * Handler for clicking the import button.
   */
  _onImportButtonClick: function(e) {
    const fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(
      window,
      L10N.getStr("recordingsList.importDialogTitle"),
      Ci.nsIFilePicker.modeOpen
    );
    fp.appendFilter(
      L10N.getStr("recordingsList.saveDialogJSONFilter"),
      "*.json"
    );
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogAllFilter"), "*.*");

    fp.open(rv => {
      if (rv == Ci.nsIFilePicker.returnOK) {
        this.emit(EVENTS.UI_IMPORT_RECORDING, fp.file);
      }
    });
  },

  /**
   * Fired when a recording is selected. Used to toggle the profiler view state.
   */
  _onRecordingSelected: function(recording) {
    if (!recording) {
      this.setState("empty");
    } else if (recording.isRecording() && recording.isConsole()) {
      this.setState("console-recording");
    } else if (recording.isRecording()) {
      this.setState("recording");
    } else {
      this.setState("recorded");
    }
  },

  /**
   * Fired when the controller has updated information on the buffer's status.
   * Update the buffer status display if shown.
   */
  _onProfilerStatusUpdated: function(profilerStatus) {
    // We only care about buffer status here, so check to see
    // if it has position.
    if (!profilerStatus || profilerStatus.position === void 0) {
      return;
    }
    // If this is our first buffer event, set the status and add a class
    if (!this._bufferStatusSupported) {
      this._bufferStatusSupported = true;
      $("#details-pane-container").setAttribute("buffer-status", "in-progress");
    }

    if (!this.getState("recording") && !this.getState("console-recording")) {
      return;
    }

    this.updateBufferStatus();
  },

  toString: () => "[object PerformanceView]",
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(PerformanceView);

exports.PerformanceView = PerformanceView;

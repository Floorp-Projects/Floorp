/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Master view handler for the performance tool.
 */
let PerformanceView = {

  _state: null,

  // Mapping of state to selectors for different panes
  // of the main profiler view. Used in `PerformanceView.setState()`
  states: {
    empty: [
      { deck: "#performance-view", pane: "#empty-notice" }
    ],
    recording: [
      { deck: "#performance-view", pane: "#performance-view-content" },
      { deck: "#details-pane-container", pane: "#recording-notice" }
    ],
    "console-recording": [
      { deck: "#performance-view", pane: "#performance-view-content" },
      { deck: "#details-pane-container", pane: "#console-recording-notice" }
    ],
    recorded: [
      { deck: "#performance-view", pane: "#performance-view-content" },
      { deck: "#details-pane-container", pane: "#details-pane" }
    ]
  },

  /**
   * Sets up the view with event binding and main subviews.
   */
  initialize: Task.async(function* () {
    this._recordButton = $("#main-record-button");
    this._importButton = $("#import-button");
    this._clearButton = $("#clear-button");

    this._onRecordButtonClick = this._onRecordButtonClick.bind(this);
    this._onImportButtonClick = this._onImportButtonClick.bind(this);
    this._onClearButtonClick = this._onClearButtonClick.bind(this);
    this._lockRecordButtons = this._lockRecordButtons.bind(this);
    this._unlockRecordButtons = this._unlockRecordButtons.bind(this);
    this._onRecordingSelected = this._onRecordingSelected.bind(this);
    this._onRecordingStopped = this._onRecordingStopped.bind(this);
    this._onRecordingStarted = this._onRecordingStarted.bind(this);

    for (let button of $$(".record-button")) {
      button.addEventListener("click", this._onRecordButtonClick);
    }
    this._importButton.addEventListener("click", this._onImportButtonClick);
    this._clearButton.addEventListener("click", this._onClearButtonClick);

    // Bind to controller events to unlock the record button
    PerformanceController.on(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelected);

    this.setState("empty");

    // Initialize the ToolbarView first, because other views may need access
    // to the OptionsView via the controller, to read prefs.
    yield ToolbarView.initialize();
    yield RecordingsView.initialize();
    yield OverviewView.initialize();
    yield DetailsView.initialize();
  }),

  /**
   * Unbinds events and destroys subviews.
   */
  destroy: Task.async(function* () {
    for (let button of $$(".record-button")) {
      button.removeEventListener("click", this._onRecordButtonClick);
    }
    this._importButton.removeEventListener("click", this._onImportButtonClick);
    this._clearButton.removeEventListener("click", this._onClearButtonClick);

    PerformanceController.off(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingSelected);

    yield ToolbarView.destroy();
    yield RecordingsView.destroy();
    yield OverviewView.destroy();
    yield DetailsView.destroy();
  }),

  /**
   * Sets the state of the profiler view. Possible options are "empty",
   * "recording", "console-recording", "recorded".
   */
  setState: function (state) {
    let viewConfig = this.states[state];
    if (!viewConfig) {
      throw new Error(`Invalid state for PerformanceView: ${state}`);
    }
    for (let { deck, pane } of viewConfig) {
      $(deck).selectedPanel = $(pane);
    }

    this._state = state;

    if (state === "console-recording") {
      let recording = PerformanceController.getCurrentRecording();
      let label = recording.getLabel() || "";
      $(".console-profile-recording-notice").value = L10N.getFormatStr("consoleProfile.recordingNotice", label);
      $(".console-profile-stop-notice").value = L10N.getFormatStr("consoleProfile.stopCommand", label);
    }
    this.emit(EVENTS.UI_STATE_CHANGED, state);
  },

  /**
   * Returns the state of the PerformanceView.
   */
  getState: function () {
    return this._state;
  },

  /**
   * Adds the `locked` attribute on the record button. This prevents it
   * from being clicked while recording is started or stopped.
   */
  _lockRecordButtons: function () {
    for (let button of $$(".record-button")) {
      button.setAttribute("locked", "true");
    }
  },

  /**
   * Removes the `locked` attribute on the record button.
   */
  _unlockRecordButtons: function () {
    for (let button of $$(".record-button")) {
      button.removeAttribute("locked");
    }
  },

  /**
   * When a recording has started.
   */
  _onRecordingStarted: function (_, recording) {
    // A stopped recording can be from `console.profileEnd` -- only unlock
    // the button if it's the main recording that was started via UI.
    if (!recording.isConsole()) {
      this._unlockRecordButtons();
    }
  },

  /**
   * When a recording is complete.
   */
  _onRecordingStopped: function (_, recording) {
    // A stopped recording can be from `console.profileEnd` -- only unlock
    // the button if it's the main recording that was started via UI.
    if (!recording.isConsole()) {
      this._unlockRecordButtons();
    }

    // If the currently selected recording is the one that just stopped,
    // switch state to "recorded".
    if (recording === PerformanceController.getCurrentRecording()) {
      this.setState("recorded");
    }
  },

  /**
   * Handler for clicking the clear button.
   */
  _onClearButtonClick: function (e) {
    this.emit(EVENTS.UI_CLEAR_RECORDINGS);
  },

  /**
   * Handler for clicking the record button.
   */
  _onRecordButtonClick: function (e) {
    if (this._recordButton.hasAttribute("checked")) {
      this.emit(EVENTS.UI_STOP_RECORDING);
      this._lockRecordButtons();
      for (let button of $$(".record-button")) {
        button.removeAttribute("checked");
      }
    } else {
      this._lockRecordButtons();
      for (let button of $$(".record-button")) {
        button.setAttribute("checked", "true");
      }
      this.emit(EVENTS.UI_START_RECORDING);
    }
  },

  /**
   * Handler for clicking the import button.
   */
  _onImportButtonClick: function(e) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, L10N.getStr("recordingsList.saveDialogTitle"), Ci.nsIFilePicker.modeOpen);
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogJSONFilter"), "*.json");
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogAllFilter"), "*.*");

    if (fp.show() == Ci.nsIFilePicker.returnOK) {
      this.emit(EVENTS.UI_IMPORT_RECORDING, fp.file);
    }
  },

  /**
   * Fired when a recording is selected. Used to toggle the profiler view state.
   */
  _onRecordingSelected: function (_, recording) {
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

  toString: () => "[object PerformanceView]"
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(PerformanceView);

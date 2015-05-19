/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Master view handler for the performance tool.
 */
let PerformanceView = {

  _state: null,
  // Set to true if the front emits a "buffer-status" event, indicating
  // that the server has support for determining buffer status.
  _bufferStatusSupported: false,

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
    ],
    loading: [
      { deck: "#performance-view", pane: "#performance-view-content" },
      { deck: "#details-pane-container", pane: "#loading-notice" }
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
    this._onProfilerStatusUpdated = this._onProfilerStatusUpdated.bind(this);
    this._onRecordingWillStop = this._onRecordingWillStop.bind(this);

    for (let button of $$(".record-button")) {
      button.addEventListener("click", this._onRecordButtonClick);
    }
    this._importButton.addEventListener("click", this._onImportButtonClick);
    this._clearButton.addEventListener("click", this._onClearButtonClick);

    // Bind to controller events to unlock the record button
    PerformanceController.on(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingSelected);
    PerformanceController.on(EVENTS.PROFILER_STATUS_UPDATED, this._onProfilerStatusUpdated);
    PerformanceController.on(EVENTS.RECORDING_WILL_STOP, this._onRecordingWillStop);

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
    PerformanceController.off(EVENTS.PROFILER_STATUS_UPDATED, this._onProfilerStatusUpdated);
    PerformanceController.off(EVENTS.RECORDING_WILL_STOP, this._onRecordingWillStop);

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
      // Wrap the label in quotes if it exists for the commands.
      label = label ? `"${label}"` : "";

      let startCommand = $(".console-profile-recording-notice .console-profile-command");
      let stopCommand = $(".console-profile-stop-notice .console-profile-command");

      startCommand.value = `console.profile(${label})`;
      stopCommand.value = `console.profileEnd(${label})`;
    }

    this.updateBufferStatus();
    this.emit(EVENTS.UI_STATE_CHANGED, state);
  },

  /**
   * Returns the state of the PerformanceView.
   */
  getState: function () {
    return this._state;
  },

  /**
   * Updates the displayed buffer status.
   */
  updateBufferStatus: function () {
    // If we've never seen a "buffer-status" event from the front, ignore
    // and keep the buffer elements hidden.
    if (!this._bufferStatusSupported) {
      return;
    }

    let recording = PerformanceController.getCurrentRecording();
    if (!recording || !recording.isRecording()) {
      return;
    }

    let bufferUsage = recording.getBufferUsage();

    // Normalize to a percentage value
    let percent = Math.floor(bufferUsage * 100);

    let $container = $("#details-pane-container");
    let $bufferLabel = $(".buffer-status-message", $container.selectedPanel);

    // Be a little flexible on the buffer status, although not sure how
    // this could happen, as RecordingModel clamps.
    if (percent >= 99) {
      $container.setAttribute("buffer-status", "full");
    } else {
      $container.setAttribute("buffer-status", "in-progress");
    }

    $bufferLabel.value = `Buffer ${percent}% full`;
    this.emit(EVENTS.UI_BUFFER_UPDATED, percent);
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
    if (recording.isRecording()) {
      this.updateBufferStatus();
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
   * Fired when a recording is stopping, but not yet completed
   */
  _onRecordingWillStop: function (_, recording) {
    // Lock the details view while the recording is being loaded in the UI.
    // Only do this if this is the current recording.
    if (recording === PerformanceController.getCurrentRecording()) {
      this.setState("loading");
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
    // TODO localize? in bug 1163763
    fp.init(window, "Import recording…", Ci.nsIFilePicker.modeOpen);
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

  /**
   * Fired when the controller has updated information on the buffer's status.
   * Update the buffer status display if shown.
   */
  _onProfilerStatusUpdated: function (_, data) {
    // We only care about buffer status here, so check to see
    // if it has position.
    if (!data || data.position === void 0) {
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

  toString: () => "[object PerformanceView]"
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(PerformanceView);

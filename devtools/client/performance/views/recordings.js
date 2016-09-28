/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../performance-controller.js */
/* import-globals-from ../performance-view.js */
/* globals document, window */
"use strict";

/**
 * Functions handling the recordings UI.
 */
var RecordingsView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function () {
    this._onSelect = this._onSelect.bind(this);
    this._onRecordingStateChange = this._onRecordingStateChange.bind(this);
    this._onNewRecording = this._onNewRecording.bind(this);
    this._onSaveButtonClick = this._onSaveButtonClick.bind(this);
    this._onRecordingDeleted = this._onRecordingDeleted.bind(this);
    this._onRecordingExported = this._onRecordingExported.bind(this);

    PerformanceController.on(EVENTS.RECORDING_STATE_CHANGE, this._onRecordingStateChange);
    PerformanceController.on(EVENTS.RECORDING_ADDED, this._onNewRecording);
    PerformanceController.on(EVENTS.RECORDING_DELETED, this._onRecordingDeleted);
    PerformanceController.on(EVENTS.RECORDING_EXPORTED, this._onRecordingExported);

    // DE-XUL: Begin migrating the recording sidebar to React. Temporarily hold state
    // here.
    this._listState = {
      recordings: [],
      labels: new WeakMap(),
      selected: null,
    };
    this._listMount = PerformanceUtils.createHtmlMount($("#recording-list-mount"));
    this._renderList();
  },

  /**
   * Get the index of the currently selected recording. Only used by tests.
   * @return {integer} index
   */
  getSelectedIndex() {
    const { recordings, selected } = this._listState;
    return recordings.indexOf(selected);
  },

  /**
   * Set the currently selected recording via its index. Only used by tests.
   * @param {integer} index
   */
  setSelectedByIndex(index) {
    this._onSelect(this._listState.recordings[index]);
    this._renderList();
  },

  /**
   * DE-XUL: During the migration, this getter will access the selected recording from
   * the private _listState object so that tests will continue to pass.
   */
  get selected() {
    return this._listState.selected;
  },

  /**
   * DE-XUL: During the migration, this getter will access the number of recordings.
   */
  get itemCount() {
    return this._listState.recordings.length;
  },

  /**
   * DE-XUL: Render the recording list using React.
   */
  _renderList: function () {
    const {recordings, labels, selected} = this._listState;

    const recordingList = RecordingList({
      itemComponent: RecordingListItem,
      items: recordings.map(recording => ({
        onSelect: () => this._onSelect(recording),
        onSave: () => this._onSaveButtonClick(recording),
        isLoading: !recording.isRecording() && !recording.isCompleted(),
        isRecording: recording.isRecording(),
        isSelected: recording === selected,
        duration: recording.getDuration().toFixed(0),
        label: labels.get(recording),
      }))
    });

    ReactDOM.render(recordingList, this._listMount);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function () {
    PerformanceController.off(EVENTS.RECORDING_STATE_CHANGE,
                              this._onRecordingStateChange);
    PerformanceController.off(EVENTS.RECORDING_ADDED, this._onNewRecording);
    PerformanceController.off(EVENTS.RECORDING_DELETED, this._onRecordingDeleted);
    PerformanceController.off(EVENTS.RECORDING_EXPORTED, this._onRecordingExported);
  },

  /**
   * Called when a new recording is stored in the UI. This handles
   * when recordings are lazily loaded (like a console.profile occurring
   * before the tool is loaded) or imported. In normal manual recording cases,
   * this will also be fired.
   */
  _onNewRecording: function (_, recording) {
    this._onRecordingStateChange(_, null, recording);
  },

  /**
   * Signals that a recording has changed state.
   *
   * @param string state
   *        Can be "recording-started", "recording-stopped", "recording-stopping"
   * @param RecordingModel recording
   *        Model of the recording that was started.
   */
  _onRecordingStateChange: function (_, state, recording) {
    const { recordings, labels } = this._listState;

    if (!recordings.includes(recording)) {
      recordings.push(recording);
      labels.set(recording, recording.getLabel() ||
        L10N.getFormatStr("recordingsList.itemLabel", recordings.length));

      // If this is a manual recording, immediately select it, or
      // select a console profile if its the only one
      if (!recording.isConsole() || !this._listState.selected) {
        this._onSelect(recording);
      }
    }

    // Determine if the recording needs to be selected.
    const isCompletedManualRecording = !recording.isConsole() && recording.isCompleted();
    if (recording.isImported() || isCompletedManualRecording) {
      this._onSelect(recording);
    }

    this._renderList();
  },

  /**
   * Clears out all non-console recordings.
   */
  _onRecordingDeleted: function (_, recording) {
    const { recordings } = this._listState;
    const index = recordings.indexOf(recording);
    if (index === -1) {
      throw new Error("Attempting to remove a recording that doesn't exist.");
    }
    recordings.splice(index, 1);
    this._renderList();
  },

  /**
   * The select listener for this container.
   */
  _onSelect: Task.async(function* (recording) {
    this._listState.selected = recording;
    this.emit(EVENTS.UI_RECORDING_SELECTED, recording);
    this._renderList();
  }),

  /**
   * The click listener for the "save" button of each item in this container.
   */
  _onSaveButtonClick: function (recording) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, L10N.getStr("recordingsList.saveDialogTitle"),
            Ci.nsIFilePicker.modeSave);
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogJSONFilter"), "*.json");
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogAllFilter"), "*.*");
    fp.defaultString = "profile.json";

    fp.open({ done: result => {
      if (result == Ci.nsIFilePicker.returnCancel) {
        return;
      }
      this.emit(EVENTS.UI_EXPORT_RECORDING, recording, fp.file);
    }});
  },

  _onRecordingExported: function (_, recording, file) {
    if (recording.isConsole()) {
      return;
    }
    const name = file.leafName.replace(/\..+$/, "");
    this._listState.labels.set(recording, name);
    this._renderList();
  }
};

/**
 * Convenient way of emitting events from the RecordingsView.
 */
EventEmitter.decorate(RecordingsView);

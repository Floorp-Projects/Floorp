/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the recordings UI.
 */
let RecordingsView = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this.widget = new SideMenuWidget($("#recordings-list"));

    this._onSelect = this._onSelect.bind(this);
    this._onRecordingStarted = this._onRecordingStarted.bind(this);
    this._onRecordingStopped = this._onRecordingStopped.bind(this);
    this._onRecordingImported = this._onRecordingImported.bind(this);
    this._onSaveButtonClick = this._onSaveButtonClick.bind(this);
    this._onRecordingsCleared = this._onRecordingsCleared.bind(this);

    this.emptyText = L10N.getStr("noRecordingsText");

    PerformanceController.on(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.on(EVENTS.RECORDING_IMPORTED, this._onRecordingImported);
    PerformanceController.on(EVENTS.RECORDINGS_CLEARED, this._onRecordingsCleared);
    this.widget.addEventListener("select", this._onSelect, false);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    PerformanceController.off(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStopped);
    PerformanceController.off(EVENTS.RECORDING_IMPORTED, this._onRecordingImported);
    PerformanceController.off(EVENTS.RECORDINGS_CLEARED, this._onRecordingsCleared);
    this.widget.removeEventListener("select", this._onSelect, false);
  },

  /**
   * Adds an empty recording to this container.
   *
   * @param RecordingModel recording
   *        A model for the new recording item created.
   */
  addEmptyRecording: function (recording) {
    let titleNode = document.createElement("label");
    titleNode.className = "plain recording-item-title";
    titleNode.setAttribute("value", recording.getLabel() ||
      L10N.getFormatStr("recordingsList.itemLabel", this.itemCount + 1));

    let durationNode = document.createElement("label");
    durationNode.className = "plain recording-item-duration";
    durationNode.setAttribute("value",
      L10N.getStr("recordingsList.recordingLabel"));

    let saveNode = document.createElement("label");
    saveNode.className = "plain recording-item-save";
    saveNode.addEventListener("click", this._onSaveButtonClick);

    let hspacer = document.createElement("spacer");
    hspacer.setAttribute("flex", "1");

    let footerNode = document.createElement("hbox");
    footerNode.className = "recording-item-footer";
    footerNode.appendChild(durationNode);
    footerNode.appendChild(hspacer);
    footerNode.appendChild(saveNode);

    let vspacer = document.createElement("spacer");
    vspacer.setAttribute("flex", "1");

    let contentsNode = document.createElement("vbox");
    contentsNode.className = "recording-item";
    contentsNode.setAttribute("flex", "1");
    contentsNode.appendChild(titleNode);
    contentsNode.appendChild(vspacer);
    contentsNode.appendChild(footerNode);

    // Append a recording item to this container.
    return this.push([contentsNode], {
      // Store the recording model that contains all the data to be
      // rendered in the item.
      attachment: recording
    });
  },

  /**
   * Signals that a recording session has started.
   *
   * @param RecordingModel recording
   *        Model of the recording that was started.
   */
  _onRecordingStarted: function (_, recording) {
    // Insert a "dummy" recording item, to hint that recording has now started.
    let recordingItem;

    // If a label is specified (e.g due to a call to `console.profile`),
    // then try reusing a pre-existing recording item, if there is one.
    // This is symmetrical to how `this.handleRecordingEnded` works.
    let profileLabel = recording.getLabel();
    if (profileLabel) {
      recordingItem = this.getItemForAttachment(e => e.getLabel() == profileLabel);
    }
    // Otherwise, create a new empty recording item.
    if (!recordingItem) {
      recordingItem = this.addEmptyRecording(recording);
    }

    // Mark the corresponding item as being a "record in progress".
    recordingItem.isRecording = true;

    // If this is a manual recording, immediately select it.
    if (!recording.getLabel()) {
      this.selectedItem = recordingItem;
    }
  },

  /**
   * Signals that a recording session has ended.
   *
   * @param RecordingModel recording
   *        The model of the recording that just stopped.
   */
  _onRecordingStopped: function (_, recording) {
    let recordingItem;

    // If a label is specified (e.g due to a call to `console.profileEnd`),
    // then try reusing a pre-existing recording item, if there is one.
    // This is symmetrical to how `this.handleRecordingStarted` works.
    let profileLabel = recording.getLabel();
    if (profileLabel) {
      recordingItem = this.getItemForAttachment(e => e.getLabel() == profileLabel);
    }
    // Otherwise, just use the first available recording item.
    if (!recordingItem) {
      recordingItem = this.getItemForPredicate(e => e.isRecording);
    }

    // Mark the corresponding item as being a "finished recording".
    recordingItem.isRecording = false;

    // Render the recording item with finalized information (timing, etc)
    this.finalizeRecording(recordingItem);
    this.forceSelect(recordingItem);
  },

  /**
   * Signals that a recording has been imported.
   *
   * @param RecordingModel model
   *        The recording model containing data on the recording session.
   */
  _onRecordingImported: function (_, model) {
    let recordingItem = this.addEmptyRecording(model);
    recordingItem.isRecording = false;

    // Immediately select the imported recording
    this.selectedItem = recordingItem;

    // Render the recording item with finalized information (timing, etc)
    this.finalizeRecording(recordingItem);
  },

  /**
   * Clears out all recordings.
   */
  _onRecordingsCleared: function () {
    this.empty();
  },

  /**
   * Adds recording data to a recording item in this container.
   *
   * @param Item recordingItem
   *        An item inserted via `RecordingsView.addEmptyRecording`.
   */
  finalizeRecording: function (recordingItem) {
    let model = recordingItem.attachment;

    let saveNode = $(".recording-item-save", recordingItem.target);
    saveNode.setAttribute("value",
      L10N.getStr("recordingsList.saveLabel"));

    let durationMillis = model.getDuration().toFixed(0);
    let durationNode = $(".recording-item-duration", recordingItem.target);
    durationNode.setAttribute("value",
      L10N.getFormatStr("recordingsList.durationLabel", durationMillis));
  },

  /**
   * The select listener for this container.
   */
  _onSelect: Task.async(function*({ detail: recordingItem }) {
    // TODO 1120699
    // show appropriate empty/recording panels for several scenarios below
    if (!recordingItem) {
      return;
    }

    let model = recordingItem.attachment;

    // If recording, don't abort completely, as we still want to fire an event
    // for selection so we can continue repainting the overview graphs.
    if (recordingItem.isRecording) {
      this.emit(EVENTS.RECORDING_SELECTED, model);
      return;
    }

    this.emit(EVENTS.RECORDING_SELECTED, model);
  }),

  /**
   * The click listener for the "save" button of each item in this container.
   */
  _onSaveButtonClick: function (e) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, L10N.getStr("recordingsList.saveDialogTitle"), Ci.nsIFilePicker.modeSave);
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogJSONFilter"), "*.json");
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogAllFilter"), "*.*");
    fp.defaultString = "profile.json";

    fp.open({ done: result => {
      if (result == Ci.nsIFilePicker.returnCancel) {
        return;
      }
      let recordingItem = this.getItemForElement(e.target);
      this.emit(EVENTS.UI_EXPORT_RECORDING, recordingItem.attachment, fp.file);
    }});
  },

  toString: () => "[object RecordingsView]"
});

/**
 * Convenient way of emitting events from the RecordingsView.
 */
EventEmitter.decorate(RecordingsView);

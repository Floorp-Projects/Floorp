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
var RecordingsView = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function () {
    this.widget = new SideMenuWidget($("#recordings-list"));

    this._onSelect = this._onSelect.bind(this);
    this._onRecordingStateChange = this._onRecordingStateChange.bind(this);
    this._onNewRecording = this._onNewRecording.bind(this);
    this._onSaveButtonClick = this._onSaveButtonClick.bind(this);
    this._onRecordingDeleted = this._onRecordingDeleted.bind(this);
    this._onRecordingExported = this._onRecordingExported.bind(this);

    this.emptyText = L10N.getStr("noRecordingsText");

    PerformanceController.on(EVENTS.RECORDING_STATE_CHANGE, this._onRecordingStateChange);
    PerformanceController.on(EVENTS.RECORDING_ADDED, this._onNewRecording);
    PerformanceController.on(EVENTS.RECORDING_DELETED, this._onRecordingDeleted);
    PerformanceController.on(EVENTS.RECORDING_EXPORTED, this._onRecordingExported);
    this.widget.addEventListener("select", this._onSelect, false);
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
    titleNode.setAttribute("crop", "end");
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
    let recordingItem = this.getItemForPredicate(e => e.attachment === recording);
    if (!recordingItem) {
      recordingItem = this.addEmptyRecording(recording);

      // If this is a manual recording, immediately select it, or
      // select a console profile if its the only one
      if (!recording.isConsole() || this.selectedIndex === -1) {
        this.selectedItem = recordingItem;
      }
    }

    recordingItem.isRecording = recording.isRecording();

    // This recording is in the process of stopping.
    if (!recording.isRecording() && !recording.isCompleted()) {
      // Mark the corresponding item as loading.
      let durationNode = $(".recording-item-duration", recordingItem.target);
      durationNode.setAttribute("value", L10N.getStr("recordingsList.loadingLabel"));
    }

    // Render the recording item with finalized information (timing, etc)
    if (recording.isCompleted() && !recordingItem.finalized) {
      this.finalizeRecording(recordingItem);
      // Select the recording if it was a manual recording only
      if (!recording.isConsole()) {
        this.forceSelect(recordingItem);
      }
    }

    // Auto select imported items.
    if (recording.isImported()) {
      this.selectedItem = recordingItem;
    }
  },

  /**
   * Clears out all non-console recordings.
   */
  _onRecordingDeleted: function (_, recording) {
    let recordingItem = this.getItemForPredicate(e => e.attachment === recording);
    this.remove(recordingItem);
  },

  /**
   * Adds recording data to a recording item in this container.
   *
   * @param Item recordingItem
   *        An item inserted via `RecordingsView.addEmptyRecording`.
   */
  finalizeRecording: function (recordingItem) {
    let model = recordingItem.attachment;
    recordingItem.finalized = true;

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
  _onSelect: Task.async(function* ({ detail: recordingItem }) {
    if (!recordingItem) {
      return;
    }

    let model = recordingItem.attachment;
    this.emit(EVENTS.UI_RECORDING_SELECTED, model);
  }),

  /**
   * The click listener for the "save" button of each item in this container.
   */
  _onSaveButtonClick: function (e) {
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
      let recordingItem = this.getItemForElement(e.target);
      this.emit(EVENTS.UI_EXPORT_RECORDING, recordingItem.attachment, fp.file);
    }});
  },

  _onRecordingExported: function (_, recording, file) {
    if (recording.isConsole()) {
      return;
    }
    let recordingItem = this.getItemForPredicate(e => e.attachment === recording);
    let titleNode = $(".recording-item-title", recordingItem.target);
    titleNode.setAttribute("value", file.leafName.replace(/\..+$/, ""));
  },

  toString: () => "[object RecordingsView]"
});

/**
 * Convenient way of emitting events from the RecordingsView.
 */
EventEmitter.decorate(RecordingsView);

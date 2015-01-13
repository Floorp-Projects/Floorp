/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Master view handler for the performance tool.
 */
let PerformanceView = {
  /**
   * Sets up the view with event binding and main subviews.
   */
  initialize: function () {
    this._recordButton = $("#record-button");
    this._importButton = $("#import-button");

    this._onRecordButtonClick = this._onRecordButtonClick.bind(this);
    this._onImportButtonClick = this._onImportButtonClick.bind(this);
    this._lockRecordButton = this._lockRecordButton.bind(this);
    this._unlockRecordButton = this._unlockRecordButton.bind(this);

    this._recordButton.addEventListener("click", this._onRecordButtonClick);
    this._importButton.addEventListener("click", this._onImportButtonClick);

    // Bind to controller events to unlock the record button
    PerformanceController.on(EVENTS.RECORDING_STARTED, this._unlockRecordButton);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._unlockRecordButton);

    return promise.all([
      RecordingsView.initialize(),
      OverviewView.initialize(),
      DetailsView.initialize()
    ]);
  },

  /**
   * Unbinds events and destroys subviews.
   */
  destroy: function () {
    this._recordButton.removeEventListener("click", this._onRecordButtonClick);
    this._importButton.removeEventListener("click", this._onImportButtonClick);

    PerformanceController.off(EVENTS.RECORDING_STARTED, this._unlockRecordButton);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._unlockRecordButton);

    return promise.all([
      RecordingsView.destroy(),
      OverviewView.destroy(),
      DetailsView.destroy()
    ]);
  },

  /**
   * Adds the `locked` attribute on the record button. This prevents it
   * from being clicked while recording is started or stopped.
   */
  _lockRecordButton: function () {
    this._recordButton.setAttribute("locked", "true");
  },

  /**
   * Removes the `locked` attribute on the record button.
   */
  _unlockRecordButton: function () {
    this._recordButton.removeAttribute("locked");
  },

  /**
   * Handler for clicking the record button.
   */
  _onRecordButtonClick: function (e) {
    if (this._recordButton.hasAttribute("checked")) {
      this._recordButton.removeAttribute("checked");
      this._lockRecordButton();
      this.emit(EVENTS.UI_STOP_RECORDING);
    } else {
      this._recordButton.setAttribute("checked", "true");
      this._lockRecordButton();
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
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(PerformanceView);

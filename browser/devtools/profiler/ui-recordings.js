/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the recordings UI.
 */
let RecordingsListView = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this.widget = new SideMenuWidget($("#recordings-list"));

    this._onSelect = this._onSelect.bind(this);
    this._onClearButtonClick = this._onClearButtonClick.bind(this);
    this._onRecordButtonClick = this._onRecordButtonClick.bind(this);
    this._onImportButtonClick = this._onImportButtonClick.bind(this);
    this._onSaveButtonClick = this._onSaveButtonClick.bind(this);

    this.emptyText = L10N.getStr("noRecordingsText");
    this.widget.addEventListener("select", this._onSelect, false);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this.widget.removeEventListener("select", this._onSelect, false);
  },

  /**
   * Adds an empty recording to this container.
   *
   * @param string profileLabel [optional]
   *        A custom label for the newly created recording item.
   */
  addEmptyRecording: function(profileLabel) {
    let titleNode = document.createElement("label");
    titleNode.className = "plain recording-item-title";
    titleNode.setAttribute("value", profileLabel ||
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
      attachment: {
        // The profiler and refresh driver ticks data will be available
        // as soon as recording finishes.
        profilerData: { profileLabel },
        ticksData: null
      }
    });
  },

  /**
   * Signals that a recording session has started.
   *
   * @param string profileLabel
   *        The provided string argument if available, undefined otherwise.
   */
  handleRecordingStarted: function(profileLabel) {
    // Insert a "dummy" recording item, to hint that recording has now started.
    let recordingItem;

    // If a label is specified (e.g due to a call to `console.profile`),
    // then try reusing a pre-existing recording item, if there is one.
    // This is symmetrical to how `this.handleRecordingEnded` works.
    if (profileLabel) {
      recordingItem = this.getItemForAttachment(e =>
        e.profilerData.profileLabel == profileLabel);
    }
    // Otherwise, create a new empty recording item.
    if (!recordingItem) {
      recordingItem = this.addEmptyRecording(profileLabel);
    }

    // Mark the corresponding item as being a "record in progress".
    recordingItem.isRecording = true;

    // If this is the first item, immediately select it.
    if (this.itemCount == 1) {
      this.selectedItem = recordingItem;
    }

    window.emit(EVENTS.RECORDING_STARTED, profileLabel);
  },

  /**
   * Signals that a recording session has ended.
   *
   * @param object recordingData
   *        The profiler and refresh driver ticks data received from the front.
   */
  handleRecordingEnded: function(recordingData) {
    let profileLabel = recordingData.profilerData.profileLabel;
    let recordingItem;

    // If a label is specified (e.g due to a call to `console.profileEnd`),
    // then try reusing a pre-existing recording item, if there is one.
    // This is symmetrical to how `this.handleRecordingStarted` works.
    if (profileLabel) {
      recordingItem = this.getItemForAttachment(e =>
        e.profilerData.profileLabel == profileLabel);
    }
    // Otherwise, just use the first available recording item.
    if (!recordingItem) {
      recordingItem = this.getItemForPredicate(e => e.isRecording);
    }

    // Mark the corresponding item as being a "finished recording".
    recordingItem.isRecording = false;

    // Store the recording data, customize and select this recording item.
    this.customizeRecording(recordingItem, recordingData);
    this.forceSelect(recordingItem);

    window.emit(EVENTS.RECORDING_ENDED, recordingData);
  },

  /**
   * Signals that a recording session has ended abruptly and the accumulated
   * data should be discarded.
   */
  handleRecordingCancelled: Task.async(function*() {
    if ($("#record-button").hasAttribute("checked")) {
      $("#record-button").removeAttribute("checked");
      yield gFront.cancelRecording();
    }
    ProfileView.showEmptyNotice();

    window.emit(EVENTS.RECORDING_LOST);
  }),

  /**
   * Adds recording data to a recording item in this container.
   *
   * @param Item recordingItem
   *        An item inserted via `RecordingsListView.addEmptyRecording`.
   * @param object recordingData
   *        The profiler and refresh driver ticks data received from the front.
   */
  customizeRecording: function(recordingItem, recordingData) {
    recordingItem.attachment = recordingData;

    let saveNode = $(".recording-item-save", recordingItem.target);
    saveNode.setAttribute("value",
      L10N.getStr("recordingsList.saveLabel"));

    let durationMillis = recordingData.recordingDuration;
    let durationNode = $(".recording-item-duration", recordingItem.target);
    durationNode.setAttribute("value",
      L10N.getFormatStr("recordingsList.durationLabel", durationMillis));
  },

  /**
   * The select listener for this container.
   */
  _onSelect: Task.async(function*({ detail: recordingItem }) {
    if (!recordingItem) {
      ProfileView.showEmptyNotice();
      return;
    }
    if (recordingItem.isRecording) {
      ProfileView.showRecordingNotice();
      return;
    }

    ProfileView.showLoadingNotice();
    ProfileView.removeAllTabs();

    let recordingData = recordingItem.attachment;
    let durationMillis = recordingData.recordingDuration;
    yield ProfileView.addTabAndPopulate(recordingData, 0, durationMillis);
    ProfileView.showTabbedBrowser();

    $("#record-button").removeAttribute("checked");
    $("#record-button").removeAttribute("locked");

    window.emit(EVENTS.RECORDING_DISPLAYED);
  }),

  /**
   * The click listener for the "clear" button in this container.
   */
  _onClearButtonClick: Task.async(function*() {
    this.empty();
    yield this.handleRecordingCancelled();
  }),

  /**
   * The click listener for the "record" button in this container.
   */
  _onRecordButtonClick: Task.async(function*() {
    if (!$("#record-button").hasAttribute("checked")) {
      $("#record-button").setAttribute("checked", "true");
      yield gFront.startRecording();
      this.handleRecordingStarted();
    } else {
      $("#record-button").setAttribute("locked", "");
      let recordingData = yield gFront.stopRecording();
      this.handleRecordingEnded(recordingData);
    }
  }),

  /**
   * The click listener for the "import" button in this container.
   */
  _onImportButtonClick: Task.async(function*() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, L10N.getStr("recordingsList.saveDialogTitle"), Ci.nsIFilePicker.modeOpen);
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogJSONFilter"), "*.json");
    fp.appendFilter(L10N.getStr("recordingsList.saveDialogAllFilter"), "*.*");

    if (fp.show() == Ci.nsIFilePicker.returnOK) {
      loadRecordingFromFile(fp.file);
    }
  }),

  /**
   * The click listener for the "save" button of each item in this container.
   */
  _onSaveButtonClick: function(e) {
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
      saveRecordingToFile(recordingItem, fp.file);
    }});
  }
});

/**
 * Gets a nsIScriptableUnicodeConverter instance with a default UTF-8 charset.
 * @return object
 */
function getUnicodeConverter() {
  let className = "@mozilla.org/intl/scriptableunicodeconverter";
  let converter = Cc[className].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
}

/**
 * Saves a recording as JSON to a file. The provided data is assumed to be
 * acyclical, so that it can be properly serialized.
 *
 * @param Item recordingItem
 *        The recording item containing the data to stream as JSON.
 * @param nsILocalFile file
 *        The file to stream the data into.
 * @return object
 *         A promise that is resolved once streaming finishes, or rejected
 *         if there was an error.
 */
function saveRecordingToFile(recordingItem, file) {
  let deferred = promise.defer();

  let recordingData = recordingItem.attachment;
  recordingData.fileType = PROFILE_SERIALIZER_IDENTIFIER;
  recordingData.version = PROFILE_SERIALIZER_VERSION;

  let string = JSON.stringify(recordingData);
  let inputStream = getUnicodeConverter().convertToInputStream(string);
  let outputStream = FileUtils.openSafeFileOutputStream(file);

  NetUtil.asyncCopy(inputStream, outputStream, status => {
    if (!Components.isSuccessCode(status)) {
      deferred.reject(new Error("Could not save recording data file."));
    }
    deferred.resolve();
  });

  return deferred.promise;
}

/**
 * Loads a recording stored as JSON from a file.
 *
 * @param nsILocalFile file
 *        The file to import the data from.
 * @return object
 *         A promise that is resolved once importing finishes, or rejected
 *         if there was an error.
 */
function loadRecordingFromFile(file) {
  let deferred = promise.defer();

  let channel = NetUtil.newChannel(file);
  channel.contentType = "text/plain";

  NetUtil.asyncFetch(channel, (inputStream, status) => {
    if (!Components.isSuccessCode(status)) {
      deferred.reject(new Error("Could not import recording data file."));
      return;
    }
    try {
      let string = NetUtil.readInputStreamToString(inputStream, inputStream.available());
      var recordingData = JSON.parse(string);
    } catch (e) {
      deferred.reject(new Error("Could not read recording data file."));
      return;
    }
    if (recordingData.fileType != PROFILE_SERIALIZER_IDENTIFIER) {
      deferred.reject(new Error("Unrecognized recording data file."));
      return;
    }

    let profileLabel = recordingData.profilerData.profileLabel;
    let recordingItem = RecordingsListView.addEmptyRecording(profileLabel);
    RecordingsListView.customizeRecording(recordingItem, recordingData);

    // If this is the first item, immediately select it.
    if (RecordingsListView.itemCount == 1) {
      RecordingsListView.selectedItem = recordingItem;
    }

    deferred.resolve();
  });

  return deferred.promise;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the recorded animation frame snapshots UI.
 */
var SnapshotsListView = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this.widget = new SideMenuWidget($("#snapshots-list"), {
      showArrows: true
    });

    this._onSelect = this._onSelect.bind(this);
    this._onClearButtonClick = this._onClearButtonClick.bind(this);
    this._onRecordButtonClick = this._onRecordButtonClick.bind(this);
    this._onImportButtonClick = this._onImportButtonClick.bind(this);
    this._onSaveButtonClick = this._onSaveButtonClick.bind(this);
    this._onRecordSuccess = this._onRecordSuccess.bind(this);
    this._onRecordFailure = this._onRecordFailure.bind(this);
    this._stopRecordingAnimation = this._stopRecordingAnimation.bind(this);

    window.on(EVENTS.SNAPSHOT_RECORDING_FINISHED, this._enableRecordButton);
    this.emptyText = L10N.getStr("noSnapshotsText");
    this.widget.addEventListener("select", this._onSelect, false);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    clearNamedTimeout("canvas-actor-recording");
    window.off(EVENTS.SNAPSHOT_RECORDING_FINISHED, this._enableRecordButton);
    this.widget.removeEventListener("select", this._onSelect, false);
  },

  /**
   * Adds a snapshot entry to this container.
   *
   * @return object
   *         The newly inserted item.
   */
  addSnapshot: function() {
    let contents = document.createElement("hbox");
    contents.className = "snapshot-item";

    let thumbnail = document.createElementNS(HTML_NS, "canvas");
    thumbnail.className = "snapshot-item-thumbnail";
    thumbnail.width = CanvasFront.THUMBNAIL_SIZE;
    thumbnail.height = CanvasFront.THUMBNAIL_SIZE;

    let title = document.createElement("label");
    title.className = "plain snapshot-item-title";
    title.setAttribute("value",
      L10N.getFormatStr("snapshotsList.itemLabel", this.itemCount + 1));

    let calls = document.createElement("label");
    calls.className = "plain snapshot-item-calls";
    calls.setAttribute("value",
      L10N.getStr("snapshotsList.loadingLabel"));

    let save = document.createElement("label");
    save.className = "plain snapshot-item-save";
    save.addEventListener("click", this._onSaveButtonClick, false);

    let spacer = document.createElement("spacer");
    spacer.setAttribute("flex", "1");

    let footer = document.createElement("hbox");
    footer.className = "snapshot-item-footer";
    footer.appendChild(save);

    let details = document.createElement("vbox");
    details.className = "snapshot-item-details";
    details.appendChild(title);
    details.appendChild(calls);
    details.appendChild(spacer);
    details.appendChild(footer);

    contents.appendChild(thumbnail);
    contents.appendChild(details);

    // Append a recorded snapshot item to this container.
    return this.push([contents], {
      attachment: {
        // The snapshot and function call actors, along with the thumbnails
        // will be available as soon as recording finishes.
        actor: null,
        calls: null,
        thumbnails: null,
        screenshot: null
      }
    });
  },

  /**
   * Removes the last snapshot added, in the event no requestAnimationFrame loop was found.
   */
  removeLastSnapshot: function () {
    this.removeAt(this.itemCount - 1);
    // If this is the only item, revert back to the empty notice
    if (this.itemCount === 0) {
      $("#empty-notice").hidden = false;
      $("#waiting-notice").hidden = true;
    }
  },

  /**
   * Customizes a shapshot in this container.
   *
   * @param Item snapshotItem
   *        An item inserted via `SnapshotsListView.addSnapshot`.
   * @param object snapshotActor
   *        The frame snapshot actor received from the backend.
   * @param object snapshotOverview
   *        Additional data about the snapshot received from the backend.
   */
  customizeSnapshot: function(snapshotItem, snapshotActor, snapshotOverview) {
    // Make sure the function call actors are stored on the item,
    // to be used when populating the CallsListView.
    snapshotItem.attachment.actor = snapshotActor;
    let functionCalls = snapshotItem.attachment.calls = snapshotOverview.calls;
    let thumbnails = snapshotItem.attachment.thumbnails = snapshotOverview.thumbnails;
    let screenshot = snapshotItem.attachment.screenshot = snapshotOverview.screenshot;

    let lastThumbnail = thumbnails[thumbnails.length - 1];
    let { width, height, flipped, pixels } = lastThumbnail;

    let thumbnailNode = $(".snapshot-item-thumbnail", snapshotItem.target);
    thumbnailNode.setAttribute("flipped", flipped);
    drawImage(thumbnailNode, width, height, pixels, { centered: true });

    let callsNode = $(".snapshot-item-calls", snapshotItem.target);
    let drawCalls = functionCalls.filter(e => CanvasFront.DRAW_CALLS.has(e.name));

    let drawCallsStr = PluralForm.get(drawCalls.length,
      L10N.getStr("snapshotsList.drawCallsLabel"));
    let funcCallsStr = PluralForm.get(functionCalls.length,
      L10N.getStr("snapshotsList.functionCallsLabel"));

    callsNode.setAttribute("value",
      drawCallsStr.replace("#1", drawCalls.length) + ", " +
      funcCallsStr.replace("#1", functionCalls.length));

    let saveNode = $(".snapshot-item-save", snapshotItem.target);
    saveNode.setAttribute("disabled", !!snapshotItem.isLoadedFromDisk);
    saveNode.setAttribute("value", snapshotItem.isLoadedFromDisk
      ? L10N.getStr("snapshotsList.loadedLabel")
      : L10N.getStr("snapshotsList.saveLabel"));

    // Make sure there's always a selected item available.
    if (!this.selectedItem) {
      this.selectedIndex = 0;
    }
  },

  /**
   * The select listener for this container.
   */
  _onSelect: function({ detail: snapshotItem }) {
    // Check to ensure the attachment has an actor, like
    // an in-progress recording.
    if (!snapshotItem || !snapshotItem.attachment.actor) {
      return;
    }
    let { calls, thumbnails, screenshot } = snapshotItem.attachment;

    $("#reload-notice").hidden = true;
    $("#empty-notice").hidden = true;
    $("#waiting-notice").hidden = false;

    $("#debugging-pane-contents").hidden = true;
    $("#screenshot-container").hidden = true;
    $("#snapshot-filmstrip").hidden = true;

    Task.spawn(function*() {
      // Wait for a few milliseconds between presenting the function calls,
      // screenshot and thumbnails, to allow each component being
      // sequentially drawn. This gives the illusion of snappiness.

      yield DevToolsUtils.waitForTime(SNAPSHOT_DATA_DISPLAY_DELAY);
      CallsListView.showCalls(calls);
      $("#debugging-pane-contents").hidden = false;
      $("#waiting-notice").hidden = true;

      yield DevToolsUtils.waitForTime(SNAPSHOT_DATA_DISPLAY_DELAY);
      CallsListView.showThumbnails(thumbnails);
      $("#snapshot-filmstrip").hidden = false;

      yield DevToolsUtils.waitForTime(SNAPSHOT_DATA_DISPLAY_DELAY);
      CallsListView.showScreenshot(screenshot);
      $("#screenshot-container").hidden = false;

      window.emit(EVENTS.SNAPSHOT_RECORDING_SELECTED);
    });
  },

  /**
   * The click listener for the "clear" button in this container.
   */
  _onClearButtonClick: function() {
    Task.spawn(function*() {
      SnapshotsListView.empty();
      CallsListView.empty();

      $("#reload-notice").hidden = true;
      $("#empty-notice").hidden = true;
      $("#waiting-notice").hidden = true;

      if (yield gFront.isInitialized()) {
        $("#empty-notice").hidden = false;
      } else {
        $("#reload-notice").hidden = false;
      }

      $("#debugging-pane-contents").hidden = true;
      $("#screenshot-container").hidden = true;
      $("#snapshot-filmstrip").hidden = true;

      window.emit(EVENTS.SNAPSHOTS_LIST_CLEARED);
    });
  },

  /**
   * The click listener for the "record" button in this container.
   */
  _onRecordButtonClick: function () {
    this._disableRecordButton();

    if (this._recording) {
      this._stopRecordingAnimation();
      return;
    }

    // Insert a "dummy" snapshot item in the view, to hint that recording
    // has now started. However, wait for a few milliseconds before actually
    // starting the recording, since that might block rendering and prevent
    // the dummy snapshot item from being drawn.
    this.addSnapshot();

    // If this is the first item, immediately show the "Loading…" notice.
    if (this.itemCount == 1) {
      $("#empty-notice").hidden = true;
      $("#waiting-notice").hidden = false;
    }

    this._recordAnimation();
  },

  /**
   * Makes the record button able to be clicked again.
   */
  _enableRecordButton: function () {
    $("#record-snapshot").removeAttribute("disabled");
  },

  /**
   * Makes the record button unable to be clicked.
   */
  _disableRecordButton: function () {
    $("#record-snapshot").setAttribute("disabled", true);
  },

  /**
   * Begins recording an animation.
   */
  _recordAnimation: Task.async(function *() {
    if (this._recording) {
      return;
    }
    this._recording = true;
    $("#record-snapshot").setAttribute("checked", "true");

    setNamedTimeout("canvas-actor-recording", CANVAS_ACTOR_RECORDING_ATTEMPT, this._stopRecordingAnimation);

    yield DevToolsUtils.waitForTime(SNAPSHOT_START_RECORDING_DELAY);
    window.emit(EVENTS.SNAPSHOT_RECORDING_STARTED);

    gFront.recordAnimationFrame().then(snapshot => {
      if (snapshot) {
        this._onRecordSuccess(snapshot);
      } else {
        this._onRecordFailure();
      }
    });

    // Wait another delay before reenabling the button to stop the recording
    // if a recording is not found.
    yield DevToolsUtils.waitForTime(SNAPSHOT_START_RECORDING_DELAY);
    this._enableRecordButton();
  }),

  /**
   * Stops recording animation. Called when a click on the stopwatch occurs during a recording,
   * or if a recording times out.
   */
  _stopRecordingAnimation: Task.async(function *() {
    clearNamedTimeout("canvas-actor-recording");
    let actorCanStop = yield gTarget.actorHasMethod("canvas", "stopRecordingAnimationFrame");

    if (actorCanStop) {
      yield gFront.stopRecordingAnimationFrame();
    }
    // If actor does not have the method to stop recording (Fx39+),
    // manually call the record failure method. This will call a connection failure
    // on disconnect as a result of `gFront.recordAnimationFrame()` never resolving,
    // but this is better than it hanging when there is no requestAnimationFrame anyway.
    else {
      this._onRecordFailure();
    }

    this._recording = false;
    $("#record-snapshot").removeAttribute("checked");
    this._enableRecordButton();
  }),

  /**
   * Resolves from the front's recordAnimationFrame to setup the interface with the screenshots.
   */
  _onRecordSuccess: Task.async(function *(snapshotActor) {
    // Clear bail-out case if frame found in CANVAS_ACTOR_RECORDING_ATTEMPT milliseconds
    clearNamedTimeout("canvas-actor-recording");
    let snapshotItem = this.getItemAtIndex(this.itemCount - 1);
    let snapshotOverview = yield snapshotActor.getOverview();
    this.customizeSnapshot(snapshotItem, snapshotActor, snapshotOverview);

    this._recording = false;
    $("#record-snapshot").removeAttribute("checked");

    window.emit(EVENTS.SNAPSHOT_RECORDING_COMPLETED);
    window.emit(EVENTS.SNAPSHOT_RECORDING_FINISHED);
  }),

  /**
   * Called as a reject from the front's recordAnimationFrame.
   */
  _onRecordFailure: function () {
    clearNamedTimeout("canvas-actor-recording");
    showNotification(gToolbox, "canvas-debugger-timeout", L10N.getStr("recordingTimeoutFailure"));
    window.emit(EVENTS.SNAPSHOT_RECORDING_CANCELLED);
    window.emit(EVENTS.SNAPSHOT_RECORDING_FINISHED);
    this.removeLastSnapshot();
  },

  /**
   * The click listener for the "import" button in this container.
   */
  _onImportButtonClick: function() {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, L10N.getStr("snapshotsList.saveDialogTitle"), Ci.nsIFilePicker.modeOpen);
    fp.appendFilter(L10N.getStr("snapshotsList.saveDialogJSONFilter"), "*.json");
    fp.appendFilter(L10N.getStr("snapshotsList.saveDialogAllFilter"), "*.*");

    if (fp.show() != Ci.nsIFilePicker.returnOK) {
      return;
    }

    let channel = NetUtil.newChannel({
      uri: NetUtil.newURI(fp.file),
      loadingNode: window.document,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER});
    channel.contentType = "text/plain";

    NetUtil.asyncFetch(channel, (inputStream, status) => {
      if (!Components.isSuccessCode(status)) {
        console.error("Could not import recorded animation frame snapshot file.");
        return;
      }
      try {
        let string = NetUtil.readInputStreamToString(inputStream, inputStream.available());
        var data = JSON.parse(string);
      } catch (e) {
        console.error("Could not read animation frame snapshot file.");
        return;
      }
      if (data.fileType != CALLS_LIST_SERIALIZER_IDENTIFIER) {
        console.error("Unrecognized animation frame snapshot file.");
        return;
      }

      // Add a `isLoadedFromDisk` flag on everything to avoid sending invalid
      // requests to the backend, since we're not dealing with actors anymore.
      let snapshotItem = this.addSnapshot();
      snapshotItem.isLoadedFromDisk = true;
      data.calls.forEach(e => e.isLoadedFromDisk = true);

      this.customizeSnapshot(snapshotItem, data.calls, data);
    });
  },

  /**
   * The click listener for the "save" button of each item in this container.
   */
  _onSaveButtonClick: function(e) {
    let snapshotItem = this.getItemForElement(e.target);

    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, L10N.getStr("snapshotsList.saveDialogTitle"), Ci.nsIFilePicker.modeSave);
    fp.appendFilter(L10N.getStr("snapshotsList.saveDialogJSONFilter"), "*.json");
    fp.appendFilter(L10N.getStr("snapshotsList.saveDialogAllFilter"), "*.*");
    fp.defaultString = "snapshot.json";

    // Start serializing all the function call actors for the specified snapshot,
    // while the nsIFilePicker dialog is being opened. Snappy.
    let serialized = Task.spawn(function*() {
      let data = {
        fileType: CALLS_LIST_SERIALIZER_IDENTIFIER,
        version: CALLS_LIST_SERIALIZER_VERSION,
        calls: [],
        thumbnails: [],
        screenshot: null
      };
      let functionCalls = snapshotItem.attachment.calls;
      let thumbnails = snapshotItem.attachment.thumbnails;
      let screenshot = snapshotItem.attachment.screenshot;

      // Prepare all the function calls for serialization.
      yield DevToolsUtils.yieldingEach(functionCalls, (call, i) => {
        let { type, name, file, line, timestamp, argsPreview, callerPreview } = call;
        return call.getDetails().then(({ stack }) => {
          data.calls[i] = {
            type: type,
            name: name,
            file: file,
            line: line,
            stack: stack,
            timestamp: timestamp,
            argsPreview: argsPreview,
            callerPreview: callerPreview
          };
        });
      });

      // Prepare all the thumbnails for serialization.
      yield DevToolsUtils.yieldingEach(thumbnails, (thumbnail, i) => {
        let { index, width, height, flipped, pixels } = thumbnail;
        data.thumbnails.push({ index, width, height, flipped, pixels });
      });

      // Prepare the screenshot for serialization.
      let { index, width, height, flipped, pixels } = screenshot;
      data.screenshot = { index, width, height, flipped, pixels };

      let string = JSON.stringify(data);
      let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
        createInstance(Ci.nsIScriptableUnicodeConverter);

      converter.charset = "UTF-8";
      return converter.convertToInputStream(string);
    });

    // Open the nsIFilePicker and wait for the function call actors to finish
    // being serialized, in order to save the generated JSON data to disk.
    fp.open({ done: result => {
      if (result == Ci.nsIFilePicker.returnCancel) {
        return;
      }
      let footer = $(".snapshot-item-footer", snapshotItem.target);
      let save = $(".snapshot-item-save", snapshotItem.target);

      // Show a throbber and a "Saving…" label if serializing isn't immediate.
      setNamedTimeout("call-list-save", CALLS_LIST_SLOW_SAVE_DELAY, () => {
        footer.classList.add("devtools-throbber");
        save.setAttribute("disabled", "true");
        save.setAttribute("value", L10N.getStr("snapshotsList.savingLabel"));
      });

      serialized.then(inputStream => {
        let outputStream = FileUtils.openSafeFileOutputStream(fp.file);

        NetUtil.asyncCopy(inputStream, outputStream, status => {
          if (!Components.isSuccessCode(status)) {
            console.error("Could not save recorded animation frame snapshot file.");
          }
          clearNamedTimeout("call-list-save");
          footer.classList.remove("devtools-throbber");
          save.removeAttribute("disabled");
          save.setAttribute("value", L10N.getStr("snapshotsList.saveLabel"));
        });
      });
    }});
  }
});

function showNotification (toolbox, name, message) {
  let notificationBox = toolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue(name);
  if (!notification) {
    notificationBox.appendNotification(message, name, "", notificationBox.PRIORITY_WARNING_HIGH);
  }
}

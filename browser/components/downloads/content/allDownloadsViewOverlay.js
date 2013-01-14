/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * THE PLACES VIEW IMPLEMENTED IN THIS FILE HAS A VERY PARTICULAR USE CASE.
 * IT IS HIGHLY RECOMMENDED NOT TO EXTEND IT FOR ANY OTHER USE CASES OR RELY
 * ON IT AS AN API.
 */

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource:///modules/DownloadsCommon.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

const nsIDM = Ci.nsIDownloadManager;

const DESTINATION_FILE_URI_ANNO  = "downloads/destinationFileURI";
const DESTINATION_FILE_NAME_ANNO = "downloads/destinationFileName";
const DOWNLOAD_STATE_ANNO        = "downloads/state";

const DOWNLOAD_VIEW_SUPPORTED_COMMANDS =
 ["cmd_delete", "cmd_copy", "cmd_paste", "cmd_selectAll",
  "downloadsCmd_pauseResume", "downloadsCmd_cancel",
  "downloadsCmd_open", "downloadsCmd_show", "downloadsCmd_retry",
  "downloadsCmd_openReferrer", "downloadsCmd_clearDownloads"];

const NOT_AVAILABLE = Number.MAX_VALUE;

function GetFileForFileURI(aFileURI)
  Cc["@mozilla.org/network/protocol;1?name=file"]
    .getService(Ci.nsIFileProtocolHandler)
    .getFileFromURLSpec(aFileURI);

/**
 * A download element shell is responsible for handling the commands and the
 * displayed data for a single download view element. The download element
 * could represent either a past download (for which we get data from places)  or
 * a "session" download (using a data-item object. See DownloadsCommon.jsm), or both.
 *
 * Once initialized with either a data item or a places node, the created richlistitem
 * can be accessed through the |element| getter, and can then be inserted/removed from
 * a richlistbox.
 *
 * The shell doesn't take care of inserting the item, or removing it when it's no longer
 * valid. That's the caller (a DownloadsPlacesView object) responsibility.
 *
 * The caller is also responsible for "passing over" notification from both the
 * download-view and the places-result-observer, in the following manner:
 *  - The DownloadsPlacesView object implements getViewItem of the download-view
 *    pseudo interface.  It returns this object (therefore we implement
 *    onStateChangea and onProgressChange here).
 *  - The DownloadsPlacesView object adds itself as a places result observer and
 *    calls this object's placesNodeIconChanged, placesNodeTitleChanged and
 *    placeNodeAnnotationChanged from its callbacks.
 *
 * @param [optional] aDataItem
 *        The data item of a the session download. Required if aPlacesNode is not set
 * @param [optional] aPlacesNode
 *        The places node for a past download. Required if aDataItem is not set.
 * @param [optional] aAnnotations
 *        Map containing annotations values, to speed up the initial loading.
 */
function DownloadElementShell(aDataItem, aPlacesNode, aAnnotations) {
  this._element = document.createElement("richlistitem");
  this._element._shell = this;

  this._element.classList.add("download");
  this._element.classList.add("download-state");

  if (aAnnotations)
    this._annotations = aAnnotations;
  if (aDataItem)
    this.dataItem = aDataItem;
  if (aPlacesNode)
    this.placesNode = aPlacesNode;
}

DownloadElementShell.prototype = {
  // The richlistitem for the download
  get element() this._element,

  /**
   * Manages the "active" state of the shell.  By default all the shells
   * without a dataItem are inactive, thus their UI is not updated.  They must
   * be activated when entering the visible area.  Session downloads are
   * always active since they always have a dataItem.
   *
   * @return whether the element was updated.
   */
  ensureActive: function DES_ensureActive() {
    if (this._active)
      return false;
    this._active = true;
    this._element.setAttribute("active", true);
    this._updateStatusUI();
    this._fetchTargetFileInfo();
    return true;
  },
  get active() !!this._active,

  // The data item for the download
  _dataItem: null,
  get dataItem() this._dataItem,

  set dataItem(aValue) {
    this._dataItem = aValue;
    let shouldUpdate = false;
    if (this._dataItem) {
      this._targetFileInfoFetched = false;
      // The dataItem can be replaced, in such a case the shell is already
      // active but must be updated regardless.
      shouldUpdate = !this.ensureActive();
    }
    else if (this._placesNode) {
      this._targetFileInfoFetched = false;
      shouldUpdate = this.active;
    }
    else {
      throw new Error("Should always have either a dataItem or a placesNode");
    }

    if (shouldUpdate) {
      this._fetchTargetFileInfo();
      this._updateStatusUI();
    }
    return aValue;
  },

  _placesNode: null,
  get placesNode() this._placesNode,
  set placesNode(aNode) {
    if (this._placesNode != aNode) {
      // Preserve the annotations map if this is the first loading and we got
      // cached values.
      if (this._placesNode || !this._annotations) {
        this._annotations = new Map();
      }
      this._placesNode = aNode;

      // We don't need to update the UI if we had a data item, because
      // the places information isn't used in this case.
      if (!this._dataItem) {
        if (!this._placesNode)
          throw new Error("Should always have either a dataItem or a placesNode");
        this._targetFileInfoFetched = false;
        if (this.active) {
          this._updateStatusUI();
          this._fetchTargetFileInfo();
        }
      }
    }
    return aNode;
  },

  // The download uri (as a string)
  get downloadURI() {
    if (this._dataItem)
     return this._dataItem.uri;
    if (this._placesNode)
      return this._placesNode.uri;
    throw new Error("Unexpected download element state");
  },

  get _downloadURIObj() {
    if (!("__downloadURIObj" in this))
      this.__downloadURIObj = NetUtil.newURI(this.downloadURI);
    return this.__downloadURIObj;
  },

  get _icon() {
    if (this._targetFileURI)
      return "moz-icon://" + this._targetFileURI + "?size=32";
    if (this._placesNode) {
      // Try to extract an extension from the uri.
      let ext = this._downloadURIObj.QueryInterface(Ci.nsIURL).fileExtension;
      if (ext)
        return "moz-icon://." + ext + "?size=32";
      return this._placesNode.icon || "moz-icon://.unknown?size=32";
    }
    if (this._dataItem)
      throw new Error("Session-download items should always have a target file uri");
    throw new Error("Unexpected download element state");
  },

  // Helper for getting a places annotation set for the download.
  _getAnnotation: function DES__getAnnotation(aAnnotation, aDefaultValue) {
    let value;
    if (this._annotations.has(aAnnotation))
      value = this._annotations.get(aAnnotation);

    // If the value is cached, or we know it doesn't exist, avoid a database
    // lookup.
    if (value === undefined) {
      try {
        value = PlacesUtils.annotations.getPageAnnotation(
          this._downloadURIObj, aAnnotation);
      }
      catch(ex) {
        value = NOT_AVAILABLE;
      }
    }

    if (value === NOT_AVAILABLE) {
      if (aDefaultValue === undefined) {
        throw new Error("Could not get required annotation '" + aAnnotation +
                        "' for download with url '" + this.downloadURI + "'");
      }
      value = aDefaultValue;
    }

    this._annotations.set(aAnnotation, value);
    return value;
  },

  // The label for the download
  get _displayName() {
    if (this._dataItem)
      return this._dataItem.target;

    try {
      return this._getAnnotation(DESTINATION_FILE_NAME_ANNO);
    }
    catch(ex) { }

    // Fallback to the places title, or, at last, to the download uri.
    return this._placesNode.title || this.downloadURI;
  },

  // The uri (as a string) of the target file, if any.
  get _targetFileURI() {
    if (this._dataItem)
      return this._dataItem.file;

    return this._getAnnotation(DESTINATION_FILE_URI_ANNO, "");
  },

  get _targetFilePath() {
    let fileURI = this._targetFileURI;
    if (fileURI)
      return GetFileForFileURI(fileURI).path;
    return "";
  },

  _fetchTargetFileInfo: function DES__fetchTargetFileInfo() {
    if (this._targetFileInfoFetched)
      throw new Error("_fetchTargetFileInfo should not be called if the information was already fetched");
    if (!this.active)
      throw new Error("Trying to _fetchTargetFileInfo on an inactive download shell");

    let path = this._targetFilePath;

    // In previous version, the target file annotations were not set,
    // so we cannot where is the file.
    if (!path) {
      this._targetFileInfoFetched = true;
      this._targetFileExists = false;
      return;
    }

    OS.File.stat(path).then(
      function onSuccess(fileInfo) {
        this._targetFileInfoFetched = true;
        this._targetFileExists = true;
        this._targetFileSize = fileInfo.size;
        delete this._state;
        this._updateDownloadStatusUI();
      }.bind(this),

      function onFailure(aReason) {
        if (reason instanceof OS.File.Error && reason.becauseNoSuchFile) {
          this._targetFileInfoFetched = true;
          this._targetFileExists = false;
        }
        else {
          Cu.reportError("Could not fetch info for target file (reason: " +
                         aReason + ")");
        }

        this._updateDownloadStatusUI();
      }.bind(this)
    );
  },

  /**
   * Get the state of the download (see nsIDownloadManager).
   * For past downloads, for which we don't know the state at first,
   * |undefined| is returned until we have info for the target file,
   * indicating the state is unknown. |undefined| is also returned
   * if the file was not found at last.
   *
   * @param [optional] aForceUpdate
   *        Whether to force update the cached download state. Default: false.
   * @return the download state if available, |undefined| otherwise.
   */
  getDownloadState: function DES_getDownloadState(aForceUpdate = false) {
    if (aForceUpdate || !("_state" in this)) {
      if (this._dataItem) {
        this._state = this._dataItem.state;
      }
      else {
        try {
          this._state = this._getAnnotation(DOWNLOAD_STATE_ANNO);
        }
        catch (ex) {
          if (!this._targetFileInfoFetched || !this._targetFileExists)
            this._state = undefined;
          else if (this._targetFileSize > 0)
            this._state = nsIDM.DOWNLOAD_FINISHED;
          else
            this._state = nsIDM.DOWNLOAD_FAILED;
        }
      }
    }
    return this._state;
  },

  // The status text for the download
  _getStatusText: function DES__getStatusText() {
    let s = DownloadsCommon.strings;
    if (this._dataItem && this._dataItem.inProgress) {
      if (this._dataItem.paused) {
        let transfer =
          DownloadUtils.getTransferTotal(this._dataItem.currBytes,
                                         this._dataItem.maxBytes);

         // We use the same XUL label to display both the state and the amount
         // transferred, for example "Paused -  1.1 MB".
         return s.statusSeparatorBeforeNumber(s.statePaused, transfer);
      }
      if (this._dataItem.state == nsIDM.DOWNLOAD_DOWNLOADING) {
        let [status, newEstimatedSecondsLeft] =
          DownloadUtils.getDownloadStatus(this.dataItem.currBytes,
                                          this.dataItem.maxBytes,
                                          this.dataItem.speed,
                                          this._lastEstimatedSecondsLeft);
        this._lastEstimatedSecondsLeft = newEstimatedSecondsLeft;
        return status;
      }
      if (this._dataItem.starting) {
        return s.stateStarting;
      }
      if (this._dataItem.state == nsIDM.DOWNLOAD_SCANNING) {
        return s.stateScanning;
      }

      throw new Error("_getStatusText called with a bogus download state");
    }

    // This is a not-in-progress or history download.
    let stateLabel = "";
    switch (this.getDownloadState()) {
      case nsIDM.DOWNLOAD_FAILED:
        stateLabel = s.stateFailed;
        break;
      case nsIDM.DOWNLOAD_CANCELED:
        stateLabel = s.stateCanceled;
        break;
      case nsIDM.DOWNLOAD_BLOCKED_PARENTAL:
        stateLabel = s.stateBlockedParentalControls;
        break;
      case nsIDM.DOWNLOAD_BLOCKED_POLICY:
        stateLabel = s.stateBlockedPolicy;
        break;
      case nsIDM.DOWNLOAD_DIRTY:
        stateLabel = s.stateDirty;
        break;
      case nsIDM.DOWNLOAD_FINISHED:{
        // For completed downloads, show the file size (e.g. "1.5 MB")
        if (this._targetFileInfoFetched && this._targetFileExists) {
          let [size, unit] = DownloadUtils.convertByteUnits(this._targetFileSize);
          stateLabel = s.sizeWithUnits(size, unit);
          break;
        }
        // Fallback to default unknown state.
      }
      default:
        stateLabel = s.sizeUnknown;
        break;
    }

    // TODO (bug 829201): history downloads should get the referrer from Places.
    let referrer = this._dataItem && this._dataItem.referrer ||
                   this.downloadURI;
    let [displayHost, fullHost] = DownloadUtils.getURIHost(referrer);

    // TODO (bug 826991): history downloads should get endTime from annotations.
    let date = this._dataItem && this._dataItem.endTime ||
               (this._placesNode.time / 1000);
    let [displayDate, fullDate] = DownloadUtils.getReadableDates(new Date(date));

    // We use the same XUL label to display the state, the host name, and the
    // end time.
    let firstPart = s.statusSeparator(stateLabel, displayHost);
    return s.statusSeparator(firstPart, displayDate);
  },

  // The progressmeter element for the download
  get _progressElement() {
    if (!("__progressElement" in this)) {
      this.__progressElement =
        document.getAnonymousElementByAttribute(this._element, "anonid",
                                                "progressmeter");
    }
    return this.__progressElement;
  },

  // Updates the download state attribute (and by that hide/unhide the
  // appropriate buttons and context menu items), the status text label,
  // and the progress meter.
  _updateDownloadStatusUI: function  DES__updateDownloadStatusUI() {
    let state = this.getDownloadState(true);
    if (state !== undefined)
      this._element.setAttribute("state", state);

    this._element.setAttribute("status", this._getStatusText());

    // For past-downloads, we're done. For session-downloads, we may also need
    // to update the progress-meter.
    if (!this._dataItem)
      return;

    // Copied from updateProgress in downloads.js.
    if (this._dataItem.starting) {
      // Before the download starts, the progress meter has its initial value.
      this._element.setAttribute("progressmode", "normal");
      this._element.setAttribute("progress", "0");
    }
    else if (this._dataItem.state == nsIDM.DOWNLOAD_SCANNING ||
             this._dataItem.percentComplete == -1) {
      // We might not know the progress of a running download, and we don't know
      // the remaining time during the malware scanning phase.
      this._element.setAttribute("progressmode", "undetermined");
    }
    else {
      // This is a running download of which we know the progress.
      this._element.setAttribute("progressmode", "normal");
      this._element.setAttribute("progress", this._dataItem.percentComplete);
    }

    // Dispatch the ValueChange event for accessibility, if possible.
    if (this._progressElement) {
      let event = document.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      this._progressElement.dispatchEvent(event);
    }
  },

  _updateStatusUI: function DES__updateStatusUI() {
    if (!this.active)
      throw new Error("Trying to _updateStatusUI on an inactive download shell");
    this._element.setAttribute("displayName", this._displayName);
    this._element.setAttribute("image", this._icon);
    this._updateDownloadStatusUI();
  },

  placesNodeIconChanged: function DES_placesNodeIconChanged() {
    if (!this._dataItem)
      this._element.setAttribute("image", this._icon);
  },

  placesNodeTitleChanged: function DES_placesNodeTitleChanged() {
    if (!this._dataItem)
      this._element.setAttribute("displayName", this._displayName);
  },

  placesNodeAnnotationChanged: function DES_placesNodeAnnotationChanged(aAnnoName) {
    this._annotations.delete(aAnnoName);
    if (!this._dataItem) {
      if (aAnnoName == DESTINATION_FILE_URI_ANNO) {
        this._element.setAttribute("image", this._icon);
        this._updateDownloadStatusUI();
      }
      else if (aAnnoName == DESTINATION_FILE_NAME_ANNO) {
        this._element.setAttribute("displayName", this._displayName);
      }
      else if (aAnnoName == DOWNLOAD_STATE_ANNO) {
        this._updateDownloadStatusUI();
        if (this._element.selected)
          goUpdateDownloadCommands();
      }
    }
  },

  /* DownloadView */
  onStateChange: function DES_onStateChange(aOldState) {
    if (aOldState != nsIDM.DOWNLOAD_FINISHED &&
        aOldState != this.dataItem.state) {
      // See comment in DVI_onStateChange in downloads.js (the panel-view)
      this._element.setAttribute("image", this._icon + "&state=normal");

      this._targetFileInfoFetched = false;
      if (this.active)
        this._fetchTargetFileInfo();
    }

    this._updateDownloadStatusUI();
    if (this._element.selected)
      goUpdateDownloadCommands();
  },

  /* DownloadView */
  onProgressChange: function DES_onProgressChange() {
    this._updateDownloadStatusUI();
  },

  /* nsIController */
  isCommandEnabled: function DES_isCommandEnabled(aCommand) {
    // The only valid command for inactive elements is cmd_delete.
    if (!this.active && aCommand != "cmd_delete")
      return false;
    switch (aCommand) {
      case "downloadsCmd_open": {
        // We cannot open a session dowload file unless it's done ("openable").
        // If it's finished, we need to make sure the file was not removed,
        // as we do for past downloads.
        if (this._dataItem && !this._dataItem.openable)
          return false;

        // Disable the command until we can yet tell whether
        // or not the file is there.
        if (!this._targetFileInfoFetched)
          return false;

        return this._targetFileExists;
      }
      case "downloadsCmd_show": {
        // TODO: Bug 827010 - Handle part-file asynchronously.
        if (this._dataItem &&
            this._dataItem.partFile && this._dataItem.partFile.exists())
          return true;

        // Disable the command until we can yet tell whether
        // or not the file is there.
        if (!this._targetFileInfoFetched)
          return false;

        return this._targetFileExists;
      }
      case "downloadsCmd_pauseResume":
        return this._dataItem && this._dataItem.inProgress && this._dataItem.resumable;
      case "downloadsCmd_retry":
        // An history download can always be retried.
        return !this._dataItem || this._dataItem.canRetry;
      case "downloadsCmd_openReferrer":
        return this._dataItem && !!this._dataItem.referrer;
      case "cmd_delete":
        // The behavior in this case is somewhat unexpected, so we disallow that.
        if (this._placesNode && this._dataItem && this._dataItem.inProgress)
          return false;
        return true;
      case "downloadsCmd_cancel":
        return this._dataItem != null;
    }
    return false;
  },

  _retryAsHistoryDownload: function DES__retryAsHistoryDownload() {
    // In future we may try to download into the same original target uri, when
    // we have it.  Though that requires verifying the path is still valid and
    // may surprise the user if he wants to be requested every time.

    // For private browsing, try to get document out of the most recent browser
    // window, or provide our own if there's no browser window.
    let browserWin = RecentWindow.getMostRecentBrowserWindow();
    let initiatingDoc = browserWin ? browserWin.document : document;
    saveURL(this.downloadURI, this._displayName, null, true, true, undefined,
            initiatingDoc);
  },

  /* nsIController */
  doCommand: function DES_doCommand(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_open": {
        if (this._dateItem)
          this._dataItem.openLocalFile(window);
        else
          DownloadsCommon.openDownloadedFile(
            GetFileForFileURI(this._targetFileURI), null, window);
        break;
      }
      case "downloadsCmd_show": {
        if (this._dataItem)
          this._dataItem.showLocalFile();
        else
          DownloadsCommon.showDownloadedFile(
            GetFileForFileURI(this._targetFileURI));
        break;
      }
      case "downloadsCmd_openReferrer": {
        openURL(this._dataItem.referrer);
        break;
      }
      case "downloadsCmd_cancel": {
        this._dataItem.cancel();
        break;
      }
      case "cmd_delete": {
        if (this._dataItem)
          this._dataItem.remove();
        if (this._placesNode)
          PlacesUtils.bhistory.removePage(this._downloadURIObj);
        break;
       }
      case "downloadsCmd_retry": {
        if (this._dataItem)
          this._dataItem.retry();
        else
          this._retryAsHistoryDownload();
        break;
      }
      case "downloadsCmd_pauseResume": {
        this._dataItem.togglePauseResume();
        break;
      }
    }
  },

  // Returns whether or not the download handled by this shell should
  // show up in the search results for the given term.  Both the display
  // name for the download and the url are searched.
  matchesSearchTerm: function DES_matchesSearchTerm(aTerm) {
    if (!aTerm)
      return true;
    aTerm = aTerm.toLowerCase();
    return this._displayName.toLowerCase().indexOf(aTerm) != -1 ||
           this.downloadURI.toLowerCase().indexOf(aTerm) != -1;
  },

  // Handles return kepress on the element (the keypress listener is
  // set in the DownloadsPlacesView object).
  doDefaultCommand: function DES_doDefaultCommand() {
    function getDefaultCommandForState(aState) {
      switch (aState) {
        case nsIDM.DOWNLOAD_FINISHED:
          return "downloadsCmd_open";
        case nsIDM.DOWNLOAD_PAUSED:
          return "downloadsCmd_pauseResume";
        case nsIDM.DOWNLOAD_NOTSTARTED:
        case nsIDM.DOWNLOAD_QUEUED:
          return "downloadsCmd_cancel";
        case nsIDM.DOWNLOAD_FAILED:
        case nsIDM.DOWNLOAD_CANCELED:
          return "downloadsCmd_retry";
        case nsIDM.DOWNLOAD_DOWNLOADING:
        case nsIDM.DOWNLOAD_SCANNING:
          return "downloadsCmd_show";
        case nsIDM.DOWNLOAD_BLOCKED_PARENTAL:
        case nsIDM.DOWNLOAD_DIRTY:
        case nsIDM.DOWNLOAD_BLOCKED_POLICY:
          return "downloadsCmd_openReferrer";
      }
      return "";
    }
    let command = getDefaultCommandForState(this.getDownloadState());
    if (this.isCommandEnabled(command))
      this.doCommand(command);
  }
};

/**
 * A Downloads Places View is a places view designed to show a places query
 * for history donwloads alongside the current "session"-downloads.
 *
 * As we don't use the places controller, some methods implemented by other
 * places views are not implemented by this view.
 *
 * A richlistitem in this view can represent either a past download or a session
 * download, or both. Session downloads are shown first in the view, and as long
 * as they exist they "collapses" their history "counterpart" (So we don't show two
 * items for every download).
 */
function DownloadsPlacesView(aRichListBox, aActive = true) {
  this._richlistbox = aRichListBox;
  this._richlistbox._placesView = this;
  this._richlistbox.controllers.appendController(this);

  // Map download URLs to download element shells regardless of their type
  this._downloadElementsShellsForURI = new Map();

  // Map download data items to their element shells.
  this._viewItemsForDataItems = new WeakMap();

  // Points to the last session download element. We keep track of this
  // in order to keep all session downloads above past downloads.
  this._lastSessionDownloadElement = null;

  this._searchTerm = "";

  this._active = aActive;

  // Register as a downloads view. The places data will be initialized by
  // the places setter.
  this._initiallySelectedElement = null;
  let downloadsData = DownloadsCommon.getData(window.opener || window);
  downloadsData.addView(this);

  // Make sure to unregister the view if the window is closed.
  window.addEventListener("unload", function() {
    this._richlistbox.controllers.removeController(this);
    downloadsData.removeView(this);
    this.result = null;
  }.bind(this), true);
  // Resizing the window may change items visibility.
  window.addEventListener("resize", function() {
    this._ensureVisibleElementsAreActive();
  }.bind(this), true);
}

DownloadsPlacesView.prototype = {
  get associatedElement() this._richlistbox,

  get active() this._active,
  set active(val) {
    this._active = val;
    if (this._active)
      this._ensureVisibleElementsAreActive();
    return this._active;
  },

  _forEachDownloadElementShellForURI:
  function DPV__forEachDownloadElementShellForURI(aURI, aCallback) {
    if (this._downloadElementsShellsForURI.has(aURI)) {
      let downloadElementShells = this._downloadElementsShellsForURI.get(aURI);
      for (let des of downloadElementShells) {
        aCallback(des);
      }
    }
  },

  _getAnnotationsFor: function DPV_getAnnotationsFor(aURI) {
    if (!this._cachedAnnotations) {
      this._cachedAnnotations = new Map();
      for (let name of [ DESTINATION_FILE_URI_ANNO,
                         DESTINATION_FILE_NAME_ANNO,
                         DOWNLOAD_STATE_ANNO ]) {
        let results = PlacesUtils.annotations.getAnnotationsWithName(name);
        for (let result of results) {
          let url = result.uri.spec;
          if (!this._cachedAnnotations.has(url))
            this._cachedAnnotations.set(url, new Map());
          let m = this._cachedAnnotations.get(url);
          m.set(result.annotationName, result.annotationValue);
        }
      }
    }

    let annotations = this._cachedAnnotations.get(aURI);
    if (!annotations) {
      // There are no annotations for this entry, that means it is quite old.
      // Make up a fake annotations entry with default values.
      annotations = new Map();
      annotations.set(DESTINATION_FILE_URI_ANNO, NOT_AVAILABLE);
      annotations.set(DESTINATION_FILE_NAME_ANNO, NOT_AVAILABLE);
    }
    // The state annotation has been added recently, so it's likely missing.
    if (!annotations.has(DOWNLOAD_STATE_ANNO)) {
      annotations.set(DOWNLOAD_STATE_ANNO, NOT_AVAILABLE);
    }
    return annotations;
  },

  /**
   * Given a data item for a session download, or a places node for a past
   * download, updates the view as necessary.
   *  1. If the given data is a places node, we check whether there are any
   *     elements for the same download url. If there are, then we just reset
   *     their places node. Otherwise we add a new download element.
   *  2. If the given data is a data item, we first check if there's a history
   *     download in the list that is not associated with a data item. If we
   *     found one, we use it for the data item as well and reposition it
   *     alongside the other session downloads. If we don't, then we go ahead
   *     and create a new element for the download.
   *
   * @param aDataItem
   *        The data item of a session download. Set to null for history
   *        downloads data.
   * @param [optional] aPlacesNode
   *        The places node for a history download. Required if there's no data
   *        item.
   * @param [optional] aNewest
   *        @see onDataItemAdded. Ignored for history downlods.
   * @param [optional] aDocumentFragment
   *        To speed up the appending of multiple elements to the end of the
   *        list which are coming in a single batch (i.e. invalidateContainer),
   *        a document fragment may be passed to which the new elements would
   *        be appended. It's the caller's job to ensure the fragment is merged
   *        to the richlistbox at the end.
   */
  _addDownloadData:
  function DPV_addDownloadData(aDataItem, aPlacesNode, aNewest = false,
                               aDocumentFragment = null) {
    let downloadURI = aPlacesNode ? aPlacesNode.uri : aDataItem.uri;
    let shellsForURI = this._downloadElementsShellsForURI.get(downloadURI, null);
    if (!shellsForURI) {
      shellsForURI = new Set();
      this._downloadElementsShellsForURI.set(downloadURI, shellsForURI);
    }

    let newOrUpdatedShell = null;

    // Trivial: if there are no shells for this download URI, we always
    // need to create one.
    let shouldCreateShell = shellsForURI.size == 0;

    // However, if we do have shells for this download uri, there are
    // few options:
    // 1) There's only one shell and it's for a history download (it has
    //    no data item). In this case, we update this shell and move it
    //    if necessary
    // 2) There are multiple shells, indicicating multiple downloads for
    //    the same download uri are running. In this case we create
    //    anoter shell for the download (so we have one shell for each data
    //    item).
    //
    // Note: If a cancelled session download is already in the list, and the
    // download is retired, onDataItemAdded is called again for the same
    // data item. Thus, we also check that we make sure we don't have a view item
    // already.
    if (!shouldCreateShell &&
        aDataItem && this.getViewItem(aDataItem) == null) {
      // If there's a past-download-only shell for this download-uri with no
      // associated data item, use it for the new data item. Otherwise, go ahead
      // and create another shell.
      shouldCreateShell = true;
      for (let shell of shellsForURI) {
        if (!shell.dataItem) {
          shouldCreateShell = false;
          shell.dataItem = aDataItem;
          newOrUpdatedShell = shell;
          this._viewItemsForDataItems.set(aDataItem, shell);
          break;
        }
      }
    }

    if (shouldCreateShell) {
      let shell = new DownloadElementShell(aDataItem, aPlacesNode,
                                           this._getAnnotationsFor(downloadURI));
      newOrUpdatedShell = shell;
      shellsForURI.add(shell);
      if (aDataItem)
        this._viewItemsForDataItems.set(aDataItem, shell);
    }
    else if (aPlacesNode) {
      for (let shell of shellsForURI) {
        if (shell.placesNode != aPlacesNode)
          shell.placesNode = aPlacesNode;
      }
    }

    if (newOrUpdatedShell) {
      if (aNewest) {
        this._richlistbox.insertBefore(newOrUpdatedShell.element,
                                       this._richlistbox.firstChild);
        if (!this._lastSessionDownloadElement) {
          this._lastSessionDownloadElement = newOrUpdatedShell.element;
        }
        // Some operations like retrying an history download move an element to
        // the top of the richlistbox, along with other session downloads.
        // More generally, if a new download is added, should be made visible.
        this._richlistbox.ensureElementIsVisible(newOrUpdatedShell.element);
      }
      else if (aDataItem) {
        let before = this._lastSessionDownloadElement ?
          this._lastSessionDownloadElement.nextSibling : this._richlistbox.firstChild;
        this._richlistbox.insertBefore(newOrUpdatedShell.element, before);
        this._lastSessionDownloadElement = newOrUpdatedShell.element;
      }
      else {
        let appendTo = aDocumentFragment || this._richlistbox;
        appendTo.appendChild(newOrUpdatedShell.element);
      }

      if (this.searchTerm) {
        newOrUpdatedShell.element.hidden =
          !newOrUpdatedShell.element._shell.matchesSearchTerm(this.searchTerm);
      }
    }

    // If aDocumentFragment is defined this is a batch change, so it's up to
    // the caller to append the fragment and activate the visible shells.
    if (!aDocumentFragment)
      this._ensureVisibleElementsAreActive();
  },

  _removeElement: function DPV__removeElement(aElement) {
    // If the element was selected exclusively, select its next
    // sibling first, if any.
    if (aElement.nextSibling &&
        this._richlistbox.selectedItems &&
        this._richlistbox.selectedItems.length > 0 &&
        this._richlistbox.selectedItems[0] == aElement) {
      this._richlistbox.selectItem(aElement.nextSibling);
    }
    this._richlistbox.removeChild(aElement);
    this._ensureVisibleElementsAreActive();
  },

  _removeHistoryDownloadFromView:
  function DPV__removeHistoryDownloadFromView(aPlacesNode) {
    let downloadURI = aPlacesNode.uri;
    let shellsForURI = this._downloadElementsShellsForURI.get(downloadURI, null);
    if (shellsForURI) {
      for (let shell of shellsForURI) {
        if (shell.dataItem) {
          shell.placesNode = null;
        }
        else {
          this._removeElement(shell.element);
          shellsForURI.delete(shell);
          if (shellsForURI.size == 0)
            this._downloadElementsShellsForURI.delete(downloadURI);
        }
      }
    }
  },

  _removeSessionDownloadFromView:
  function DPV__removeSessionDownloadFromView(aDataItem) {
    let shells = this._downloadElementsShellsForURI.get(aDataItem.uri, null);
    if (shells.size == 0)
      throw new Error("Should have had at leaat one shell for this uri");

    let shell = this.getViewItem(aDataItem);
    if (!shells.has(shell))
      throw new Error("Missing download element shell in shells list for url");

    // If there's more than one item for this download uri, we can let the
    // view item for this this particular data item go away.
    // If there's only one item for this download uri, we should only
    // keep it if it is associated with a history download.
    if (shells.size > 1 || !shell.placesNode) {
      this._removeElement(shell.element);
      shells.delete(shell);
      if (shells.size == 0)
        this._downloadElementsShellsForURI.delete(aDataItem.uri);
    }
    else {
      shell.dataItem = null;
      // Move it below the session-download items;
      if (this._lastSessionDownloadElement == shell.dataItem) {
        this._lastSessionDownloadElement = shell.dataItem.previousSibling;
      }
      else {
        let before = this._lastSessionDownloadElement ?
          this._lastSessionDownloadElement.nextSibling : this._richlistbox.firstChild;
        this._richlistbox.insertBefore(shell.element, before);
      }
    }
  },

  _ensureVisibleElementsAreActive:
  function DPV__ensureVisibleElementsAreActive() {
    if (!this.active || this._ensureVisibleTimer || !this._richlistbox.firstChild)
      return;

    this._ensureVisibleTimer = setTimeout(function() {
      delete this._ensureVisibleTimer;
      if (!this._richlistbox.firstChild)
        return;

      let rlbRect = this._richlistbox.getBoundingClientRect();
      let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      let nodes = winUtils.nodesFromRect(rlbRect.left, rlbRect.top,
                                         0, rlbRect.width, rlbRect.height, 0,
                                         true, false);
      // nodesFromRect returns nodes in z-index order, and for the same z-index
      // sorts them in inverted DOM order, thus starting from the one that would
      // be on top.
      let firstVisibleNode, lastVisibleNode;
      for (let node of nodes) {
        if (node.localName === "richlistitem" && node._shell) {
          node._shell.ensureActive();
          // The first visible node is the last match.
          firstVisibleNode = node;
          // While the last visible node is the first match.
          if (!lastVisibleNode)
            lastVisibleNode = node;
        }
      }

      // Also activate the first invisible nodes in both boundaries (that is,
      // above and below the visible area) to ensure proper keyboard navigation
      // in both directions.
      let nodeBelowVisibleArea = lastVisibleNode && lastVisibleNode.nextSibling;
      if (nodeBelowVisibleArea && nodeBelowVisibleArea._shell)
        nodeBelowVisibleArea._shell.ensureActive();

      let nodeABoveVisibleArea =
        firstVisibleNode && firstVisibleNode.previousSibling;
      if (nodeABoveVisibleArea && nodeABoveVisibleArea._shell)
        nodeABoveVisibleArea._shell.ensureActive();
    }.bind(this), 10);
  },

  _place: "",
  get place() this._place,
  set place(val) {
    // Don't reload everything if we don't have to.
    if (this._place == val) {
      // XXXmano: places.js relies on this behavior (see Bug 822203).
      this.searchTerm = "";
      return val;
    }

    this._place = val;

    let history = PlacesUtils.history;
    let queries = { }, options = { };
    history.queryStringToQueries(val, queries, { }, options);
    if (!queries.value.length)
      queries.value = [history.getNewQuery()];

    let result = history.executeQueries(queries.value, queries.value.length,
                                        options.value);
    result.addObserver(this, false);
    return val;
  },

  _result: null,
  get result() this._result,
  set result(val) {
    if (this._result == val)
      return val;

    if (this._result) {
      this._result.removeObserver(this);
      this._resultNode.containerOpen = false;
    }

    if (val) {
      this._result = val;
      this._resultNode = val.root;
      this._resultNode.containerOpen = true;
      this._ensureInitialSelection();
    }
    else {
      delete this._resultNode;
      delete this._result;
    }

    return val;
  },

  get selectedNodes() {
    let placesNodes = [];
    let selectedElements = this._richlistbox.selectedItems;
    for (let elt of selectedElements) {
      if (elt._shell.placesNode)
        placesNodes.push(elt._shell.placesNode);
    }
    return placesNodes;
  },

  get selectedNode() {
    let selectedNodes = this.selectedNodes;
    return selectedNodes.length == 1 ? selectedNodes[0] : null;
  },

  get hasSelection() this.selectedNodes.length > 0,

  containerStateChanged:
  function DPV_containerStateChanged(aNode, aOldState, aNewState) {
    this.invalidateContainer(aNode)
  },

  invalidateContainer:
  function DPV_invalidateContainer(aContainer) {
    if (aContainer != this._resultNode)
      throw new Error("Unexpected container node");
    if (!aContainer.containerOpen)
      throw new Error("Root container for the downloads query cannot be closed");

    // Remove the invalidated history downloads from the list and unset the
    // places node for data downloads.
    for (let element of this._richlistbox.childNodes) {
      if (element._shell.placesNode)
        this._removeHistoryDownloadFromView(element._shell.placesNode);
    }

    let elementsToAppendFragment = document.createDocumentFragment();
    for (let i = 0; i < aContainer.childCount; i++) {
      try {
        this._addDownloadData(null, aContainer.getChild(i), false,
                              elementsToAppendFragment);
      }
      catch(ex) {
        Cu.reportError(ex);
      }
    }

    this._appendDownloadsFragment(elementsToAppendFragment);
    this._ensureVisibleElementsAreActive();
  },

  _appendDownloadsFragment: function DPV__appendDownloadsFragment(aDOMFragment) {
    // Workaround multiple reflows hang by removing the richlistbox
    // and adding it back when we're done.
    let parentNode = this._richlistbox.parentNode;
    let nextSibling = this._richlistbox.nextSibling;
    this._richlistbox.controllers.removeController(this);
    parentNode.removeChild(this._richlistbox);
    this._richlistbox.appendChild(aDOMFragment);
    parentNode.insertBefore(this._richlistbox, nextSibling);
    this._richlistbox.controllers.appendController(this);
  },

  nodeInserted: function DPV_nodeInserted(aParent, aPlacesNode) {
    this._addDownloadData(null, aPlacesNode);
  },

  nodeRemoved: function DPV_nodeRemoved(aParent, aPlacesNode, aOldIndex) {
    this._removeHistoryDownloadFromView(aPlacesNode);
  },

  nodeIconChanged: function DPV_nodeIconChanged(aNode) {
    this._forEachDownloadElementShellForURI(aNode.uri, function(aDownloadElementShell) {
      aDownloadElementShell.placesNodeIconChanged();
    });
  },

  nodeAnnotationChanged: function DPV_nodeAnnotationChanged(aNode, aAnnoName) {
    this._forEachDownloadElementShellForURI(aNode.uri, function(aDownloadElementShell) {
      aDownloadElementShell.placesNodeAnnotationChanged(aAnnoName);
    });
  },

  nodeTitleChanged: function DPV_nodeTitleChanged(aNode, aNewTitle) {
    this._forEachDownloadElementShellForURI(aNode.uri, function(aDownloadElementShell) {
      aDownloadElementShell.placesNodeTitleChanged();
    });
  },

  nodeKeywordChanged: function() {},
  nodeDateAddedChanged: function() {},
  nodeLastModifiedChanged: function() {},
  nodeReplaced: function() {},
  nodeHistoryDetailsChanged: function() {},
  nodeTagsChanged: function() {},
  sortingChanged: function() {},
  nodeMoved: function() {},
  nodeURIChanged: function() {},
  batching: function() {},

  get controller() this._richlistbox.controller,

  get searchTerm() this._searchTerm,
  set searchTerm(aValue) {
    if (this._searchTerm != aValue) {
      for (let element of this._richlistbox.childNodes) {
        element.hidden = !element._shell.matchesSearchTerm(aValue);
      }
      this._ensureVisibleElementsAreActive();
    }
    return this._searchTerm = aValue;
  },

  /**
   * When the view loads, we want to select the first item.
   * However, because session downloads, for which the data is loaded
   * asynchronously, always come first in the list, and because the list
   * may (or may not) already contain history downloads at that point, it
   * turns out that by the time we can select the first item, the user may
   * have already started using the view.
   * To make things even more complicated, in other cases, the places data
   * may be loaded after the session downloads data.  Thus we cannot rely on
   * the order in which the data comes in.
   * We work around this by attempting to select the first element twice,
   * once after the places data is loaded and once when the session downloads
   * data is done loading.  However, if the selection has changed in-between,
   * we assume the user has already started using the view and give up.
   */
  _ensureInitialSelection: function DPV__ensureInitialSelection() {
    // Either they're both null, or the selection has not changed in between.
    if (this._richlistbox.selectedItem == this._initiallySelectedElement) {
      let firstDownloadElement = this._richlistbox.firstChild;
      if (firstDownloadElement != this._initiallySelectedElement) {
        // We may be called before _ensureVisibleElementsAreActive,
        // or before the download binding is attached. Therefore, ensure the
        // first item is activated, and pass the item to the richlistbox
        // setters only at a point we know for sure the binding is attached.
        firstDownloadElement._shell.ensureActive();
        Services.tm.mainThread.dispatch(function() {
          this._richlistbox.selectedItem = firstDownloadElement;
          this._richlistbox.currentItem = firstDownloadElement;
          this._initiallySelectedElement = firstDownloadElement;
        }.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
      }
    }
  },

  onDataLoadStarting: function() { },
  onDataLoadCompleted: function DPV_onDataLoadCompleted() {
    this._ensureInitialSelection();
  },

  onDataItemAdded: function DPV_onDataItemAdded(aDataItem, aNewest) {
    this._addDownloadData(aDataItem, null, aNewest);
  },

  onDataItemRemoved: function DPV_onDataItemRemoved(aDataItem) {
    this._removeSessionDownloadFromView(aDataItem);
  },

  getViewItem: function(aDataItem)
    this._viewItemsForDataItems.get(aDataItem, null),

  supportsCommand: function(aCommand)
    DOWNLOAD_VIEW_SUPPORTED_COMMANDS.indexOf(aCommand) != -1,

  isCommandEnabled: function DPV_isCommandEnabled(aCommand) {
    let selectedElements = this._richlistbox.selectedItems;
    switch (aCommand) {
      case "cmd_copy":
        return selectedElements && selectedElements.length > 0;
      case "cmd_selectAll":
        return true;
      case "cmd_paste":
        return this._canDownloadClipboardURL();
      case "downloadsCmd_clearDownloads":
        return !!this._richlistbox.firstChild;
      default:
        return Array.every(selectedElements, function(element) {
          return element._shell.isCommandEnabled(aCommand);
        });
    }
  },

  _copySelectedDownloadsToClipboard:
  function DPV__copySelectedDownloadsToClipboard() {
    let selectedElements = this._richlistbox.selectedItems;
    let urls = [e._shell.downloadURI for each (e in selectedElements)];

    Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper).copyString(urls.join("\n"), document);
  },

  _getURLFromClipboardData: function DPV__getURLFromClipboardData() {
    let trans = Cc["@mozilla.org/widget/transferable;1"].
                createInstance(Ci.nsITransferable);
    trans.init(null);

    let flavors = ["text/x-moz-url", "text/unicode"];
    flavors.forEach(trans.addDataFlavor);

    Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);

    // Getting the data or creating the nsIURI might fail.
    try {
      let data = {};
      trans.getAnyTransferData({}, data, {});
      let [url, name] = data.value.QueryInterface(Ci.nsISupportsString)
                            .data.split("\n");
      if (url)
        return [NetUtil.newURI(url, null, null).spec, name];
    }
    catch(ex) { }

    return ["", ""];
  },

  _canDownloadClipboardURL: function DPV__canDownloadClipboardURL() {
    let [url, name] = this._getURLFromClipboardData();
    return url != "";
  },

  _downloadURLFromClipboard: function DPV__downloadURLFromClipboard() {
    let [url, name] = this._getURLFromClipboardData();
    saveURL(url, name || url, null, true, true, undefined, document);
  },

  doCommand: function DPV_doCommand(aCommand) {
    switch (aCommand) {
      case "cmd_copy":
        this._copySelectedDownloadsToClipboard();
        break;
      case "cmd_selectAll":
        this._richlistbox.selectAll();
        break;
      case "cmd_paste":
        this._downloadURLFromClipboard();
        break;
      case "downloadsCmd_clearDownloads":
        if (PrivateBrowsingUtils.isWindowPrivate(window)) {
          Services.downloads.cleanUpPrivate();
        } else {
          Services.downloads.cleanUp();
        }
        if (this.result) {
          Cc["@mozilla.org/browser/download-history;1"]
            .getService(Ci.nsIDownloadHistory)
            .removeAllDownloads();
        }
        break;
      default: {
        let selectedElements = this._richlistbox.selectedItems;
        for (let element of selectedElements) {
          element._shell.doCommand(aCommand);
        }
      }
    }
  },

  onEvent: function() { },

  onContextMenu: function DPV_onContextMenu(aEvent)
  {
    let element = this._richlistbox.selectedItem;
    if (!element || !element._shell)
      return false;

    // Set the state attribute so that only the appropriate items are displayed.
    let contextMenu = document.getElementById("downloadsContextMenu");
    let state = element._shell.getDownloadState();
    if (state !== undefined)
      contextMenu.setAttribute("state", state);
    else
      contextMenu.removeAttribute("state");

    return true;
  },

  onKeyPress: function DPV_onKeyPress(aEvent) {
    let selectedElements = this._richlistbox.selectedItems;
    if (!selectedElements)
      return;

    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      // In the content tree, opening bookmarks by pressing return is only
      // supported when a single item is selected. To be consistent, do the
      // same here.
      if (selectedElements.length == 1) {
        let element = selectedElements[0];
        if (element._shell)
          element._shell.doDefaultCommand();
      }
    }
    else if (aEvent.charCode == " ".charCodeAt(0)) {
      // Pausue/Resume every selected download
      for (let element of selectedElements) {
        if (element._shell.isCommandEnabled("downloadsCmd_pauseResume"))
          element._shell.doCommand("downloadsCmd_pauseResume");
      }
    }
  },

  onDoubleClick: function DPV_onDoubleClick(aEvent) {
    if (aEvent.button != 0)
      return;

    let selectedElements = this._richlistbox.selectedItems;
    if (!selectedElements || selectedElements.length != 1)
      return;

    let element = selectedElements[0];
    if (element._shell)
      element._shell.doDefaultCommand();
  },

  onScroll: function DPV_onScroll() {
    this._ensureVisibleElementsAreActive();
  }
};

for (let methodName of ["load", "applyFilter", "selectNode", "selectItems"]) {
  DownloadsPlacesView.prototype[methodName] = function() {
    throw new Error("|" + methodName + "| is not implemented by the downloads view.");
  }
}

function goUpdateDownloadCommands() {
  for (let command of DOWNLOAD_VIEW_SUPPORTED_COMMANDS) {
    goUpdateCommand(command);
  }
}

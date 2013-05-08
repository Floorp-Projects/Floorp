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
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

const nsIDM = Ci.nsIDownloadManager;

const DESTINATION_FILE_URI_ANNO  = "downloads/destinationFileURI";
const DOWNLOAD_META_DATA_ANNO    = "downloads/metaData";

const DOWNLOAD_VIEW_SUPPORTED_COMMANDS =
 ["cmd_delete", "cmd_copy", "cmd_paste", "cmd_selectAll",
  "downloadsCmd_pauseResume", "downloadsCmd_cancel",
  "downloadsCmd_open", "downloadsCmd_show", "downloadsCmd_retry",
  "downloadsCmd_openReferrer", "downloadsCmd_clearDownloads"];

const NOT_AVAILABLE = Number.MAX_VALUE;

/**
 * Download a URL.
 *
 * @param aURL
 *        the url to download (nsIURI object)
 * @param [optional] aFileName
 *        the destination file name
 */
function DownloadURL(aURL, aFileName) {
  // For private browsing, try to get document out of the most recent browser
  // window, or provide our own if there's no browser window.
  let browserWin = RecentWindow.getMostRecentBrowserWindow();
  let initiatingDoc = browserWin ? browserWin.document : document;
  saveURL(aURL, aFileName, null, true, true, undefined, initiatingDoc);
}

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
   */
  ensureActive: function DES_ensureActive() {
    if (!this._active) {
      this._active = true;
      this._element.setAttribute("active", true);
      this._updateUI();
    }
  },
  get active() !!this._active,

  // The data item for the download
  _dataItem: null,
  get dataItem() this._dataItem,

  set dataItem(aValue) {
    if (this._dataItem != aValue) {
      if (!aValue && !this._placesNode)
        throw new Error("Should always have either a dataItem or a placesNode");

      this._dataItem = aValue;
      if (!this.active)
        this.ensureActive();
      else
        this._updateUI();
    }
    return aValue;
  },

  _placesNode: null,
  get placesNode() this._placesNode,
  set placesNode(aValue) {
    if (this._placesNode != aValue) {
      if (!aValue && !this._dataItem)
        throw new Error("Should always have either a dataItem or a placesNode");

      // Preserve the annotations map if this is the first loading and we got
      // cached values.
      if (this._placesNode || !this._annotations) {
        this._annotations = new Map();
      }

      this._placesNode = aValue;

      // We don't need to update the UI if we had a data item, because
      // the places information isn't used in this case.
      if (!this._dataItem && this.active)
        this._updateUI();
    }
    return aValue;
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

  _getIcon: function DES__getIcon() {
    let metaData = this.getDownloadMetaData();
    if ("filePath" in metaData)
      return "moz-icon://" + metaData.filePath + "?size=32";

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

  _fetchTargetFileInfo: function DES__fetchTargetFileInfo(aUpdateMetaDataAndStatusUI = false) {
    if (this._targetFileInfoFetched)
      throw new Error("_fetchTargetFileInfo should not be called if the information was already fetched");
    if (!this.active)
      throw new Error("Trying to _fetchTargetFileInfo on an inactive download shell");

    let path = this.getDownloadMetaData().filePath;

    // In previous version, the target file annotations were not set,
    // so we cannot tell where is the file.
    if (path === undefined) {
      this._targetFileInfoFetched = true;
      this._targetFileExists = false;
      if (aUpdateMetaDataAndStatusUI) {
        this._metaData = null;
        this._updateDownloadStatusUI();
      }
      // Here we don't need to update the download commands,
      // as the state is unknown as it was.
      return;
    }

    OS.File.stat(path).then(
      function onSuccess(fileInfo) {
        this._targetFileInfoFetched = true;
        this._targetFileExists = true;
        this._targetFileSize = fileInfo.size;
        if (aUpdateMetaDataAndStatusUI) {
          this._metaData = null;
          this._updateDownloadStatusUI();
        }
        if (this._element.selected)
          goUpdateDownloadCommands();
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

        if (aUpdateMetaDataAndStatusUI) {
          this._metaData = null;
          this._updateDownloadStatusUI();
        }

        if (this._element.selected)
          goUpdateDownloadCommands();
      }.bind(this)
    );
  },

  _getAnnotatedMetaData: function DES__getAnnotatedMetaData()
    JSON.parse(this._getAnnotation(DOWNLOAD_META_DATA_ANNO)),

  _extractFilePathAndNameFromFileURI:
  function DES__extractFilePathAndNameFromFileURI(aFileURI) {
    let file = Cc["@mozilla.org/network/protocol;1?name=file"]
                .getService(Ci.nsIFileProtocolHandler)
                .getFileFromURLSpec(aFileURI);
    return [file.path, file.leafName];
  },

  /**
   * Retrieve the meta data object for the download.  The following fields
   * may be set.
   *
   * - state - any download state defined in nsIDownloadManager.  If this field
   *   is not set, the download state is unknown.
   * - endTime: the end time of the download.
   * - filePath: the downloaded file path on the file system, when it
   *   was downloaded.  The file may not exist.  This is set for session
   *   downloads that have a local file set, and for history downloads done
   *   after the landing of bug 591289.
   * - fileName: the downloaded file name on the file system. Set if filePath
   *   is set.
   * - displayName: the user-facing label for the download.  This is always
   *   set.  If available, it's set to the downloaded file name.  If not,
   *   the places title for the download uri is used it's set.  As a last
   *   resort, we fallback to the download uri.
   * - fileSize (only set for downloads which completed succesfully):
   *   the downloaded file size.  For downloads done after the landing of
   *   bug 826991, this value is "static" - that is, it does not necessarily
   *   mean that the file is in place and has this size.
   */
  getDownloadMetaData: function DES_getDownloadMetaData() {
    if (!this._metaData) {
      if (this._dataItem) {
        this._metaData = {
          state:       this._dataItem.state,
          endTime:     this._dataItem.endTime,
          fileName:    this._dataItem.target,
          displayName: this._dataItem.target
        };
        if (this._dataItem.done)
          this._metaData.fileSize = this._dataItem.maxBytes;
        if (this._dataItem.localFile)
          this._metaData.filePath = this._dataItem.localFile.path;
      }
      else {
        try {
          this._metaData = this._getAnnotatedMetaData();
        }
        catch(ex) {
          this._metaData = { };
          if (this._targetFileInfoFetched && this._targetFileExists) {
            this._metaData.state = this._targetFileSize > 0 ?
              nsIDM.DOWNLOAD_FINISHED : nsIDM.DOWNLOAD_FAILED;
            this._metaData.fileSize = this._targetFileSize;
          }

          // This is actually the start-time, but it's the best we can get.
          this._metaData.endTime = this._placesNode.time / 1000;
        }

        try {
          let targetFileURI = this._getAnnotation(DESTINATION_FILE_URI_ANNO);
          [this._metaData.filePath, this._metaData.fileName] =
            this._extractFilePathAndNameFromFileURI(targetFileURI);
          this._metaData.displayName = this._metaData.fileName;
        }
        catch(ex) {
          this._metaData.displayName = this._placesNode.title || this.downloadURI;
        }
      }
    }
    return this._metaData;
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
                                          this._lastEstimatedSecondsLeft || Infinity);
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
    let state = this.getDownloadMetaData().state;
    switch (state) {
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
        let metaData = this.getDownloadMetaData();
        if ("fileSize" in metaData) {
          let [size, unit] = DownloadUtils.convertByteUnits(metaData.fileSize);
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

    let date = new Date(this.getDownloadMetaData().endTime);
    let [displayDate, fullDate] = DownloadUtils.getReadableDates(date);

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
    if (!this.active)
      throw new Error("_updateDownloadStatusUI called for an inactive item.");

    let state = this.getDownloadMetaData().state;
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

  _updateDisplayNameAndIcon: function DES__updateDisplayNameAndIcon() {
    let metaData = this.getDownloadMetaData();
    this._element.setAttribute("displayName", metaData.displayName);
    this._element.setAttribute("image", this._getIcon());
  },

  _updateUI: function DES__updateUI() {
    if (!this.active)
      throw new Error("Trying to _updateUI on an inactive download shell");

    this._metaData = null;
    this._targetFileInfoFetched = false;

    this._updateDisplayNameAndIcon();

    // For history downloads done in past releases, the downloads/metaData
    // annotation is not set, and therefore we cannot tell the download
    // state without the target file information.
    if (this._dataItem || this.getDownloadMetaData().state !== undefined)
      this._updateDownloadStatusUI();
    else
      this._fetchTargetFileInfo(true);
  },

  placesNodeIconChanged: function DES_placesNodeIconChanged() {
    if (!this._dataItem)
      this._element.setAttribute("image", this._getIcon());
  },

  placesNodeTitleChanged: function DES_placesNodeTitleChanged() {
    // If there's a file path, we use the leaf name for the title.
    if (!this._dataItem && this.active && !this.getDownloadMetaData().filePath) {
      this._metaData = null;
      this._updateDisplayNameAndIcon();
    }
  },

  placesNodeAnnotationChanged: function DES_placesNodeAnnotationChanged(aAnnoName) {
    this._annotations.delete(aAnnoName);
    if (!this._dataItem && this.active) {
      if (aAnnoName == DOWNLOAD_META_DATA_ANNO) {
        let metaData = this.getDownloadMetaData();
        let annotatedMetaData = this._getAnnotatedMetaData();
        metaData.endTme = annotatedMetaData.endTime;
        if ("fileSize" in annotatedMetaData)
          metaData.fileSize = annotatedMetaData.fileSize;
        else
          delete metaData.fileSize;

        if (metaData.state != annotatedMetaData.state) {
          metaData.state = annotatedMetaData.state;
          if (this._element.selected)
            goUpdateDownloadCommands();
        }

        this._updateDownloadStatusUI();
      }
      else if (aAnnoName == DESTINATION_FILE_URI_ANNO) {
        let metaData = this.getDownloadMetaData();
        let targetFileURI = this._getAnnotation(DESTINATION_FILE_URI_ANNO);
        [metaData.filePath, metaData.fileName] =
            this._extractFilePathAndNameFromFileURI(targetFileURI);
        metaData.displayName = metaData.fileName;
        this._updateDisplayNameAndIcon();

        if (this._targetFileInfoFetched) {
          // This will also update the download commands if necessary.
          this._targetFileInfoFetched = false;
          this._fetchTargetFileInfo();
        }
      }
    }
  },

  /* DownloadView */
  onStateChange: function DES_onStateChange(aOldState) {
    let metaData = this.getDownloadMetaData();
    metaData.state = this.dataItem.state;
    if (aOldState != nsIDM.DOWNLOAD_FINISHED && aOldState != metaData.state) {
      // See comment in DVI_onStateChange in downloads.js (the panel-view)
      this._element.setAttribute("image", this._getIcon() + "&state=normal");
      metaData.fileSize = this._dataItem.maxBytes;
      if (this._targetFileInfoFetched) {
        this._targetFileInfoFetched = false;
        this._fetchTargetFileInfo();
      }
    }

    this._updateDownloadStatusUI();
    if (this._element.selected)
      goUpdateDownloadCommands();
    else
      goUpdateCommand("downloadsCmd_clearDownloads");
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

        if (this._targetFileInfoFetched)
          return this._targetFileExists;

        // If the target file information is not yet fetched,
        // temporarily assume that the file is in place.
        return this.getDownloadMetaData().state == nsIDM.DOWNLOAD_FINISHED;
      }
      case "downloadsCmd_show": {
        // TODO: Bug 827010 - Handle part-file asynchronously.
        if (this._dataItem &&
            this._dataItem.partFile && this._dataItem.partFile.exists())
          return true;

        if (this._targetFileInfoFetched)
          return this._targetFileExists;

        // If the target file information is not yet fetched,
        // temporarily assume that the file is in place.
        return this.getDownloadMetaData().state == nsIDM.DOWNLOAD_FINISHED;
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
    DownloadURL(this.downloadURI, this.getDownloadMetaData().fileName);
  },

  /* nsIController */
  doCommand: function DES_doCommand(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_open": {
        let file = this._dataItem ?
          this.dataItem.localFile :
          new FileUtils.File(this.getDownloadMetaData().filePath);

        DownloadsCommon.openDownloadedFile(file, null, window);
        break;
      }
      case "downloadsCmd_show": {
        if (this._dataItem) {
          this._dataItem.showLocalFile();
        }
        else {
          let file = new FileUtils.File(this.getDownloadMetaData().filePath);
          DownloadsCommon.showDownloadedFile(file);
        }
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
    return this.getDownloadMetaData().displayName.toLowerCase().contains(aTerm) ||
           this.downloadURI.toLowerCase().contains(aTerm);
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
        case nsIDM.DOWNLOAD_SCANNING:
          return "downloadsCmd_show";
        case nsIDM.DOWNLOAD_BLOCKED_PARENTAL:
        case nsIDM.DOWNLOAD_DIRTY:
        case nsIDM.DOWNLOAD_BLOCKED_POLICY:
          return "downloadsCmd_openReferrer";
      }
      return "";
    }
    let command = getDefaultCommandForState(this.getDownloadMetaData().state);
    if (command && this.isCommandEnabled(command))
      this.doCommand(command);
  },

  /**
   * At the first time an item is selected, we don't yet have
   * the target file information.  Thus the call to goUpdateDownloadCommands
   * in DPV_onSelect would result in best-guess enabled/disabled result.
   * That way we let the user perform command immediately. However, once
   * we have the target file information, we can update the commands
   * appropriately (_fetchTargetFileInfo() calls goUpdateDownloadCommands).
   */
  onSelect: function DES_onSelect() {
    if (!this.active)
      return;
    if (!this._targetFileInfoFetched)
      this._fetchTargetFileInfo();
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
  window.controllers.insertControllerAt(0, this);

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

  // Get the Download button out of the attention state since we're about to
  // view all downloads.
  DownloadsCommon.getIndicatorData(window).attention = false;

  // Make sure to unregister the view if the window is closed.
  window.addEventListener("unload", function() {
    window.controllers.removeController(this);
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
                         DOWNLOAD_META_DATA_ANNO ]) {
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
    }
    // The meta-data annotation has been added recently, so it's likely missing.
    if (!annotations.has(DOWNLOAD_META_DATA_ANNO)) {
      annotations.set(DOWNLOAD_META_DATA_ANNO, NOT_AVAILABLE);
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
    let shellsForURI = this._downloadElementsShellsForURI.get(downloadURI);
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
      // Bug 836271: The annotations for a url should be cached only when the
      // places node is available, i.e. when we know we we'd be notified for
      // annoation changes. 
      // Otherwise we may cache NOT_AVILABLE values first for a given session
      // download, and later use these NOT_AVILABLE values when a history
      // download for the same URL is added.
      let cachedAnnotations = aPlacesNode ? this._getAnnotationsFor(downloadURI) : null;
      let shell = new DownloadElementShell(aDataItem, aPlacesNode, cachedAnnotations);
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
    if (!aDocumentFragment) {
      this._ensureVisibleElementsAreActive();
      goUpdateCommand("downloadsCmd_clearDownloads");
    }
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

    if (this._lastSessionDownloadElement == aElement)
      this._lastSessionDownloadElement = aElement.previousSibling;

    this._richlistbox.removeChild(aElement);
    this._ensureVisibleElementsAreActive();
    goUpdateCommand("downloadsCmd_clearDownloads");
  },

  _removeHistoryDownloadFromView:
  function DPV__removeHistoryDownloadFromView(aPlacesNode) {
    let downloadURI = aPlacesNode.uri;
    let shellsForURI = this._downloadElementsShellsForURI.get(downloadURI);
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
    let shells = this._downloadElementsShellsForURI.get(aDataItem.uri);
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
      if (this._lastSessionDownloadElement == shell.element) {
        this._lastSessionDownloadElement = shell.element.previousSibling;
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

    let suppressOnSelect = this._richlistbox.suppressOnSelect;
    this._richlistbox.suppressOnSelect = true;
    try {
      // Remove the invalidated history downloads from the list and unset the
      // places node for data downloads.
      // Loop backwards since _removeHistoryDownloadFromView may removeChild().
      for (let i = this._richlistbox.childNodes.length - 1; i >= 0; --i) {
        let element = this._richlistbox.childNodes[i];
        if (element._shell.placesNode)
          this._removeHistoryDownloadFromView(element._shell.placesNode);
      }
    }
    finally {
      this._richlistbox.suppressOnSelect = suppressOnSelect;
    }

    if (aContainer.childCount > 0) {
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

      // _addDownloadData may not add new elements if there were already
      // data items in place.
      if (elementsToAppendFragment.firstChild) {
        this._appendDownloadsFragment(elementsToAppendFragment);
        this._ensureVisibleElementsAreActive();
      }
    }

    goUpdateDownloadCommands();
  },

  _appendDownloadsFragment: function DPV__appendDownloadsFragment(aDOMFragment) {
    // Workaround multiple reflows hang by removing the richlistbox
    // and adding it back when we're done.

    // Hack for bug 836283: reset xbl fields to their old values after the
    // binding is reattached to avoid breaking the selection state
    let xblFields = new Map();
    for (let [key, value] in Iterator(this._richlistbox)) {
      xblFields.set(key, value);
    }

    let parentNode = this._richlistbox.parentNode;
    let nextSibling = this._richlistbox.nextSibling;
    parentNode.removeChild(this._richlistbox);
    this._richlistbox.appendChild(aDOMFragment);
    parentNode.insertBefore(this._richlistbox, nextSibling);

    for (let [key, value] of xblFields) {
      this._richlistbox[key] = value;
    }
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

  supportsCommand: function DPV_supportsCommand(aCommand) {
    if (DOWNLOAD_VIEW_SUPPORTED_COMMANDS.indexOf(aCommand) != -1) {
      // The clear-downloads command may be performed by the toolbar-button,
      // which can be focused on OS X.  Thus enable this command even if the
      // richlistbox is not focused.
      // For other commands, be prudent and disable them unless the richlistview
      // is focused. It's important to make the decision here rather than in
      // isCommandEnabled.  Otherwise our controller may "steal" commands from
      // other controls in the window (see goUpdateCommand &
      // getControllerForCommand).
      if (document.activeElement == this._richlistbox ||
          aCommand == "downloadsCmd_clearDownloads") {
        return true;
      }
    }
    return false;
  },

  isCommandEnabled: function DPV_isCommandEnabled(aCommand) {
    switch (aCommand) {
      case "cmd_copy":
        return this._richlistbox.selectedItems.length > 0;
      case "cmd_selectAll":
        return true;
      case "cmd_paste":
        return this._canDownloadClipboardURL();
      case "downloadsCmd_clearDownloads":
        return this._canClearDownloads();
      default:
        return Array.every(this._richlistbox.selectedItems, function(element) {
          return element._shell.isCommandEnabled(aCommand);
        });
    }
  },

  _canClearDownloads: function DPV__canClearDownloads() {
    // Downloads can be cleared if there's at least one removeable download in
    // the list (either a history download or a completed session download).
    // Because history downloads are always removable and are listed after the
    // session downloads, check from bottom to top.
    for (let elt = this._richlistbox.lastChild; elt; elt = elt.previousSibling) {
      if (elt._shell.placesNode || !elt._shell.dataItem.inProgress)
        return true;
    }
    return false;
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
    DownloadURL(url, name);
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
        // There may be no selection or focus change as a result
        // of these change, and we want the command updated immediately.
        goUpdateCommand("downloadsCmd_clearDownloads");
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
    let state = element._shell.getDownloadMetaData().state;
    if (state !== undefined)
      contextMenu.setAttribute("state", state);
    else
      contextMenu.removeAttribute("state");

    return true;
  },

  onKeyPress: function DPV_onKeyPress(aEvent) {
    let selectedElements = this._richlistbox.selectedItems;
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
    if (selectedElements.length != 1)
      return;

    let element = selectedElements[0];
    if (element._shell)
      element._shell.doDefaultCommand();
  },

  onScroll: function DPV_onScroll() {
    this._ensureVisibleElementsAreActive();
  },

  onSelect: function DPV_onSelect() {
    goUpdateDownloadCommands();

    let selectedElements = this._richlistbox.selectedItems;
    for (let elt of selectedElements) {
      if (elt._shell)
        elt._shell.onSelect();
    }
  },

  onDragStart: function DPV_onDragStart(aEvent) {
    // TODO Bug 831358: Support d&d for multiple selection.
    // For now, we just drag the first element.
    let selectedItem = this._richlistbox.selectedItem;
    if (!selectedItem)
      return;

    let metaData = selectedItem._shell.getDownloadMetaData();
    if (!("filePath" in metaData))
      return;
    let file = new FileUtils.File(metaData.filePath);
    if (!file.exists())
      return;

    let dt = aEvent.dataTransfer;
    dt.mozSetDataAt("application/x-moz-file", file, 0);
    let url = Services.io.newFileURI(file).spec;
    dt.setData("text/uri-list", url);
    dt.setData("text/plain", url);
    dt.effectAllowed = "copyMove";
    dt.addElement(selectedItem);
  },

  onDragOver: function DPV_onDragOver(aEvent) {
    let types = aEvent.dataTransfer.types;
    if (types.contains("text/uri-list") ||
        types.contains("text/x-moz-url") ||
        types.contains("text/plain")) {
      aEvent.preventDefault();
    }
  },

  onDrop: function DPV_onDrop(aEvent) {
    let dt = aEvent.dataTransfer;
    // If dragged item is from our source, do not try to
    // redownload already downloaded file.
    if (dt.mozGetDataAt("application/x-moz-file", 0))
      return;

    let name = { };
    let url = Services.droppedLinkHandler.dropLink(aEvent, name);
    if (url)
      DownloadURL(url, name.value);
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

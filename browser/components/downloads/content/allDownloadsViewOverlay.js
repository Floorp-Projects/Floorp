/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsViewUI",
                                  "resource:///modules/DownloadsViewUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const nsIDM = Ci.nsIDownloadManager;

const DESTINATION_FILE_URI_ANNO  = "downloads/destinationFileURI";
const DOWNLOAD_META_DATA_ANNO    = "downloads/metaData";

const DOWNLOAD_VIEW_SUPPORTED_COMMANDS =
 ["cmd_delete", "cmd_copy", "cmd_paste", "cmd_selectAll",
  "downloadsCmd_pauseResume", "downloadsCmd_cancel", "downloadsCmd_unblock",
  "downloadsCmd_confirmBlock", "downloadsCmd_open", "downloadsCmd_show",
  "downloadsCmd_retry", "downloadsCmd_openReferrer", "downloadsCmd_clearDownloads"];

/**
 * Represents a download from the browser history. It implements part of the
 * interface of the Download object.
 *
 * @param aPlacesNode
 *        The Places node from which the history download should be initialized.
 */
function HistoryDownload(aPlacesNode) {
  // TODO (bug 829201): history downloads should get the referrer from Places.
  this.source = {
    url: aPlacesNode.uri,
  };
  this.target = {
    path: undefined,
    exists: false,
    size: undefined,
  };

  // In case this download cannot obtain its end time from the Places metadata,
  // use the time from the Places node, that is the start time of the download.
  this.endTime = aPlacesNode.time / 1000;
}

HistoryDownload.prototype = {
  /**
   * Pushes information from Places metadata into this object.
   */
  updateFromMetaData(metaData) {
    try {
      this.target.path = Cc["@mozilla.org/network/protocol;1?name=file"]
                           .getService(Ci.nsIFileProtocolHandler)
                           .getFileFromURLSpec(metaData.targetFileSpec).path;
    } catch (ex) {
      this.target.path = undefined;
    }

    if ("state" in metaData) {
      this.succeeded = metaData.state == nsIDM.DOWNLOAD_FINISHED;
      this.error = metaData.state == nsIDM.DOWNLOAD_FAILED
                   ? { message: "History download failed." }
                   : metaData.state == nsIDM.DOWNLOAD_BLOCKED_PARENTAL
                   ? { becauseBlockedByParentalControls: true }
                   : metaData.state == nsIDM.DOWNLOAD_DIRTY
                   ? { becauseBlockedByReputationCheck: true }
                   : null;
      this.canceled = metaData.state == nsIDM.DOWNLOAD_CANCELED ||
                      metaData.state == nsIDM.DOWNLOAD_PAUSED;
      this.endTime = metaData.endTime;

      // Normal history downloads are assumed to exist until the user interface
      // is refreshed, at which point these values may be updated.
      this.target.exists = true;
      this.target.size = metaData.fileSize;
    } else {
      // Metadata might be missing from a download that has started but hasn't
      // stopped already. Normally, this state is overridden with the one from
      // the corresponding in-progress session download. But if the browser is
      // terminated abruptly and additionally the file with information about
      // in-progress downloads is lost, we may end up using this state. We use
      // the failed state to allow the download to be restarted.
      //
      // On the other hand, if the download is missing the target file
      // annotation as well, it is just a very old one, and we can assume it
      // succeeded.
      this.succeeded = !this.target.path;
      this.error = this.target.path ? { message: "Unstarted download." } : null;
      this.canceled = false;

      // These properties may be updated if the user interface is refreshed.
      this.target.exists = false;
      this.target.size = undefined;
    }
  },

  /**
   * History downloads are never in progress.
   */
  stopped: true,

  /**
   * No percentage indication is shown for history downloads.
   */
  hasProgress: false,

  /**
   * History downloads cannot be restarted using their partial data, even if
   * they are indicated as paused in their Places metadata. The only way is to
   * use the information from a persisted session download, that will be shown
   * instead of the history download. In case this session download is not
   * available, we show the history download as canceled, not paused.
   */
  hasPartialData: false,

  /**
   * This method mimicks the "start" method of session downloads, and is called
   * when the user retries a history download.
   *
   * At present, we always ask the user for a new target path when retrying a
   * history download. In the future we may consider reusing the known target
   * path if the folder still exists and the file name is not already used,
   * except when the user preferences indicate that the target path should be
   * requested every time a new download is started.
   */
  start() {
    let browserWin = RecentWindow.getMostRecentBrowserWindow();
    let initiatingDoc = browserWin ? browserWin.document : document;

    // Do not suggest a file name if we don't know the original target.
    let leafName = this.target.path ? OS.Path.basename(this.target.path) : null;
    DownloadURL(this.source.url, leafName, initiatingDoc);

    return Promise.resolve();
  },

  /**
   * This method mimicks the "refresh" method of session downloads, except that
   * it cannot notify that the data changed to the Downloads View.
   */
  refresh: Task.async(function* () {
    try {
      this.target.size = (yield OS.File.stat(this.target.path)).size;
      this.target.exists = true;
    } catch (ex) {
      // We keep the known file size from the metadata, if any.
      this.target.exists = false;
    }
  }),
};

/**
 * A download element shell is responsible for handling the commands and the
 * displayed data for a single download view element.
 *
 * The shell may contain a session download, a history download, or both.  When
 * both a history and a session download are present, the session download gets
 * priority and its information is displayed.
 *
 * On construction, a new richlistitem is created, and can be accessed through
 * the |element| getter. The shell doesn't insert the item in a richlistbox, the
 * caller must do it and remove the element when it's no longer needed.
 *
 * The caller is also responsible for forwarding status notifications for
 * session downloads, calling the onStateChanged and onChanged methods.
 *
 * @param [optional] aSessionDownload
 *        The session download, required if aHistoryDownload is not set.
 * @param [optional] aHistoryDownload
 *        The history download, required if aSessionDownload is not set.
 */
function HistoryDownloadElementShell(aSessionDownload, aHistoryDownload) {
  this.element = document.createElement("richlistitem");
  this.element._shell = this;

  this.element.classList.add("download");
  this.element.classList.add("download-state");

  if (aSessionDownload) {
    this.sessionDownload = aSessionDownload;
  }
  if (aHistoryDownload) {
    this.historyDownload = aHistoryDownload;
  }
}

HistoryDownloadElementShell.prototype = {
  __proto__: DownloadsViewUI.DownloadElementShell.prototype,

  /**
   * Manages the "active" state of the shell.  By default all the shells without
   * a session download are inactive, thus their UI is not updated.  They must
   * be activated when entering the visible area.  Session downloads are always
   * active.
   */
  ensureActive() {
    if (!this._active) {
      this._active = true;
      this.element.setAttribute("active", true);
      this._updateUI();
    }
  },
  get active() {
    return !!this._active;
  },

  /**
   * Overrides the base getter to return the Download or HistoryDownload object
   * for displaying information and executing commands in the user interface.
   */
  get download() {
    return this._sessionDownload || this._historyDownload;
  },

  _sessionDownload: null,
  get sessionDownload() {
    return this._sessionDownload;
  },
  set sessionDownload(aValue) {
    if (this._sessionDownload != aValue) {
      if (!aValue && !this._historyDownload) {
        throw new Error("Should always have either a Download or a HistoryDownload");
      }

      this._sessionDownload = aValue;

      this.ensureActive();
      this._updateUI();
    }
    return aValue;
  },

  _historyDownload: null,
  get historyDownload() {
    return this._historyDownload;
  },
  set historyDownload(aValue) {
    if (this._historyDownload != aValue) {
      if (!aValue && !this._sessionDownload) {
        throw new Error("Should always have either a Download or a HistoryDownload");
      }

      this._historyDownload = aValue;

      // We don't need to update the UI if we had a session data item, because
      // the places information isn't used in this case.
      if (!this._sessionDownload) {
        this._updateUI();
      }
    }
    return aValue;
  },

  _updateUI() {
    // There is nothing to do if the item has always been invisible.
    if (!this.active) {
      return;
    }

    // Since the state changed, we may need to check the target file again.
    this._targetFileChecked = false;

    this._updateState();
  },

  get statusTextAndTip() {
    let status = this.rawStatusTextAndTip;

    // The base object would show extended progress information in the tooltip,
    // but we move this to the main view and never display a tooltip.
    if (!this.download.stopped) {
      status.text = status.tip;
    }
    status.tip = "";

    return status;
  },

  onStateChanged() {
    this.element.setAttribute("image", this.image);
    this.element.setAttribute("state",
                              DownloadsCommon.stateOfDownload(this.download));

    if (this.element.selected) {
      goUpdateDownloadCommands();
    } else {
      goUpdateCommand("downloadsCmd_clearDownloads");
    }
  },

  onChanged() {
    // This cannot be placed within onStateChanged because
    // when a download goes from hasBlockedData to !hasBlockedData
    // it will still remain in the same state.
    this.element.classList.toggle("temporary-block",
                                  !!this.download.hasBlockedData);
    this._updateProgress();
  },

  /* nsIController */
  isCommandEnabled(aCommand) {
    // The only valid command for inactive elements is cmd_delete.
    if (!this.active && aCommand != "cmd_delete") {
      return false;
    }
    switch (aCommand) {
      case "downloadsCmd_open":
        // This property is false if the download did not succeed.
        return this.download.target.exists;
      case "downloadsCmd_show":
        // TODO: Bug 827010 - Handle part-file asynchronously.
        if (this._sessionDownload && this.download.target.partFilePath) {
          let partFile = new FileUtils.File(this.download.target.partFilePath);
          if (partFile.exists()) {
            return true;
          }
        }

        // This property is false if the download did not succeed.
        return this.download.target.exists;
      case "downloadsCmd_pauseResume":
        return this.download.hasPartialData && !this.download.error;
      case "downloadsCmd_retry":
        return this.download.canceled || this.download.error;
      case "downloadsCmd_openReferrer":
        return !!this.download.source.referrer;
      case "cmd_delete":
        // We don't want in-progress downloads to be removed accidentally.
        return this.download.stopped;
      case "downloadsCmd_cancel":
        return !!this._sessionDownload;
      case "downloadsCmd_confirmBlock":
      case "downloadsCmd_unblock":
        return this.download.hasBlockedData;
    }
    return false;
  },

  /* nsIController */
  doCommand(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_open": {
        let file = new FileUtils.File(this.download.target.path);
        DownloadsCommon.openDownloadedFile(file, null, window);
        break;
      }
      case "downloadsCmd_show": {
        let file = new FileUtils.File(this.download.target.path);
        DownloadsCommon.showDownloadedFile(file);
        break;
      }
      case "downloadsCmd_openReferrer": {
        openURL(this.download.source.referrer);
        break;
      }
      case "downloadsCmd_cancel": {
        this.download.cancel().catch(() => {});
        this.download.removePartialData().catch(Cu.reportError);
        break;
      }
      case "cmd_delete": {
        if (this._sessionDownload) {
          DownloadsCommon.removeAndFinalizeDownload(this.download);
        }
        if (this._historyDownload) {
          let uri = NetUtil.newURI(this.download.source.url);
          PlacesUtils.bhistory.removePage(uri);
        }
        break;
      }
      case "downloadsCmd_retry": {
        // Errors when retrying are already reported as download failures.
        this.download.start().catch(() => {});
        break;
      }
      case "downloadsCmd_pauseResume": {
        // This command is only enabled for session downloads.
        if (this.download.stopped) {
          this.download.start();
        } else {
          this.download.cancel();
        }
        break;
      }
      case "downloadsCmd_unblock": {
        DownloadsCommon.confirmUnblockDownload(DownloadsCommon.BLOCK_VERDICT_MALWARE,
                                               window).then((confirmed) => {
          if (confirmed) {
            return this.download.unblock();
          }
        }).catch(Cu.reportError);
        break;
      }
      case "downloadsCmd_confirmBlock": {
        this.download.confirmBlock().catch(Cu.reportError);
        break;
      }
    }
  },

  // Returns whether or not the download handled by this shell should
  // show up in the search results for the given term.  Both the display
  // name for the download and the url are searched.
  matchesSearchTerm(aTerm) {
    if (!aTerm) {
      return true;
    }
    aTerm = aTerm.toLowerCase();
    return this.displayName.toLowerCase().includes(aTerm) ||
           this.download.source.url.toLowerCase().includes(aTerm);
  },

  // Handles return keypress on the element (the keypress listener is
  // set in the DownloadsPlacesView object).
  doDefaultCommand() {
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
    let state = DownloadsCommon.stateOfDownload(this.download);
    let command = getDefaultCommandForState(state);
    if (command && this.isCommandEnabled(command)) {
      this.doCommand(command);
    }
  },

  /**
   * This method is called by the outer download view, after the controller
   * commands have already been updated. In case we did not check for the
   * existence of the target file already, we can do it now and then update
   * the commands as needed.
   */
  onSelect() {
    if (!this.active) {
      return;
    }

    // If this is a history download for which no target file information is
    // available, we cannot retrieve information about the target file.
    if (!this.download.target.path) {
      return;
    }

    // Start checking for existence.  This may be done twice if onSelect is
    // called again before the information is collected.
    if (!this._targetFileChecked) {
      this._checkTargetFileOnSelect().catch(Cu.reportError);
    }
  },

  _checkTargetFileOnSelect: Task.async(function* () {
    try {
      yield this.download.refresh();
    } finally {
      // Do not try to check for existence again if this failed once.
      this._targetFileChecked = true;
    }

    // Update the commands only if the element is still selected.
    if (this.element.selected) {
      goUpdateDownloadCommands();
    }

    // Ensure the interface has been updated based on the new values. We need to
    // do this because history downloads can't trigger update notifications.
    this._updateProgress();
  }),
};

/**
 * A Downloads Places View is a places view designed to show a places query
 * for history downloads alongside the session downloads.
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
  this._viewItemsForDownloads = new WeakMap();

  // Points to the last session download element. We keep track of this
  // in order to keep all session downloads above past downloads.
  this._lastSessionDownloadElement = null;

  this._searchTerm = "";

  this._active = aActive;

  // Register as a downloads view. The places data will be initialized by
  // the places setter.
  this._initiallySelectedElement = null;
  this._downloadsData = DownloadsCommon.getData(window.opener || window);
  this._downloadsData.addView(this);

  // Get the Download button out of the attention state since we're about to
  // view all downloads.
  DownloadsCommon.getIndicatorData(window).attention = false;

  // Make sure to unregister the view if the window is closed.
  window.addEventListener("unload", () => {
    window.controllers.removeController(this);
    this._downloadsData.removeView(this);
    this.result = null;
  }, true);
  // Resizing the window may change items visibility.
  window.addEventListener("resize", () => {
    this._ensureVisibleElementsAreActive();
  }, true);
}

DownloadsPlacesView.prototype = {
  get associatedElement() {
    return this._richlistbox;
  },

  get active() {
    return this._active;
  },
  set active(val) {
    this._active = val;
    if (this._active)
      this._ensureVisibleElementsAreActive();
    return this._active;
  },

  /**
   * This cache exists in order to optimize the load of the Downloads View, when
   * Places annotations for history downloads must be read. In fact, annotations
   * are stored in a single table, and reading all of them at once is much more
   * efficient than an individual query.
   *
   * When this property is first requested, it reads the annotations for all the
   * history downloads and stores them indefinitely.
   *
   * The historical annotations are not expected to change for the duration of
   * the session, except in the case where a session download is running for the
   * same URI as a history download. To ensure we don't use stale data, URIs
   * corresponding to session downloads are permanently removed from the cache.
   * This is a very small mumber compared to history downloads.
   *
   * This property returns a Map from each download source URI found in Places
   * annotations to an object with the format:
   *
   * { targetFileSpec, state, endTime, fileSize, ... }
   *
   * The targetFileSpec property is the value of "downloads/destinationFileURI",
   * while the other properties are taken from "downloads/metaData". Any of the
   * properties may be missing from the object.
   */
  get _cachedPlacesMetaData() {
    if (!this.__cachedPlacesMetaData) {
      this.__cachedPlacesMetaData = new Map();

      // Read the metadata annotations first, but ignore invalid JSON.
      for (let result of PlacesUtils.annotations.getAnnotationsWithName(
                                                 DOWNLOAD_META_DATA_ANNO)) {
        try {
          this.__cachedPlacesMetaData.set(result.uri.spec,
                                          JSON.parse(result.annotationValue));
        } catch (ex) {}
      }

      // Add the target file annotations to the metadata.
      for (let result of PlacesUtils.annotations.getAnnotationsWithName(
                                                 DESTINATION_FILE_URI_ANNO)) {
        let metaData = this.__cachedPlacesMetaData.get(result.uri.spec);
        if (!metaData) {
          metaData = {};
          this.__cachedPlacesMetaData.set(result.uri.spec, metaData);
        }
        metaData.targetFileSpec = result.annotationValue;
      }
    }

    return this.__cachedPlacesMetaData;
  },
  __cachedPlacesMetaData: null,

  /**
   * Reads current metadata from Places annotations for the specified URI, and
   * returns an object with the format:
   *
   * { targetFileSpec, state, endTime, fileSize, ... }
   *
   * The targetFileSpec property is the value of "downloads/destinationFileURI",
   * while the other properties are taken from "downloads/metaData". Any of the
   * properties may be missing from the object.
   */
  _getPlacesMetaDataFor(spec) {
    let metaData = {};

    try {
      let uri = NetUtil.newURI(spec);
      try {
        metaData = JSON.parse(PlacesUtils.annotations.getPageAnnotation(
                                          uri, DOWNLOAD_META_DATA_ANNO));
      } catch (ex) {}
      metaData.targetFileSpec = PlacesUtils.annotations.getPageAnnotation(
                                            uri, DESTINATION_FILE_URI_ANNO);
    } catch (ex) {}

    return metaData;
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
   * @param [optional] sessionDownload
   *        A Download object, or null for history downloads.
   * @param [optional] aPlacesNode
   *        The Places node for a history download, or null for session downloads.
   * @param [optional] aNewest
   *        @see onDownloadAdded. Ignored for history downloads.
   * @param [optional] aDocumentFragment
   *        To speed up the appending of multiple elements to the end of the
   *        list which are coming in a single batch (i.e. invalidateContainer),
   *        a document fragment may be passed to which the new elements would
   *        be appended. It's the caller's job to ensure the fragment is merged
   *        to the richlistbox at the end.
   */
  _addDownloadData(sessionDownload, aPlacesNode, aNewest = false,
                   aDocumentFragment = null) {
    let downloadURI = aPlacesNode ? aPlacesNode.uri
                                  : sessionDownload.source.url;
    let shellsForURI = this._downloadElementsShellsForURI.get(downloadURI);
    if (!shellsForURI) {
      shellsForURI = new Set();
      this._downloadElementsShellsForURI.set(downloadURI, shellsForURI);
    }

    // When a session download is attached to a shell, we ensure not to keep
    // stale metadata around for the corresponding history download. This
    // prevents stale state from being used if the view is rebuilt.
    //
    // Note that we will eagerly load the data in the cache at this point, even
    // if we have seen no history download. The case where no history download
    // will appear at all is rare enough in normal usage, so we can apply this
    // simpler solution rather than keeping a list of cache items to ignore.
    if (sessionDownload) {
      this._cachedPlacesMetaData.delete(sessionDownload.source.url);
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
    // 2) There are multiple shells, indicating multiple downloads for
    //    the same download uri are running. In this case we create
    //    another shell for the download (so we have one shell for each data
    //    item).
    //
    // Note: If a cancelled session download is already in the list, and the
    // download is retried, onDownloadAdded is called again for the same
    // data item. Thus, we also check that we make sure we don't have a view item
    // already.
    if (!shouldCreateShell &&
        sessionDownload && !this._viewItemsForDownloads.has(sessionDownload)) {
      // If there's a past-download-only shell for this download-uri with no
      // associated data item, use it for the new data item. Otherwise, go ahead
      // and create another shell.
      shouldCreateShell = true;
      for (let shell of shellsForURI) {
        if (!shell.sessionDownload) {
          shouldCreateShell = false;
          shell.sessionDownload = sessionDownload;
          newOrUpdatedShell = shell;
          this._viewItemsForDownloads.set(sessionDownload, shell);
          break;
        }
      }
    }

    if (shouldCreateShell) {
      // If we are adding a new history download here, it means there is no
      // associated session download, thus we must read the Places metadata,
      // because it will not be obscured by the session download.
      let historyDownload = null;
      if (aPlacesNode) {
        let metaData = this._cachedPlacesMetaData.get(aPlacesNode.uri) ||
                       this._getPlacesMetaDataFor(aPlacesNode.uri);
        historyDownload = new HistoryDownload(aPlacesNode);
        historyDownload.updateFromMetaData(metaData);
      }
      let shell = new HistoryDownloadElementShell(sessionDownload,
                                                  historyDownload);
      shell.element._placesNode = aPlacesNode;
      newOrUpdatedShell = shell;
      shellsForURI.add(shell);
      if (sessionDownload) {
        this._viewItemsForDownloads.set(sessionDownload, shell);
      }
    } else if (aPlacesNode) {
      // We are updating information for a history download for which we have
      // at least one download element shell already. There are two cases:
      // 1) There are one or more download element shells for this source URI,
      //    each with an associated session download. We update the Places node
      //    because we may need it later, but we don't need to read the Places
      //    metadata until the last session download is removed.
      // 2) Occasionally, we may receive a duplicate notification for a history
      //    download with no associated session download. We have exactly one
      //    download element shell in this case, but the metdata cannot have
      //    changed, just the reference to the Places node object is different.
      // So, we update all the node references and keep the metadata intact.
      for (let shell of shellsForURI) {
        if (!shell.historyDownload) {
          // Create the element to host the metadata when needed.
          shell.historyDownload = new HistoryDownload(aPlacesNode);
        }
        shell.element._placesNode = aPlacesNode;
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
      } else if (sessionDownload) {
        let before = this._lastSessionDownloadElement ?
          this._lastSessionDownloadElement.nextSibling : this._richlistbox.firstChild;
        this._richlistbox.insertBefore(newOrUpdatedShell.element, before);
        this._lastSessionDownloadElement = newOrUpdatedShell.element;
      } else {
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

  _removeElement(aElement) {
    // If the element was selected exclusively, select its next
    // sibling first, if not, try for previous sibling, if any.
    if ((aElement.nextSibling || aElement.previousSibling) &&
        this._richlistbox.selectedItems &&
        this._richlistbox.selectedItems.length == 1 &&
        this._richlistbox.selectedItems[0] == aElement) {
      this._richlistbox.selectItem(aElement.nextSibling ||
                                   aElement.previousSibling);
    }

    if (this._lastSessionDownloadElement == aElement) {
      this._lastSessionDownloadElement = aElement.previousSibling;
    }

    this._richlistbox.removeItemFromSelection(aElement);
    this._richlistbox.removeChild(aElement);
    this._ensureVisibleElementsAreActive();
    goUpdateCommand("downloadsCmd_clearDownloads");
  },

  _removeHistoryDownloadFromView(aPlacesNode) {
    let downloadURI = aPlacesNode.uri;
    let shellsForURI = this._downloadElementsShellsForURI.get(downloadURI);
    if (shellsForURI) {
      for (let shell of shellsForURI) {
        if (shell.sessionDownload) {
          shell.historyDownload = null;
        } else {
          this._removeElement(shell.element);
          shellsForURI.delete(shell);
          if (shellsForURI.size == 0)
            this._downloadElementsShellsForURI.delete(downloadURI);
        }
      }
    }
  },

  _removeSessionDownloadFromView(download) {
    let shells = this._downloadElementsShellsForURI
                     .get(download.source.url);
    if (shells.size == 0) {
      throw new Error("Should have had at leaat one shell for this uri");
    }

    let shell = this._viewItemsForDownloads.get(download);
    if (!shells.has(shell)) {
      throw new Error("Missing download element shell in shells list for url");
    }

    // If there's more than one item for this download uri, we can let the
    // view item for this this particular data item go away.
    // If there's only one item for this download uri, we should only
    // keep it if it is associated with a history download.
    if (shells.size > 1 || !shell.historyDownload) {
      this._removeElement(shell.element);
      shells.delete(shell);
      if (shells.size == 0) {
        this._downloadElementsShellsForURI.delete(download.source.url);
      }
    } else {
      // We have one download element shell containing both a session download
      // and a history download, and we are now removing the session download.
      // Previously, we did not use the Places metadata because it was obscured
      // by the session download. Since this is no longer the case, we have to
      // read the latest metadata before removing the session download.
      let url = shell.historyDownload.source.url;
      let metaData = this._getPlacesMetaDataFor(url);
      shell.historyDownload.updateFromMetaData(metaData);
      shell.sessionDownload = null;
      // Move it below the session-download items;
      if (this._lastSessionDownloadElement == shell.element) {
        this._lastSessionDownloadElement = shell.element.previousSibling;
      } else {
        let before = this._lastSessionDownloadElement ?
          this._lastSessionDownloadElement.nextSibling : this._richlistbox.firstChild;
        this._richlistbox.insertBefore(shell.element, before);
      }
    }
  },

  _ensureVisibleElementsAreActive() {
    if (!this.active || this._ensureVisibleTimer ||
        !this._richlistbox.firstChild) {
      return;
    }

    this._ensureVisibleTimer = setTimeout(() => {
      delete this._ensureVisibleTimer;
      if (!this._richlistbox.firstChild) {
        return;
      }

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
          if (!lastVisibleNode) {
            lastVisibleNode = node;
          }
        }
      }

      // Also activate the first invisible nodes in both boundaries (that is,
      // above and below the visible area) to ensure proper keyboard navigation
      // in both directions.
      let nodeBelowVisibleArea = lastVisibleNode && lastVisibleNode.nextSibling;
      if (nodeBelowVisibleArea && nodeBelowVisibleArea._shell) {
        nodeBelowVisibleArea._shell.ensureActive();
      }

      let nodeAboveVisibleArea = firstVisibleNode &&
                                 firstVisibleNode.previousSibling;
      if (nodeAboveVisibleArea && nodeAboveVisibleArea._shell) {
        nodeAboveVisibleArea._shell.ensureActive();
      }
    }, 10);
  },

  _place: "",
  get place() {
    return this._place;
  },
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
    if (!queries.value.length) {
      queries.value = [history.getNewQuery()];
    }

    let result = history.executeQueries(queries.value, queries.value.length,
                                        options.value);
    result.addObserver(this, false);
    return val;
  },

  _result: null,
  get result() {
    return this._result;
  },
  set result(val) {
    if (this._result == val) {
      return val;
    }

    if (this._result) {
      this._result.removeObserver(this);
      this._resultNode.containerOpen = false;
    }

    if (val) {
      this._result = val;
      this._resultNode = val.root;
      this._resultNode.containerOpen = true;
      this._ensureInitialSelection();
    } else {
      delete this._resultNode;
      delete this._result;
    }

    return val;
  },

  get selectedNodes() {
    return [for (element of this._richlistbox.selectedItems)
            if (element._placesNode)
            element._placesNode];
  },

  get selectedNode() {
    let selectedNodes = this.selectedNodes;
    return selectedNodes.length == 1 ? selectedNodes[0] : null;
  },

  get hasSelection() {
    return this.selectedNodes.length > 0;
  },

  containerStateChanged(aNode, aOldState, aNewState) {
    this.invalidateContainer(aNode)
  },

  invalidateContainer(aContainer) {
    if (aContainer != this._resultNode) {
      throw new Error("Unexpected container node");
    }
    if (!aContainer.containerOpen) {
      throw new Error("Root container for the downloads query cannot be closed");
    }

    let suppressOnSelect = this._richlistbox.suppressOnSelect;
    this._richlistbox.suppressOnSelect = true;
    try {
      // Remove the invalidated history downloads from the list and unset the
      // places node for data downloads.
      // Loop backwards since _removeHistoryDownloadFromView may removeChild().
      for (let i = this._richlistbox.childNodes.length - 1; i >= 0; --i) {
        let element = this._richlistbox.childNodes[i];
        if (element._placesNode) {
          this._removeHistoryDownloadFromView(element._placesNode);
        }
      }
    } finally {
      this._richlistbox.suppressOnSelect = suppressOnSelect;
    }

    if (aContainer.childCount > 0) {
      let elementsToAppendFragment = document.createDocumentFragment();
      for (let i = 0; i < aContainer.childCount; i++) {
        try {
          this._addDownloadData(null, aContainer.getChild(i), false,
                                elementsToAppendFragment);
        } catch (ex) {
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

  _appendDownloadsFragment(aDOMFragment) {
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

  nodeInserted(aParent, aPlacesNode) {
    this._addDownloadData(null, aPlacesNode);
  },

  nodeRemoved(aParent, aPlacesNode, aOldIndex) {
    this._removeHistoryDownloadFromView(aPlacesNode);
  },

  nodeAnnotationChanged() {},
  nodeIconChanged() {},
  nodeTitleChanged() {},
  nodeKeywordChanged() {},
  nodeDateAddedChanged() {},
  nodeLastModifiedChanged() {},
  nodeHistoryDetailsChanged() {},
  nodeTagsChanged() {},
  sortingChanged() {},
  nodeMoved() {},
  nodeURIChanged() {},
  batching() {},

  get controller() {
    return this._richlistbox.controller;
  },

  get searchTerm() {
    return this._searchTerm;
  },
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
  _ensureInitialSelection() {
    // Either they're both null, or the selection has not changed in between.
    if (this._richlistbox.selectedItem == this._initiallySelectedElement) {
      let firstDownloadElement = this._richlistbox.firstChild;
      if (firstDownloadElement != this._initiallySelectedElement) {
        // We may be called before _ensureVisibleElementsAreActive,
        // or before the download binding is attached. Therefore, ensure the
        // first item is activated, and pass the item to the richlistbox
        // setters only at a point we know for sure the binding is attached.
        firstDownloadElement._shell.ensureActive();
        Services.tm.mainThread.dispatch(() => {
          this._richlistbox.selectedItem = firstDownloadElement;
          this._richlistbox.currentItem = firstDownloadElement;
          this._initiallySelectedElement = firstDownloadElement;
        }, Ci.nsIThread.DISPATCH_NORMAL);
      }
    }
  },

  onDataLoadStarting() {},
  onDataLoadCompleted() {
    this._ensureInitialSelection();
  },

  onDownloadAdded(download, newest) {
    this._addDownloadData(download, null, newest);
  },

  onDownloadStateChanged(download) {
    this._viewItemsForDownloads.get(download).onStateChanged();
  },

  onDownloadChanged(download) {
    this._viewItemsForDownloads.get(download).onChanged();
  },

  onDownloadRemoved(download) {
    this._removeSessionDownloadFromView(download);
  },

  supportsCommand(aCommand) {
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

  isCommandEnabled(aCommand) {
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
        return Array.every(this._richlistbox.selectedItems,
                           element => element._shell.isCommandEnabled(aCommand));
    }
  },

  _canClearDownloads() {
    // Downloads can be cleared if there's at least one removable download in
    // the list (either a history download or a completed session download).
    // Because history downloads are always removable and are listed after the
    // session downloads, check from bottom to top.
    for (let elt = this._richlistbox.lastChild; elt; elt = elt.previousSibling) {
      // Stopped, paused, and failed downloads with partial data are removed.
      let download = elt._shell.download;
      if (download.stopped && !(download.canceled && download.hasPartialData)) {
        return true;
      }
    }
    return false;
  },

  _copySelectedDownloadsToClipboard() {
    let urls = [for (element of this._richlistbox.selectedItems)
                element._shell.download.source.url];

    Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper)
      .copyString(urls.join("\n"));
  },

  _getURLFromClipboardData() {
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
      if (url) {
        return [NetUtil.newURI(url, null, null).spec, name];
      }
    } catch (ex) {}

    return ["", ""];
  },

  _canDownloadClipboardURL() {
    let [url, name] = this._getURLFromClipboardData();
    return url != "";
  },

  _downloadURLFromClipboard() {
    let [url, name] = this._getURLFromClipboardData();
    let browserWin = RecentWindow.getMostRecentBrowserWindow();
    let initiatingDoc = browserWin ? browserWin.document : document;
    DownloadURL(url, name, initiatingDoc);
  },

  doCommand(aCommand) {
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
        this._downloadsData.removeFinished();
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
        // Slicing the array to get a freezed list of selected items. Otherwise,
        // the selectedItems array is live and doCommand may alter the selection
        // while we are trying to do one particular action, like removing items
        // from history.
        let selectedElements = this._richlistbox.selectedItems.slice();
        for (let element of selectedElements) {
          element._shell.doCommand(aCommand);
        }
      }
    }
  },

  onEvent() {},

  onContextMenu(aEvent) {
    let element = this._richlistbox.selectedItem;
    if (!element || !element._shell) {
      return false;
    }

    // Set the state attribute so that only the appropriate items are displayed.
    let contextMenu = document.getElementById("downloadsContextMenu");
    let download = element._shell.download;
    contextMenu.setAttribute("state",
                             DownloadsCommon.stateOfDownload(download));
    contextMenu.classList.toggle("temporary-block",
                                 !!download.hasBlockedData);

    if (!download.stopped) {
      // The hasPartialData property of a download may change at any time after
      // it has started, so ensure we update the related command now.
      goUpdateCommand("downloadsCmd_pauseResume");
    }

    return true;
  },

  onKeyPress(aEvent) {
    let selectedElements = this._richlistbox.selectedItems;
    if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
      // In the content tree, opening bookmarks by pressing return is only
      // supported when a single item is selected. To be consistent, do the
      // same here.
      if (selectedElements.length == 1) {
        let element = selectedElements[0];
        if (element._shell) {
          element._shell.doDefaultCommand();
        }
      }
    }
    else if (aEvent.charCode == " ".charCodeAt(0)) {
      // Pause/Resume every selected download
      for (let element of selectedElements) {
        if (element._shell.isCommandEnabled("downloadsCmd_pauseResume")) {
          element._shell.doCommand("downloadsCmd_pauseResume");
        }
      }
    }
  },

  onDoubleClick(aEvent) {
    if (aEvent.button != 0) {
      return;
    }

    let selectedElements = this._richlistbox.selectedItems;
    if (selectedElements.length != 1) {
      return;
    }

    let element = selectedElements[0];
    if (element._shell) {
      element._shell.doDefaultCommand();
    }
  },

  onScroll() {
    this._ensureVisibleElementsAreActive();
  },

  onSelect() {
    goUpdateDownloadCommands();

    let selectedElements = this._richlistbox.selectedItems;
    for (let elt of selectedElements) {
      if (elt._shell) {
        elt._shell.onSelect();
      }
    }
  },

  onDragStart(aEvent) {
    // TODO Bug 831358: Support d&d for multiple selection.
    // For now, we just drag the first element.
    let selectedItem = this._richlistbox.selectedItem;
    if (!selectedItem) {
      return;
    }

    let targetPath = selectedItem._shell.download.target.path;
    if (!targetPath) {
      return;
    }

    // We must check for existence synchronously because this is a DOM event.
    let file = new FileUtils.File(targetPath);
    if (!file.exists()) {
      return;
    }

    let dt = aEvent.dataTransfer;
    dt.mozSetDataAt("application/x-moz-file", file, 0);
    let url = Services.io.newFileURI(file).spec;
    dt.setData("text/uri-list", url);
    dt.setData("text/plain", url);
    dt.effectAllowed = "copyMove";
    dt.addElement(selectedItem);
  },

  onDragOver(aEvent) {
    let types = aEvent.dataTransfer.types;
    if (types.contains("text/uri-list") ||
        types.contains("text/x-moz-url") ||
        types.contains("text/plain")) {
      aEvent.preventDefault();
    }
  },

  onDrop(aEvent) {
    let dt = aEvent.dataTransfer;
    // If dragged item is from our source, do not try to
    // redownload already downloaded file.
    if (dt.mozGetDataAt("application/x-moz-file", 0)) {
      return;
    }

    let name = {};
    let url = Services.droppedLinkHandler.dropLink(aEvent, name);
    if (url) {
      let browserWin = RecentWindow.getMostRecentBrowserWindow();
      let initiatingDoc = browserWin ? browserWin.document : document;
      DownloadURL(url, name.value, initiatingDoc);
    }
  },
};

for (let methodName of ["load", "applyFilter", "selectNode", "selectItems"]) {
  DownloadsPlacesView.prototype[methodName] = function () {
    throw new Error("|" + methodName +
                    "| is not implemented by the downloads view.");
  }
}

function goUpdateDownloadCommands() {
  for (let command of DOWNLOAD_VIEW_SUPPORTED_COMMANDS) {
    goUpdateCommand(command);
  }
}

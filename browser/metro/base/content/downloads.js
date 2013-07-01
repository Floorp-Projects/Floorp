// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const URI_GENERIC_ICON_DOWNLOAD = "chrome://browser/skin/images/alert-downloads-30.png";

var Downloads = {
  _inited: false,
  _progressAlert: null,

  get manager() {
    return Cc["@mozilla.org/download-manager;1"]
             .getService(Ci.nsIDownloadManager);
  },

  _getReferrerOrSource: function dh__getReferrerOrSource(aDownload) {
    return aDownload.referrer.spec || aDownload.source.spec;
  },

  _getLocalFile: function dh__getLocalFile(aFileURI) {
    // XXX it's possible that using a null char-set here is bad
    let spec = ('string' == typeof aFileURI) ? aFileURI : aFileURI.spec;
    let fileUrl;
    try {
      fileUrl = Services.io.newURI(spec, null, null).QueryInterface(Ci.nsIFileURL);
    } catch (ex) {
      Util.dumpLn("_getLocalFile: Caught exception creating newURI from file spec: "+aFileURI.spec+": " + ex.message);
      return;
    }
    return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
  },

  init: function dh_init() {
    if (this._inited)
      return;

    this._inited = true;

    Services.obs.addObserver(this, "dl-start", true);
    Services.obs.addObserver(this, "dl-done", true);
  },

  uninit: function dh_uninit() {
    if (this._inited) {
      Services.obs.removeObserver(this, "dl-start");
      Services.obs.removeObserver(this, "dl-done");
    }
  },

  openDownload: function dh_openDownload(aDownload) {
    // expects xul item
    let id = aDownload.getAttribute("downloadId");
    let download = this.manager.getDownload(id);
    let fileURI = download.target;

    if (!(fileURI && fileURI.spec)) {
      Util.dumpLn("Cant open download "+id+", fileURI is invalid");
      return;
    }

    let file = this._getLocalFile(fileURI);
    try {
      file && file.launch();
    } catch (ex) {
      Util.dumpLn("Failed to open download, with id: "+id+", download target URI spec: " + fileURI.spec);
      Util.dumpLn("Failed download source:"+(aDownload.source && aDownload.source.spec));
    }
  },

  removeDownload: function dh_removeDownload(aDownload) {
    // aDownload is the XUL element here,
    // and .target currently returns the target attribute (string value)
    let id = aDownload.getAttribute("downloadId");
    let download = this.manager.getDownload(id);

    if (download) {
      // TODO <sfoster> add class/mark element for removal transition?
      this.manager.removeDownload(id);
    }
  },

  cancelDownload: function dh_cancelDownload(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    let download = this.manager.getDownload(id);
    this.manager.cancelDownload(id);

    let fileURI = download.target;

    if (!(fileURI && fileURI.spec)) {
      Util.dumpLn("Cant remove download file for: "+id+", fileURI is invalid");
      return;
    }

    let file = this._getLocalFile(fileURI);
    try {
      if (file && file.exists())
        file.remove(false);
    } catch (ex) {
      Util.dumpLn("Failed to cancel download, with id: "+id+", download target URI spec: " + fileURI.spec);
      Util.dumpLn("Failed download source:"+(aDownload.source && aDownload.source.spec));
    }
  },

  pauseDownload: function dh_pauseDownload(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    this.manager.pauseDownload(id);
  },

  resumeDownload: function dh_resumeDownload(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    this.manager.resumeDownload(id);
  },

  retryDownload: function dh_retryDownload(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    this.manager.retryDownload(id);
  },

  showPage: function dh_showPage(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    let download = this.manager.getDownload(id);
    let uri = this._getReferrerOrSource(download);
    if (uri)
      BrowserUI.newTab(uri, Browser.selectedTab);
  },

  showAlert: function dh_showAlert(aName, aMessage, aTitle, aIcon) {
    var notifier = Cc["@mozilla.org/alerts-service;1"]
                     .getService(Ci.nsIAlertsService);

    // Callback for tapping on the alert popup
    let observer = {
      observe: function (aSubject, aTopic, aData) {
        if (aTopic == "alertclickcallback")
          PanelUI.show("downloads-container");
      }
    };

    if (!aTitle)
      aTitle = Strings.browser.GetStringFromName("alertDownloads");

    if (!aIcon)
      aIcon = URI_GENERIC_ICON_DOWNLOAD;

    notifier.showAlertNotification(aIcon, aTitle, aMessage, true, "", observer, aName);
  },

  observe: function (aSubject, aTopic, aData) {
    let download = aSubject.QueryInterface(Ci.nsIDownload);
    let msgKey = "";

    switch (aTopic) {
      case "dl-start":
        msgKey = "alertDownloadsStart";
        if (!this._progressAlert) {
          this._progressAlert = new AlertDownloadProgressListener();
          this.manager.addListener(this._progressAlert);
        }
        break;
      case "dl-done":
        msgKey = "alertDownloadsDone";
        break;
    }

    if (msgKey) {
      let message = Strings.browser.formatStringFromName(msgKey, [download.displayName], 1);
      let url = download.target.spec.replace("file:", "download:");
      this.showAlert(url, message);
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * Wraps a list/grid control implementing nsIDOMXULSelectControlElement and
 * fills it with the user's downloads.  It observes download notifications,
 * so it'll automatically update to reflect current download progress.
 *
 * @param           aSet    Control implementing nsIDOMXULSelectControlElement.
 * @param {Number}  aLimit  Maximum number of items to show in the view.
 */
function DownloadsView(aSet, aLimit) {
  this._set = aSet;
  this._limit = aLimit;

  this._progress = new DownloadProgressListener(this);
  Downloads.manager.addListener(this._progress);

  // Look for the removed download notification
  let obs = Cc["@mozilla.org/observer-service;1"].
                  getService(Ci.nsIObserverService);
  obs.addObserver(this, "download-manager-remove-download-guid", false);

  this.getDownloads();
}

DownloadsView.prototype = {
  _progress: null,
  _stmt: null,
  _timeoutID: null,
  _set: null,
  _limit: null,

  _getItemForDownloadGuid: function dv__getItemForDownload(aGuid) {
    return this._set.querySelector("richgriditem[downloadGuid='" + aGuid + "']");
  },

  _getItemForDownload: function dv__getItemForDownload(aDownload) {
    return this._set.querySelector("richgriditem[downloadId='" + aDownload.id + "']");
  },

  _getDownloadForItem: function dv__getDownloadForItem(anItem) {
    let id = anItem.getAttribute("downloadId");
    return Downloads.manager.getDownload(id);
  },

  _getAttrsForDownload: function dv__getAttrsForDownload(aDownload) {
    // params: nsiDownload
    return {
      typeName: 'download',
      downloadId: aDownload.id,
      downloadGuid: aDownload.guid,
      name: aDownload.displayName,
      // use the stringified version of the target nsIURI for the item attribute
      target: aDownload.target.spec,
      iconURI: "moz-icon://" + aDownload.displayName + "?size=64",
      date: DownloadUtils.getReadableDates(new Date())[0],
      domain: DownloadUtils.getURIHost(aDownload.source.spec)[0],
      size: this._getDownloadSize(aDownload.size),
      state: aDownload.state
    };

  },
  _updateItemWithAttrs: function dv__updateItemWithAttrs(anItem, aAttrs) {
    for (let name in aAttrs)
      anItem.setAttribute(name, aAttrs[name]);
    if (anItem.refresh)
      anItem.refresh();
  },

  _getDownloadSize: function dv__getDownloadSize (aSize) {
    let displaySize = DownloadUtils.convertByteUnits(aSize);
    if (displaySize[0] > 0) // [0] is size, [1] is units
      return displaySize.join("");
    else
      return Strings.browser.GetStringFromName("downloadsUnknownSize");
  },

  _initStatement: function dv__initStatement() {
    if (this._stmt)
      this._stmt.finalize();

    let limitClause = this._limit ? ("LIMIT " + this._limit) : "";

    this._stmt = Downloads.manager.DBConnection.createStatement(
      "SELECT id, guid, name, target, source, state, startTime, endTime, referrer, " +
             "currBytes, maxBytes, state IN (?1, ?2, ?3, ?4, ?5) isActive " +
      "FROM moz_downloads " +
      "ORDER BY isActive DESC, endTime DESC, startTime DESC " +
      limitClause);
  },

  _stepDownloads: function dv__stepDownloads(aNumItems) {
    try {
      if (!this._stmt.executeStep()) {
        // final record;
        this._stmt.finalize();
        this._stmt = null;
        this._fire("DownloadsReady", this._set);
        return;
      }
      let attrs = {
        typeName: 'download',
        // TODO: <sfoster> the remove event gives us guid not id; should we use guid as the download (record) id?
        downloadGuid: this._stmt.row.guid,
        downloadId: this._stmt.row.id,
        name: this._stmt.row.name,
        target: this._stmt.row.target,
        iconURI: "moz-icon://" + this._stmt.row.name + "?size=25",
        date: DownloadUtils.getReadableDates(new Date(this._stmt.row.endTime / 1000))[0],
        domain: DownloadUtils.getURIHost(this._stmt.row.source)[0],
        size: this._getDownloadSize(this._stmt.row.maxBytes),
        state: this._stmt.row.state
      };

      let item = this._set.appendItem(attrs.target, attrs.downloadId);
      this._updateItemWithAttrs(item, attrs);
    } catch (e) {
      // Something went wrong when stepping or getting values, so quit
      this._stmt.reset();
      return;
    }

    // Add another item to the list if we should;
    // otherwise, let the UI update and continue later
    if (aNumItems > 1) {
      this._stepDownloads(aNumItems - 1);
    } else {
      // Use a shorter delay for earlier downloads to display them faster
      let delay = Math.min(this._set.itemCount * 10, 300);
      let self = this;
      this._timeoutID = setTimeout(function() { self._stepDownloads(5); }, delay);
    }
  },

  _fire: function _fire(aName, anElement) {
    let event = document.createEvent("Events");
    event.initEvent(aName, true, true);
    anElement.dispatchEvent(event);
  },

  observe: function dv_managerObserver(aSubject, aTopic, aData) {
    // observer-service message handler
    switch (aTopic) {
      case "download-manager-remove-download-guid":
        let guid = aSubject.QueryInterface(Ci.nsISupportsCString);
        this.removeDownload({
          guid: guid
        });
        return;
    }
  },

  getDownloads: function dv_getDownloads() {
    this._initStatement();
    clearTimeout(this._timeoutID);

    // Since we're pulling in all downloads, clear the list to avoid duplication
    this.clearDownloads();

    this._stmt.reset();
    this._stmt.bindInt32Parameter(0, Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED);
    this._stmt.bindInt32Parameter(1, Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING);
    this._stmt.bindInt32Parameter(2, Ci.nsIDownloadManager.DOWNLOAD_PAUSED);
    this._stmt.bindInt32Parameter(3, Ci.nsIDownloadManager.DOWNLOAD_QUEUED);
    this._stmt.bindInt32Parameter(4, Ci.nsIDownloadManager.DOWNLOAD_SCANNING);

    // Take a quick break before we actually start building the list
    let self = this;
    this._timeoutID = setTimeout(function() {
      self._stepDownloads(1);
    }, 0);
  },

  clearDownloads: function dv_clearDownloads() {
    this._set.clearAll();
  },

  addDownload: function dv_addDownload(aDownload) {
    // expects a download manager download object
    let attrs = this._getAttrsForDownload(aDownload);
    let item = this._set.insertItemAt(0, attrs.target, attrs.downloadId);
    this._updateItemWithAttrs(item, attrs);
  },

  updateDownload: function dv_updateDownload(aDownload) {
    // expects a download manager download object
    let item = this._getItemForDownload(aDownload);

    if (!item)
      return;

    let attrs = this._getAttrsForDownload(aDownload);
    this._updateItemWithAttrs(item, attrs);
  },

  removeDownload: function dv_removeDownload(aDownload) {
    // expects an object with id or guid property
    let item;
    if (aDownload.id) {
      item = this._getItemForDownload(aDownload.id);
    } else if (aDownload.guid) {
      item = this._getItemForDownloadGuid(aDownload.guid);
    }
    if (!item)
      return;

    let idx = this._set.getIndexOfItem(item);
    if (idx < 0)
        return;
    // any transition needed here?
    this._set.removeItemAt(idx);
  },

  destruct: function dv_destruct() {
    Downloads.manager.removeListener(this._progress);
  }
};

var DownloadsPanelView = {
  _view: null,

  get _grid() { return document.getElementById("downloads-list"); },
  get visible() { return PanelUI.isPaneVisible("downloads-container"); },

  init: function init() {
    this._view = new DownloadsView(this._grid);
  },

  show: function show() {
    this._grid.arrangeItems();
  },

  uninit: function uninit() {
    this._view.destruct();
  }
};

/**
 * Notifies a DownloadsView about updates in the state of various downloads.
 *
 * @param aView An instance of DownloadsView.
 */
function DownloadProgressListener(aView) {
  this._view = aView;
}

DownloadProgressListener.prototype = {
  _view: null,

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener
  onDownloadStateChange: function dPL_onDownloadStateChange(aState, aDownload) {
    let state = aDownload.state;
    switch (state) {
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_POLICY:
        this._view.addDownload(aDownload);
        break;
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED:
        break;
    }

    this._view.updateDownload(aDownload);
  },

  onProgressChange: function dPL_onProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload) {
    // TODO <jwilde>: Add more detailed progress information.
    this._view.updateDownload(aDownload);
  },

  onStateChange: function(aWebProgress, aRequest, aState, aStatus, aDownload) { },
  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener])
};


/**
 * Tracks download progress so that additional information can be displayed
 * about its download in alert popups.
 */
function AlertDownloadProgressListener() { }

AlertDownloadProgressListener.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener
  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload) {
    let strings = Strings.browser;
    let availableSpace = -1;

    try {
      // diskSpaceAvailable is not implemented on all systems
      let availableSpace = aDownload.targetFile.diskSpaceAvailable;
    } catch(ex) { }

    let contentLength = aDownload.size;
    if (availableSpace > 0 && contentLength > 0 && contentLength > availableSpace) {
      Downloads.showAlert(aDownload.target.spec.replace("file:", "download:"),
                          strings.GetStringFromName("alertDownloadsNoSpace"),
                          strings.GetStringFromName("alertDownloadsSize"));
      Downloads.cancelDownload(aDownload.id);
    }
  },

  onDownloadStateChange: function(aState, aDownload) { },
  onStateChange: function(aWebProgress, aRequest, aState, aStatus, aDownload) { },
  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener])
};

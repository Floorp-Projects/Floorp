// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const URI_GENERIC_ICON_DOWNLOAD = "chrome://browser/skin/images/alert-downloads-30.png";

var Downloads = {
  /**
   * _downloadCount keeps track of the number of downloads that a single
   * notification bar groups together. A download is grouped with other
   * downloads if it starts before other downloads have completed.
   */
  _downloadCount: 0,
  _inited: false,
  _progressAlert: null,
  _lastSec: Infinity,
  _notificationBox: null,
  _progressNotification: null,
  _progressNotificationInfo: new Map(),
  _runDownloadBooleanMap: new Map(),

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
    Services.obs.addObserver(this, "dl-run", true);
    Services.obs.addObserver(this, "dl-failed", true);

    this._notificationBox = Browser.getNotificationBox();

    this._progress = new DownloadProgressListener(this);
    this.manager.addListener(this._progress);
  },

  uninit: function dh_uninit() {
    if (this._inited) {
      Services.obs.removeObserver(this, "dl-start");
      Services.obs.removeObserver(this, "dl-done");
      Services.obs.removeObserver(this, "dl-run");
      Services.obs.removeObserver(this, "dl-failed");
    }
  },

  openDownload: function dh_openDownload(aDownload) {
    let fileURI = aDownload.target

    if (!(fileURI && fileURI.spec)) {
      Util.dumpLn("Cant open download "+id+", fileURI is invalid");
      return;
    }

    let file = this._getLocalFile(fileURI);
    try {
      file && MetroUtils.launchInDesktop(aDownload.target.spec, "");
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
      this.manager.removeDownload(id);
    }
  },

  cancelDownload: function dh_cancelDownload(aDownload) {
    this._progressNotificationInfo.delete(aDownload.guid);
    this._runDownloadBooleanMap.delete(aDownload.targetFile.path);
    this._downloadCount--;
    if (this._progressNotificationInfo.size == 0) {
      this._notificationBox.removeNotification(this._progressNotification);
      this._progressNotification = null;
    }

    let fileURI = aDownload.target;
    if (!(fileURI && fileURI.spec)) {
      Util.dumpLn("Cant remove download file for: "+aDownload.id+", fileURI is invalid");
      return;
    }

    let file = this._getLocalFile(fileURI);
    try {
      this.manager.cancelDownload(aDownload.id);
      if (file && file.exists())
        file.remove(false);
    } catch (ex) {
      Util.dumpLn("Failed to cancel download, with id: "+aDownload.id+", download target URI spec: " + fileURI.spec);
      Util.dumpLn("Failed download source:"+(aDownload.source && aDownload.source.spec));
    }
  },

  // Cancels all downloads.
  cancelDownloads: function dh_cancelDownloads() {
    for (let info of this._progressNotificationInfo) {
      this.cancelDownload(info[1].download);
    }
    this._downloadCount = 0;
    this._progressNotificationInfo.clear();
    this._runDownloadBooleanMap.clear();
  },

  pauseDownload: function dh_pauseDownload(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    this.manager.pauseDownload(id);
  },

  resumeDownload: function dh_resumeDownload(aDownload) {
    let id = aDownload.getAttribute("downloadId");
    this.manager.resumeDownload(id);
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
        if (aTopic == "alertclickcallback") {
          // TODO: Bug 783232 turns this alert into a native toast. 
        }
      }
    };

    if (!aTitle)
      aTitle = Strings.browser.GetStringFromName("alertDownloads");

    if (!aIcon)
      aIcon = URI_GENERIC_ICON_DOWNLOAD;

    notifier.showAlertNotification(aIcon, aTitle, aMessage, true, "", observer, aName);
  },

  showNotification: function dh_showNotification(title, msg, buttons, priority) {
    return this._notificationBox.appendNotification(msg,
                                              title,
                                              URI_GENERIC_ICON_DOWNLOAD,
                                              priority,
                                              buttons);
  },

  _showDownloadFailedNotification: function (aDownload) {
    let tryAgainButtonText =
      Strings.browser.GetStringFromName("downloadTryAgain");
    let cancelButtonText =
      Strings.browser.GetStringFromName("downloadCancel");

    let message = Strings.browser.formatStringFromName("alertDownloadFailed",
      [aDownload.displayName], 1);

    let buttons = [
      {
        isDefault: true,
        label: tryAgainButtonText,
        accessKey: "",
        callback: function() {
          Downloads.manager.retryDownload(aDownload.id);
        }
      },
      {
        label: cancelButtonText,
        accessKey: "",
        callback: function() {
          Downloads.cancelDownload(aDownload);
        }
      }
    ];
    this.showNotification("download-failed", message, buttons,
      this._notificationBox.PRIORITY_WARNING_HIGH);
  },

  _showDownloadCompleteNotification: function (aDownload) {
    let message = "";
    let showInFilesButtonText = Strings.browser.GetStringFromName("downloadShowInFiles");

    let buttons = [
      {
        label: showInFilesButtonText,
        accessKey: "",
        callback: function() {
          let fileURI = aDownload.target;
          let file = Downloads._getLocalFile(fileURI);
          file.reveal();
        }
      }
    ];

    if (this._downloadCount > 1) {
      message = PluralForm.get(this._downloadCount,
                               Strings.browser.GetStringFromName("alertMultipleDownloadsComplete"))
                               .replace("#1", this._downloadCount)
    } else {
      let runButtonText =
        Strings.browser.GetStringFromName("downloadRun");
      message = Strings.browser.formatStringFromName("alertDownloadsDone2",
        [aDownload.displayName], 1);

      buttons.unshift({
        isDefault: true,
        label: runButtonText,
        accessKey: "",
        callback: function() {
          Downloads.openDownload(aDownload);
        }
      });
    }
    this.showNotification("download-complete", message, buttons,
      this._notificationBox.PRIORITY_WARNING_MEDIUM);
  },

  _computeDownloadProgressString: function dv_computeDownloadProgressString(aDownload) {
    let totTransferred = 0, totSize = 0, totSecondsLeft = 0;
    for (let info of this._progressNotificationInfo) {
      let size = info[1].download.size;
      let amountTransferred = info[1].download.amountTransferred;
      let speed = info[1].download.speed;

      totTransferred += amountTransferred;
      totSize += size;
      totSecondsLeft += ((size - amountTransferred) / speed);
    }
    // Compute progress in bytes.
    let amountTransferred = Util.getDownloadSize(totTransferred);
    let size = Util.getDownloadSize(totSize);
    let progress = amountTransferred + "/" + size;

    // Compute progress in time.;
    let [timeLeft, newLast] = DownloadUtils.getTimeLeft(totSecondsLeft, this._lastSec);
    this._lastSec = newLast;

    if (this._downloadCount == 1) {
      return Strings.browser.GetStringFromName("alertDownloadsStart2")
        .replace("#1", aDownload.displayName)
        .replace("#2", progress)
        .replace("#3", timeLeft)
    }

    let numDownloads = this._downloadCount;
    return PluralForm.get(numDownloads,
                          Strings.browser.GetStringFromName("alertDownloadMultiple"))
                          .replace("#1", numDownloads)
                          .replace("#2", progress)
                          .replace("#3", timeLeft);
  },

  _saveDownloadData: function dv_saveDownloadData(aDownload) {
    if (!this._progressNotificationInfo.get(aDownload.guid)) {
      this._progressNotificationInfo.set(aDownload.guid, {});
    }
    let infoObj = this._progressNotificationInfo.get(aDownload.guid);
    infoObj.download = aDownload;
    this._progressNotificationInfo.set(aDownload.guid, infoObj);
  },

  updateInfobar: function dv_updateInfobar(aDownload) {
    this._saveDownloadData(aDownload);
    let message = this._computeDownloadProgressString(aDownload);

    if (this._progressNotification == null ||
        !this._notificationBox.getNotificationWithValue("download-progress")) {

      let cancelButtonText =
              Strings.browser.GetStringFromName("downloadCancel");

      let buttons = [
        {
          isDefault: false,
          label: cancelButtonText,
          accessKey: "",
          callback: function() {
            Downloads.cancelDownloads();
          }
        }
      ];

      this._progressNotification =
        this.showNotification("download-progress", message, buttons,
        this._notificationBox.PRIORITY_WARNING_LOW);
    } else {
      this._progressNotification.label = message;
    }
  },

  updateDownload: function dv_updateDownload(aDownload) {
    if (this._progressNotification != null) {
      this._saveDownloadData(aDownload);
      this._progressNotification.label =
        this._computeDownloadProgressString(aDownload);
    }
  },

  observe: function (aSubject, aTopic, aData) {
    let message = "";
    let msgTitle = "";

    switch (aTopic) {
      case "dl-run":
        let file = aSubject.QueryInterface(Ci.nsIFile);
        this._runDownloadBooleanMap.set(file.path, (aData == 'true'));
        break;
      case "dl-start":
        this._downloadCount++;
        let download = aSubject.QueryInterface(Ci.nsIDownload);
        if (!this._progressNotificationInfo.get(download.guid)) {
          this._progressNotificationInfo.set(download.guid, {});
        }
        if (!this._progressAlert) {
          this._progressAlert = new AlertDownloadProgressListener();
          this.manager.addListener(this._progressAlert);
        }
        this.updateInfobar(download);
        break;
      case "dl-done":
        download = aSubject.QueryInterface(Ci.nsIDownload);
        let runAfterDownload = this._runDownloadBooleanMap.get(download.targetFile.path);
        if (runAfterDownload) {
          this.openDownload(download);
        }

        this._progressNotificationInfo.delete(download.guid);
        this._runDownloadBooleanMap.delete(download.targetFile.path);
        if (this._progressNotificationInfo.size == 0) {
          if (this._downloadCount > 1 || !runAfterDownload) {
            this._showDownloadCompleteNotification(download);
          }
          this._downloadCount = 0;
          this._notificationBox.removeNotification(this._progressNotification);
          this._progressNotification = null;
        }
        break;
      case "dl-failed":
        download = aSubject.QueryInterface(Ci.nsIDownload);
        break;
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
 * Notifies Downloads object about updates in the state of various downloads.
 *
 * @param aDownloads An instance of Downloads.
 */
function DownloadProgressListener(aDownloads) {
  this._downloads = aDownloads;
}

DownloadProgressListener.prototype = {
  _downloads: null,

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener
  onDownloadStateChange: function dPL_onDownloadStateChange(aState, aDownload) {
    // TODO: Use DownloadProgressListener instead of observers in the Downloads object.
    this._downloads.updateDownload(aDownload);
  },

  onProgressChange: function dPL_onProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload) {
    // TODO <jwilde>: Add more detailed progress information.
    this._downloads.updateDownload(aDownload);
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
      Downloads.cancelDownload(aDownload);
    }
  },

  onDownloadStateChange: function(aState, aDownload) { },
  onStateChange: function(aWebProgress, aRequest, aState, aStatus, aDownload) { },
  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener])
};

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadsCommon",
];

/**
 * Handles the Downloads panel shared methods and data access.
 *
 * This file includes the following constructors and global objects:
 *
 * DownloadsCommon
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides shared methods for all the instances of the user interface.
 *
 * DownloadsData
 * Retrieves the list of past and completed downloads from the underlying
 * Download Manager data, and provides asynchronous notifications allowing
 * to build a consistent view of the available data.
 *
 * DownloadsDataItem
 * Represents a single item in the list of downloads.  This object either wraps
 * an existing nsIDownload from the Download Manager, or provides the same
 * information read directly from the downloads database, with the possibility
 * of querying the nsIDownload lazily, for performance reasons.
 *
 * DownloadsIndicatorData
 * This object registers itself with DownloadsData as a view, and transforms the
 * notifications it receives into overall status data, that is then broadcast to
 * the registered download status indicators.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUIHelper",
                                  "resource://gre/modules/DownloadUIHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsLogger",
                                  "resource:///modules/DownloadsLogger.jsm");

const nsIDM = Ci.nsIDownloadManager;

const kDownloadsStringBundleUrl =
  "chrome://browser/locale/downloads/downloads.properties";

const kDownloadsStringsRequiringFormatting = {
  sizeWithUnits: true,
  shortTimeLeftSeconds: true,
  shortTimeLeftMinutes: true,
  shortTimeLeftHours: true,
  shortTimeLeftDays: true,
  statusSeparator: true,
  statusSeparatorBeforeNumber: true,
  fileExecutableSecurityWarning: true
};

const kDownloadsStringsRequiringPluralForm = {
  otherDownloads2: true
};

XPCOMUtils.defineLazyGetter(this, "DownloadsLocalFileCtor", function () {
  return Components.Constructor("@mozilla.org/file/local;1",
                                "nsILocalFile", "initWithPath");
});

const kPartialDownloadSuffix = ".part";

const kPrefBranch = Services.prefs.getBranch("browser.download.");

let PrefObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
  getPref: function PO_getPref(name) {
    try {
      switch (typeof this.prefs[name]) {
        case "boolean":
          return kPrefBranch.getBoolPref(name);
      }
    } catch (ex) { }
    return this.prefs[name];
  },
  observe: function PO_observe(aSubject, aTopic, aData) {
    if (this.prefs.hasOwnProperty(aData)) {
      return this[aData] = this.getPref(aData);
    }
  },
  register: function PO_register(prefs) {
    this.prefs = prefs;
    kPrefBranch.addObserver("", this, true);
    for (let key in prefs) {
      let name = key;
      XPCOMUtils.defineLazyGetter(this, name, function () {
        return PrefObserver.getPref(name);
      });
    }
  },
};

PrefObserver.register({
  // prefName: defaultValue
  debug: false,
  animateNotifications: true
});


////////////////////////////////////////////////////////////////////////////////
//// DownloadsCommon

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides shared methods for all the instances of the user interface.
 */
this.DownloadsCommon = {
  log: function DC_log(...aMessageArgs) {
    delete this.log;
    this.log = function DC_log(...aMessageArgs) {
      if (!PrefObserver.debug) {
        return;
      }
      DownloadsLogger.log.apply(DownloadsLogger, aMessageArgs);
    }
    this.log.apply(this, aMessageArgs);
  },

  error: function DC_error(...aMessageArgs) {
    delete this.error;
    this.error = function DC_error(...aMessageArgs) {
      if (!PrefObserver.debug) {
        return;
      }
      DownloadsLogger.reportError.apply(DownloadsLogger, aMessageArgs);
    }
    this.error.apply(this, aMessageArgs);
  },
  /**
   * Returns an object whose keys are the string names from the downloads string
   * bundle, and whose values are either the translated strings or functions
   * returning formatted strings.
   */
  get strings()
  {
    let strings = {};
    let sb = Services.strings.createBundle(kDownloadsStringBundleUrl);
    let enumerator = sb.getSimpleEnumeration();
    while (enumerator.hasMoreElements()) {
      let string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
      let stringName = string.key;
      if (stringName in kDownloadsStringsRequiringFormatting) {
        strings[stringName] = function () {
          // Convert "arguments" to a real array before calling into XPCOM.
          return sb.formatStringFromName(stringName,
                                         Array.slice(arguments, 0),
                                         arguments.length);
        };
      } else if (stringName in kDownloadsStringsRequiringPluralForm) {
        strings[stringName] = function (aCount) {
          // Convert "arguments" to a real array before calling into XPCOM.
          let formattedString = sb.formatStringFromName(stringName,
                                         Array.slice(arguments, 0),
                                         arguments.length);
          return PluralForm.get(aCount, formattedString);
        };
      } else {
        strings[stringName] = string.value;
      }
    }
    delete this.strings;
    return this.strings = strings;
  },

  /**
   * Generates a very short string representing the given time left.
   *
   * @param aSeconds
   *        Value to be formatted.  It represents the number of seconds, it must
   *        be positive but does not need to be an integer.
   *
   * @return Formatted string, for example "30s" or "2h".  The returned value is
   *         maximum three characters long, at least in English.
   */
  formatTimeLeft: function DC_formatTimeLeft(aSeconds)
  {
    // Decide what text to show for the time
    let seconds = Math.round(aSeconds);
    if (!seconds) {
      return "";
    } else if (seconds <= 30) {
      return DownloadsCommon.strings["shortTimeLeftSeconds"](seconds);
    }
    let minutes = Math.round(aSeconds / 60);
    if (minutes < 60) {
      return DownloadsCommon.strings["shortTimeLeftMinutes"](minutes);
    }
    let hours = Math.round(minutes / 60);
    if (hours < 48) { // two days
      return DownloadsCommon.strings["shortTimeLeftHours"](hours);
    }
    let days = Math.round(hours / 24);
    return DownloadsCommon.strings["shortTimeLeftDays"](Math.min(days, 99));
  },

  /**
   * Indicates whether we should show visual notification on the indicator
   * when a download event is triggered.
   */
  get animateNotifications()
  {
    return PrefObserver.animateNotifications;
  },

  /**
   * Get access to one of the DownloadsData or PrivateDownloadsData objects,
   * depending on the privacy status of the window in question.
   *
   * @param aWindow
   *        The browser window which owns the download button.
   */
  getData: function DC_getData(aWindow) {
    if (PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
      return PrivateDownloadsData;
    } else {
      return DownloadsData;
    }
  },

  /**
   * Initializes the Downloads back-end and starts receiving events for both the
   * private and non-private downloads data objects.
   */
  initializeAllDataLinks: function () {
    DownloadsData.initializeDataLink();
    PrivateDownloadsData.initializeDataLink();
  },

  /**
   * Get access to one of the DownloadsIndicatorData or
   * PrivateDownloadsIndicatorData objects, depending on the privacy status of
   * the window in question.
   */
  getIndicatorData: function DC_getIndicatorData(aWindow) {
    if (PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
      return PrivateDownloadsIndicatorData;
    } else {
      return DownloadsIndicatorData;
    }
  },

  /**
   * Returns a reference to the DownloadsSummaryData singleton - creating one
   * in the process if one hasn't been instantiated yet.
   *
   * @param aWindow
   *        The browser window which owns the download button.
   * @param aNumToExclude
   *        The number of items on the top of the downloads list to exclude
   *        from the summary.
   */
  getSummary: function DC_getSummary(aWindow, aNumToExclude)
  {
    if (PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
      if (this._privateSummary) {
        return this._privateSummary;
      }
      return this._privateSummary = new DownloadsSummaryData(true, aNumToExclude);
    } else {
      if (this._summary) {
        return this._summary;
      }
      return this._summary = new DownloadsSummaryData(false, aNumToExclude);
    }
  },
  _summary: null,
  _privateSummary: null,

  /**
   * Given an iterable collection of DownloadDataItems, generates and returns
   * statistics about that collection.
   *
   * @param aDataItems An iterable collection of DownloadDataItems.
   *
   * @return Object whose properties are the generated statistics. Currently,
   *         we return the following properties:
   *
   *         numActive       : The total number of downloads.
   *         numPaused       : The total number of paused downloads.
   *         numScanning     : The total number of downloads being scanned.
   *         numDownloading  : The total number of downloads being downloaded.
   *         totalSize       : The total size of all downloads once completed.
   *         totalTransferred: The total amount of transferred data for these
   *                           downloads.
   *         slowestSpeed    : The slowest download rate.
   *         rawTimeLeft     : The estimated time left for the downloads to
   *                           complete.
   *         percentComplete : The percentage of bytes successfully downloaded.
   */
  summarizeDownloads: function DC_summarizeDownloads(aDataItems)
  {
    let summary = {
      numActive: 0,
      numPaused: 0,
      numScanning: 0,
      numDownloading: 0,
      totalSize: 0,
      totalTransferred: 0,
      // slowestSpeed is Infinity so that we can use Math.min to
      // find the slowest speed. We'll set this to 0 afterwards if
      // it's still at Infinity by the time we're done iterating all
      // dataItems.
      slowestSpeed: Infinity,
      rawTimeLeft: -1,
      percentComplete: -1
    }

    for (let dataItem of aDataItems) {
      summary.numActive++;
      switch (dataItem.state) {
        case nsIDM.DOWNLOAD_PAUSED:
          summary.numPaused++;
          break;
        case nsIDM.DOWNLOAD_SCANNING:
          summary.numScanning++;
          break;
        case nsIDM.DOWNLOAD_DOWNLOADING:
          summary.numDownloading++;
          if (dataItem.maxBytes > 0 && dataItem.speed > 0) {
            let sizeLeft = dataItem.maxBytes - dataItem.currBytes;
            summary.rawTimeLeft = Math.max(summary.rawTimeLeft,
                                           sizeLeft / dataItem.speed);
            summary.slowestSpeed = Math.min(summary.slowestSpeed,
                                            dataItem.speed);
          }
          break;
      }
      // Only add to total values if we actually know the download size.
      if (dataItem.maxBytes > 0 &&
          dataItem.state != nsIDM.DOWNLOAD_CANCELED &&
          dataItem.state != nsIDM.DOWNLOAD_FAILED) {
        summary.totalSize += dataItem.maxBytes;
        summary.totalTransferred += dataItem.currBytes;
      }
    }

    if (summary.numActive != 0 && summary.totalSize != 0 &&
        summary.numActive != summary.numScanning) {
      summary.percentComplete = (summary.totalTransferred /
                                 summary.totalSize) * 100;
    }

    if (summary.slowestSpeed == Infinity) {
      summary.slowestSpeed = 0;
    }

    return summary;
  },

  /**
   * If necessary, smooths the estimated number of seconds remaining for one
   * or more downloads to complete.
   *
   * @param aSeconds
   *        Current raw estimate on number of seconds left for one or more
   *        downloads. This is a floating point value to help get sub-second
   *        accuracy for current and future estimates.
   */
  smoothSeconds: function DC_smoothSeconds(aSeconds, aLastSeconds)
  {
    // We apply an algorithm similar to the DownloadUtils.getTimeLeft function,
    // though tailored to a single time estimation for all downloads.  We never
    // apply something if the new value is less than half the previous value.
    let shouldApplySmoothing = aLastSeconds >= 0 &&
                               aSeconds > aLastSeconds / 2;
    if (shouldApplySmoothing) {
      // Apply hysteresis to favor downward over upward swings.  Trust only 30%
      // of the new value if lower, and 10% if higher (exponential smoothing).
      let (diff = aSeconds - aLastSeconds) {
        aSeconds = aLastSeconds + (diff < 0 ? .3 : .1) * diff;
      }

      // If the new time is similar, reuse something close to the last time
      // left, but subtract a little to provide forward progress.
      let diff = aSeconds - aLastSeconds;
      let diffPercent = diff / aLastSeconds * 100;
      if (Math.abs(diff) < 5 || Math.abs(diffPercent) < 5) {
        aSeconds = aLastSeconds - (diff < 0 ? .4 : .2);
      }
    }

    // In the last few seconds of downloading, we are always subtracting and
    // never adding to the time left.  Ensure that we never fall below one
    // second left until all downloads are actually finished.
    return aLastSeconds = Math.max(aSeconds, 1);
  },

  /**
   * Opens a downloaded file.
   * If you've a dataItem, you should call dataItem.openLocalFile.
   * @param aFile
   *        the downloaded file to be opened.
   * @param aMimeInfo
   *        the mime type info object.  May be null.
   * @param aOwnerWindow
   *        the window with which this action is associated.
   */
  openDownloadedFile: function DC_openDownloadedFile(aFile, aMimeInfo, aOwnerWindow) {
    if (!(aFile instanceof Ci.nsIFile))
      throw new Error("aFile must be a nsIFile object");
    if (aMimeInfo && !(aMimeInfo instanceof Ci.nsIMIMEInfo))
      throw new Error("Invalid value passed for aMimeInfo");
    if (!(aOwnerWindow instanceof Ci.nsIDOMWindow))
      throw new Error("aOwnerWindow must be a dom-window object");

    let promiseShouldLaunch;
    if (aFile.isExecutable()) {
      // We get a prompter for the provided window here, even though anchoring
      // to the most recently active window should work as well.
      promiseShouldLaunch =
        DownloadUIHelper.getPrompter(aOwnerWindow)
                        .confirmLaunchExecutable(aFile.path);
    } else {
      promiseShouldLaunch = Promise.resolve(true);
    }

    promiseShouldLaunch.then(shouldLaunch => {
      if (!shouldLaunch) {
        return;
      }
  
      // Actually open the file.
      try {
        if (aMimeInfo && aMimeInfo.preferredAction == aMimeInfo.useHelperApp) {
          aMimeInfo.launchWithFile(aFile);
          return;
        }
      }
      catch(ex) { }
  
      // If either we don't have the mime info, or the preferred action failed,
      // attempt to launch the file directly.
      try {
        aFile.launch();
      }
      catch(ex) {
        // If launch fails, try sending it through the system's external "file:"
        // URL handler.
        Cc["@mozilla.org/uriloader/external-protocol-service;1"]
          .getService(Ci.nsIExternalProtocolService)
          .loadUrl(NetUtil.newURI(aFile));
      }
    }).then(null, Cu.reportError);
  },

  /**
   * Show a downloaded file in the system file manager.
   * If you have a dataItem, use dataItem.showLocalFile.
   *
   * @param aFile
   *        a downloaded file.
   */
  showDownloadedFile: function DC_showDownloadedFile(aFile) {
    if (!(aFile instanceof Ci.nsIFile))
      throw new Error("aFile must be a nsIFile object");
    try {
      // Show the directory containing the file and select the file.
      aFile.reveal();
    } catch (ex) {
      // If reveal fails for some reason (e.g., it's not implemented on unix
      // or the file doesn't exist), try using the parent if we have it.
      let parent = aFile.parent;
      if (parent) {
        try {
          // Open the parent directory to show where the file should be.
          parent.launch();
        } catch (ex) {
          // If launch also fails (probably because it's not implemented), let
          // the OS handler try to open the parent.
          Cc["@mozilla.org/uriloader/external-protocol-service;1"]
            .getService(Ci.nsIExternalProtocolService)
            .loadUrl(NetUtil.newURI(parent));
        }
      }
    }
  }
};

/**
 * Returns true if we are executing on Windows Vista or a later version.
 */
XPCOMUtils.defineLazyGetter(DownloadsCommon, "isWinVistaOrHigher", function () {
  let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
  if (os != "WINNT") {
    return false;
  }
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  return parseFloat(sysInfo.getProperty("version")) >= 6;
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsData

/**
 * Retrieves the list of past and completed downloads from the underlying
 * Download Manager data, and provides asynchronous notifications allowing to
 * build a consistent view of the available data.
 *
 * This object responds to real-time changes in the underlying Download Manager
 * data.  For example, the deletion of one or more downloads is notified through
 * the nsIObserver interface, while any state or progress change is notified
 * through the nsIDownloadProgressListener interface.
 *
 * Note that using this object does not automatically start the Download Manager
 * service.  Consumers will see an empty list of downloads until the service is
 * actually started.  This is useful to display a neutral progress indicator in
 * the main browser window until the autostart timeout elapses.
 *
 * Note that DownloadsData and PrivateDownloadsData are two equivalent singleton
 * objects, one accessing non-private downloads, and the other accessing private
 * ones.
 */
function DownloadsDataCtor(aPrivate) {
  this._isPrivate = aPrivate;

  // This Object contains all the available DownloadsDataItem objects, indexed by
  // their globally unique identifier.  The identifiers of downloads that have
  // been removed from the Download Manager data are still present, however the
  // associated objects are replaced with the value "null".  This is required to
  // prevent race conditions when populating the list asynchronously.
  this.dataItems = {};

  // Array of view objects that should be notified when the available download
  // data changes.
  this._views = [];

  // Maps Download objects to DownloadDataItem objects.
  this._downloadToDataItemMap = new Map();
}

DownloadsDataCtor.prototype = {
  /**
   * Starts receiving events for current downloads.
   */
  initializeDataLink: function ()
  {
    if (!this._dataLinkInitialized) {
      let promiseList = Downloads.getList(this._isPrivate ? Downloads.PRIVATE
                                                          : Downloads.PUBLIC);
      promiseList.then(list => list.addView(this)).then(null, Cu.reportError);
      this._dataLinkInitialized = true;
    }
  },
  _dataLinkInitialized: false,

  /**
   * True if there are finished downloads that can be removed from the list.
   */
  get canRemoveFinished()
  {
    for (let [, dataItem] of Iterator(this.dataItems)) {
      if (dataItem && !dataItem.inProgress) {
        return true;
      }
    }
    return false;
  },

  /**
   * Asks the back-end to remove finished downloads from the list.
   */
  removeFinished: function DD_removeFinished()
  {
    let promiseList = Downloads.getList(this._isPrivate ? Downloads.PRIVATE
                                                        : Downloads.PUBLIC);
    promiseList.then(list => list.removeFinished())
               .then(null, Cu.reportError);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Integration with the asynchronous Downloads back-end

  onDownloadAdded: function (aDownload)
  {
    let dataItem = new DownloadsDataItem(aDownload);
    this._downloadToDataItemMap.set(aDownload, dataItem);
    this.dataItems[dataItem.downloadGuid] = dataItem;

    for (let view of this._views) {
      view.onDataItemAdded(dataItem, true);
    }

    this._updateDataItemState(dataItem);
  },

  onDownloadChanged: function (aDownload)
  {
    let dataItem = this._downloadToDataItemMap.get(aDownload);
    if (!dataItem) {
      Cu.reportError("Download doesn't exist.");
      return;
    }

    this._updateDataItemState(dataItem);
  },

  onDownloadRemoved: function (aDownload)
  {
    let dataItem = this._downloadToDataItemMap.get(aDownload);
    if (!dataItem) {
      Cu.reportError("Download doesn't exist.");
      return;
    }

    this._downloadToDataItemMap.delete(aDownload);
    this.dataItems[dataItem.downloadGuid] = null;
    for (let view of this._views) {
      view.onDataItemRemoved(dataItem);
    }
  },

  /**
   * Updates the given data item and sends related notifications.
   */
  _updateDataItemState: function (aDataItem)
  {
    let oldState = aDataItem.state;
    let wasInProgress = aDataItem.inProgress;
    let wasDone = aDataItem.done;

    aDataItem.updateFromDownload();

    if (wasInProgress && !aDataItem.inProgress) {
      aDataItem.endTime = Date.now();
    }

    if (oldState != aDataItem.state) {
      for (let view of this._views) {
        try {
          view.getViewItem(aDataItem).onStateChange(oldState);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }

      // This state transition code should actually be located in a Downloads
      // API module (bug 941009).  Moreover, the fact that state is stored as
      // annotations should be ideally hidden behind methods of
      // nsIDownloadHistory (bug 830415).
      if (!this._isPrivate && !aDataItem.inProgress) {
        try {
          let downloadMetaData = { state: aDataItem.state,
                                   endTime: aDataItem.endTime };
          if (aDataItem.done) {
            downloadMetaData.fileSize = aDataItem.maxBytes;
          }

          PlacesUtils.annotations.setPageAnnotation(
                        NetUtil.newURI(aDataItem.uri), "downloads/metaData",
                        JSON.stringify(downloadMetaData), 0,
                        PlacesUtils.annotations.EXPIRE_WITH_HISTORY);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    }

    if (!aDataItem.newDownloadNotified) {
      aDataItem.newDownloadNotified = true;
      this._notifyDownloadEvent("start");
    }

    if (!wasDone && aDataItem.done) {
      this._notifyDownloadEvent("finish");
    }

    for (let view of this._views) {
      view.getViewItem(aDataItem).onProgressChange();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Registration of views

  /**
   * Adds an object to be notified when the available download data changes.
   * The specified object is initialized with the currently available downloads.
   *
   * @param aView
   *        DownloadsView object to be added.  This reference must be passed to
   *        removeView before termination.
   */
  addView: function DD_addView(aView)
  {
    this._views.push(aView);
    this._updateView(aView);
  },

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        DownloadsView object to be removed.
   */
  removeView: function DD_removeView(aView)
  {
    let index = this._views.indexOf(aView);
    if (index != -1) {
      this._views.splice(index, 1);
    }
  },

  /**
   * Ensures that the currently loaded data is added to the specified view.
   *
   * @param aView
   *        DownloadsView object to be initialized.
   */
  _updateView: function DD_updateView(aView)
  {
    // Indicate to the view that a batch loading operation is in progress.
    aView.onDataLoadStarting();

    // Sort backwards by start time, ensuring that the most recent
    // downloads are added first regardless of their state.
    let loadedItemsArray = [dataItem
                            for each (dataItem in this.dataItems)
                            if (dataItem)];
    loadedItemsArray.sort(function(a, b) b.startTime - a.startTime);
    loadedItemsArray.forEach(
      function (dataItem) aView.onDataItemAdded(dataItem, false)
    );

    // Notify the view that all data is available.
    aView.onDataLoadCompleted();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Notifications sent to the most recent browser window only

  /**
   * Set to true after the first download causes the downloads panel to be
   * displayed.
   */
  get panelHasShownBefore() {
    try {
      return Services.prefs.getBoolPref("browser.download.panel.shown");
    } catch (ex) { }
    return false;
  },

  set panelHasShownBefore(aValue) {
    Services.prefs.setBoolPref("browser.download.panel.shown", aValue);
    return aValue;
  },

  /**
   * Displays a new or finished download notification in the most recent browser
   * window, if one is currently available with the required privacy type.
   *
   * @param aType
   *        Set to "start" for new downloads, "finish" for completed downloads.
   */
  _notifyDownloadEvent: function DD_notifyDownloadEvent(aType)
  {
    DownloadsCommon.log("Attempting to notify that a new download has started or finished.");

    // Show the panel in the most recent browser window, if present.
    let browserWin = RecentWindow.getMostRecentBrowserWindow({ private: this._isPrivate });
    if (!browserWin) {
      return;
    }

    if (this.panelHasShownBefore) {
      // For new downloads after the first one, don't show the panel
      // automatically, but provide a visible notification in the topmost
      // browser window, if the status indicator is already visible.
      DownloadsCommon.log("Showing new download notification.");
      browserWin.DownloadsIndicatorView.showEventNotification(aType);
      return;
    }
    this.panelHasShownBefore = true;
    browserWin.DownloadsPanel.showPanel();
  }
};

XPCOMUtils.defineLazyGetter(this, "PrivateDownloadsData", function() {
  return new DownloadsDataCtor(true);
});

XPCOMUtils.defineLazyGetter(this, "DownloadsData", function() {
  return new DownloadsDataCtor(false);
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsDataItem

/**
 * Represents a single item in the list of downloads.
 *
 * The endTime property is initialized to the current date and time.
 *
 * @param aDownload
 *        The Download object with the current state.
 */
function DownloadsDataItem(aDownload)
{
  this._download = aDownload;

  this.downloadGuid = "id:" + this._autoIncrementId;
  this.file = aDownload.target.path;
  this.target = OS.Path.basename(aDownload.target.path);
  this.uri = aDownload.source.url;
  this.endTime = Date.now();

  this.updateFromDownload();
}

DownloadsDataItem.prototype = {
  /**
   * The JavaScript API does not need identifiers for Download objects, so they
   * are generated sequentially for the corresponding DownloadDataItem.
   */
  get _autoIncrementId() ++DownloadsDataItem.prototype.__lastId,
  __lastId: 0,

  /**
   * Updates this object from the underlying Download object.
   */
  updateFromDownload: function ()
  {
    // Collapse state using the correct priority.
    if (this._download.succeeded) {
      this.state = nsIDM.DOWNLOAD_FINISHED;
    } else if (this._download.error &&
               this._download.error.becauseBlockedByParentalControls) {
      this.state = nsIDM.DOWNLOAD_BLOCKED_PARENTAL;
    } else if (this._download.error &&
               this._download.error.becauseBlockedByReputationCheck) {
      this.state = nsIDM.DOWNLOAD_DIRTY;
    } else if (this._download.error) {
      this.state = nsIDM.DOWNLOAD_FAILED;
    } else if (this._download.canceled && this._download.hasPartialData) {
      this.state = nsIDM.DOWNLOAD_PAUSED;
    } else if (this._download.canceled) {
      this.state = nsIDM.DOWNLOAD_CANCELED;
    } else if (this._download.stopped) {
      this.state = nsIDM.DOWNLOAD_NOTSTARTED;
    } else {
      this.state = nsIDM.DOWNLOAD_DOWNLOADING;
    }

    this.referrer = this._download.source.referrer;
    this.startTime = this._download.startTime;
    this.currBytes = this._download.currentBytes;
    this.resumable = this._download.hasPartialData;
    this.speed = this._download.speed;

    if (this._download.succeeded) {
      // If the download succeeded, show the final size if available, otherwise
      // use the last known number of bytes transferred.  The final size on disk
      // will be available when bug 941063 is resolved.
      this.maxBytes = this._download.hasProgress ?
                             this._download.totalBytes :
                             this._download.currentBytes;
      this.percentComplete = 100;
    } else if (this._download.hasProgress) {
      // If the final size and progress are known, use them.
      this.maxBytes = this._download.totalBytes;
      this.percentComplete = this._download.progress;
    } else {
      // The download final size and progress percentage is unknown.
      this.maxBytes = -1;
      this.percentComplete = -1;
    }
  },

  /**
   * Indicates whether the download is proceeding normally, and not finished
   * yet.  This includes paused downloads.  When this property is true, the
   * "progress" property represents the current progress of the download.
   */
  get inProgress()
  {
    return [
      nsIDM.DOWNLOAD_NOTSTARTED,
      nsIDM.DOWNLOAD_QUEUED,
      nsIDM.DOWNLOAD_DOWNLOADING,
      nsIDM.DOWNLOAD_PAUSED,
      nsIDM.DOWNLOAD_SCANNING,
    ].indexOf(this.state) != -1;
  },

  /**
   * This is true during the initial phases of a download, before the actual
   * download of data bytes starts.
   */
  get starting()
  {
    return this.state == nsIDM.DOWNLOAD_NOTSTARTED ||
           this.state == nsIDM.DOWNLOAD_QUEUED;
  },

  /**
   * Indicates whether the download is paused.
   */
  get paused()
  {
    return this.state == nsIDM.DOWNLOAD_PAUSED;
  },

  /**
   * Indicates whether the download is in a final state, either because it
   * completed successfully or because it was blocked.
   */
  get done()
  {
    return [
      nsIDM.DOWNLOAD_FINISHED,
      nsIDM.DOWNLOAD_BLOCKED_PARENTAL,
      nsIDM.DOWNLOAD_BLOCKED_POLICY,
      nsIDM.DOWNLOAD_DIRTY,
    ].indexOf(this.state) != -1;
  },

  /**
   * Indicates whether the download is finished and can be opened.
   */
  get openable()
  {
    return this.state == nsIDM.DOWNLOAD_FINISHED;
  },

  /**
   * Indicates whether the download stopped because of an error, and can be
   * resumed manually.
   */
  get canRetry()
  {
    return this.state == nsIDM.DOWNLOAD_CANCELED ||
           this.state == nsIDM.DOWNLOAD_FAILED;
  },

  /**
   * Returns the nsILocalFile for the download target.
   *
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   */
  get localFile()
  {
    return this._getFile(this.file);
  },

  /**
   * Returns the nsILocalFile for the partially downloaded target.
   *
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   */
  get partFile()
  {
    return this._getFile(this.file + kPartialDownloadSuffix);
  },

  /**
   * Returns an nsILocalFile for aFilename. aFilename might be a file URL or
   * a native path.
   *
   * @param aFilename the filename of the file to retrieve.
   * @return an nsILocalFile for the file.
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   * @note This function makes no guarantees about the file's existence -
   *       callers should check that the returned file exists.
   */
  _getFile: function DDI__getFile(aFilename)
  {
    // The download database may contain targets stored as file URLs or native
    // paths.  This can still be true for previously stored items, even if new
    // items are stored using their file URL.  See also bug 239948 comment 12.
    if (aFilename.startsWith("file:")) {
      // Assume the file URL we obtained from the downloads database or from the
      // "spec" property of the target has the UTF-8 charset.
      let fileUrl = NetUtil.newURI(aFilename).QueryInterface(Ci.nsIFileURL);
      return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
    } else {
      // The downloads database contains a native path.  Try to create a local
      // file, though this may throw an exception if the path is invalid.
      return new DownloadsLocalFileCtor(aFilename);
    }
  },

  /**
   * Open the target file for this download.
   */
  openLocalFile: function () {
    this._download.launch().then(null, Cu.reportError);
  },

  /**
   * Show the downloaded file in the system file manager.
   */
  showLocalFile: function DDI_showLocalFile() {
    DownloadsCommon.showDownloadedFile(this.localFile);
  },

  /**
   * Resumes the download if paused, pauses it if active.
   * @throws if the download is not resumable or if has already done.
   */
  togglePauseResume: function DDI_togglePauseResume() {
    if (this._download.stopped) {
      this._download.start();
    } else {
      this._download.cancel();
    }
  },

  /**
   * Attempts to retry the download.
   * @throws if we cannot.
   */
  retry: function DDI_retry() {
    this._download.start();
  },

  /**
   * Cancels the download.
   */
  cancel: function() {
    this._download.cancel();
    this._download.removePartialData().then(null, Cu.reportError);
  },

  /**
   * Remove the download.
   */
  remove: function DDI_remove() {
    Downloads.getList(Downloads.ALL)
             .then(list => list.remove(this._download))
             .then(() => this._download.finalize(true))
             .then(null, Cu.reportError);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsViewPrototype

/**
 * A prototype for an object that registers itself with DownloadsData as soon
 * as a view is registered with it.
 */
const DownloadsViewPrototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// Registration of views

  /**
   * Array of view objects that should be notified when the available status
   * data changes.
   *
   * SUBCLASSES MUST OVERRIDE THIS PROPERTY.
   */
  _views: null,

  /**
   * Determines whether this view object is over the private or non-private
   * downloads.
   *
   * SUBCLASSES MUST OVERRIDE THIS PROPERTY.
   */
  _isPrivate: false,

  /**
   * Adds an object to be notified when the available status data changes.
   * The specified object is initialized with the currently available status.
   *
   * @param aView
   *        View object to be added.  This reference must be
   *        passed to removeView before termination.
   */
  addView: function DVP_addView(aView)
  {
    // Start receiving events when the first of our views is registered.
    if (this._views.length == 0) {
      if (this._isPrivate) {
        PrivateDownloadsData.addView(this);
      } else {
        DownloadsData.addView(this);
      }
    }

    this._views.push(aView);
    this.refreshView(aView);
  },

  /**
   * Updates the properties of an object previously added using addView.
   *
   * @param aView
   *        View object to be updated.
   */
  refreshView: function DVP_refreshView(aView)
  {
    // Update immediately even if we are still loading data asynchronously.
    // Subclasses must provide these two functions!
    this._refreshProperties();
    this._updateView(aView);
  },

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        View object to be removed.
   */
  removeView: function DVP_removeView(aView)
  {
    let index = this._views.indexOf(aView);
    if (index != -1) {
      this._views.splice(index, 1);
    }

    // Stop receiving events when the last of our views is unregistered.
    if (this._views.length == 0) {
      if (this._isPrivate) {
        PrivateDownloadsData.removeView(this);
      } else {
        DownloadsData.removeView(this);
      }
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsData

  /**
   * Indicates whether we are still loading downloads data asynchronously.
   */
  _loading: false,

  /**
   * Called before multiple downloads are about to be loaded.
   */
  onDataLoadStarting: function DVP_onDataLoadStarting()
  {
    this._loading = true;
  },

  /**
   * Called after data loading finished.
   */
  onDataLoadCompleted: function DVP_onDataLoadCompleted()
  {
    this._loading = false;
  },

  /**
   * Called when a new download data item is available, either during the
   * asynchronous data load or when a new download is started.
   *
   * @param aDataItem
   *        DownloadsDataItem object that was just added.
   * @param aNewest
   *        When true, indicates that this item is the most recent and should be
   *        added in the topmost position.  This happens when a new download is
   *        started.  When false, indicates that the item is the least recent
   *        with regard to the items that have been already added. The latter
   *        generally happens during the asynchronous data load.
   *
   * @note Subclasses should override this.
   */
  onDataItemAdded: function DVP_onDataItemAdded(aDataItem, aNewest)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Called when a data item is removed, ensures that the widget associated with
   * the view item is removed from the user interface.
   *
   * @param aDataItem
   *        DownloadsDataItem object that is being removed.
   *
   * @note Subclasses should override this.
   */
  onDataItemRemoved: function DVP_onDataItemRemoved(aDataItem)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Returns the view item associated with the provided data item for this view.
   *
   * @param aDataItem
   *        DownloadsDataItem object for which the view item is requested.
   *
   * @return Object that can be used to notify item status events.
   *
   * @note Subclasses should override this.
   */
  getViewItem: function DID_getViewItem(aDataItem)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Private function used to refresh the internal properties being sent to
   * each registered view.
   *
   * @note Subclasses should override this.
   */
  _refreshProperties: function DID_refreshProperties()
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Private function used to refresh an individual view.
   *
   * @note Subclasses should override this.
   */
  _updateView: function DID_updateView()
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsIndicatorData

/**
 * This object registers itself with DownloadsData as a view, and transforms the
 * notifications it receives into overall status data, that is then broadcast to
 * the registered download status indicators.
 *
 * Note that using this object does not automatically start the Download Manager
 * service.  Consumers will see an empty list of downloads until the service is
 * actually started.  This is useful to display a neutral progress indicator in
 * the main browser window until the autostart timeout elapses.
 */
function DownloadsIndicatorDataCtor(aPrivate) {
  this._isPrivate = aPrivate;
  this._views = [];
}
DownloadsIndicatorDataCtor.prototype = {
  __proto__: DownloadsViewPrototype,

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        DownloadsIndicatorView object to be removed.
   */
  removeView: function DID_removeView(aView)
  {
    DownloadsViewPrototype.removeView.call(this, aView);

    if (this._views.length == 0) {
      this._itemCount = 0;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsData

  /**
   * Called after data loading finished.
   */
  onDataLoadCompleted: function DID_onDataLoadCompleted()
  {
    DownloadsViewPrototype.onDataLoadCompleted.call(this);
    this._updateViews();
  },

  /**
   * Called when a new download data item is available, either during the
   * asynchronous data load or when a new download is started.
   *
   * @param aDataItem
   *        DownloadsDataItem object that was just added.
   * @param aNewest
   *        When true, indicates that this item is the most recent and should be
   *        added in the topmost position.  This happens when a new download is
   *        started.  When false, indicates that the item is the least recent
   *        with regard to the items that have been already added. The latter
   *        generally happens during the asynchronous data load.
   */
  onDataItemAdded: function DID_onDataItemAdded(aDataItem, aNewest)
  {
    this._itemCount++;
    this._updateViews();
  },

  /**
   * Called when a data item is removed, ensures that the widget associated with
   * the view item is removed from the user interface.
   *
   * @param aDataItem
   *        DownloadsDataItem object that is being removed.
   */
  onDataItemRemoved: function DID_onDataItemRemoved(aDataItem)
  {
    this._itemCount--;
    this._updateViews();
  },

  /**
   * Returns the view item associated with the provided data item for this view.
   *
   * @param aDataItem
   *        DownloadsDataItem object for which the view item is requested.
   *
   * @return Object that can be used to notify item status events.
   */
  getViewItem: function DID_getViewItem(aDataItem)
  {
    let data = this._isPrivate ? PrivateDownloadsIndicatorData
                               : DownloadsIndicatorData;
    return Object.freeze({
      onStateChange: function DIVI_onStateChange(aOldState)
      {
        if (aDataItem.state == nsIDM.DOWNLOAD_FINISHED ||
            aDataItem.state == nsIDM.DOWNLOAD_FAILED) {
          data.attention = true;
        }

        // Since the state of a download changed, reset the estimated time left.
        data._lastRawTimeLeft = -1;
        data._lastTimeLeft = -1;

        data._updateViews();
      },
      onProgressChange: function DIVI_onProgressChange()
      {
        data._updateViews();
      }
    });
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Propagation of properties to our views

  // The following properties are updated by _refreshProperties and are then
  // propagated to the views.  See _refreshProperties for details.
  _hasDownloads: false,
  _counter: "",
  _percentComplete: -1,
  _paused: false,

  /**
   * Indicates whether the download indicators should be highlighted.
   */
  set attention(aValue)
  {
    this._attention = aValue;
    this._updateViews();
    return aValue;
  },
  _attention: false,

  /**
   * Indicates whether the user is interacting with downloads, thus the
   * attention indication should not be shown even if requested.
   */
  set attentionSuppressed(aValue)
  {
    this._attentionSuppressed = aValue;
    this._attention = false;
    this._updateViews();
    return aValue;
  },
  _attentionSuppressed: false,

  /**
   * Computes aggregate values and propagates the changes to our views.
   */
  _updateViews: function DID_updateViews()
  {
    // Do not update the status indicators during batch loads of download items.
    if (this._loading) {
      return;
    }

    this._refreshProperties();
    this._views.forEach(this._updateView, this);
  },

  /**
   * Updates the specified view with the current aggregate values.
   *
   * @param aView
   *        DownloadsIndicatorView object to be updated.
   */
  _updateView: function DID_updateView(aView)
  {
    aView.hasDownloads = this._hasDownloads;
    aView.counter = this._counter;
    aView.percentComplete = this._percentComplete;
    aView.paused = this._paused;
    aView.attention = this._attention && !this._attentionSuppressed;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Property updating based on current download status

  /**
   * Number of download items that are available to be displayed.
   */
  _itemCount: 0,

  /**
   * Floating point value indicating the last number of seconds estimated until
   * the longest download will finish.  We need to store this value so that we
   * don't continuously apply smoothing if the actual download state has not
   * changed.  This is set to -1 if the previous value is unknown.
   */
  _lastRawTimeLeft: -1,

  /**
   * Last number of seconds estimated until all in-progress downloads with a
   * known size and speed will finish.  This value is stored to allow smoothing
   * in case of small variations.  This is set to -1 if the previous value is
   * unknown.
   */
  _lastTimeLeft: -1,

  /**
   * A generator function for the dataItems that this summary is currently
   * interested in. This generator is passed off to summarizeDownloads in order
   * to generate statistics about the dataItems we care about - in this case,
   * it's all dataItems for active downloads.
   */
  _activeDataItems: function DID_activeDataItems()
  {
    let dataItems = this._isPrivate ? PrivateDownloadsData.dataItems
                                    : DownloadsData.dataItems;
    for each (let dataItem in dataItems) {
      if (dataItem && dataItem.inProgress) {
        yield dataItem;
      }
    }
  },

  /**
   * Computes aggregate values based on the current state of downloads.
   */
  _refreshProperties: function DID_refreshProperties()
  {
    let summary =
      DownloadsCommon.summarizeDownloads(this._activeDataItems());

    // Determine if the indicator should be shown or get attention.
    this._hasDownloads = (this._itemCount > 0);

    // If all downloads are paused, show the progress indicator as paused.
    this._paused = summary.numActive > 0 &&
                   summary.numActive == summary.numPaused;

    this._percentComplete = summary.percentComplete;

    // Display the estimated time left, if present.
    if (summary.rawTimeLeft == -1) {
      // There are no downloads with a known time left.
      this._lastRawTimeLeft = -1;
      this._lastTimeLeft = -1;
      this._counter = "";
    } else {
      // Compute the new time left only if state actually changed.
      if (this._lastRawTimeLeft != summary.rawTimeLeft) {
        this._lastRawTimeLeft = summary.rawTimeLeft;
        this._lastTimeLeft = DownloadsCommon.smoothSeconds(summary.rawTimeLeft,
                                                           this._lastTimeLeft);
      }
      this._counter = DownloadsCommon.formatTimeLeft(this._lastTimeLeft);
    }
  }
};

XPCOMUtils.defineLazyGetter(this, "PrivateDownloadsIndicatorData", function() {
  return new DownloadsIndicatorDataCtor(true);
});

XPCOMUtils.defineLazyGetter(this, "DownloadsIndicatorData", function() {
  return new DownloadsIndicatorDataCtor(false);
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsSummaryData

/**
 * DownloadsSummaryData is a view for DownloadsData that produces a summary
 * of all downloads after a certain exclusion point aNumToExclude. For example,
 * if there were 5 downloads in progress, and a DownloadsSummaryData was
 * constructed with aNumToExclude equal to 3, then that DownloadsSummaryData
 * would produce a summary of the last 2 downloads.
 *
 * @param aIsPrivate
 *        True if the browser window which owns the download button is a private
 *        window.
 * @param aNumToExclude
 *        The number of items to exclude from the summary, starting from the
 *        top of the list.
 */
function DownloadsSummaryData(aIsPrivate, aNumToExclude) {
  this._numToExclude = aNumToExclude;
  // Since we can have multiple instances of DownloadsSummaryData, we
  // override these values from the prototype so that each instance can be
  // completely separated from one another.
  this._loading = false;

  this._dataItems = [];

  // Floating point value indicating the last number of seconds estimated until
  // the longest download will finish.  We need to store this value so that we
  // don't continuously apply smoothing if the actual download state has not
  // changed.  This is set to -1 if the previous value is unknown.
  this._lastRawTimeLeft = -1;

  // Last number of seconds estimated until all in-progress downloads with a
  // known size and speed will finish.  This value is stored to allow smoothing
  // in case of small variations.  This is set to -1 if the previous value is
  // unknown.
  this._lastTimeLeft = -1;

  // The following properties are updated by _refreshProperties and are then
  // propagated to the views.
  this._showingProgress = false;
  this._details = "";
  this._description = "";
  this._numActive = 0;
  this._percentComplete = -1;

  this._isPrivate = aIsPrivate;
  this._views = [];
}

DownloadsSummaryData.prototype = {
  __proto__: DownloadsViewPrototype,

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        DownloadsSummary view to be removed.
   */
  removeView: function DSD_removeView(aView)
  {
    DownloadsViewPrototype.removeView.call(this, aView);

    if (this._views.length == 0) {
      // Clear out our collection of DownloadDataItems. If we ever have
      // another view registered with us, this will get re-populated.
      this._dataItems = [];
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsData - see the documentation in
  //// DownloadsViewPrototype for more information on what these functions
  //// are used for.

  onDataLoadCompleted: function DSD_onDataLoadCompleted()
  {
    DownloadsViewPrototype.onDataLoadCompleted.call(this);
    this._updateViews();
  },

  onDataItemAdded: function DSD_onDataItemAdded(aDataItem, aNewest)
  {
    if (aNewest) {
      this._dataItems.unshift(aDataItem);
    } else {
      this._dataItems.push(aDataItem);
    }

    this._updateViews();
  },

  onDataItemRemoved: function DSD_onDataItemRemoved(aDataItem)
  {
    let itemIndex = this._dataItems.indexOf(aDataItem);
    this._dataItems.splice(itemIndex, 1);
    this._updateViews();
  },

  getViewItem: function DSD_getViewItem(aDataItem)
  {
    let self = this;
    return Object.freeze({
      onStateChange: function DIVI_onStateChange(aOldState)
      {
        // Since the state of a download changed, reset the estimated time left.
        self._lastRawTimeLeft = -1;
        self._lastTimeLeft = -1;
        self._updateViews();
      },
      onProgressChange: function DIVI_onProgressChange()
      {
        self._updateViews();
      }
    });
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Propagation of properties to our views

  /**
   * Computes aggregate values and propagates the changes to our views.
   */
  _updateViews: function DSD_updateViews()
  {
    // Do not update the status indicators during batch loads of download items.
    if (this._loading) {
      return;
    }

    this._refreshProperties();
    this._views.forEach(this._updateView, this);
  },

  /**
   * Updates the specified view with the current aggregate values.
   *
   * @param aView
   *        DownloadsIndicatorView object to be updated.
   */
  _updateView: function DSD_updateView(aView)
  {
    aView.showingProgress = this._showingProgress;
    aView.percentComplete = this._percentComplete;
    aView.description = this._description;
    aView.details = this._details;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Property updating based on current download status

  /**
   * A generator function for the dataItems that this summary is currently
   * interested in. This generator is passed off to summarizeDownloads in order
   * to generate statistics about the dataItems we care about - in this case,
   * it's the dataItems in this._dataItems after the first few to exclude,
   * which was set when constructing this DownloadsSummaryData instance.
   */
  _dataItemsForSummary: function DSD_dataItemsForSummary()
  {
    if (this._dataItems.length > 0) {
      for (let i = this._numToExclude; i < this._dataItems.length; ++i) {
        yield this._dataItems[i];
      }
    }
  },

  /**
   * Computes aggregate values based on the current state of downloads.
   */
  _refreshProperties: function DSD_refreshProperties()
  {
    // Pre-load summary with default values.
    let summary =
      DownloadsCommon.summarizeDownloads(this._dataItemsForSummary());

    this._description = DownloadsCommon.strings
                                       .otherDownloads2(summary.numActive);
    this._percentComplete = summary.percentComplete;

    // If all downloads are paused, show the progress indicator as paused.
    this._showingProgress = summary.numDownloading > 0 ||
                            summary.numPaused > 0;

    // Display the estimated time left, if present.
    if (summary.rawTimeLeft == -1) {
      // There are no downloads with a known time left.
      this._lastRawTimeLeft = -1;
      this._lastTimeLeft = -1;
      this._details = "";
    } else {
      // Compute the new time left only if state actually changed.
      if (this._lastRawTimeLeft != summary.rawTimeLeft) {
        this._lastRawTimeLeft = summary.rawTimeLeft;
        this._lastTimeLeft = DownloadsCommon.smoothSeconds(summary.rawTimeLeft,
                                                           this._lastTimeLeft);
      }
      [this._details] = DownloadUtils.getDownloadStatusNoRate(
        summary.totalTransferred, summary.totalSize, summary.slowestSpeed,
        this._lastTimeLeft);
    }
  }
}

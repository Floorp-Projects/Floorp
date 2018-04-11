/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
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
 * Downloads API data, and provides asynchronous notifications allowing
 * to build a consistent view of the available data.
 *
 * DownloadsIndicatorData
 * This object registers itself with DownloadsData as a view, and transforms the
 * notifications it receives into overall status data, that is then broadcast to
 * the registered download status indicators.
 */

// Globals

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  DownloadHistory: "resource://gre/modules/DownloadHistory.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  DownloadUIHelper: "resource://gre/modules/DownloadUIHelper.jsm",
  DownloadUtils: "resource://gre/modules/DownloadUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "DownloadsLogger", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  let consoleOptions = {
    maxLogLevelPref: "browser.download.loglevel",
    prefix: "Downloads"
  };
  return new ConsoleAPI(consoleOptions);
});

const kDownloadsStringBundleUrl =
  "chrome://browser/locale/downloads/downloads.properties";

const kDownloadsStringsRequiringFormatting = {
  sizeWithUnits: true,
  statusSeparator: true,
  statusSeparatorBeforeNumber: true,
  fileExecutableSecurityWarning: true
};

const kDownloadsStringsRequiringPluralForm = {
  otherDownloads3: true
};

const kMaxHistoryResultsForLimitedView = 42;

const kPrefBranch = Services.prefs.getBranch("browser.download.");

var PrefObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
  getPref(name) {
    try {
      switch (typeof this.prefs[name]) {
        case "boolean":
          return kPrefBranch.getBoolPref(name);
      }
    } catch (ex) { }
    return this.prefs[name];
  },
  observe(aSubject, aTopic, aData) {
    if (this.prefs.hasOwnProperty(aData)) {
      delete this[aData];
      this[aData] = this.getPref(aData);
    }
  },
  register(prefs) {
    this.prefs = prefs;
    kPrefBranch.addObserver("", this, true);
    for (let key in prefs) {
      let name = key;
      XPCOMUtils.defineLazyGetter(this, name, function() {
        return PrefObserver.getPref(name);
      });
    }
  },
};

PrefObserver.register({
  // prefName: defaultValue
  animateNotifications: true,
});


// DownloadsCommon

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides shared methods for all the instances of the user interface.
 */
var DownloadsCommon = {
  // The following legacy constants are still returned by stateOfDownload, but
  // individual properties of the Download object should normally be used.
  DOWNLOAD_NOTSTARTED: -1,
  DOWNLOAD_DOWNLOADING: 0,
  DOWNLOAD_FINISHED: 1,
  DOWNLOAD_FAILED: 2,
  DOWNLOAD_CANCELED: 3,
  DOWNLOAD_PAUSED: 4,
  DOWNLOAD_BLOCKED_PARENTAL: 6,
  DOWNLOAD_DIRTY: 8,
  DOWNLOAD_BLOCKED_POLICY: 9,

  // The following are the possible values of the "attention" property.
  ATTENTION_NONE: "",
  ATTENTION_SUCCESS: "success",
  ATTENTION_WARNING: "warning",
  ATTENTION_SEVERE: "severe",

  /**
   * Returns an object whose keys are the string names from the downloads string
   * bundle, and whose values are either the translated strings or functions
   * returning formatted strings.
   */
  get strings() {
    let strings = {};
    let sb = Services.strings.createBundle(kDownloadsStringBundleUrl);
    let enumerator = sb.getSimpleEnumeration();
    while (enumerator.hasMoreElements()) {
      let string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
      let stringName = string.key;
      if (stringName in kDownloadsStringsRequiringFormatting) {
        strings[stringName] = function() {
          // Convert "arguments" to a real array before calling into XPCOM.
          return sb.formatStringFromName(stringName,
                                         Array.slice(arguments, 0),
                                         arguments.length);
        };
      } else if (stringName in kDownloadsStringsRequiringPluralForm) {
        strings[stringName] = function(aCount) {
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
   * Indicates whether we should show visual notification on the indicator
   * when a download event is triggered.
   */
  get animateNotifications() {
    return PrefObserver.animateNotifications;
  },

  /**
   * Get access to one of the DownloadsData, PrivateDownloadsData, or
   * HistoryDownloadsData objects, depending on the privacy status of the
   * specified window and on whether history downloads should be included.
   *
   * @param window
   *        The browser window which owns the download button.
   * @param [optional] history
   *        True to include history downloads when the window is public.
   * @param [optional] privateAll
   *        Whether to force the public downloads data to be returned together
   *        with the private downloads data for a private window.
   * @param [optional] limited
   *        True to limit the amount of downloads returned to
   *        `kMaxHistoryResultsForLimitedView`.
   */
  getData(window, history = false, privateAll = false, limited = false) {
    let isPrivate = PrivateBrowsingUtils.isContentWindowPrivate(window);
    if (isPrivate && !privateAll) {
      return PrivateDownloadsData;
    }
    if (history) {
      if (isPrivate && privateAll)
        return LimitedPrivateHistoryDownloadData;
      return limited ? LimitedHistoryDownloadsData : HistoryDownloadsData;
    }
    return DownloadsData;
  },

  /**
   * Initializes the Downloads back-end and starts receiving events for both the
   * private and non-private downloads data objects.
   */
  initializeAllDataLinks() {
    DownloadsData.initializeDataLink();
    PrivateDownloadsData.initializeDataLink();
  },

  /**
   * Get access to one of the DownloadsIndicatorData or
   * PrivateDownloadsIndicatorData objects, depending on the privacy status of
   * the window in question.
   */
  getIndicatorData(aWindow) {
    if (PrivateBrowsingUtils.isContentWindowPrivate(aWindow)) {
      return PrivateDownloadsIndicatorData;
    }
    return DownloadsIndicatorData;
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
  getSummary(aWindow, aNumToExclude) {
    if (PrivateBrowsingUtils.isContentWindowPrivate(aWindow)) {
      if (this._privateSummary) {
        return this._privateSummary;
      }
      return this._privateSummary = new DownloadsSummaryData(true, aNumToExclude);
    }
    if (this._summary) {
      return this._summary;
    }
    return this._summary = new DownloadsSummaryData(false, aNumToExclude);
  },
  _summary: null,
  _privateSummary: null,

  /**
   * Returns the legacy state integer value for the provided Download object.
   */
  stateOfDownload(download) {
    // Collapse state using the correct priority.
    if (!download.stopped) {
      return DownloadsCommon.DOWNLOAD_DOWNLOADING;
    }
    if (download.succeeded) {
      return DownloadsCommon.DOWNLOAD_FINISHED;
    }
    if (download.error) {
      if (download.error.becauseBlockedByParentalControls) {
        return DownloadsCommon.DOWNLOAD_BLOCKED_PARENTAL;
      }
      if (download.error.becauseBlockedByReputationCheck) {
        return DownloadsCommon.DOWNLOAD_DIRTY;
      }
      return DownloadsCommon.DOWNLOAD_FAILED;
    }
    if (download.canceled) {
      if (download.hasPartialData) {
        return DownloadsCommon.DOWNLOAD_PAUSED;
      }
      return DownloadsCommon.DOWNLOAD_CANCELED;
    }
    return DownloadsCommon.DOWNLOAD_NOTSTARTED;
  },

  /**
   * Given an iterable collection of Download objects, generates and returns
   * statistics about that collection.
   *
   * @param downloads An iterable collection of Download objects.
   *
   * @return Object whose properties are the generated statistics. Currently,
   *         we return the following properties:
   *
   *         numActive       : The total number of downloads.
   *         numPaused       : The total number of paused downloads.
   *         numDownloading  : The total number of downloads being downloaded.
   *         totalSize       : The total size of all downloads once completed.
   *         totalTransferred: The total amount of transferred data for these
   *                           downloads.
   *         slowestSpeed    : The slowest download rate.
   *         rawTimeLeft     : The estimated time left for the downloads to
   *                           complete.
   *         percentComplete : The percentage of bytes successfully downloaded.
   */
  summarizeDownloads(downloads) {
    let summary = {
      numActive: 0,
      numPaused: 0,
      numDownloading: 0,
      totalSize: 0,
      totalTransferred: 0,
      // slowestSpeed is Infinity so that we can use Math.min to
      // find the slowest speed. We'll set this to 0 afterwards if
      // it's still at Infinity by the time we're done iterating all
      // download.
      slowestSpeed: Infinity,
      rawTimeLeft: -1,
      percentComplete: -1
    };

    for (let download of downloads) {
      summary.numActive++;

      if (!download.stopped) {
        summary.numDownloading++;
        if (download.hasProgress && download.speed > 0) {
          let sizeLeft = download.totalBytes - download.currentBytes;
          summary.rawTimeLeft = Math.max(summary.rawTimeLeft,
                                         sizeLeft / download.speed);
          summary.slowestSpeed = Math.min(summary.slowestSpeed,
                                          download.speed);
        }
      } else if (download.canceled && download.hasPartialData) {
        summary.numPaused++;
      }

      // Only add to total values if we actually know the download size.
      if (download.succeeded) {
        summary.totalSize += download.target.size;
        summary.totalTransferred += download.target.size;
      } else if (download.hasProgress) {
        summary.totalSize += download.totalBytes;
        summary.totalTransferred += download.currentBytes;
      }
    }

    if (summary.totalSize != 0) {
      summary.percentComplete =
        Math.floor((summary.totalTransferred / summary.totalSize) * 100);
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
  smoothSeconds(aSeconds, aLastSeconds) {
    // We apply an algorithm similar to the DownloadUtils.getTimeLeft function,
    // though tailored to a single time estimation for all downloads.  We never
    // apply something if the new value is less than half the previous value.
    let shouldApplySmoothing = aLastSeconds >= 0 &&
                               aSeconds > aLastSeconds / 2;
    if (shouldApplySmoothing) {
      // Apply hysteresis to favor downward over upward swings.  Trust only 30%
      // of the new value if lower, and 10% if higher (exponential smoothing).
      let diff = aSeconds - aLastSeconds;
      aSeconds = aLastSeconds + (diff < 0 ? .3 : .1) * diff;

      // If the new time is similar, reuse something close to the last time
      // left, but subtract a little to provide forward progress.
      diff = aSeconds - aLastSeconds;
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
   *
   * @param aFile
   *        the downloaded file to be opened.
   * @param aMimeInfo
   *        the mime type info object.  May be null.
   * @param aOwnerWindow
   *        the window with which this action is associated.
   */
  openDownloadedFile(aFile, aMimeInfo, aOwnerWindow) {
    if (!(aFile instanceof Ci.nsIFile)) {
      throw new Error("aFile must be a nsIFile object");
    }
    if (aMimeInfo && !(aMimeInfo instanceof Ci.nsIMIMEInfo)) {
      throw new Error("Invalid value passed for aMimeInfo");
    }
    if (!(aOwnerWindow instanceof Ci.nsIDOMWindow)) {
      throw new Error("aOwnerWindow must be a dom-window object");
    }

    let isWindowsExe = AppConstants.platform == "win" &&
      aFile.leafName.toLowerCase().endsWith(".exe");

    let promiseShouldLaunch;
    // Don't prompt on Windows for .exe since there will be a native prompt.
    if (aFile.isExecutable() && !isWindowsExe) {
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
      } catch (ex) { }

      // If either we don't have the mime info, or the preferred action failed,
      // attempt to launch the file directly.
      try {
        aFile.launch();
      } catch (ex) {
        // If launch fails, try sending it through the system's external "file:"
        // URL handler.
        Cc["@mozilla.org/uriloader/external-protocol-service;1"]
          .getService(Ci.nsIExternalProtocolService)
          .loadURI(NetUtil.newURI(aFile));
      }
    }).catch(Cu.reportError);
  },

  /**
   * Show a downloaded file in the system file manager.
   *
   * @param aFile
   *        a downloaded file.
   */
  showDownloadedFile(aFile) {
    if (!(aFile instanceof Ci.nsIFile)) {
      throw new Error("aFile must be a nsIFile object");
    }
    try {
      // Show the directory containing the file and select the file.
      aFile.reveal();
    } catch (ex) {
      // If reveal fails for some reason (e.g., it's not implemented on unix
      // or the file doesn't exist), try using the parent if we have it.
      let parent = aFile.parent;
      if (parent) {
        this.showDirectory(parent);
      }
    }
  },

  /**
   * Show the specified folder in the system file manager.
   *
   * @param aDirectory
   *        a directory to be opened with system file manager.
   */
  showDirectory(aDirectory) {
    if (!(aDirectory instanceof Ci.nsIFile)) {
      throw new Error("aDirectory must be a nsIFile object");
    }
    try {
      aDirectory.launch();
    } catch (ex) {
      // If launch fails (probably because it's not implemented), let
      // the OS handler try to open the directory.
      Cc["@mozilla.org/uriloader/external-protocol-service;1"]
        .getService(Ci.nsIExternalProtocolService)
        .loadURI(NetUtil.newURI(aDirectory));
    }
  },

  /**
   * Displays an alert message box which asks the user if they want to
   * unblock the downloaded file or not.
   *
   * @param options
   *        An object with the following properties:
   *        {
   *          verdict:
   *            The detailed reason why the download was blocked, according to
   *            the "Downloads.Error.BLOCK_VERDICT_" constants. If an unknown
   *            reason is specified, "Downloads.Error.BLOCK_VERDICT_MALWARE" is
   *            assumed.
   *          window:
   *            The window with which this action is associated.
   *          dialogType:
   *            String that determines which actions are available:
   *             - "unblock" to offer just "unblock".
   *             - "chooseUnblock" to offer "unblock" and "confirmBlock".
   *             - "chooseOpen" to offer "open" and "confirmBlock".
   *        }
   *
   * @return {Promise}
   * @resolves String representing the action that should be executed:
   *            - "open" to allow the download and open the file.
   *            - "unblock" to allow the download without opening the file.
   *            - "confirmBlock" to delete the blocked data permanently.
   *            - "cancel" to do nothing and cancel the operation.
   */
  async confirmUnblockDownload({ verdict, window,
                                                  dialogType }) {
    let s = DownloadsCommon.strings;

    // All the dialogs have an action button and a cancel button, while only
    // some of them have an additonal button to remove the file. The cancel
    // button must always be the one at BUTTON_POS_1 because this is the value
    // returned by confirmEx when using ESC or closing the dialog (bug 345067).
    let title = s.unblockHeaderUnblock;
    let firstButtonText = s.unblockButtonUnblock;
    let firstButtonAction = "unblock";
    let buttonFlags =
        (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
        (Ci.nsIPrompt.BUTTON_TITLE_CANCEL * Ci.nsIPrompt.BUTTON_POS_1);

    switch (dialogType) {
      case "unblock":
        // Use only the unblock action. The default is to cancel.
        buttonFlags += Ci.nsIPrompt.BUTTON_POS_1_DEFAULT;
        break;
      case "chooseUnblock":
        // Use the unblock and remove file actions. The default is remove file.
        buttonFlags +=
          (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_2) +
          Ci.nsIPrompt.BUTTON_POS_2_DEFAULT;
        break;
      case "chooseOpen":
        // Use the unblock and open file actions. The default is open file.
        title = s.unblockHeaderOpen;
        firstButtonText = s.unblockButtonOpen;
        firstButtonAction = "open";
        buttonFlags +=
          (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_2) +
          Ci.nsIPrompt.BUTTON_POS_0_DEFAULT;
        break;
      default:
        Cu.reportError("Unexpected dialog type: " + dialogType);
        return "cancel";
    }

    let message;
    switch (verdict) {
      case Downloads.Error.BLOCK_VERDICT_UNCOMMON:
        message = s.unblockTypeUncommon2;
        break;
      case Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED:
        message = s.unblockTypePotentiallyUnwanted2;
        break;
      default: // Assume Downloads.Error.BLOCK_VERDICT_MALWARE
        message = s.unblockTypeMalware;
        break;
    }
    message += "\n\n" + s.unblockTip2;

    Services.ww.registerNotification(function onOpen(subj, topic) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        // Make sure to listen for "DOMContentLoaded" because it is fired
        // before the "load" event.
        subj.addEventListener("DOMContentLoaded", function() {
          if (subj.document.documentURI ==
              "chrome://global/content/commonDialog.xul") {
            Services.ww.unregisterNotification(onOpen);
            let dialog = subj.document.getElementById("commonDialog");
            if (dialog) {
              // Change the dialog to use a warning icon.
              dialog.classList.add("alert-dialog");
            }
          }
        }, {once: true});
      }
    });

    let rv = Services.prompt.confirmEx(window, title, message, buttonFlags,
                                       firstButtonText, null,
                                       s.unblockButtonConfirmBlock, null, {});
    return [firstButtonAction, "cancel", "confirmBlock"][rv];
  },
};

XPCOMUtils.defineLazyGetter(this.DownloadsCommon, "log", () => {
  return DownloadsLogger.log.bind(DownloadsLogger);
});
XPCOMUtils.defineLazyGetter(this.DownloadsCommon, "error", () => {
  return DownloadsLogger.error.bind(DownloadsLogger);
});

/**
 * Returns true if we are executing on Windows Vista or a later version.
 */
XPCOMUtils.defineLazyGetter(DownloadsCommon, "isWinVistaOrHigher", function() {
  let os = Services.appinfo.OS;
  if (os != "WINNT") {
    return false;
  }
  return parseFloat(Services.sysinfo.getProperty("version")) >= 6;
});

// DownloadsData

/**
 * Retrieves the list of past and completed downloads from the underlying
 * Downloads API data, and provides asynchronous notifications allowing to
 * build a consistent view of the available data.
 *
 * Note that using this object does not automatically initialize the list of
 * downloads. This is useful to display a neutral progress indicator in
 * the main browser window until the autostart timeout elapses.
 *
 * This powers the DownloadsData, PrivateDownloadsData, and HistoryDownloadsData
 * singleton objects.
 */
function DownloadsDataCtor({ isPrivate, isHistory, maxHistoryResults } = {}) {
  this._isPrivate = !!isPrivate;

  // Contains all the available Download objects and their integer state.
  this.oldDownloadStates = new Map();

  // For the history downloads list we don't need to register this as a view,
  // but we have to ensure that the DownloadsData object is initialized before
  // we register more views. This ensures that the view methods of DownloadsData
  // are invoked before those of views registered on HistoryDownloadsData,
  // allowing the endTime property to be set correctly.
  if (isHistory) {
    if (isPrivate) {
      PrivateDownloadsData.initializeDataLink();
    }
    DownloadsData.initializeDataLink();
    this._promiseList = DownloadsData._promiseList.then(() => {
      // For history downloads in Private Browsing mode, we'll fetch the combined
      // list of public and private downloads.
      return DownloadHistory.getList({
        type: isPrivate ? Downloads.ALL : Downloads.PUBLIC,
        maxHistoryResults
      });
    });
    return;
  }

  // This defines "initializeDataLink" and "_promiseList" synchronously, then
  // continues execution only when "initializeDataLink" is called, allowing the
  // underlying data to be loaded only when actually needed.
  this._promiseList = (async () => {
    await new Promise(resolve => this.initializeDataLink = resolve);
    let list = await Downloads.getList(isPrivate ? Downloads.PRIVATE
                                                 : Downloads.PUBLIC);
    await list.addView(this);
    return list;
  })();
}

DownloadsDataCtor.prototype = {
  /**
   * Starts receiving events for current downloads.
   */
  initializeDataLink() {},

  /**
   * Promise resolved with the underlying DownloadList object once we started
   * receiving events for current downloads.
   */
  _promiseList: null,

  /**
   * Iterator for all the available Download objects. This is empty until the
   * data has been loaded using the JavaScript API for downloads.
   */
  get downloads() {
    return this.oldDownloadStates.keys();
  },

  /**
   * True if there are finished downloads that can be removed from the list.
   */
  get canRemoveFinished() {
    for (let download of this.downloads) {
      // Stopped, paused, and failed downloads with partial data are removed.
      if (download.stopped && !(download.canceled && download.hasPartialData)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Asks the back-end to remove finished downloads from the list. This method
   * is only called after the data link has been initialized.
   */
  removeFinished() {
    Downloads.getList(this._isPrivate ? Downloads.PRIVATE : Downloads.PUBLIC)
             .then(list => list.removeFinished())
             .catch(Cu.reportError);
    let indicatorData = this._isPrivate ? PrivateDownloadsIndicatorData
                                        : DownloadsIndicatorData;
    indicatorData.attention = DownloadsCommon.ATTENTION_NONE;
  },

  // Integration with the asynchronous Downloads back-end

  onDownloadAdded(download) {
    // Download objects do not store the end time of downloads, as the Downloads
    // API does not need to persist this information for all platforms. Once a
    // download terminates on a Desktop browser, it becomes a history download,
    // for which the end time is stored differently, as a Places annotation.
    download.endTime = Date.now();

    this.oldDownloadStates.set(download,
                               DownloadsCommon.stateOfDownload(download));
  },

  onDownloadChanged(download) {
    let oldState = this.oldDownloadStates.get(download);
    let newState = DownloadsCommon.stateOfDownload(download);
    this.oldDownloadStates.set(download, newState);

    if (oldState != newState) {
      if (download.succeeded ||
          (download.canceled && !download.hasPartialData) ||
          download.error) {
        // Store the end time that may be displayed by the views.
        download.endTime = Date.now();

        // This state transition code should actually be located in a Downloads
        // API module (bug 941009).
        DownloadHistory.updateMetaData(download);
      }

      if (download.succeeded ||
          (download.error && download.error.becauseBlocked)) {
        this._notifyDownloadEvent("finish");
      }
    }

    if (!download.newDownloadNotified) {
      download.newDownloadNotified = true;
      this._notifyDownloadEvent("start");
    }
  },

  onDownloadRemoved(download) {
    this.oldDownloadStates.delete(download);
  },

  // Registration of views

  /**
   * Adds an object to be notified when the available download data changes.
   * The specified object is initialized with the currently available downloads.
   *
   * @param aView
   *        DownloadsView object to be added.  This reference must be passed to
   *        removeView before termination.
   */
  addView(aView) {
    this._promiseList.then(list => list.addView(aView))
                     .catch(Cu.reportError);
  },

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        DownloadsView object to be removed.
   */
  removeView(aView) {
    this._promiseList.then(list => list.removeView(aView))
                     .catch(Cu.reportError);
  },

  // Notifications sent to the most recent browser window only

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
  _notifyDownloadEvent(aType) {
    DownloadsCommon.log("Attempting to notify that a new download has started or finished.");

    // Show the panel in the most recent browser window, if present.
    let browserWin = BrowserWindowTracker.getTopWindow({ private: this._isPrivate });
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

XPCOMUtils.defineLazyGetter(this, "HistoryDownloadsData", function() {
  return new DownloadsDataCtor({ isHistory: true });
});

XPCOMUtils.defineLazyGetter(this, "LimitedHistoryDownloadsData", function() {
  return new DownloadsDataCtor({ isHistory: true, maxHistoryResults: kMaxHistoryResultsForLimitedView });
});

XPCOMUtils.defineLazyGetter(this, "LimitedPrivateHistoryDownloadData", function() {
  return new DownloadsDataCtor({ isPrivate: true, isHistory: true,
    maxHistoryResults: kMaxHistoryResultsForLimitedView });
});

XPCOMUtils.defineLazyGetter(this, "PrivateDownloadsData", function() {
  return new DownloadsDataCtor({ isPrivate: true });
});

XPCOMUtils.defineLazyGetter(this, "DownloadsData", function() {
  return new DownloadsDataCtor();
});

// DownloadsViewPrototype

/**
 * A prototype for an object that registers itself with DownloadsData as soon
 * as a view is registered with it.
 */
const DownloadsViewPrototype = {
  /**
   * Contains all the available Download objects and their current state value.
   *
   * SUBCLASSES MUST OVERRIDE THIS PROPERTY.
   */
  _oldDownloadStates: null,

  // Registration of views

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
  addView(aView) {
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
  refreshView(aView) {
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
  removeView(aView) {
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

  // Callback functions from DownloadList

  /**
   * Indicates whether we are still loading downloads data asynchronously.
   */
  _loading: false,

  /**
   * Called before multiple downloads are about to be loaded.
   */
  onDownloadBatchStarting() {
    this._loading = true;
  },

  /**
   * Called after data loading finished.
   */
  onDownloadBatchEnded() {
    this._loading = false;
    this._updateViews();
  },

  /**
   * Called when a new download data item is available, either during the
   * asynchronous data load or when a new download is started.
   *
   * @param download
   *        Download object that was just added.
   *
   * @note Subclasses should override this and still call the base method.
   */
  onDownloadAdded(download) {
    this._oldDownloadStates.set(download,
                                DownloadsCommon.stateOfDownload(download));
  },

  /**
   * Called when the overall state of a Download has changed. In particular,
   * this is called only once when the download succeeds or is blocked
   * permanently, and is never called if only the current progress changed.
   *
   * The onDownloadChanged notification will always be sent afterwards.
   *
   * @note Subclasses should override this.
   */
  onDownloadStateChanged(download) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Called every time any state property of a Download may have changed,
   * including progress properties.
   *
   * Note that progress notification changes are throttled at the Downloads.jsm
   * API level, and there is no throttling mechanism in the front-end.
   *
   * @note Subclasses should override this and still call the base method.
   */
  onDownloadChanged(download) {
    let oldState = this._oldDownloadStates.get(download);
    let newState = DownloadsCommon.stateOfDownload(download);
    this._oldDownloadStates.set(download, newState);

    if (oldState != newState) {
      this.onDownloadStateChanged(download);
    }
  },

  /**
   * Called when a data item is removed, ensures that the widget associated with
   * the view item is removed from the user interface.
   *
   * @param download
   *        Download object that is being removed.
   *
   * @note Subclasses should override this.
   */
  onDownloadRemoved(download) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Private function used to refresh the internal properties being sent to
   * each registered view.
   *
   * @note Subclasses should override this.
   */
  _refreshProperties() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Private function used to refresh an individual view.
   *
   * @note Subclasses should override this.
   */
  _updateView() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /**
   * Computes aggregate values and propagates the changes to our views.
   */
  _updateViews() {
    // Do not update the status indicators during batch loads of download items.
    if (this._loading) {
      return;
    }

    this._refreshProperties();
    this._views.forEach(this._updateView, this);
  },
};

// DownloadsIndicatorData

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
  this._oldDownloadStates = new WeakMap();
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
  removeView(aView) {
    DownloadsViewPrototype.removeView.call(this, aView);

    if (this._views.length == 0) {
      this._itemCount = 0;
    }
  },

  onDownloadAdded(download) {
    DownloadsViewPrototype.onDownloadAdded.call(this, download);
    this._itemCount++;
    this._updateViews();
  },

  onDownloadStateChanged(download) {
    if (!download.succeeded && download.error && download.error.reputationCheckVerdict) {
      switch (download.error.reputationCheckVerdict) {
        case Downloads.Error.BLOCK_VERDICT_UNCOMMON: // fall-through
        case Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED:
          // Existing higher level attention indication trumps ATTENTION_WARNING.
          if (this._attention != DownloadsCommon.ATTENTION_SEVERE) {
            this.attention = DownloadsCommon.ATTENTION_WARNING;
          }
          break;
        case Downloads.Error.BLOCK_VERDICT_MALWARE:
          this.attention = DownloadsCommon.ATTENTION_SEVERE;
          break;
        default:
          this.attention = DownloadsCommon.ATTENTION_SEVERE;
          Cu.reportError("Unknown reputation verdict: " +
                         download.error.reputationCheckVerdict);
      }
    } else if (download.succeeded) {
      // Existing higher level attention indication trumps ATTENTION_SUCCESS.
      if (this._attention != DownloadsCommon.ATTENTION_SEVERE &&
          this._attention != DownloadsCommon.ATTENTION_WARNING) {
        this.attention = DownloadsCommon.ATTENTION_SUCCESS;
      }
    } else if (download.error) {
      // Existing higher level attention indication trumps ATTENTION_WARNING.
      if (this._attention != DownloadsCommon.ATTENTION_SEVERE) {
        this.attention = DownloadsCommon.ATTENTION_WARNING;
      }
    }
  },

  onDownloadChanged(download) {
    DownloadsViewPrototype.onDownloadChanged.call(this, download);
    this._updateViews();
  },

  onDownloadRemoved(download) {
    this._itemCount--;
    this._updateViews();
  },

  // Propagation of properties to our views

  // The following properties are updated by _refreshProperties and are then
  // propagated to the views.  See _refreshProperties for details.
  _hasDownloads: false,
  _percentComplete: -1,

  /**
   * Indicates whether the download indicators should be highlighted.
   */
  set attention(aValue) {
    this._attention = aValue;
    this._updateViews();
    return aValue;
  },
  _attention: DownloadsCommon.ATTENTION_NONE,

  /**
   * Indicates whether the user is interacting with downloads, thus the
   * attention indication should not be shown even if requested.
   */
  set attentionSuppressed(aValue) {
    this._attentionSuppressed = aValue;
    this._attention = DownloadsCommon.ATTENTION_NONE;
    this._updateViews();
    return aValue;
  },
  _attentionSuppressed: false,

  /**
   * Updates the specified view with the current aggregate values.
   *
   * @param aView
   *        DownloadsIndicatorView object to be updated.
   */
  _updateView(aView) {
    aView.hasDownloads = this._hasDownloads;
    aView.percentComplete = this._percentComplete;
    aView.attention = this._attentionSuppressed ? DownloadsCommon.ATTENTION_NONE
                                                : this._attention;
  },

  // Property updating based on current download status

  /**
   * Number of download items that are available to be displayed.
   */
  _itemCount: 0,

  /**
   * A generator function for the Download objects this summary is currently
   * interested in. This generator is passed off to summarizeDownloads in order
   * to generate statistics about the downloads we care about - in this case,
   * it's all active downloads.
   */
  * _activeDownloads() {
    let downloads = this._isPrivate ? PrivateDownloadsData.downloads
                                    : DownloadsData.downloads;
    for (let download of downloads) {
      if (!download.stopped || (download.canceled && download.hasPartialData)) {
        yield download;
      }
    }
  },

  /**
   * Computes aggregate values based on the current state of downloads.
   */
  _refreshProperties() {
    let summary =
      DownloadsCommon.summarizeDownloads(this._activeDownloads());

    // Determine if the indicator should be shown or get attention.
    this._hasDownloads = (this._itemCount > 0);

    // Always show a progress bar if there are downloads in progress.
    if (summary.percentComplete >= 0) {
      this._percentComplete = summary.percentComplete;
    } else if (summary.numDownloading > 0) {
      this._percentComplete = 0;
    } else {
      this._percentComplete = -1;
    }
  }
};

XPCOMUtils.defineLazyGetter(this, "PrivateDownloadsIndicatorData", function() {
  return new DownloadsIndicatorDataCtor(true);
});

XPCOMUtils.defineLazyGetter(this, "DownloadsIndicatorData", function() {
  return new DownloadsIndicatorDataCtor(false);
});

// DownloadsSummaryData

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

  this._downloads = [];

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

  this._oldDownloadStates = new WeakMap();
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
  removeView(aView) {
    DownloadsViewPrototype.removeView.call(this, aView);

    if (this._views.length == 0) {
      // Clear out our collection of Download objects. If we ever have
      // another view registered with us, this will get re-populated.
      this._downloads = [];
    }
  },

  onDownloadAdded(download) {
    DownloadsViewPrototype.onDownloadAdded.call(this, download);
    this._downloads.unshift(download);
    this._updateViews();
  },

  onDownloadStateChanged() {
    // Since the state of a download changed, reset the estimated time left.
    this._lastRawTimeLeft = -1;
    this._lastTimeLeft = -1;
  },

  onDownloadChanged(download) {
    DownloadsViewPrototype.onDownloadChanged.call(this, download);
    this._updateViews();
  },

  onDownloadRemoved(download) {
    let itemIndex = this._downloads.indexOf(download);
    this._downloads.splice(itemIndex, 1);
    this._updateViews();
  },

  // Propagation of properties to our views

  /**
   * Updates the specified view with the current aggregate values.
   *
   * @param aView
   *        DownloadsIndicatorView object to be updated.
   */
  _updateView(aView) {
    aView.showingProgress = this._showingProgress;
    aView.percentComplete = this._percentComplete;
    aView.description = this._description;
    aView.details = this._details;
  },

  // Property updating based on current download status

  /**
   * A generator function for the Download objects this summary is currently
   * interested in. This generator is passed off to summarizeDownloads in order
   * to generate statistics about the downloads we care about - in this case,
   * it's the downloads in this._downloads after the first few to exclude,
   * which was set when constructing this DownloadsSummaryData instance.
   */
  * _downloadsForSummary() {
    if (this._downloads.length > 0) {
      for (let i = this._numToExclude; i < this._downloads.length; ++i) {
        yield this._downloads[i];
      }
    }
  },

  /**
   * Computes aggregate values based on the current state of downloads.
   */
  _refreshProperties() {
    // Pre-load summary with default values.
    let summary =
      DownloadsCommon.summarizeDownloads(this._downloadsForSummary());

    this._description = DownloadsCommon.strings
                                       .otherDownloads3(summary.numDownloading);
    this._percentComplete = summary.percentComplete;

    // Only show the downloading items.
    this._showingProgress = summary.numDownloading > 0;

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
  },
};

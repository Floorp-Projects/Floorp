/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

XPCOMUtils.defineLazyServiceGetter(this, "gBrowserGlue",
                                   "@mozilla.org/browser/browserglue;1",
                                   "nsIBrowserGlue");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

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

XPCOMUtils.defineLazyGetter(this, "DownloadsLocalFileCtor", function () {
  return Components.Constructor("@mozilla.org/file/local;1",
                                "nsILocalFile", "initWithPath");
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsCommon

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides shared methods for all the instances of the user interface.
 */
const DownloadsCommon = {
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
   * Indicates whether we should show the full Download Manager window interface
   * instead of the simplified panel interface.  The behavior of downloads
   * across browsing session is consistent with the selected interface.
   */
  get useToolkitUI()
  {
    try {
      return Services.prefs.getBoolPref("browser.download.useToolkitUI");
    } catch (ex) { }
    return false;
  },

  /**
   * Returns a reference to the DownloadsData singleton.
   *
   * This does not need to be a lazy getter, since no initialization is required
   * at present.
   */
  get data() DownloadsData,

  /**
   * Returns a reference to the DownloadsData singleton.
   *
   * This does not need to be a lazy getter, since no initialization is required
   * at present.
   */
  get indicatorData() DownloadsIndicatorData
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
 */
const DownloadsData = {
  /**
   * Starts receiving events for current downloads.
   *
   * @param aDownloadManagerService
   *        Reference to the service implementing nsIDownloadManager.  We need
   *        this because getService isn't available for us when this method is
   *        called, and we must ensure to register our listeners before the
   *        getService call for the Download Manager returns.
   */
  initializeDataLink: function DD_initializeDataLink(aDownloadManagerService)
  {
    // Start receiving real-time events.
    aDownloadManagerService.addListener(this);
    Services.obs.addObserver(this, "download-manager-remove-download", false);
    Services.obs.addObserver(this, "download-manager-database-type-changed",
                             false);
  },

  /**
   * Stops receiving events for current downloads and cancels any pending read.
   */
  terminateDataLink: function DD_terminateDataLink()
  {
    this._terminateDataAccess();

    // Stop receiving real-time events.
    Services.obs.removeObserver(this, "download-manager-database-type-changed");
    Services.obs.removeObserver(this, "download-manager-remove-download");
    Services.downloads.removeListener(this);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Registration of views

  /**
   * Array of view objects that should be notified when the available download
   * data changes.
   */
  _views: [],

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

    // Sort backwards by download identifier, ensuring that the most recent
    // downloads are added first regardless of their state.
    let loadedItemsArray = [dataItem
                            for each (dataItem in this.dataItems)
                            if (dataItem)];
    loadedItemsArray.sort(function(a, b) b.downloadId - a.downloadId);
    loadedItemsArray.forEach(
      function (dataItem) aView.onDataItemAdded(dataItem, false)
    );

    // Notify the view that all data is available unless loading is in progress.
    if (!this._pendingStatement) {
      aView.onDataLoadCompleted();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// In-memory downloads data store

  /**
   * Object containing all the available DownloadsDataItem objects, indexed by
   * their numeric download identifier.  The identifiers of downloads that have
   * been removed from the Download Manager data are still present, however the
   * associated objects are replaced with the value "null".  This is required to
   * prevent race conditions when populating the list asynchronously.
   */
  dataItems: {},

  /**
   * While operating in Private Browsing Mode, persistent data items are parked
   * here until we return to the normal mode.
   */
  _persistentDataItems: {},

  /**
   * Clears the loaded data.
   */
  clear: function DD_clear()
  {
    this._terminateDataAccess();
    this.dataItems = {};
  },

  /**
   * Returns the data item associated with the provided source object.  The
   * source can be a download object that we received from the Download Manager
   * because of a real-time notification, or a row from the downloads database,
   * during the asynchronous data load.
   *
   * In case we receive download status notifications while we are still
   * populating the list of downloads from the database, we want the real-time
   * status to take precedence over the state that is read from the database,
   * which might be older.  This is achieved by creating the download item if
   * it's not already in the list, but never updating the returned object using
   * the data from the database, if the object already exists.
   *
   * @param aSource
   *        Object containing the data with which the item should be initialized
   *        if it doesn't already exist in the list.  This should implement
   *        either nsIDownload or mozIStorageRow.  If the item exists, this
   *        argument is only used to retrieve the download identifier.
   * @param aMayReuseId
   *        If false, indicates that the download should not be added if a
   *        download with the same identifier was removed in the meantime.  This
   *        ensures that, while loading the list asynchronously, downloads that
   *        have been removed in the meantime do no reappear inadvertently.
   *
   * @return New or existing data item, or null if the item was deleted from the
   *         list of available downloads.
   */
  _getOrAddDataItem: function DD_getOrAddDataItem(aSource, aMayReuseId)
  {
    let downloadId = (aSource instanceof Ci.nsIDownload)
                     ? aSource.id
                     : aSource.getResultByName("id");
    if (downloadId in this.dataItems) {
      let existingItem = this.dataItems[downloadId];
      if (existingItem || !aMayReuseId) {
        // Returns null if the download was removed and we can't reuse the item.
        return existingItem;
      }
    }

    let dataItem = new DownloadsDataItem(aSource);
    this.dataItems[downloadId] = dataItem;

    // Create the view items before returning.
    let addToStartOfList = aSource instanceof Ci.nsIDownload;
    this._views.forEach(
      function (view) view.onDataItemAdded(dataItem, addToStartOfList)
    );
    return dataItem;
  },

  /**
   * Removes the data item with the specified identifier.
   *
   * This method can be called at most once per download identifier.
   */
  _removeDataItem: function DD_removeDataItem(aDownloadId)
  {
    if (aDownloadId in this.dataItems) {
      let dataItem = this.dataItems[aDownloadId];
      this._views.forEach(
        function (view) view.onDataItemRemoved(dataItem)
      );
    }
    this.dataItems[aDownloadId] = null;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Persistent data loading

  /**
   * Represents an executing statement, allowing its cancellation.
   */
  _pendingStatement: null,

  /**
   * Indicates which kind of items from the persistent downloads database have
   * been fully loaded in memory and are available to the views.  This can
   * assume the value of one of the kLoad constants.
   */
  _loadState: 0,

  /** No downloads have been fully loaded yet. */
  get kLoadNone() 0,
  /** All the active downloads in the database are loaded in memory. */
  get kLoadActive() 1,
  /** All the downloads in the database are loaded in memory. */
  get kLoadAll() 2,

  /**
   * Reloads the specified kind of downloads from the persistent database.  This
   * method must only be called when Private Browsing Mode is disabled.
   *
   * @param aActiveOnly
   *        True to load only active downloads from the database.
   */
  ensurePersistentDataLoaded:
  function DD_ensurePersistentDataLoaded(aActiveOnly)
  {
    if (this._pendingStatement) {
      // We are already in the process of reloading all downloads.
      return;
    }

    if (aActiveOnly) {
      if (this._loadState == this.kLoadNone) {
        // Indicate to the views that a batch loading operation is in progress.
        this._views.forEach(
          function (view) view.onDataLoadStarting()
        );

        // Reload the list using the Download Manager service.
        let downloads = Services.downloads.activeDownloads;
        while (downloads.hasMoreElements()) {
          let download = downloads.getNext().QueryInterface(Ci.nsIDownload);
          this._getOrAddDataItem(download, true);
        }
        this._loadState = this.kLoadActive;

        // Indicate to the views that the batch loading operation is complete.
        this._views.forEach(
          function (view) view.onDataLoadCompleted()
        );
      }
    } else {
      if (this._loadState != this.kLoadAll) {
        // Load only the relevant columns from the downloads database.  The
        // columns are read in the init_FromDataRow method of DownloadsDataItem.
        let statement = Services.downloads.DBConnection.createAsyncStatement(
          "SELECT id, target, name, source, referrer, state, "
        +        "startTime, endTime, currBytes, maxBytes "
        + "FROM moz_downloads "
        + "ORDER BY id DESC"
        );
        try {
          this._pendingStatement = statement.executeAsync(this);
        } finally {
          statement.finalize();
        }
      }
    }
  },

  /**
   * Cancels any pending data access and ensures views are notified.
   */
  _terminateDataAccess: function DD_terminateDataAccess()
  {
    if (this._pendingStatement) {
      this._pendingStatement.cancel();
      this._pendingStatement = null;
    }

    // Close all the views on the current data.  Create a copy of the array
    // because some views might unregister while processing this event.
    Array.slice(this._views, 0).forEach(
      function (view) view.onDataInvalidated()
    );
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function DD_handleResult(aResultSet)
  {
    for (let row = aResultSet.getNextRow();
         row;
         row = aResultSet.getNextRow()) {
      // Add the download to the list and initialize it with the data we read,
      // unless we already received a notification providing more reliable
      // information for this download.
      this._getOrAddDataItem(row, false);
    }
  },

  handleError: function DD_handleError(aError)
  {
    Cu.reportError("Database statement execution error (" + aError.result +
                   "): " + aError.message);
  },

  handleCompletion: function DD_handleCompletion(aReason)
  {
    this._pendingStatement = null;

    // To ensure that we don't inadvertently delete more downloads from the
    // database than needed on shutdown, we should update the load state only if
    // the operation completed successfully.
    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      this._loadState = this.kLoadAll;
    }

    // Indicate to the views that the batch loading operation is complete, even
    // if the lookup failed or was canceled.  The only possible glitch happens
    // in case the database backend changes while loading data, when the views
    // would open and immediately close.  This case is rare enough not to need a
    // special treatment.
    this._views.forEach(
      function (view) view.onDataLoadCompleted()
    );
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DD_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "download-manager-remove-download":
        // If a single download was removed, remove the corresponding data item.
        if (aSubject) {
          this._removeDataItem(aSubject.QueryInterface(Ci.nsISupportsPRUint32));
          break;
        }

        // Multiple downloads have been removed.  Iterate over known downloads
        // and remove those that don't exist anymore.
        for each (let dataItem in this.dataItems) {
          if (dataItem) {
            try {
              Services.downloads.getDownload(dataItem.downloadId);
            } catch (ex) {
              this._removeDataItem(dataItem.downloadId);
            }
          }
        }
        break;

      case "download-manager-database-type-changed":
        let pbs = Cc["@mozilla.org/privatebrowsing;1"]
                  .getService(Ci.nsIPrivateBrowsingService);
        if (pbs.privateBrowsingEnabled) {
          // Save a reference to the persistent store before terminating access.
          this._persistentDataItems = this.dataItems;
          this.clear();
        } else {
          // Terminate data access, then restore the persistent store.
          this.clear();
          this.dataItems = this._persistentDataItems;
          this._persistentDataItems = null;
        }
        // Reinitialize the views with the current items.  View data has been
        // already invalidated by the previous calls.
        this._views.forEach(this._updateView, this);
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener

  onDownloadStateChange: function DD_onDownloadStateChange(aState, aDownload)
  {
    // When a new download is added, it may have the same identifier of a
    // download that we previously deleted during this session, and we also
    // want to provide a visible indication that the download started.
    let isNew = aState == nsIDM.DOWNLOAD_NOTSTARTED ||
                aState == nsIDM.DOWNLOAD_QUEUED;

    let dataItem = this._getOrAddDataItem(aDownload, isNew);
    if (!dataItem) {
      return;
    }

    dataItem.state = aDownload.state;
    dataItem.referrer = aDownload.referrer && aDownload.referrer.spec;
    dataItem.resumable = aDownload.resumable;
    dataItem.startTime = Math.round(aDownload.startTime / 1000);
    dataItem.currBytes = aDownload.amountTransferred;
    dataItem.maxBytes = aDownload.size;

    this._views.forEach(
      function (view) view.getViewItem(dataItem).onStateChange()
    );

    if (isNew && !dataItem.newDownloadNotified) {
      dataItem.newDownloadNotified = true;
      this._notifyNewDownload();
    }
  },

  onProgressChange: function DD_onProgressChange(aWebProgress, aRequest,
                                                  aCurSelfProgress,
                                                  aMaxSelfProgress,
                                                  aCurTotalProgress,
                                                  aMaxTotalProgress, aDownload)
  {
    let dataItem = this._getOrAddDataItem(aDownload, false);
    if (!dataItem) {
      return;
    }

    dataItem.currBytes = aDownload.amountTransferred;
    dataItem.maxBytes = aDownload.size;
    dataItem.speed = aDownload.speed;
    dataItem.percentComplete = aDownload.percentComplete;

    this._views.forEach(
      function (view) view.getViewItem(dataItem).onProgressChange()
    );
  },

  onStateChange: function () { },

  onSecurityChange: function () { },

  //////////////////////////////////////////////////////////////////////////////
  //// Notifications sent to the most recent browser window only

  /**
   * Set to true after the first download in the session caused the downloads
   * panel to be displayed.
   */
  firstDownloadShown: false,

  /**
   * Displays a new download notification in the most recent browser window, if
   * one is currently available.
   */
  _notifyNewDownload: function DD_notifyNewDownload()
  {
    if (DownloadsCommon.useToolkitUI) {
      return;
    }

    // Show the panel in the most recent browser window, if present.
    let browserWin = gBrowserGlue.getMostRecentBrowserWindow();
    if (!browserWin) {
      return;
    }

    browserWin.focus();
    if (this.firstDownloadShown) {
      // For new downloads after the first one in the session, don't show the
      // panel automatically, but provide a visible notification in the topmost
      // browser window, if the status indicator is already visible.
      browserWin.DownloadsIndicatorView.showEventNotification();
      return;
    }
    this.firstDownloadShown = true;
    browserWin.DownloadsPanel.showPanel();
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsDataItem

/**
 * Represents a single item in the list of downloads.  This object either wraps
 * an existing nsIDownload from the Download Manager, or provides the same
 * information read directly from the downloads database, with the possibility
 * of querying the nsIDownload lazily, for performance reasons.
 *
 * @param aSource
 *        Object containing the data with which the item should be initialized.
 *        This should implement either nsIDownload or mozIStorageRow.
 */
function DownloadsDataItem(aSource)
{
  if (aSource instanceof Ci.nsIDownload) {
    this._initFromDownload(aSource);
  } else {
    this._initFromDataRow(aSource);
  }
}

DownloadsDataItem.prototype = {
  /**
   * Initializes this object from a download object of the Download Manager.
   *
   * The endTime property is initialized to the current date and time.
   *
   * @param aDownload
   *        The nsIDownload with the current state.
   */
  _initFromDownload: function DDI_initFromDownload(aDownload)
  {
    this.download = aDownload;

    // Fetch all the download properties eagerly.
    this.downloadId = aDownload.id;
    this.file = aDownload.target.spec;
    this.target = aDownload.displayName;
    this.uri = aDownload.source.spec;
    this.referrer = aDownload.referrer && aDownload.referrer.spec;
    this.state = aDownload.state;
    this.startTime = Math.round(aDownload.startTime / 1000);
    this.endTime = Date.now();
    this.currBytes = aDownload.amountTransferred;
    this.maxBytes = aDownload.size;
    this.resumable = aDownload.resumable;
    this.speed = aDownload.speed;
    this.percentComplete = aDownload.percentComplete;
  },

  /**
   * Initializes this object from a data row in the downloads database, without
   * querying the associated nsIDownload object, to improve performance when
   * loading the list of downloads asynchronously.
   *
   * When this object is initialized in this way, accessing the "download"
   * property loads the underlying nsIDownload object synchronously, and should
   * be avoided unless the object is really required.
   *
   * @param aStorageRow
   *        The mozIStorageRow from the downloads database.
   */
  _initFromDataRow: function DDI_initFromDataRow(aStorageRow)
  {
    // Get the download properties from the data row.
    this.downloadId = aStorageRow.getResultByName("id");
    this.file = aStorageRow.getResultByName("target");
    this.target = aStorageRow.getResultByName("name");
    this.uri = aStorageRow.getResultByName("source");
    this.referrer = aStorageRow.getResultByName("referrer");
    this.state = aStorageRow.getResultByName("state");
    this.startTime = Math.round(aStorageRow.getResultByName("startTime") / 1000);
    this.endTime = Math.round(aStorageRow.getResultByName("endTime") / 1000);
    this.currBytes = aStorageRow.getResultByName("currBytes");
    this.maxBytes = aStorageRow.getResultByName("maxBytes");

    // Allows accessing the underlying download object lazily.
    XPCOMUtils.defineLazyGetter(this, "download", function ()
                                Services.downloads.getDownload(this.downloadId));

    // Now we have to determine if the download is resumable, but don't want to
    // access the underlying download object unnecessarily.  The only case where
    // the property is relevant is when we are currently downloading data, and
    // in this case the download object is already loaded in memory or will be
    // loaded very soon in any case.  In all the other cases, including a paused
    // download, we assume that the download is resumable.  The property will be
    // updated as soon as the underlying download state changes.
    if (this.state == nsIDM.DOWNLOAD_DOWNLOADING) {
      this.resumable = this.download.resumable;
    } else {
      this.resumable = true;
    }

    // Compute the other properties without accessing the download object.
    this.speed = 0;
    this.percentComplete = this.maxBytes <= 0
                           ? -1
                           : Math.round(this.currBytes / this.maxBytes * 100);
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
    // The download database may contain targets stored as file URLs or native
    // paths.  This can still be true for previously stored items, even if new
    // items are stored using their file URL.  See also bug 239948 comment 12.
    if (/^file:/.test(this.file)) {
      // Assume the file URL we obtained from the downloads database or from the
      // "spec" property of the target has the UTF-8 charset.
      let fileUrl = NetUtil.newURI(this.file).QueryInterface(Ci.nsIFileURL);
      return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
    } else {
      // The downloads database contains a native path.  Try to create a local
      // file, though this may throw an exception if the path is invalid.
      return new DownloadsLocalFileCtor(this.file);
    }
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
const DownloadsIndicatorData = {
  //////////////////////////////////////////////////////////////////////////////
  //// Registration of views

  /**
   * Array of view objects that should be notified when the available status
   * data changes.
   */
  _views: [],

  /**
   * Adds an object to be notified when the available status data changes.
   * The specified object is initialized with the currently available status.
   *
   * @param aView
   *        DownloadsIndicatorView object to be added.  This reference must be
   *        passed to removeView before termination.
   */
  addView: function DID_addView(aView)
  {
    // Start receiving events when the first of our views is registered.
    if (this._views.length == 0) {
      DownloadsCommon.data.addView(this);
    }

    this._views.push(aView);
    this.refreshView(aView);
  },

  /**
   * Updates the properties of an object previously added using addView.
   *
   * @param aView
   *        DownloadsIndicatorView object to be updated.
   */
  refreshView: function DID_refreshView(aView)
  {
    // Update immediately even if we are still loading data asynchronously.
    this._refreshProperties();
    this._updateView(aView);
  },

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        DownloadsIndicatorView object to be removed.
   */
  removeView: function DID_removeView(aView)
  {
    let index = this._views.indexOf(aView);
    if (index != -1) {
      this._views.splice(index, 1);
    }

    // Stop receiving events when the last of our views is unregistered.
    if (this._views.length == 0) {
      DownloadsCommon.data.removeView(this);
      this._itemCount = 0;
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
  onDataLoadStarting: function DID_onDataLoadStarting()
  {
    this._loading = true;
  },

  /**
   * Called after data loading finished.
   */
  onDataLoadCompleted: function DID_onDataLoadCompleted()
  {
    this._loading = false;
    this._updateViews();
  },

  /**
   * Called when the downloads database becomes unavailable (for example, we
   * entered Private Browsing Mode and the database backend changed).
   * References to existing data should be discarded.
   */
  onDataInvalidated: function DID_onDataInvalidated()
  {
    this._itemCount = 0;
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
    return Object.freeze({
      onStateChange: function DIVI_onStateChange()
      {
        if (aDataItem.state == nsIDM.DOWNLOAD_FINISHED ||
            aDataItem.state == nsIDM.DOWNLOAD_FAILED) {
          DownloadsIndicatorData.attention = true;
        }

        // Since the state of a download changed, reset the estimated time left.
        DownloadsIndicatorData._lastRawTimeLeft = -1;
        DownloadsIndicatorData._lastTimeLeft = -1;

        DownloadsIndicatorData._updateViews();
      },
      onProgressChange: function DIVI_onProgressChange()
      {
        DownloadsIndicatorData._updateViews();
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
   * Update the estimated time until all in-progress downloads will finish.
   *
   * @param aSeconds
   *        Current raw estimate on number of seconds left for all downloads.
   *        This is a floating point value to help get sub-second accuracy for
   *        current and future estimates.
   */
  _updateTimeLeft: function DID_updateTimeLeft(aSeconds)
  {
    // We apply an algorithm similar to the DownloadUtils.getTimeLeft function,
    // though tailored to a single time estimation for all downloads.  We never
    // apply sommothing if the new value is less than half the previous value.
    let shouldApplySmoothing = this._lastTimeLeft >= 0 &&
                               aSeconds > this._lastTimeLeft / 2;
    if (shouldApplySmoothing) {
      // Apply hysteresis to favor downward over upward swings.  Trust only 30%
      // of the new value if lower, and 10% if higher (exponential smoothing).
      let (diff = aSeconds - this._lastTimeLeft) {
        aSeconds = this._lastTimeLeft + (diff < 0 ? .3 : .1) * diff;
      }

      // If the new time is similar, reuse something close to the last time
      // left, but subtract a little to provide forward progress.
      let diff = aSeconds - this._lastTimeLeft;
      let diffPercent = diff / this._lastTimeLeft * 100;
      if (Math.abs(diff) < 5 || Math.abs(diffPercent) < 5) {
        aSeconds = this._lastTimeLeft - (diff < 0 ? .4 : .2);
      }
    }

    // In the last few seconds of downloading, we are always subtracting and
    // never adding to the time left.  Ensure that we never fall below one
    // second left until all downloads are actually finished.
    this._lastTimeLeft = Math.max(aSeconds, 1);
  },

  /**
   * Computes aggregate values based on the current state of downloads.
   */
  _refreshProperties: function DID_refreshProperties()
  {
    let numActive = 0;
    let numPaused = 0;
    let numScanning = 0;
    let totalSize = 0;
    let totalTransferred = 0;
    let rawTimeLeft = -1;

    // If no download has been loaded, don't use the methods of the Download
    // Manager service, so that it is not initialized unnecessarily.
    if (this._itemCount > 0) {
      let downloads = Services.downloads.activeDownloads;
      while (downloads.hasMoreElements()) {
        let download = downloads.getNext().QueryInterface(Ci.nsIDownload);
        numActive++;
        switch (download.state) {
          case nsIDM.DOWNLOAD_PAUSED:
            numPaused++;
            break;
          case nsIDM.DOWNLOAD_SCANNING:
            numScanning++;
            break;
          case nsIDM.DOWNLOAD_DOWNLOADING:
            if (download.size > 0 && download.speed > 0) {
              let sizeLeft = download.size - download.amountTransferred;
              rawTimeLeft = Math.max(rawTimeLeft, sizeLeft / download.speed);
            }
            break;
        }
        // Only add to total values if we actually know the download size.
        if (download.size > 0) {
          totalSize += download.size;
          totalTransferred += download.amountTransferred;
        }
      }
    }

    // Determine if the indicator should be shown or get attention.
    this._hasDownloads = (this._itemCount > 0);

    if (numActive == 0 || totalSize == 0 || numActive == numScanning) {
      // Don't display the current progress.
      this._percentComplete = -1;
    } else {
      // Display the current progress.
      this._percentComplete = (totalTransferred / totalSize) * 100;
    }

    // If all downloads are paused, show the progress indicator as paused.
    this._paused = numActive > 0 && numActive == numPaused;

    // Display the estimated time left, if present.
    if (rawTimeLeft == -1) {
      // There are no downloads with a known time left.
      this._lastRawTimeLeft = -1;
      this._lastTimeLeft = -1;
      this._counter = "";
    } else {
      // Compute the new time left only if state actually changed.
      if (this._lastRawTimeLeft != rawTimeLeft) {
        this._lastRawTimeLeft = rawTimeLeft;
        this._updateTimeLeft(rawTimeLeft);
      }
      this._counter = DownloadsCommon.formatTimeLeft(this._lastTimeLeft);
    }
  }
}

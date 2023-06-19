/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides functions to prevent multiple automatic downloads.
 */

import {
  Download,
  DownloadError,
} from "resource://gre/modules/DownloadCore.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  DownloadList: "resource://gre/modules/DownloadList.sys.mjs",
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  DownloadsCommon: "resource:///modules/DownloadsCommon.sys.mjs",
});

/**
 * Each window tracks download spam independently, so one of these objects is
 * constructed for each window. This is responsible for tracking the spam and
 * updating the window's downloads UI accordingly.
 */
class WindowSpamProtection {
  constructor(window) {
    this._window = window;
  }

  /**
   * This map stores blocked spam downloads for the window, keyed by the
   * download's source URL. This is done so we can track the number of times a
   * given download has been blocked.
   * @type {Map<String, DownloadSpam>}
   */
  _downloadSpamForUrl = new Map();

  /**
   * This set stores views that are waiting to have download notification
   * listeners attached. They will be attached when the spamList is created
   * (i.e. when the first spam download is blocked).
   * @type {Set<Object>}
   */
  _pendingViews = new Set();

  /**
   * Set to true when we first start _blocking downloads in the window. This is
   * used to lazily load the spamList. Spam downloads are rare enough that many
   * sessions will have no blocked downloads. So we don't want to create a
   * DownloadList unless we actually need it.
   * @type {Boolean}
   */
  _blocking = false;

  /**
   * A per-window DownloadList for blocked spam downloads. Registered views will
   * be sent notifications about downloads in this list, so that blocked spam
   * downloads can be represented in the UI. If spam downloads haven't been
   * blocked in the window, this will be undefined. See DownloadList.sys.mjs.
   * @type {DownloadList | undefined}
   */
  get spamList() {
    if (!this._blocking) {
      return undefined;
    }
    if (!this._spamList) {
      this._spamList = new lazy.DownloadList();
    }
    return this._spamList;
  }

  /**
   * A per-window downloads indicator whose state depends on notifications from
   * DownloadLists registered in the window (for example, the visual state of
   * the downloads toolbar button). See DownloadsCommon.sys.mjs for more details.
   * @type {DownloadsIndicatorData}
   */
  get indicator() {
    if (!this._indicator) {
      this._indicator = lazy.DownloadsCommon.getIndicatorData(this._window);
    }
    return this._indicator;
  }

  /**
   * Add a blocked download to the spamList or increment the count of an
   * existing blocked download, then notify listeners about this.
   * @param {String} url
   */
  addDownloadSpam(url) {
    this._blocking = true;
    // Start listening on registered downloads views, if any exist.
    this._maybeAddViews();
    // If this URL is already paired with a DownloadSpam object, increment its
    // blocked downloads count by 1 and don't open the downloads panel.
    if (this._downloadSpamForUrl.has(url)) {
      let downloadSpam = this._downloadSpamForUrl.get(url);
      downloadSpam.blockedDownloadsCount += 1;
      this.indicator.onDownloadStateChanged(downloadSpam);
      return;
    }
    // Otherwise, create a new DownloadSpam object for the URL, add it to the
    // spamList, and open the downloads panel.
    let downloadSpam = new DownloadSpam(url);
    this.spamList.add(downloadSpam);
    this._downloadSpamForUrl.set(url, downloadSpam);
    this._notifyDownloadSpamAdded(downloadSpam);
  }

  /**
   * Notify the downloads panel that a new download has been added to the
   * spamList. This is invoked when a new DownloadSpam object is created.
   * @param {DownloadSpam} downloadSpam
   */
  _notifyDownloadSpamAdded(downloadSpam) {
    let hasActiveDownloads = lazy.DownloadsCommon.summarizeDownloads(
      this.indicator._activeDownloads()
    ).numDownloading;
    if (
      !hasActiveDownloads &&
      this._window === lazy.BrowserWindowTracker.getTopWindow()
    ) {
      // If there are no active downloads, open the downloads panel.
      this._window.DownloadsPanel.showPanel();
    } else {
      // Otherwise, flash a taskbar/dock icon notification if available.
      this._window.getAttention();
    }
    this.indicator.onDownloadAdded(downloadSpam);
  }

  /**
   * Remove the download spam data for a given source URL.
   * @param {String} url
   */
  removeDownloadSpamForUrl(url) {
    if (this._downloadSpamForUrl.has(url)) {
      let downloadSpam = this._downloadSpamForUrl.get(url);
      this.spamList.remove(downloadSpam);
      this.indicator.onDownloadRemoved(downloadSpam);
      this._downloadSpamForUrl.delete(url);
    }
  }

  /**
   * Set up a downloads view (e.g. the downloads panel) to receive notifications
   * about downloads in the spamList.
   * @param {Object} view An object that implements handlers for download
   *                      related notifications, like onDownloadAdded.
   */
  registerView(view) {
    if (!view || this.spamList?._views.has(view)) {
      return;
    }
    this._pendingViews.add(view);
    this._maybeAddViews();
  }

  /**
   * If any downloads have been blocked in the window, add download notification
   * listeners for each downloads view that has been registered.
   */
  _maybeAddViews() {
    if (this.spamList) {
      for (let view of this._pendingViews) {
        if (!this.spamList._views.has(view)) {
          this.spamList.addView(view);
        }
      }
      this._pendingViews.clear();
    }
  }

  /**
   * Remove download notification listeners for all views. This is invoked when
   * the window is closed.
   */
  removeAllViews() {
    if (this.spamList) {
      for (let view of this.spamList._views) {
        this.spamList.removeView(view);
      }
    }
    this._pendingViews.clear();
  }
}

/**
 * Responsible for detecting events related to downloads spam and notifying the
 * relevant window's WindowSpamProtection object. This is a singleton object,
 * constructed by DownloadIntegration.sys.mjs when the first download is blocked.
 */
export class DownloadSpamProtection {
  /**
   * Stores spam protection data per-window.
   * @type {WeakMap<Window, WindowSpamProtection>}
   */
  _forWindowMap = new WeakMap();

  /**
   * Add download spam data for a given source URL in the window where the
   * download was blocked. This is invoked when a download is blocked by
   * nsExternalAppHandler::IsDownloadSpam
   * @param {String} url
   * @param {Window} window
   */
  update(url, window) {
    if (window == null) {
      lazy.DownloadsCommon.log(
        "Download spam blocked in a non-chrome window. URL: ",
        url
      );
      return;
    }
    // Get the spam protection object for a given window or create one if it
    // does not already exist. Also attach notification listeners to any pending
    // downloads views.
    let wsp =
      this._forWindowMap.get(window) ?? new WindowSpamProtection(window);
    this._forWindowMap.set(window, wsp);
    wsp.addDownloadSpam(url);
  }

  /**
   * Get the spam list for a given window (provided it exists).
   * @param {Window} window
   * @returns {DownloadList}
   */
  getSpamListForWindow(window) {
    return this._forWindowMap.get(window)?.spamList;
  }

  /**
   * Remove the download spam data for a given source URL in the passed window,
   * if any exists.
   * @param {String} url
   * @param {Window} window
   */
  removeDownloadSpamForWindow(url, window) {
    let wsp = this._forWindowMap.get(window);
    wsp?.removeDownloadSpamForUrl(url);
  }

  /**
   * Create the spam protection object for a given window (if not already
   * created) and prepare to start listening for notifications on the passed
   * downloads view. The bulk of resources won't be expended until a download is
   * blocked. To add multiple views, call this method multiple times.
   * @param {Object} view An object that implements handlers for download
   *                      related notifications, like onDownloadAdded.
   * @param {Window} window
   */
  register(view, window) {
    let wsp =
      this._forWindowMap.get(window) ?? new WindowSpamProtection(window);
    // Try setting up the view now; it will be deferred if there's no spam.
    wsp.registerView(view);
    this._forWindowMap.set(window, wsp);
  }

  /**
   * Remove the spam protection object for a window when it is closed.
   * @param {Window} window
   */
  unregister(window) {
    let wsp = this._forWindowMap.get(window);
    if (wsp) {
      // Stop listening on the view if it was previously set up.
      wsp.removeAllViews();
      this._forWindowMap.delete(window);
    }
  }
}

/**
 * Represents a special Download object for download spam.
 * @extends Download
 */
class DownloadSpam extends Download {
  constructor(url) {
    super();
    this.hasBlockedData = true;
    this.stopped = true;
    this.error = new DownloadError({
      becauseBlockedByReputationCheck: true,
      reputationCheckVerdict: lazy.Downloads.Error.BLOCK_VERDICT_DOWNLOAD_SPAM,
    });
    this.target = { path: "" };
    this.source = { url };
    this.blockedDownloadsCount = 1;
  }
}

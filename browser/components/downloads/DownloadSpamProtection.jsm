/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides functions to prevent multiple automatic downloads.
 */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadSpamProtection"];

const { Download, DownloadError } = ChromeUtils.import(
  "resource://gre/modules/DownloadCore.jsm"
);

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  DownloadsCommon: "resource:///modules/DownloadsCommon.jsm",
  DownloadList: "resource://gre/modules/DownloadList.jsm",
});

/**
 * Responsible for detecting events related to downloads spam and updating the
 * downloads UI with this information.
 */
class DownloadSpamProtection {
  constructor() {
    /**
     * Tracks URLs we have detected download spam for.
     * @type {Map<string, DownloadSpam>}
     */
    this._blockedURLToDownloadSpam = new Map();
    this._browserWin = lazy.BrowserWindowTracker.getTopWindow();
    this._indicator = lazy.DownloadsCommon.getIndicatorData(this._browserWin);
    this.list = new lazy.DownloadList();
  }

  get spamList() {
    return this.list;
  }

  update(url) {
    if (this._blockedURLToDownloadSpam.has(url)) {
      let downloadSpam = this._blockedURLToDownloadSpam.get(url);
      this.spamList.remove(downloadSpam);
      downloadSpam.blockedDownloadsCount += 1;
      this.spamList.add(downloadSpam);
      this._indicator.onDownloadStateChanged(downloadSpam);
      return;
    }

    let downloadSpam = new DownloadSpam(url);
    this.spamList.add(downloadSpam);
    this._blockedURLToDownloadSpam.set(url, downloadSpam);
    let hasActiveDownloads = lazy.DownloadsCommon.summarizeDownloads(
      this._indicator._activeDownloads()
    ).numDownloading;
    if (!hasActiveDownloads) {
      this._browserWin.DownloadsPanel.showPanel();
    }
    this._indicator.onDownloadAdded(downloadSpam);
  }

  /**
   * Removes the download spam data for the current url.
   */
  clearDownloadSpam(URL) {
    if (this._blockedURLToDownloadSpam.has(URL)) {
      let downloadSpam = this._blockedURLToDownloadSpam.get(URL);
      this.spamList.remove(downloadSpam);
      this._indicator.onDownloadRemoved(downloadSpam);
      this._blockedURLToDownloadSpam.delete(URL);
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

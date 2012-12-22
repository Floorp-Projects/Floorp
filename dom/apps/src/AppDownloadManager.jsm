/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/FreeSpaceWatcher.jsm");

this.EXPORTED_SYMBOLS = ["AppDownloadManager"];

function debug(aMsg) {
  //dump("-*-*- AppDownloadManager.jsm : " + aMsg + "\n");
}

this.AppDownloadManager = {
  // Minimum disk free space we want to keep, in bytes.
  // Keep synchronized with Webapps.jsm
  MIN_REMAINING_FREESPACE: 5 * 1024 * 1024,

  downloads: {},
  count: 0,
  cancelFunc: null,
  timer: null,

  /**
   * Registers the function called when we need to cancel a download.
   * The function will be called with a single parameter being the
   * manifest URL.
   */
  registerCancelFunction: function app_dlMgr_registerCancel(aFunction) {
    this.cancelFunc = aFunction;
  },

  /**
   * Adds a download to the list of current downloads.
   * @param aManifestURL The manifest URL for the application being downloaded.
   * @param aDownload    An opaque object representing the download.
   */
  add: function app_dlMgr_add(aManifestURL, aDownload) {
    debug("Adding " + aManifestURL);
    if (!(aManifestURL in this.downloads)) {
      this.count++;
      if (this.count == 1) {
        this.timer = FreeSpaceWatcher.create(this.MIN_REMAINING_FREESPACE,
                                             this._spaceWatcher.bind(this));
      }
    }
    this.downloads[aManifestURL] = aDownload;
  },

  /**
   * Retrieves a download from the list of current downloads.
   * @param  aManifestURL The manifest URL for the application being retrieved.
   * @return              The opaque object representing the download.
   */
  get: function app_dlMgr_get(aManifestURL) {
    debug("Getting " + aManifestURL);
    return this.downloads[aManifestURL];
  },

  /**
   * Removes a download of the list of current downloads.
   * @param aManifestURL The manifest URL for the application being removed.
   */
  remove: function app_dlMgr_remove(aManifestURL) {
    debug("Removing " + aManifestURL);
    if (aManifestURL in this.downloads) {
      this.count--;
      delete this.downloads[aManifestURL];
      if (this.count == 0) {
        FreeSpaceWatcher.stop(this.timer);
      }
    }
  },

  /**
   * Callback for the free space watcher. This will call cancel on downloads
   * if needed.
   */
  _spaceWatcher: function app_dlMgr_watcher(aStatus) {
    debug("Disk space is now " + aStatus);
    if (aStatus == "free") {
      // Nothing to do.
      return;
    }

    // We cancel all downloads, because we don't know which ones we could
    // keep running. We can improve that later if we have better heuristics,
    // or when we'll support pause & resume we should just pause downloads.
    for (let url in this.downloads) {
      this.cancelFunc(url, "INSUFFICIENT_STORAGE");
    }
  }
}

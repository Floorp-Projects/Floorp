/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles the download progress indicator of the macOS Finder.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
});

export var DownloadsMacFinderProgress = {
  /**
   * Maps the path of the download, to the according progress indicator instance.
   */
  _finderProgresses: null,

  /**
   * This method is called after a new browser window on macOS is opened, it
   * registers for receiving download events for the progressbar of the Finder.
   */
  register() {
    // Ensure to register only once per process and not for every window.
    if (!this._finderProgresses) {
      this._finderProgresses = new Map();
      lazy.Downloads.getList(lazy.Downloads.ALL).then(list =>
        list.addView(this)
      );
    }
  },

  onDownloadAdded(download) {
    if (download.stopped) {
      return;
    }

    let finderProgress = Cc[
      "@mozilla.org/widget/macfinderprogress;1"
    ].createInstance(Ci.nsIMacFinderProgress);

    let path = download.target.path;

    finderProgress.init(path, () => {
      download.cancel().catch(console.error);
      download.removePartialData().catch(console.error);
    });

    if (download.hasProgress) {
      finderProgress.updateProgress(download.currentBytes, download.totalBytes);
    } else {
      finderProgress.updateProgress(0, 0);
    }
    this._finderProgresses.set(path, finderProgress);
  },

  onDownloadChanged(download) {
    let path = download.target.path;
    let finderProgress = this._finderProgresses.get(path);
    if (!finderProgress) {
      // The download is not tracked, it may have been restarted,
      // thus forward the call to onDownloadAdded to check if it should be tracked.
      this.onDownloadAdded(download);
    } else if (download.stopped) {
      finderProgress.end();
      this._finderProgresses.delete(path);
    } else {
      finderProgress.updateProgress(download.currentBytes, download.totalBytes);
    }
  },

  onDownloadRemoved(download) {
    let path = download.target.path;
    let finderProgress = this._finderProgresses.get(path);
    if (finderProgress) {
      finderProgress.end();
      this._finderProgresses.delete(path);
    }
  },
};

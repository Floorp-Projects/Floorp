/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");

let gSanitizeDialog = Object.freeze({
  /**
   * Sets up the UI.
   */
  init: function ()
  {
    let downloadsPref = document.getElementById("privacy.clearOnShutdown.downloads");
    downloadsPref.disabled = !DownloadsCommon.useToolkitUI;
    this.onClearHistoryChanged();
  },

  onClearHistoryChanged: function () {
    if (DownloadsCommon.useToolkitUI)
      return;
    let downloadsPref = document.getElementById("privacy.clearOnShutdown.downloads");
    let historyPref = document.getElementById("privacy.clearOnShutdown.history");
    downloadsPref.value = historyPref.value;
  }
});

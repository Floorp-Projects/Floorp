/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gSanitizeDialog = Object.freeze({
  init: function() {
    let customWidthElements = document.getElementsByAttribute("dialogWidth", "*");
    let isInSubdialog = document.documentElement.hasAttribute("subdialog");
    for (let element of customWidthElements) {
      element.style.width = element.getAttribute(isInSubdialog ? "subdialogWidth" : "dialogWidth");
    }
    this.onClearHistoryChanged();
  },

  onClearHistoryChanged: function () {
    let downloadsPref = document.getElementById("privacy.clearOnShutdown.downloads");
    let historyPref = document.getElementById("privacy.clearOnShutdown.history");
    downloadsPref.value = historyPref.value;
  }
});

/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../toolkit/content/preferencesBindings.js */

document
  .querySelector("dialog")
  .addEventListener("dialoghelp", window.top.openPrefsHelp);

Preferences.addAll([
  { id: "privacy.clearOnShutdown.history", type: "bool" },
  { id: "privacy.clearOnShutdown.formdata", type: "bool" },
  { id: "privacy.clearOnShutdown.downloads", type: "bool" },
  { id: "privacy.clearOnShutdown.cookies", type: "bool" },
  { id: "privacy.clearOnShutdown.cache", type: "bool" },
  { id: "privacy.clearOnShutdown.offlineApps", type: "bool" },
  { id: "privacy.clearOnShutdown.sessions", type: "bool" },
  { id: "privacy.clearOnShutdown.siteSettings", type: "bool" },
]);

var gSanitizeDialog = Object.freeze({
  init() {
    this.onClearHistoryChanged();

    Preferences.get("privacy.clearOnShutdown.history").on(
      "change",
      this.onClearHistoryChanged.bind(this)
    );
  },

  onClearHistoryChanged() {
    let downloadsPref = Preferences.get("privacy.clearOnShutdown.downloads");
    let historyPref = Preferences.get("privacy.clearOnShutdown.history");
    downloadsPref.value = historyPref.value;
  },
});

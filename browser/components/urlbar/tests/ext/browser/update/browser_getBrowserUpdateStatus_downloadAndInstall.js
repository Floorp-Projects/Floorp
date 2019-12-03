/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_fc_downloadOptIn.js

"use strict";

let params = { queryString: "&invalidCompleteSize=1" };

let downloadInfo = [];
if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED, false)) {
  downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
} else {
  downloadInfo[0] = { patchType: "partial", internalResult: "0" };
}

let steps = [
  {
    panelId: "checkingForUpdates",
    checkActiveUpdate: null,
    continueFile: CONTINUE_CHECK,
  },
  {
    panelId: "downloadAndInstall",
    checkActiveUpdate: null,
    continueFile: null,
  },
];

add_getBrowserUpdateStatus_task(
  params,
  steps,
  "downloadAndInstall",
  async () => {
    await UpdateUtils.setAppUpdateAutoEnabled(false);
  }
);

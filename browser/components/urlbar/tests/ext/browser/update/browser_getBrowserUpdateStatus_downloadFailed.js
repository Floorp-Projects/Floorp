/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_fc_patch_partialBadSize.js

"use strict";

let downloadInfo = [];
if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED, false)) {
  downloadInfo[0] = { patchType: "partial", bitsResult: gBadSizeResult };
  downloadInfo[1] = { patchType: "partial", internalResult: gBadSizeResult };
} else {
  downloadInfo[0] = { patchType: "partial", internalResult: gBadSizeResult };
}

let params = { queryString: "&partialPatchOnly=1&invalidPartialSize=1" };
let steps = [
  {
    panelId: "checkingForUpdates",
    checkActiveUpdate: null,
    continueFile: CONTINUE_CHECK,
  },
  {
    panelId: "downloading",
    checkActiveUpdate: { state: STATE_DOWNLOADING },
    continueFile: CONTINUE_DOWNLOAD,
    downloadInfo,
  },
  {
    panelId: "downloadFailed",
    checkActiveUpdate: null,
    continueFile: null,
  },
];

add_getBrowserUpdateStatus_task(params, steps, "downloadFailed");

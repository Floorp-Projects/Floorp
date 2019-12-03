/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_fc_check_noUpdate.js

"use strict";

let params = { queryString: "&noUpdates=1" };
let steps = [
  {
    panelId: "checkingForUpdates",
    checkActiveUpdate: null,
    continueFile: CONTINUE_CHECK,
  },
  {
    panelId: "noUpdatesFound",
    checkActiveUpdate: null,
    continueFile: null,
  },
];

add_getBrowserUpdateStatus_task(params, steps, "noUpdatesFound");

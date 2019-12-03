/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_bc_downloaded_staged.js

"use strict";

let params = {
  queryString: "&invalidCompleteSize=1",
  backgroundUpdate: true,
  continueFile: CONTINUE_STAGING,
  waitForUpdateState: STATE_APPLIED,
};
let steps = [
  {
    panelId: "apply",
    checkActiveUpdate: { state: STATE_APPLIED },
    continueFile: null,
  },
];

add_getBrowserUpdateStatus_task(params, steps, "readyForRestart", async () => {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]],
  });
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks the UPDATE_RESTART tip.
//
// The update parts of this test are adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_bc_downloaded_staged.js

"use strict";

let params = {
  queryString: "&invalidCompleteSize=1",
  backgroundUpdate: true,
  continueFile: CONTINUE_STAGING,
  waitForUpdateState: STATE_APPLIED,
};

let preSteps = [
  {
    panelId: "apply",
    checkActiveUpdate: { state: STATE_APPLIED },
    continueFile: null,
  },
];

add_task(async function test() {
  // Enable the pref that automatically downloads and installs updates.
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });

  // Set up the "apply" update state.
  await initUpdate(params);
  UrlbarProviderInterventions.checkForBrowserUpdate(true);
  await processUpdateSteps(preSteps);

  // Picking the tip should attempt to restart the browser.
  await doUpdateTest({
    searchString: SEARCH_STRINGS.UPDATE,
    tip: UrlbarProviderInterventions.TIP_TYPE.UPDATE_RESTART,
    title: /^The latest .+ is downloaded and ready to install\.$/,
    button: "Restart to Update",
    awaitCallback: awaitAppRestartRequest,
  });
});

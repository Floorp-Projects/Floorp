/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks the UPDATE_REFRESH tip.
//
// The update parts of this test are adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_fc_check_noUpdate.js

"use strict";

let params = { queryString: "&noUpdates=1" };

let preSteps = [
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

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });

  makeProfileResettable();

  // Set up the "no updates" update state.
  await initUpdate(params);
  UrlbarProviderInterventions.checkForBrowserUpdate(true);
  await processUpdateSteps(preSteps);

  // Picking the tip should open the refresh dialog.  Click its cancel
  // button.
  await doUpdateTest({
    searchString: SEARCH_STRINGS.UPDATE,
    tip: UrlbarProviderInterventions.TIP_TYPE.UPDATE_REFRESH,
    title: /^.+ is up to date\. Trying to fix a problem\? Restore default settings and remove old add-ons for optimal performance\.$/,
    button: /^Refresh .+…$/,
    awaitCallback() {
      return BrowserTestUtils.promiseAlertDialog(
        "cancel",
        "chrome://global/content/resetProfile.xhtml",
        { isSubDialog: true }
      );
    },
  });
});

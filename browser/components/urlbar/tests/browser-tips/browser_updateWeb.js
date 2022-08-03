/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Checks the UPDATE_WEB tip.
//
// The update parts of this test are adapted from:
// https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/browser_aboutDialog_fc_check_unsupported.js

"use strict";

let params = { queryString: "&unsupported=1" };

let preSteps = [
  {
    panelId: "checkingForUpdates",
    checkActiveUpdate: null,
    continueFile: CONTINUE_CHECK,
  },
  {
    panelId: "unsupportedSystem",
    checkActiveUpdate: null,
    continueFile: null,
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });

  // Set up the "unsupported update" update state.
  await initUpdate(params);
  UrlbarProviderInterventions.checkForBrowserUpdate(true);
  await processUpdateSteps(preSteps);

  // Picking the tip should open the download page in a new tab.
  let downloadTab = await doUpdateTest({
    searchString: SEARCH_STRINGS.UPDATE,
    tip: UrlbarProviderInterventions.TIP_TYPE.UPDATE_WEB,
    title: /^Get the latest .+ browser\.$/,
    button: "Download Now",
    awaitCallback() {
      return BrowserTestUtils.waitForNewTab(
        gBrowser,
        "https://www.mozilla.org/firefox/new/"
      );
    },
  });

  Assert.equal(gBrowser.selectedTab, downloadTab);
  BrowserTestUtils.removeTab(downloadTab);
});

/*
 * This file contains tests for the Preferences search bar.
 */

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.search", true]],
  });
});

/**
 * Test for searching for the "Block Lists" subdialog.
 */
add_task(async function() {
  async function doTest() {
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    await evaluateSearchResults("block online trackers", "trackingGroup");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
  await doTest();
});

/**
 * Test for searching for the "Allowed Sites - Pop-ups" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  await evaluateSearchResults("open pop-up windows", "permissionsGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

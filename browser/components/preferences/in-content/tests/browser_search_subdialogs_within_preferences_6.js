/*
* This file contains tests for the Preferences search bar.
*/

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.preferences.search", true],
  ]});
});

/**
 * Test for searching for the "Block Lists" subdialog.
 */
add_task(async function() {
  async function doTest() {
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
    await evaluateSearchResults("block Web elements", "trackingGroup");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.contentblocking.ui.enabled", true],
  ]});
  info("Run the test with Content Blocking UI enabled");
  await doTest();
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.contentblocking.ui.enabled", false],
  ]});
  info("Run the test with Content Blocking UI disabled");
  await doTest();
});

/**
 * Test for searching for the "Allowed Sites - Pop-ups" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("open pop-up windows", "permissionsGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
* This file contains tests for the Preferences search bar.
*/

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Test for searching for the "Update History" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("updates have been installed", "updateApp");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Location Permissions" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("set location permissions", "permissionsGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

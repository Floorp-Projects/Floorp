/*
* This file contains tests for the Preferences search bar.
*/

requestLongerTimeout(2);

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Test for searching for the "Camera Permissions" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("set camera permissions", "permissionsGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Microphone Permissions" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("set microphone permissions", "permissionsGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Notification Permissions" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("set notifications permissions", "permissionsGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

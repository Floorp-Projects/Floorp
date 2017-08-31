/*
* This file contains tests for the Preferences search bar.
*/

requestLongerTimeout(2);

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.preferences.search", true],
    ["browser.storageManager.enabled", true]
  ]});
});

/**
 * Test for searching for the "Device Manager" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("Security Modules and Devices", "certSelection");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Connection Settings" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("Use system proxy settings", "connectionGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Settings - Site Data" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("store site data on your computer", "siteDataGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

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
 * Test for searching for the "Saved Logins" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("sites are stored", "passwordsGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Exceptions - Tracking Protection" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("disabled Tracking Protection", "trackingGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

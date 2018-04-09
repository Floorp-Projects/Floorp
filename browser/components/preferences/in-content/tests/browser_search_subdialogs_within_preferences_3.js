/*
* This file contains tests for the Preferences search bar.
*/

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.preferences.search", true]
  ]});
});

/**
 * Test for searching for the "Allowed Sites - Add-ons Installation" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("allowed to install add-ons", "permissionsGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Certificate Manager" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("identify these certificate authorities", "certSelection");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

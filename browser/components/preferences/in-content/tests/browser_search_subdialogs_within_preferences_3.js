/*
* This file contains tests for the Preferences search bar.
*/

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Test for searching for the "Allowed Sites - Add-ons Installation" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  evaluateSearchResults("allowed to install add-ons", "addonsPhishingGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Certificate Manager" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  evaluateSearchResults("identify these certificate authorities", "certSelection");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Device Manager" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  evaluateSearchResults("Security Modules and Devices", "certSelection");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Connection Settings" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  evaluateSearchResults("Use system proxy settings", "connectionGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Settings - Site Data" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  evaluateSearchResults("store site data on your computer", "siteDataGroup");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
* This file contains tests for the Preferences search bar.
*/

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

/**
 * Test for searching for the "Settings - Site Data" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("cookies", ["siteDataGroup"]);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("site data", ["siteDataGroup"]);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("cache", ["siteDataGroup"]);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("third-party", "siteDataGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
* This file contains tests for the Preferences search bar.
*/

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.preferences.search", true]]});
});

// First, run the tests without the Content Blocking UI.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.contentblocking.ui.enabled", false]]});
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

// Now, run the tests with the Content Blocking UI.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.contentblocking.ui.enabled", true]]});
});

/**
 * Test for searching for the "Settings - Site Data" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  await evaluateSearchResults("cookies", ["siteDataGroup", "trackingGroup"]);
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
  await evaluateSearchResults("third-party", ["siteDataGroup", "trackingGroup"]);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

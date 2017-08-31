"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(async function test_openPreferences() {
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences");
  await gContentAPI.openPreferences();
  let tab = await promiseTabOpened;
  await BrowserTestUtils.removeTab(tab);
});

add_UITour_task(async function test_openInvalidPreferences() {
  await gContentAPI.openPreferences(999);

  try {
    await waitForConditionPromise(() => {
      return gBrowser.selectedBrowser.currentURI.spec.startsWith("about:preferences");
    }, "Check if about:preferences opened");
    ok(false, "No about:preferences tab should have opened");
  } catch (ex) {
    ok(true, "No about:preferences tab opened: " + ex);
  }
});

add_UITour_task(async function test_openPrivacyPreferences() {
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy");
  await gContentAPI.openPreferences("privacy");
  let tab = await promiseTabOpened;
  await BrowserTestUtils.removeTab(tab);
});

add_UITour_task(async function test_openOldDataChoicesTab() {
  if (!AppConstants.MOZ_DATA_REPORTING) {
    return;
  }
  await SpecialPowers.pushPrefEnv({set: [["browser.preferences.useOldOrganization", true]]});
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#advanced");
  await gContentAPI.openPreferences("privacy-reports");
  let tab = await promiseTabOpened;
  await BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "Initialized");
  let doc = gBrowser.selectedBrowser.contentDocument;
  let selectedTab = doc.getElementById("advancedPrefs").selectedTab;
  is(selectedTab.id, "dataChoicesTab", "Should open to the dataChoicesTab in the old Preferences");
  await BrowserTestUtils.removeTab(tab);
});

add_UITour_task(async function test_openPrivacyReports() {
  if (!AppConstants.MOZ_TELEMETRY_REPORTING &&
      !(AppConstants.MOZ_DATA_REPORTING && AppConstants.MOZ_CRASHREPORTER)) {
    return;
  }
  await SpecialPowers.pushPrefEnv({set: [["browser.preferences.useOldOrganization", false]]});
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy-reports");
  await gContentAPI.openPreferences("privacy-reports");
  let tab = await promiseTabOpened;
  await BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "Initialized");
  let doc = gBrowser.selectedBrowser.contentDocument;
  let reports = doc.querySelector("groupbox[data-subcategory='reports']");
  is(doc.location.hash, "#privacy", "Should not display the reports subcategory in the location hash.");
  is(reports.hidden, false, "Should open to the reports subcategory in the privacy pane in the new Preferences.");
  await BrowserTestUtils.removeTab(tab);
});

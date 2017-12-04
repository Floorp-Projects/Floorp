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

add_UITour_task(async function test_openPrivacyReports() {
  if (!AppConstants.MOZ_TELEMETRY_REPORTING &&
      !(AppConstants.MOZ_DATA_REPORTING && AppConstants.MOZ_CRASHREPORTER)) {
    return;
  }
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy-reports");
  await gContentAPI.openPreferences("privacy-reports");
  let tab = await promiseTabOpened;
  await BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "Initialized");
  let doc = gBrowser.selectedBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "Should not display the reports subcategory in the location hash.");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the reports section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "reports", "The reports section is spotlighted.");
  await BrowserTestUtils.removeTab(tab);
});

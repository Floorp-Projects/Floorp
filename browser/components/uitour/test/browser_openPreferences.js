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

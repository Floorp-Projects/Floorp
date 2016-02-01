"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(function* test_openPreferences() {
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences");
  yield gContentAPI.openPreferences();
  let tab = yield promiseTabOpened;
  yield BrowserTestUtils.removeTab(tab);
});

add_UITour_task(function* test_openInvalidPreferences() {
  yield gContentAPI.openPreferences(999);

  try {
    yield waitForConditionPromise(() => {
      return gBrowser.selectedBrowser.currentURI.spec.startsWith("about:preferences");
    }, "Check if about:preferences opened");
    ok(false, "No about:preferences tab should have opened");
  } catch (ex) {
    ok(true, "No about:preferences tab opened: " + ex);
  }
});

add_UITour_task(function* test_openPrivacyPreferences() {
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#privacy");
  yield gContentAPI.openPreferences("privacy");
  let tab = yield promiseTabOpened;
  yield BrowserTestUtils.removeTab(tab);
});

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(function* test_closeTab() {
  // Setting gTestTab to null indicates that the tab has already been closed,
  // and if this does not happen the test run will fail.
  let closePromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabClose");
  yield gContentAPI.closeTab();
  yield closePromise;
  gTestTab = null;
});

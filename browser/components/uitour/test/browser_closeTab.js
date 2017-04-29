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
  // In the e10s-case, having content request a tab close might mean
  // that the ContentTask used to send this closeTab message won't
  // get a response (since the message manager may have closed down).
  // So we ignore the Promise that closeTab returns, and use the TabClose
  // event to tell us when the tab has gone away.
  gContentAPI.closeTab();
  yield closePromise;
  gTestTab = null;
});

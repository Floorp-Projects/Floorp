"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

// Test that we can switch to about:newtab
add_UITour_task(function* test_aboutNewTab() {
  let newTabLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, "about:newtab");
  info("Showing about:newtab");
  yield gContentAPI.showNewTab();
  info("Waiting for about:newtab to load");
  yield newTabLoaded;
  is(gBrowser.selectedBrowser.currentURI.spec, "about:newtab", "Loaded about:newtab");
});

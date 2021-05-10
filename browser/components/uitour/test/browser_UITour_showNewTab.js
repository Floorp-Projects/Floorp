"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

// Test that we can switch to about:newtab
add_UITour_task(async function test_aboutNewTab() {
  let newTabLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:newtab"
  );
  info("Showing about:newtab");
  await gContentAPI.showNewTab();
  info("Waiting for about:newtab to load");
  await newTabLoaded;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:newtab",
    "Loaded about:newtab"
  );
  ok(gURLBar.focused, "Address bar gets focus");
});

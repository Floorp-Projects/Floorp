/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

// Test that we can switch to about:protections
add_UITour_task(async function test_openProtectionReport() {
  let aboutProtectionsLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:protections"
  );
  info("Showing about:protections");
  await gContentAPI.showProtectionReport();
  info("Waiting for about:protections to load");
  await aboutProtectionsLoaded;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:protections",
    "Loaded about:protections"
  );
});

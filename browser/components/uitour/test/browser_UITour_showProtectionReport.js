/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;

add_task(setup_UITourTest);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentblocking.database.enabled", false],
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
      ["browser.contentblocking.report.proxy.enabled", false],
    ],
  });
});

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
  // When the graph is built it means the messaging has finished,
  // we can close the tab.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => {
      let bars = content.document.querySelectorAll(".graph-bar");
      return bars.length;
    }, "The graph has been built");
  });

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:protections",
    "Loaded about:protections"
  );
});

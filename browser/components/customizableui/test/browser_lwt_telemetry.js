"use strict";

add_task(function clearTelemetry() {
  Services.telemetry.clearEvents();
});

add_task(async function testCustomize() {
  let getMoreURL = "about:blank#getMoreThemes";

  // Reset the theme prefs to ensure they haven't been messed with.
  await SpecialPowers.pushPrefEnv({set: [
    ["lightweightThemes.getMoreURL", getMoreURL],
  ]});

  await startCustomizing();

  // Find the footer buttons to test.
  let footerRow = document.getElementById("customization-lwtheme-menu-footer");
  let [manageButton, getMoreButton] = footerRow.childNodes;

  // Check the manage button, it should open about:addons.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  manageButton.click();
  let addonsTab = await waitForNewTab;

  is(gBrowser.currentURI.spec, "about:addons", "Manage opened about:addons");
  BrowserTestUtils.removeTab(addonsTab);

  // Check the get more button, we mocked it to open getMoreURL.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser, getMoreURL);
  getMoreButton.click();
  addonsTab = await waitForNewTab;

  is(gBrowser.currentURI.spec, getMoreURL, "Get more opened AMO");
  BrowserTestUtils.removeTab(addonsTab);

  let snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true);

  // Make sure we got some data.
  ok(snapshot.parent && snapshot.parent.length > 0, "Got parent telemetry events in the snapshot");

  // Only look at the related events after stripping the timestamp and category.
  let relatedEvents = snapshot.parent
    .filter(([timestamp, category, method, object]) =>
      category == "addonsManager" && object == "customize")
    .map(relatedEvent => relatedEvent.slice(2, 6));

  // Events are now [method, object, value, extra] as expected.
  Assert.deepEqual(relatedEvents, [
    ["link", "customize", "manageThemes"],
    ["link", "customize", "getThemes"],
  ], "The events are recorded correctly");

  // Wait for customize mode to be re-entered now that the customize tab is
  // active. This is needed for endCustomizing() to work properly.
  await TestUtils.waitForCondition(
    () => document.documentElement.getAttribute("customizing") == "true");
  await endCustomizing();
});

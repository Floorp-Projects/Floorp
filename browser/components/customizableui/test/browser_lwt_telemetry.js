"use strict";

add_task(function clearTelemetry() {
  Services.telemetry.clearEvents();
});

add_task(async function testCustomize() {
  let getMoreURL = "about:blank#getMoreThemes";

  // Reset the theme prefs to ensure they haven't been messed with.
  await SpecialPowers.pushPrefEnv({
    set: [["lightweightThemes.getMoreURL", getMoreURL]],
  });

  await startCustomizing();

  // Find the footer buttons to test.
  let manageLink = document.querySelector("#customization-lwtheme-link");

  // Check the manage themes button, it should open about:addons.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  manageLink.click();
  let addonsTab = await waitForNewTab;

  is(gBrowser.currentURI.spec, "about:addons", "Manage opened about:addons");
  BrowserTestUtils.removeTab(addonsTab);

  let snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );

  // Make sure we got some data.
  ok(
    snapshot.parent && !!snapshot.parent.length,
    "Got parent telemetry events in the snapshot"
  );

  // Only look at the related events after stripping the timestamp and category.
  let relatedEvents = snapshot.parent
    .filter(
      ([timestamp, category, method, object]) =>
        category == "addonsManager" && object == "customize"
    )
    .map(relatedEvent => relatedEvent.slice(2, 6));

  // Events are now [method, object, value, extra] as expected.
  Assert.deepEqual(
    relatedEvents,
    [["link", "customize", "manageThemes"]],
    "The events are recorded correctly"
  );

  // Wait for customize mode to be re-entered now that the customize tab is
  // active. This is needed for endCustomizing() to work properly.
  await TestUtils.waitForCondition(
    () => document.documentElement.getAttribute("customizing") == "true"
  );
  await endCustomizing();
});

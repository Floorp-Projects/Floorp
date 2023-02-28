/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for preference telemetry.

add_setup(async function() {
  // Simulate initialization for TelemetryEvent.
  // 1. Turn browser.urlbar.searchEngagementTelemetry.enabled on.
  // 2. Remove all Glean data.
  // 3. Call onPrefChanged() that will be called from TelemetryEvent constructor.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", true]],
  });
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  gURLBar.controller.engagementEvent.onPrefChanged(
    "searchEngagementTelemetry.enabled"
  );
});

add_task(async function prefMaxRichResults() {
  Assert.ok(UrlbarPrefs.get("maxRichResults"), "Sanity check");
  Assert.equal(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Record prefMaxResults when UrlbarController is initialized"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxRichResults", 0]],
  });
  Assert.equal(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Record prefMaxResults when the maxRichResults pref is updated"
  );
});

add_task(async function prefSuggestTopsites() {
  Assert.equal(
    Glean.urlbar.prefSuggestTopsites.testGetValue(),
    UrlbarPrefs.get("suggest.topsites"),
    "Record prefSuggestTopsites when UrlbarController is initialized"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.topsites", !UrlbarPrefs.get("suggest.topsites")],
    ],
  });
  Assert.equal(
    Glean.urlbar.prefSuggestTopsites.testGetValue(),
    UrlbarPrefs.get("suggest.topsites"),
    "Record prefSuggestTopsites when the suggest.topsites pref is updated"
  );
});

add_task(async function searchEngagementTelemetryEnabled() {
  info("Disable search engagement telemetry");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", false]],
  });
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.topsites", !UrlbarPrefs.get("suggest.topsites")],
      ["browser.urlbar.maxRichResults", 100],
    ],
  });
  Assert.notEqual(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Did not record prefMaxResults"
  );
  Assert.notEqual(
    Glean.urlbar.prefSuggestTopsites.testGetValue(),
    UrlbarPrefs.get("suggest.topsites"),
    "Did not record prefSuggestTopsites"
  );

  info("Enable search engagement telemetry");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", true]],
  });
  Assert.equal(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Update prefMaxResults when search engagement telemetry is enabled"
  );
  Assert.equal(
    Glean.urlbar.prefSuggestTopsites.testGetValue(),
    UrlbarPrefs.get("suggest.topsites"),
    "Update prefSuggestTopsites when search engagement telemetry is enabled"
  );
});

add_task(async function nimbusSearchEngagementTelemetryEnabled() {
  info("Disable search engagement telemetry");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", false]],
  });

  info("Enable search engagement telemetry from Nimbus");
  // eslint-disable-next-line mozilla/valid-lazy
  const doCleanup = await lazy.UrlbarTestUtils.initNimbusFeature({
    searchEngagementTelemetryEnabled: true,
  });
  Assert.equal(
    Glean.urlbar.prefMaxResults.testGetValue(),
    UrlbarPrefs.get("maxRichResults"),
    "Update prefMaxResults when search engagement telemetry is enabled"
  );
  Assert.equal(
    Glean.urlbar.prefSuggestTopsites.testGetValue(),
    UrlbarPrefs.get("suggest.topsites"),
    "Update prefSuggestTopsites when search engagement telemetry is enabled"
  );

  doCleanup();
});

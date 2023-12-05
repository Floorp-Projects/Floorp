/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(10);

const TELEMETRY_PREF =
  "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar";

add_task(async function test_newtab_handoff_performance_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [[TELEMETRY_PREF, true]],
  });

  Services.fog.testResetFOG();
  let TelemetryFeed =
    AboutNewTab.activityStream.store.feeds.get("feeds.telemetry");
  TelemetryFeed.init(); // INIT action doesn't happen by default.

  Assert.equal(true, Glean.newtabHandoffPreference.enabled.testGetValue());

  await SpecialPowers.pushPrefEnv({
    set: [[TELEMETRY_PREF, false]],
  });
  Assert.equal(false, Glean.newtabHandoffPreference.enabled.testGetValue());
  await SpecialPowers.popPrefEnv();
});

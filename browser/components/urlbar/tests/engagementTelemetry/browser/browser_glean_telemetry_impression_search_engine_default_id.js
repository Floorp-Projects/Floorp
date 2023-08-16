/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - search_engine_default_id

add_setup(async function () {
  await initSearchEngineDefaultIdTest();
  // Increase the pausing time to ensure to ready for all suggestions.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs",
        500,
      ],
    ],
  });
});

add_task(async function basic() {
  await doSearchEngineDefaultIdTest({
    trigger: () => waitForPauseImpression(),
    assert: engineId =>
      assertImpressionTelemetry([{ search_engine_default_id: engineId }]),
  });
});

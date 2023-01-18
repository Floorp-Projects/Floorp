/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the impression telemetry behavior with its preferences.

add_setup(async function() {
  await initPreferencesTest();
});

add_task(async function enabled() {
  await doSearchEngagementTelemetryTest({
    enabled: true,
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", sap: "urlbar_newtab" }]),
  });
});

add_task(async function disabled() {
  await doSearchEngagementTelemetryTest({
    enabled: false,
    trigger: () => waitForPauseImpression(),
    assert: () => assertImpressionTelemetry([]),
  });
});

add_task(async function nimbusEnabled() {
  await doNimbusTest({
    enabled: true,
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", sap: "urlbar_newtab" }]),
  });
});

add_task(async function nimbusDisabled() {
  await doNimbusTest({
    enabled: false,
    trigger: () => waitForPauseImpression(),
    assert: () => assertImpressionTelemetry([]),
  });
});

add_task(async function pauseImpressionIntervalMs() {
  const additionalInterval = 1000;
  const originalInterval = UrlbarPrefs.get(
    "searchEngagementTelemetry.pauseImpressionIntervalMs"
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.searchEngagementTelemetry.enabled", true],
      [
        "browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs",
        originalInterval + additionalInterval,
      ],
    ],
  });

  await doTest(async browser => {
    await openPopup("https://example.com");

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, originalInterval));
    await Services.fog.testFlushAllChildren();
    assertImpressionTelemetry([]);

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, additionalInterval));
    await Services.fog.testFlushAllChildren();
    assertImpressionTelemetry([{ sap: "urlbar_newtab" }]);
  });

  await SpecialPowers.popPrefEnv();
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the impression telemetry behavior with its preferences.

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_task(async function enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", true]],
  });

  await doTest(async browser => {
    await openPopup("https://example.com");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", sap: "urlbar_newtab" }]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", false]],
  });

  await doTest(async browser => {
    await openPopup("https://example.com");
    await waitForPauseImpression();

    assertImpressionTelemetry([]);
  });

  await SpecialPowers.popPrefEnv();
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

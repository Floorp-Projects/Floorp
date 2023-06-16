/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the impression telemetry behavior with its preferences.

add_setup(async function () {
  await setup();
});

add_task(async function pauseImpressionIntervalMs() {
  const additionalInterval = 1000;
  const originalInterval = UrlbarPrefs.get(
    "searchEngagementTelemetry.pauseImpressionIntervalMs"
  );
  await SpecialPowers.pushPrefEnv({
    set: [
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

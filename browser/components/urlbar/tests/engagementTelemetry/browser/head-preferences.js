/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doSearchEngagementTelemetryTest({ trigger, assert, enabled }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", enabled]],
  });

  await doTest(async browser => {
    await openPopup("https://example.com");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doNimbusTest({ trigger, assert, enabled }) {
  const doCleanup = await setupNimbus({
    searchEngagementTelemetryEnabled: enabled,
  });

  await doTest(async browser => {
    await openPopup("https://example.com");

    await trigger();
    await assert();
  });

  doCleanup();
}

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Glean telemetry behavior with its preferences.

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
    await doBlur();

    assertAbandonmentTelemetry([{ sap: "urlbar_newtab" }]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchEngagementTelemetry.enabled", false]],
  });

  await doTest(async browser => {
    await openPopup("https://example.com");
    await doBlur();

    assertAbandonmentTelemetry([]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function nimbusEnabled() {
  const doCleanup = await setupNimbus({
    searchEngagementTelemetryEnabled: true,
  });

  await doTest(async browser => {
    await openPopup("https://example.com");
    await doBlur();

    assertAbandonmentTelemetry([{ sap: "urlbar_newtab" }]);
  });

  doCleanup();
});

add_task(async function nimbusDisabled() {
  const doCleanup = await setupNimbus({
    searchEngagementTelemetryEnabled: false,
  });

  await doTest(async browser => {
    await openPopup("https://example.com");
    await doBlur();

    assertAbandonmentTelemetry([]);
  });

  doCleanup();
});

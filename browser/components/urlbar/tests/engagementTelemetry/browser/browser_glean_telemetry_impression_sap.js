/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - sap

add_setup(async function () {
  await initSapTest();
});

add_task(async function urlbar() {
  await doUrlbarTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        { reason: "pause", sap: "urlbar_newtab" },
        { reason: "pause", sap: "urlbar" },
      ]),
  });
});

add_task(async function handoff() {
  await doHandoffTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", sap: "handoff" }]),
  });
});

add_task(async function urlbar_addonpage() {
  await doUrlbarAddonpageTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", sap: "urlbar_addonpage" }]),
  });
});

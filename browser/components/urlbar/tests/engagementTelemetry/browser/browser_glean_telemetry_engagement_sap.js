/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of engagement telemetry.
// - sap

add_setup(async function () {
  await initSapTest();
});

add_task(async function urlbar() {
  await doUrlbarTest({
    trigger: () => doEnter(),
    assert: () =>
      assertEngagementTelemetry([{ sap: "urlbar_newtab" }, { sap: "urlbar" }]),
  });
});

add_task(async function handoff() {
  await doHandoffTest({
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ sap: "handoff" }]),
  });
});

add_task(async function urlbar_addonpage() {
  await doUrlbarAddonpageTest({
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ sap: "urlbar_addonpage" }]),
  });
});

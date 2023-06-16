/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - sap

add_setup(async function () {
  await initSapTest();
});

add_task(async function urlbar_newtab() {
  await doUrlbarNewTabTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ sap: "urlbar_newtab" }]),
  });
});

add_task(async function urlbar() {
  await doUrlbarTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ sap: "urlbar" }]),
  });
});

add_task(async function handoff() {
  await doHandoffTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ sap: "handoff" }]),
  });
});

add_task(async function urlbar_addonpage() {
  await doUrlbarAddonpageTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ sap: "urlbar_addonpage" }]),
  });
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Glean telemetry behavior with its preferences.

add_setup(async function() {
  await initPreferencesTest();
});

add_task(async function enabled() {
  await doSearchEngagementTelemetryTest({
    enabled: true,
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ sap: "urlbar_newtab" }]),
  });
});

add_task(async function disabled() {
  await doSearchEngagementTelemetryTest({
    enabled: false,
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([]),
  });
});

add_task(async function nimbusEnabled() {
  await doNimbusTest({
    enabled: true,
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ sap: "urlbar_newtab" }]),
  });
});

add_task(async function nimbusDisabled() {
  await doNimbusTest({
    enabled: false,
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([]),
  });
});

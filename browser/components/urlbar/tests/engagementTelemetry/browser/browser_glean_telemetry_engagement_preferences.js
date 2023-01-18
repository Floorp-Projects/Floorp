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
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ sap: "urlbar_newtab" }]),
  });
});

add_task(async function disabled() {
  await doSearchEngagementTelemetryTest({
    enabled: false,
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([]),
  });
});

add_task(async function nimbusEnabled() {
  await doNimbusTest({
    enabled: true,
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([{ sap: "urlbar_newtab" }]),
  });
});

add_task(async function nimbusDisabled() {
  await doNimbusTest({
    enabled: false,
    trigger: () => doEnter(),
    assert: () => assertEngagementTelemetry([]),
  });
});

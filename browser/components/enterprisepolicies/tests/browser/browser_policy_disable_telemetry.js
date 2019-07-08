/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_telemetry() {
  const { TelemetryReportingPolicy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryReportingPolicy.jsm"
  );

  ok(TelemetryReportingPolicy, "TelemetryReportingPolicy exists");
  is(TelemetryReportingPolicy.canUpload(), true, "Telemetry is enabled");

  await setupPolicyEngineWithJson({
    policies: {
      DisableTelemetry: true,
    },
  });

  is(TelemetryReportingPolicy.canUpload(), false, "Telemetry is disabled");
});

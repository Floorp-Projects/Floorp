/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AboutWelcomeTelemetry } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const TELEMETRY_PREF = "browser.newtabpage.activity-stream.telemetry";

add_task(function test_enabled() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(TELEMETRY_PREF);
  });
  Services.prefs.setBoolPref(TELEMETRY_PREF, true);

  const AWTelemetry = new AboutWelcomeTelemetry();

  equal(AWTelemetry.telemetryEnabled, true, "Telemetry should be on");

  Services.prefs.setBoolPref(TELEMETRY_PREF, false);

  equal(AWTelemetry.telemetryEnabled, false, "Telemetry should be off");
});

add_task(async function test_pingPayload() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(TELEMETRY_PREF);
  });
  Services.prefs.setBoolPref(TELEMETRY_PREF, true);
  const AWTelemetry = new AboutWelcomeTelemetry();
  const stub = sinon.stub(
    AWTelemetry.pingCentre,
    "sendStructuredIngestionPing"
  );
  sinon.stub(AWTelemetry, "_createPing").resolves({ event: "MOCHITEST" });

  await AWTelemetry.sendTelemetry();

  equal(stub.callCount, 1, "Call was made");
  // check the endpoint
  ok(
    stub.firstCall.args[1].includes("/messaging-system/onboarding"),
    "Endpoint is correct"
  );
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AboutWelcomeTelemetry } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
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

add_task(function test_mayAttachAttribution() {
  const sandbox = sinon.createSandbox();
  const AWTelemetry = new AboutWelcomeTelemetry();

  sandbox.stub(AttributionCode, "getCachedAttributionData").returns(null);

  let ping = AWTelemetry._maybeAttachAttribution({});

  equal(ping.attribution, undefined, "Should not set attribution if it's null");

  sandbox.restore();
  sandbox.stub(AttributionCode, "getCachedAttributionData").returns({});
  ping = AWTelemetry._maybeAttachAttribution({});

  equal(
    ping.attribution,
    undefined,
    "Should not set attribution if it's empty"
  );

  const attr = {
    source: "google.com",
    medium: "referral",
    campaign: "Firefox-Brand-US-Chrome",
    content: "(not set)",
    experiment: "(not set)",
    variation: "(not set)",
    ua: "chrome",
  };
  sandbox.restore();
  sandbox.stub(AttributionCode, "getCachedAttributionData").returns(attr);
  ping = AWTelemetry._maybeAttachAttribution({});

  equal(ping.attribution, attr, "Should set attribution if it presents");
});

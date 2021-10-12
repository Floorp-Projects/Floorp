/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PingCentre, PingCentreConstants } = ChromeUtils.import(
  "resource:///modules/PingCentre.jsm"
);
const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { UpdateUtils } = ChromeUtils.import(
  "resource://gre/modules/UpdateUtils.jsm"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const FAKE_PING = { event: "fake_event", value: "fake_value", locale: "en-US" };
const FAKE_ENDPOINT = "https://www.test.com";

let pingCentre;
let sandbox;

function _setUp() {
  Services.prefs.setBoolPref(PingCentreConstants.TELEMETRY_PREF, true);
  Services.prefs.setBoolPref(PingCentreConstants.FHR_UPLOAD_ENABLED_PREF, true);
  Services.prefs.setBoolPref(PingCentreConstants.LOGGING_PREF, true);
  sandbox.restore();
}

add_task(function setup() {
  sandbox = sinon.createSandbox();
  _setUp();
  pingCentre = new PingCentre({ topic: "test_topic" });

  registerCleanupFunction(() => {
    sandbox.restore();
    Services.prefs.clearUserPref(PingCentreConstants.TELEMETRY_PREF);
    Services.prefs.clearUserPref(PingCentreConstants.FHR_UPLOAD_ENABLED_PREF);
    Services.prefs.clearUserPref(PingCentreConstants.LOGGING_PREF);
  });
});

add_task(function test_enabled() {
  _setUp();
  Assert.ok(pingCentre.enabled, "Telemetry should be on");
});

add_task(function test_disabled_by_pingCentre() {
  _setUp();
  Services.prefs.setBoolPref(PingCentreConstants.TELEMETRY_PREF, false);

  Assert.ok(!pingCentre.enabled, "Telemetry should be off");
});

add_task(function test_disabled_by_FirefoxHealthReport() {
  _setUp();
  Services.prefs.setBoolPref(
    PingCentreConstants.FHR_UPLOAD_ENABLED_PREF,
    false
  );

  Assert.ok(!pingCentre.enabled, "Telemetry should be off");
});

add_task(function test_logging() {
  _setUp();
  Assert.ok(pingCentre.logging, "Logging should be on");

  Services.prefs.setBoolPref(PingCentreConstants.LOGGING_PREF, false);

  Assert.ok(!pingCentre.logging, "Logging should be off");
});

add_task(function test_createExperimentsPayload() {
  _setUp();
  const activeExperiments = {
    exp1: { branch: "foo", enrollmentID: "SOME_RANDON_ID" },
    exp2: { branch: "bar", type: "PrefStudy" },
    exp3: {},
  };
  sandbox
    .stub(TelemetryEnvironment, "getActiveExperiments")
    .returns(activeExperiments);
  const expected = {
    exp1: { branch: "foo" },
    exp2: { branch: "bar" },
  };

  const experiments = pingCentre._createExperimentsPayload();

  Assert.deepEqual(
    experiments,
    expected,
    "Should create experiments with all the required context"
  );
});

add_task(function test_createExperimentsPayload_without_active_experiments() {
  _setUp();
  sandbox.stub(TelemetryEnvironment, "getActiveExperiments").returns({});
  const experiments = pingCentre._createExperimentsPayload({});

  Assert.deepEqual(experiments, {}, "Should send an empty object");
});

add_task(function test_createStructuredIngestionPing() {
  _setUp();
  sandbox
    .stub(TelemetryEnvironment, "getActiveExperiments")
    .returns({ exp1: { branch: "foo" } });
  const ping = pingCentre._createStructuredIngestionPing(FAKE_PING);
  const expected = {
    experiments: { exp1: { branch: "foo" } },
    locale: "en-US",
    version: AppConstants.MOZ_APP_VERSION,
    release_channel: UpdateUtils.getUpdateChannel(false),
    ...FAKE_PING,
  };

  Assert.deepEqual(ping, expected, "Should create a valid ping");
});

add_task(function test_sendStructuredIngestionPing_disabled() {
  _setUp();
  sandbox.stub(PingCentre, "_sendInGzip").resolves();
  Services.prefs.setBoolPref(PingCentreConstants.TELEMETRY_PREF, false);
  pingCentre.sendStructuredIngestionPing(FAKE_PING, FAKE_ENDPOINT);

  Assert.ok(PingCentre._sendInGzip.notCalled, "Should not be sent");
});

add_task(function test_sendStructuredIngestionPing_success() {
  _setUp();
  sandbox.stub(PingCentre, "_sendInGzip").resolves();
  pingCentre.sendStructuredIngestionPing(FAKE_PING, FAKE_ENDPOINT);

  Assert.equal(PingCentre._sendInGzip.callCount, 1, "Should be sent");
});

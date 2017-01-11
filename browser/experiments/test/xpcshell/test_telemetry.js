/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/TelemetryLog.jsm");
var {TELEMETRY_LOG, Experiments} = Cu.import("resource:///modules/experiments/Experiments.jsm", {});


const MANIFEST_HANDLER         = "manifests/handler";

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;


var gHttpServer          = null;
var gHttpRoot            = null;
var gDataRoot            = null;
var gPolicy              = null;
var gManifestObject      = null;
var gManifestHandlerURI  = null;

const TLOG = TELEMETRY_LOG;

function checkEvent(event, id, data) {
  do_print("Checking message " + id);
  Assert.equal(event[0], id, "id should match");
  Assert.ok(event[1] > 0, "timestamp should be greater than 0");

  if (data === undefined) {
   Assert.equal(event.length, 2, "event array should have 2 entries");
  } else {
    Assert.equal(event.length, data.length + 2, "event entry count should match expected count");
    for (var i = 0; i < data.length; ++i) {
      Assert.equal(typeof(event[i + 2]), "string", "event entry should be a string");
      Assert.equal(event[i + 2], data[i], "event entry should match expected entry");
    }
  }
}

function run_test() {
  run_next_test();
}

add_task(function* test_setup() {
  loadAddonManager();

  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let port = gHttpServer.identity.primaryPort;
  gHttpRoot = "http://localhost:" + port + "/";
  gDataRoot = gHttpRoot + "data/";
  gManifestHandlerURI = gHttpRoot + MANIFEST_HANDLER;
  gHttpServer.registerDirectory("/data/", do_get_cwd());
  gHttpServer.registerPathHandler("/" + MANIFEST_HANDLER, (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify(gManifestObject));
    response.processAsync();
    response.finish();
  });
  do_register_cleanup(() => gHttpServer.stop(() => {}));

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
  Services.prefs.setCharPref(PREF_MANIFEST_URI, gManifestHandlerURI);
  Services.prefs.setIntPref(PREF_FETCHINTERVAL, 0);

  gPolicy = new Experiments.Policy();
  let dummyTimer = { cancel: () => {}, clear: () => {} };
  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    oneshotTimer: (callback, timeout, thisObj, name) => dummyTimer,
  });

  yield removeCacheFile();
});

// Test basic starting and stopping of experiments.

add_task(function* test_telemetryBasics() {
  // Check TelemetryLog instead of TelemetrySession.getPayload().log because
  // TelemetrySession gets Experiments.instance() and side-effects log entries.

  let expectedLogLength = 0;

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate1 = futureDate(baseDate, 50 * MS_IN_ONE_DAY);
  let endDate1   = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let startDate2 = futureDate(baseDate, 150 * MS_IN_ONE_DAY);
  let endDate2   = futureDate(baseDate, 200 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate1),
        endTime:          dateToSeconds(endDate1),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
      {
        id:               EXPERIMENT2_ID,
        xpiURL:           gDataRoot + EXPERIMENT2_XPI_NAME,
        xpiHash:          EXPERIMENT2_XPI_SHA1,
        startTime:        dateToSeconds(startDate2),
        endTime:          dateToSeconds(endDate2),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.
  // Use updateManifest() to provide for coverage of that path.

  let now = baseDate;
  defineNow(gPolicy, now);

  yield experiments.updateManifest();
  let list = yield experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  expectedLogLength += 2;
  let log = TelemetryLog.entries();
  do_print("Telemetry log: " + JSON.stringify(log));
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 2], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.REJECTED, EXPERIMENT1_ID, "startTime"]);
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.REJECTED, EXPERIMENT2_ID, "startTime"]);

  // Trigger update, clock set for experiment 1 to start.

  now = futureDate(startDate1, 5 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries. Got " + log.toSource());
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.ACTIVATED, EXPERIMENT1_ID]);

  // Trigger update, clock set for experiment 1 to stop.

  now = futureDate(endDate1, 1000);
  defineNow(gPolicy, now);

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  expectedLogLength += 2;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 2], TLOG.TERMINATION_KEY,
             [TLOG.TERMINATION.EXPIRED, EXPERIMENT1_ID]);
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.REJECTED, EXPERIMENT2_ID, "startTime"]);

  // Trigger update, clock set for experiment 2 to start with invalid hash.

  now = startDate2;
  defineNow(gPolicy, now);
  gManifestObject.experiments[1].xpiHash = "sha1:0000000000000000000000000000000000000000";

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.INSTALL_FAILURE, EXPERIMENT2_ID]);

  // Trigger update, clock set for experiment 2 to properly start now.

  now = futureDate(now, MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[1].xpiHash = EXPERIMENT2_XPI_SHA1;

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.ACTIVATED, EXPERIMENT2_ID]);

  // Fake user uninstall of experiment via add-on manager.

  now = futureDate(now, MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.disableExperiment(TLOG.TERMINATION.ADDON_UNINSTALLED);
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.TERMINATION_KEY,
             [TLOG.TERMINATION.ADDON_UNINSTALLED, EXPERIMENT2_ID]);

  // Trigger update with experiment 1a ready to start.

  now = futureDate(now, MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].id      = EXPERIMENT3_ID;
  gManifestObject.experiments[0].endTime = dateToSeconds(futureDate(now, 50 * MS_IN_ONE_DAY));

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 3, "Experiment list should have 3 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.ACTIVATED, EXPERIMENT3_ID]);

  // Trigger disable of an experiment via the API.

  now = futureDate(now, MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.disableExperiment(TLOG.TERMINATION.FROM_API);
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 3, "Experiment list should have 3 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.TERMINATION_KEY,
             [TLOG.TERMINATION.FROM_API, EXPERIMENT3_ID]);

  // Trigger update with experiment 1a ready to start.

  now = futureDate(now, MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].id      = EXPERIMENT4_ID;
  gManifestObject.experiments[0].endTime = dateToSeconds(futureDate(now, 50 * MS_IN_ONE_DAY));

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 4, "Experiment list should have 4 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.ACTIVATION_KEY,
             [TLOG.ACTIVATION.ACTIVATED, EXPERIMENT4_ID]);

  // Trigger experiment termination by something other than expiry via the manifest.

  now = futureDate(now, MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].os = "Plan9";

  yield experiments.updateManifest();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 4, "Experiment list should have 4 entries.");

  expectedLogLength += 1;
  log = TelemetryLog.entries();
  Assert.equal(log.length, expectedLogLength, "Telemetry log should have " + expectedLogLength + " entries.");
  checkEvent(log[log.length - 1], TLOG.TERMINATION_KEY,
             [TLOG.TERMINATION.RECHECK, EXPERIMENT4_ID, "os"]);

  // Cleanup.

  yield promiseRestartManager();
  yield removeCacheFile();
});

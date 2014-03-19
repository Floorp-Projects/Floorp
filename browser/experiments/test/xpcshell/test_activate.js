/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource:///modules/experiments/Experiments.jsm");

const FILE_MANIFEST            = "experiments.manifest";
const PREF_EXPERIMENTS_ENABLED = "experiments.enabled";
const PREF_LOGGING_LEVEL       = "experiments.logging.level";
const PREF_LOGGING_DUMP        = "experiments.logging.dump";
const PREF_MANIFEST_URI        = "experiments.manifest.uri";

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;

let gProfileDir = null;
let gHttpServer = null;
let gHttpRoot   = null;
let gReporter   = null;
let gPolicy     = null;

let gGlobalScope = this;
function loadAddonManager() {
  let ns = {};
  Cu.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../toolkit/mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
}

function ManifestEntry(data) {
  this.id        = data.id        || EXPERIMENT1_ID;
  this.xpiURL    = data.xpiURL    || gHttpRoot + EXPERIMENT1_XPI_NAME;
  this.xpiHash   = data.xpiHash   || EXPERIMENT1_XPI_SHA1;
  this.appName   = data.appName   || ["XPCShell"];
  this.channel   = data.appName   || ["nightly"];
  this.startTime = data.startTime || new Date(2010, 0, 1, 12).getTime() / 1000;
  this.endTime   = data.endTime   || new Date(9001, 0, 1, 12).getTime() / 1000;
  this.maxActiveSeconds = data.maxActiveSeconds || 5 * SEC_IN_ONE_DAY;
}

function run_test() {
  run_next_test();
}

add_task(function* test_setup() {
  loadAddonManager();
  gProfileDir = do_get_profile();
  gPolicy = new Experiments.Policy();

  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let port = gHttpServer.identity.primaryPort;
  gHttpRoot = "http://localhost:" + port + "/";
  gHttpServer.registerDirectory("/", do_get_cwd());
  do_register_cleanup(() => gHttpServer.stop(() => {}));

  gReporter = yield getReporter("json_payload_simple");
  yield gReporter.collectMeasurements();
  let payload = yield gReporter.getJSONPayload(true);

  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    healthReportPayload: () => Promise.resolve(payload),
  });

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);

  let experiments = new Experiments.Experiments();
});

function isApplicable(experiment) {
  let deferred = Promise.defer();
  experiment.isApplicable().then(
    result => deferred.resolve({ applicable: true,  reason: null }),
    reason => deferred.resolve({ applicable: false, reason: reason })
  );

  return deferred.promise;
}

add_task(function* test_startStop() {
  let baseDate  = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 30 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 60 * MS_IN_ONE_DAY);
  let manifestData = new ManifestEntry({
    startTime:        dateToSeconds(startDate),
    endTime:          dateToSeconds(endDate),
    maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
  });
  let experiment = new Experiments.ExperimentEntry(gPolicy);
  experiment.initFromManifestData(manifestData);
  let result;

  defineNow(gPolicy, baseDate);
  result = yield isApplicable(experiment);
  Assert.equal(result.applicable, false, "Experiment should not be applicable.");
  Assert.equal(experiment.enabled, false, "Experiment should not be enabled.");

  defineNow(gPolicy, futureDate(startDate, 5 * MS_IN_ONE_DAY));
  result = yield isApplicable(experiment);
  Assert.equal(result.applicable, true, "Experiment should now be applicable.");
  Assert.equal(experiment.enabled, false, "Experiment should not be enabled.");

  yield experiment.start();
  Assert.equal(experiment.enabled, true, "Experiment should now be enabled.");

  yield experiment.stop();
  Assert.equal(experiment.enabled, false, "Experiment should not be enabled.");

  yield experiment.start();
  Assert.equal(experiment.enabled, true, "Experiment should now be enabled.");

  let shouldStop = yield experiment._shouldStop();
  Assert.equal(shouldStop, false, "shouldStop should be false.");
  let maybeStop = yield experiment.maybeStop();
  Assert.equal(maybeStop, false, "Experiment should not have been stopped.");
  Assert.equal(experiment.enabled, true, "Experiment should be enabled.");

  defineNow(gPolicy, futureDate(endDate, MS_IN_ONE_DAY));
  shouldStop = yield experiment._shouldStop();
  Assert.equal(shouldStop, true, "shouldStop should now be true.");
  maybeStop = yield experiment.maybeStop();
  Assert.equal(maybeStop, true, "Experiment should have been stopped.");
  Assert.equal(experiment.enabled, false, "Experiment should be disabled.");
});

add_task(function* shutdown() {
  yield gReporter._shutdown();
  yield removeCacheFile();
});

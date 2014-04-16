/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
XPCOMUtils.defineLazyModuleGetter(this, "Experiments",
  "resource:///modules/experiments/Experiments.jsm");

const FILE_MANIFEST            = "experiments.manifest";
const MANIFEST_HANDLER         = "manifests/handler";

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;

let gProfileDir          = null;
let gHttpServer          = null;
let gHttpRoot            = null;
let gDataRoot            = null;
let gReporter            = null;
let gPolicy              = null;
let gManifestObject      = null;
let gManifestHandlerURI  = null;
let gTimerScheduleOffset = -1;

function run_test() {
  run_next_test();
}

add_task(function* test_setup() {
  loadAddonManager();
  gProfileDir = do_get_profile();
  yield removeCacheFile();

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

  disableCertificateChecks();

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
  Services.prefs.setCharPref(PREF_MANIFEST_URI, gManifestHandlerURI);
  Services.prefs.setIntPref(PREF_FETCHINTERVAL, 0);

  gReporter = yield getReporter("json_payload_simple");
  yield gReporter.collectMeasurements();
  let payload = yield gReporter.getJSONPayload(true);
  do_register_cleanup(() => gReporter._shutdown());

  gPolicy = new Experiments.Policy();
  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    healthReportPayload: () => {},
    oneshotTimer: (callback, timeout, thisObj, name) => gTimerScheduleOffset = timeout,
  });
});

function checkExperimentListsEqual(list, list2) {
  Assert.equal(list.length, list2.length, "Lists should have the same length.")

  for (let i=0; i<list.length; ++i) {
    for (let k of Object.keys(list[i])) {
      Assert.equal(list[i][k], list2[i][k],
                   "Field '" + k + "' should match for list entry " + i + ".");
    }
  }
}

function checkExperimentSerializations(experimentEntryIterator) {
  for (let experiment of experimentEntryIterator) {
    let experiment2 = new Experiments.ExperimentEntry(gPolicy);
    let jsonStr = JSON.stringify(experiment.toJSON());
    Assert.ok(experiment2.initFromCacheData(JSON.parse(jsonStr)),
              "Should have initialized successfully from JSON serialization.");
    Assert.equal(JSON.stringify(experiment), JSON.stringify(experiment2),
                 "Object stringifications should match.");
  }
}

// Set up an experiments instance and check if it is properly restored from cache.

add_task(function* test_cache() {
  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
      {
        id:               EXPERIMENT2_ID,
        xpiURL:           gDataRoot + EXPERIMENT2_XPI_NAME,
        xpiHash:          EXPERIMENT2_XPI_SHA1,
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
      {
        id:               EXPERIMENT3_ID,
        xpiURL:           "https://inval.id/foo.xpi",
        xpiHash:          "sha1:0000000000000000000000000000000000000000",
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  // Setup dates for the experiments.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDates = [];
  let endDates   = [];

  for (let i=0; i<gManifestObject.experiments.length; ++i) {
    let experiment = gManifestObject.experiments[i];
    startDates.push(futureDate(baseDate, (50 + (150 * i)) * MS_IN_ONE_DAY));
    endDates  .push(futureDate(startDates[i], 50 * MS_IN_ONE_DAY));
    experiment.startTime = dateToSeconds(startDates[i]);
    experiment.endTime   = dateToSeconds(endDates[i]);
  }

  // Data to compare the result of Experiments.getExperiments() against.

  let experimentListData = [
    {
      id: EXPERIMENT2_ID,
      name: "Test experiment 2",
      description: "And yet another experiment that experiments experimentally.",
    },
    {
      id: EXPERIMENT1_ID,
      name: EXPERIMENT1_NAME,
      description: "Yet another experiment that experiments experimentally.",
    },
  ];

  // Trigger update & re-init, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);

  let experiments = new Experiments.Experiments(gPolicy);
  yield experiments.updateManifest();
  let list = yield experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");
  checkExperimentSerializations(experiments._experiments.values());

  yield experiments.uninit();
  experiments = new Experiments.Experiments(gPolicy);

  yield experiments._run();
  list = yield experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");
  checkExperimentSerializations(experiments._experiments.values());

  // Re-init, clock set for experiment 1 to start.

  now = futureDate(startDates[0], 5 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.uninit();
  experiments = new Experiments.Experiments(gPolicy);
  yield experiments._run();

  list = yield experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");

  experimentListData[1].active = true;
  experimentListData[1].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  checkExperimentListsEqual(experimentListData.slice(1), list);
  checkExperimentSerializations(experiments._experiments.values());

  // Re-init, clock set for experiment 1 to stop.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.uninit();
  experiments = new Experiments.Experiments(gPolicy);
  yield experiments._run();

  list = yield experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  experimentListData[1].active = false;
  experimentListData[1].endDate = now.getTime();
  checkExperimentListsEqual(experimentListData.slice(1), list);
  checkExperimentSerializations(experiments._experiments.values());

  // Re-init, clock set for experiment 2 to start.

  now = futureDate(startDates[1], 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.uninit();
  experiments = new Experiments.Experiments(gPolicy);
  yield experiments._run();

  list = yield experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  experimentListData[0].active = true;
  experimentListData[0].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  checkExperimentListsEqual(experimentListData, list);
  checkExperimentSerializations(experiments._experiments.values());

  // Re-init, clock set for experiment 2 to stop.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  yield experiments.uninit();
  experiments = new Experiments.Experiments(gPolicy);
  yield experiments._run();

  list = yield experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  experimentListData[0].active = false;
  experimentListData[0].endDate = now.getTime();
  checkExperimentListsEqual(experimentListData, list);
  checkExperimentSerializations(experiments._experiments.values());

  // Cleanup.

  yield experiments.disableExperiment();
  yield experiments.uninit();
  yield removeCacheFile();
});

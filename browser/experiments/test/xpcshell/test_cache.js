/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
XPCOMUtils.defineLazyModuleGetter(this, "Experiments",
  "resource:///modules/experiments/Experiments.jsm");

const MANIFEST_HANDLER         = "manifests/handler";

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;

var gHttpServer          = null;
var gHttpRoot            = null;
var gDataRoot            = null;
var gPolicy              = null;
var gManifestObject      = null;
var gManifestHandlerURI  = null;

add_task(async function test_setup() {
  loadAddonManager();
  await removeCacheFile();

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
  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    oneshotTimer: (callback, timeout, thisObj, name) => {},
  });
});

function checkExperimentListsEqual(list, list2) {
  Assert.equal(list.length, list2.length, "Lists should have the same length.");

  for (let i = 0; i < list.length; ++i) {
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

function validateCache(cachedExperiments, experimentIds) {
  let cachedExperimentIds = new Set(cachedExperiments);
  Assert.equal(cachedExperimentIds.size, experimentIds.length,
               "The number of cached experiments does not match with the provided list");
  for (let id of experimentIds) {
    Assert.ok(cachedExperimentIds.has(id), "The cache must contain the experiment with id " + id);
  }
}

// Set up an experiments instance and check if it is properly restored from cache.

add_task(async function test_cache() {
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

  for (let i = 0; i < gManifestObject.experiments.length; ++i) {
    let experiment = gManifestObject.experiments[i];
    startDates.push(futureDate(baseDate, (50 + (150 * i)) * MS_IN_ONE_DAY));
    endDates.push(futureDate(startDates[i], 50 * MS_IN_ONE_DAY));
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
  await experiments.updateManifest();
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");
  checkExperimentSerializations(experiments._experiments.values());

  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);

  await experiments._run();
  list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");
  checkExperimentSerializations(experiments._experiments.values());

  // Re-init, clock set for experiment 1 to start.

  now = futureDate(startDates[0], 5 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);
  await experiments._run();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");

  experimentListData[1].active = true;
  experimentListData[1].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  checkExperimentListsEqual(experimentListData.slice(1), list);
  checkExperimentSerializations(experiments._experiments.values());

  let branch = await experiments.getExperimentBranch(EXPERIMENT1_ID);
  Assert.strictEqual(branch, null);

  await experiments.setExperimentBranch(EXPERIMENT1_ID, "testbranch");
  branch = await experiments.getExperimentBranch(EXPERIMENT1_ID);
  Assert.strictEqual(branch, "testbranch");

  // Re-init, clock set for experiment 1 to stop.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);
  await experiments._run();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  experimentListData[1].active = false;
  experimentListData[1].endDate = now.getTime();
  checkExperimentListsEqual(experimentListData.slice(1), list);
  checkExperimentSerializations(experiments._experiments.values());

  branch = await experiments.getExperimentBranch(EXPERIMENT1_ID);
  Assert.strictEqual(branch, "testbranch");

  // Re-init, clock set for experiment 2 to start.

  now = futureDate(startDates[1], 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);
  await experiments._run();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  experimentListData[0].active = true;
  experimentListData[0].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  checkExperimentListsEqual(experimentListData, list);
  checkExperimentSerializations(experiments._experiments.values());

  // Re-init, clock set for experiment 2 to stop.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);

  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);
  await experiments._run();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  experimentListData[0].active = false;
  experimentListData[0].endDate = now.getTime();
  checkExperimentListsEqual(experimentListData, list);
  checkExperimentSerializations(experiments._experiments.values());

  // Cleanup.

  await experiments._toggleExperimentsEnabled(false);
  await promiseRestartManager();
  await removeCacheFile();
});

add_task(async function test_expiration() {
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
        maxActiveSeconds: 50 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
      // The 3rd experiment will never run, so it's ok to use experiment's 2 data.
      {
        id:               EXPERIMENT3_ID,
        xpiURL:           gDataRoot + EXPERIMENT2_XPI_NAME,
        xpiHash:          EXPERIMENT2_XPI_SHA1,
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      }
    ],
  };

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

  // Setup dates for the experiments.
  let baseDate   = new Date(2014, 5, 1, 12);
  let startDates = [];
  let endDates   = [];

  for (let i = 0; i < gManifestObject.experiments.length; ++i) {
    let experiment = gManifestObject.experiments[i];
    // Spread out experiments in time so that one experiment can end and expire while
    // the next is still running.
    startDates.push(futureDate(baseDate, (50 + (200 * i)) * MS_IN_ONE_DAY));
    endDates.push(futureDate(startDates[i], 50 * MS_IN_ONE_DAY));
    experiment.startTime = dateToSeconds(startDates[i]);
    experiment.endTime   = dateToSeconds(endDates[i]);
  }

  let now = null;
  let experiments = null;

  let setDateAndRestartExperiments = async function(newDate) {
    now = newDate;
    defineNow(gPolicy, now);

    await promiseRestartManager();
    experiments = new Experiments.Experiments(gPolicy);
    await experiments._run();
  };

  // Trigger update & re-init, clock set to before any activation.
  now = baseDate;
  defineNow(gPolicy, now);

  experiments = new Experiments.Experiments(gPolicy);
  await experiments.updateManifest();
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Re-init, clock set for experiment 1 to start...
  await setDateAndRestartExperiments(startDates[0]);
  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "The first experiment should have started.");

  // ... init again, and set the clock so that the first experiment ends.
  await setDateAndRestartExperiments(endDates[0]);

  // The experiment just ended, it should still be in the cache, but marked
  // as finished.
  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  experimentListData[1].active = false;
  experimentListData[1].endDate = now.getTime();
  checkExperimentListsEqual(experimentListData.slice(1), list);
  validateCache([...experiments._experiments.keys()], [EXPERIMENT1_ID, EXPERIMENT2_ID, EXPERIMENT3_ID]);

  // Start the second experiment.
  await setDateAndRestartExperiments(startDates[1]);

  // The experiments cache should contain the finished experiment and the
  // one that's still running.
  list = await experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries.");

  experimentListData[0].active = true;
  experimentListData[0].endDate = now.getTime() + 50 * MS_IN_ONE_DAY;
  checkExperimentListsEqual(experimentListData, list);

  // Move the clock in the future, just 31 days after the start date of the second experiment,
  // so that the cache for the first experiment expires and the second experiment is still running.
  await setDateAndRestartExperiments(futureDate(startDates[1], 31 * MS_IN_ONE_DAY));
  validateCache([...experiments._experiments.keys()], [EXPERIMENT2_ID, EXPERIMENT3_ID]);

  // Make sure that the expired experiment is not reported anymore.
  let history = await experiments.getExperiments();
  Assert.equal(history.length, 1, "Experiments older than 180 days must be removed from the cache.");

  // Test that we don't write expired experiments in the cache.
  await setDateAndRestartExperiments(now);
  validateCache([...experiments._experiments.keys()], [EXPERIMENT2_ID, EXPERIMENT3_ID]);

  // The first experiment should be expired and not in the cache, it ended more than
  // 180 days ago. We should see the one still running in the cache.
  history = await experiments.getExperiments();
  Assert.equal(history.length, 1, "Expired experiments must not be saved to cache.");
  checkExperimentListsEqual(experimentListData.slice(0, 1), history);

  // Test that experiments that are cached locally but never ran are removed from cache
  // when they are removed from the manifest (this is cached data, not really history).
  gManifestObject.experiments = gManifestObject.experiments.slice(1, 1);
  await experiments.updateManifest();
  validateCache([...experiments._experiments.keys()], [EXPERIMENT2_ID]);

  // Cleanup.
  await experiments._toggleExperimentsEnabled(false);
  await promiseRestartManager();
  await removeCacheFile();
});

add_task(async function test_invalid_cache() {
  // Save uncompressed data to the cache file to trigger a loading error.
  let encoder = new TextEncoder();
  let data = encoder.encode("foo");

  let path = OS.Path.join(OS.Constants.Path.profileDir, "experiments.json");
  let options = { tmpPath: path + ".tmp" };
  await OS.File.writeAtomic(path, data, options);

  // Trigger loading from the cache. This should not throw and gracefully recover.
  let experiments = new Experiments.Experiments(gPolicy);
  let list = await experiments.getExperiments();

  Assert.deepEqual(list, [], "The experiments cache should be empty.");
});

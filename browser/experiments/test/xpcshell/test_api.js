/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/AddonManagerTesting.jsm");

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
var gTimerScheduleOffset = -1;

function uninstallExperimentAddons() {
  return (async function() {
    let addons = await getExperimentAddons();
    for (let a of addons) {
      await AddonManagerTesting.uninstallAddonByID(a.id);
    }
  })();
}

function testCleanup(experimentsInstance) {
  return (async function() {
    await promiseRestartManager();
    await uninstallExperimentAddons();
    await removeCacheFile();
  })();
}

add_task(async function test_setup() {
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
  registerCleanupFunction(() => gHttpServer.stop(() => {}));

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
  Services.prefs.setCharPref(PREF_MANIFEST_URI, gManifestHandlerURI);
  Services.prefs.setIntPref(PREF_FETCHINTERVAL, 0);

  gPolicy = new Experiments.Policy();
  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    oneshotTimer: (callback, timeout, thisObj, name) => gTimerScheduleOffset = timeout,
  });
});

add_task(async function test_contract() {
  Cc["@mozilla.org/browser/experiments-service;1"].getService();
});

// Test basic starting and stopping of experiments.

add_task(async function test_getExperiments() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

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
        id:               EXPERIMENT2_ID,
        xpiURL:           gDataRoot + EXPERIMENT2_XPI_NAME,
        xpiHash:          EXPERIMENT2_XPI_SHA1,
        startTime:        dateToSeconds(startDate2),
        endTime:          dateToSeconds(endDate2),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
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

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.
  // Use updateManifest() to provide for coverage of that path.

  let now = baseDate;
  gTimerScheduleOffset = -1;
  defineNow(gPolicy, now);

  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  Assert.equal(experiments.getActiveExperimentID(), null,
               "getActiveExperimentID should return null");

  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");
  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Precondition: No experiment add-ons are installed.");

  try {
    await experiments.getExperimentBranch();
    Assert.ok(false, "getExperimentBranch should fail with no experiment");
  } catch (e) {
    Assert.ok(true, "getExperimentBranch correctly threw");
  }

  // Trigger update, clock set for experiment 1 to start.

  now = futureDate(startDate1, 5 * MS_IN_ONE_DAY);
  gTimerScheduleOffset = -1;
  defineNow(gPolicy, now);

  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  Assert.equal(experiments.getActiveExperimentID(), EXPERIMENT1_ID,
               "getActiveExperimentID should return the active experiment1");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "An experiment add-on was installed.");

  experimentListData[1].active = true;
  experimentListData[1].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  for (let k of Object.keys(experimentListData[1])) {
    Assert.equal(experimentListData[1][k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  let b = await experiments.getExperimentBranch();
  Assert.strictEqual(b, null, "getExperimentBranch should return null by default");

  b = await experiments.getExperimentBranch(EXPERIMENT1_ID);
  Assert.strictEqual(b, null, "getExperimentsBranch should return null (with id)");

  await experiments.setExperimentBranch(EXPERIMENT1_ID, "foo");
  b = await experiments.getExperimentBranch();
  Assert.strictEqual(b, "foo", "getExperimentsBranch should return the set value");

  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  Assert.equal(gTimerScheduleOffset, 10 * MS_IN_ONE_DAY,
               "Experiment re-evaluation should have been scheduled correctly.");

  // Trigger update, clock set for experiment 1 to stop.

  now = futureDate(endDate1, 1000);
  gTimerScheduleOffset = -1;
  defineNow(gPolicy, now);

  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  Assert.equal(experiments.getActiveExperimentID(), null,
               "getActiveExperimentID should return null again");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "The experiment add-on should be uninstalled.");

  experimentListData[1].active = false;
  experimentListData[1].endDate = now.getTime();
  for (let k of Object.keys(experimentListData[1])) {
    Assert.equal(experimentListData[1][k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  Assert.equal(gTimerScheduleOffset, startDate2 - now,
               "Experiment re-evaluation should have been scheduled correctly.");

  // Trigger update, clock set for experiment 2 to start.
  // Use notify() to provide for coverage of that path.

  now = startDate2;
  gTimerScheduleOffset = -1;
  defineNow(gPolicy, now);

  await experiments.notify();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  Assert.equal(experiments.getActiveExperimentID(), EXPERIMENT2_ID,
               "getActiveExperimentID should return the active experiment2");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries now.");
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "An experiment add-on is installed.");

  experimentListData[0].active = true;
  experimentListData[0].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  for (let i = 0; i < experimentListData.length; ++i) {
    let entry = experimentListData[i];
    for (let k of Object.keys(entry)) {
      Assert.equal(entry[k], list[i][k],
                   "Entry " + i + " - Property '" + k + "' should match reference data.");
    }
  }

  Assert.equal(gTimerScheduleOffset, 10 * MS_IN_ONE_DAY,
               "Experiment re-evaluation should have been scheduled correctly.");

  // Trigger update, clock set for experiment 2 to stop.

  now = futureDate(startDate2, 10 * MS_IN_ONE_DAY + 1000);
  gTimerScheduleOffset = -1;
  defineNow(gPolicy, now);
  await experiments.notify();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  Assert.equal(experiments.getActiveExperimentID(), null,
               "getActiveExperimentID should return null again2");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries now.");
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiments add-ons are installed.");

  experimentListData[0].active = false;
  experimentListData[0].endDate = now.getTime();
  for (let i = 0; i < experimentListData.length; ++i) {
    let entry = experimentListData[i];
    for (let k of Object.keys(entry)) {
      Assert.equal(entry[k], list[i][k],
                   "Entry " + i + " - Property '" + k + "' should match reference data.");
    }
  }

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

add_task(async function test_getActiveExperimentID() {
  // Check that getActiveExperimentID returns the correct result even
  // after .uninit()

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate1 = futureDate(baseDate, 50 * MS_IN_ONE_DAY);
  let endDate1   = futureDate(baseDate, 100 * MS_IN_ONE_DAY);

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
    ],
  };

  let now = futureDate(startDate1, 5 * MS_IN_ONE_DAY);
  gTimerScheduleOffset = -1;
  defineNow(gPolicy, now);

  let experiments = new Experiments.Experiments(gPolicy);
  await experiments.updateManifest();

  Assert.equal(experiments.getActiveExperimentID(), EXPERIMENT1_ID,
               "getActiveExperimentID should return the active experiment1");

  await promiseRestartManager();
  Assert.equal(experiments.getActiveExperimentID(), EXPERIMENT1_ID,
               "getActiveExperimentID should return the active experiment1 after uninit()");

  await testCleanup(experiments);
});

// Test that we handle the experiments addon already being
// installed properly.
// We should just pave over them.

add_task(async function test_addonAlreadyInstalled() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate  = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate    = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for the experiment to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");

  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "1 add-on is installed.");

  // Install conflicting addon.

  await AddonManagerTesting.installXPIFromURL(gDataRoot + EXPERIMENT1_XPI_NAME, EXPERIMENT1_XPI_SHA1);
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "1 add-on is installed.");
  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should still have 1 entry.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

add_task(async function test_lastActiveToday() {
  let experiments = new Experiments.Experiments(gPolicy);

  replaceExperiments(experiments, FAKE_EXPERIMENTS_1);

  let e = await experiments.getExperiments();
  Assert.equal(e.length, 1, "Monkeypatch successful.");
  Assert.equal(e[0].id, "id1", "ID looks sane");
  Assert.ok(e[0].active, "Experiment is active.");

  let lastActive = await experiments.lastActiveToday();
  Assert.equal(e[0], lastActive, "Last active object is expected.");

  replaceExperiments(experiments, FAKE_EXPERIMENTS_2);
  e = await experiments.getExperiments();
  Assert.equal(e.length, 2, "Monkeypatch successful.");

  defineNow(gPolicy, e[0].endDate);

  lastActive = await experiments.lastActiveToday();
  Assert.ok(lastActive, "Have a last active experiment");
  Assert.equal(lastActive, e[0], "Last active object is expected.");

  await testCleanup(experiments);
});

// Test explicitly disabling experiments.

add_task(async function test_disableExperiment() {
  // Dates this test is based on.

  let startDate = new Date(2004, 10, 9, 12);
  let endDate   = futureDate(startDate, 100 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  // Data to compare the result of Experiments.getExperiments() against.

  let experimentInfo = {
    id: EXPERIMENT1_ID,
    name: EXPERIMENT1_NAME,
    description: "Yet another experiment that experiments experimentally.",
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set for the experiment to start.

  let now = futureDate(startDate, 5 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();

  let list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");

  experimentInfo.active = true;
  experimentInfo.endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  for (let k of Object.keys(experimentInfo)) {
    Assert.equal(experimentInfo[k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  // Test disabling the experiment.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.disableExperiment("foo");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  experimentInfo.active = false;
  experimentInfo.endDate = now.getTime();
  for (let k of Object.keys(experimentInfo)) {
    Assert.equal(experimentInfo[k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  // Test that updating the list doesn't re-enable it.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  for (let k of Object.keys(experimentInfo)) {
    Assert.equal(experimentInfo[k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  await testCleanup(experiments);
});

add_task(async function test_disableExperimentsFeature() {
  // Dates this test is based on.

  let startDate = new Date(2004, 10, 9, 12);
  let endDate   = futureDate(startDate, 100 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  // Data to compare the result of Experiments.getExperiments() against.

  let experimentInfo = {
    id: EXPERIMENT1_ID,
    name: EXPERIMENT1_NAME,
    description: "Yet another experiment that experiments experimentally.",
  };

  let experiments = new Experiments.Experiments(gPolicy);
  Assert.equal(experiments.enabled, true, "Experiments feature should be enabled.");

  // Trigger update, clock set for the experiment to start.

  let now = futureDate(startDate, 5 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();

  let list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");

  experimentInfo.active = true;
  experimentInfo.endDate = now.getTime() + 10 * MS_IN_ONE_DAY;
  for (let k of Object.keys(experimentInfo)) {
    Assert.equal(experimentInfo[k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  // Test disabling experiments.

  experiments._toggleExperimentsEnabled(false);
  await experiments.notify();
  Assert.equal(experiments.enabled, false, "Experiments feature should be disabled now.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  experimentInfo.active = false;
  experimentInfo.endDate = now.getTime();
  for (let k of Object.keys(experimentInfo)) {
    Assert.equal(experimentInfo[k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  // Test that updating the list doesn't re-enable it.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  try {
    await experiments.updateManifest();
  } catch (e) {
    // Exception expected, the feature is disabled.
  }

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry.");

  for (let k of Object.keys(experimentInfo)) {
    Assert.equal(experimentInfo[k], list[0][k],
                 "Property " + k + " should match reference data.");
  }

  await testCleanup(experiments);
});

// Test that after a failed experiment install:
// * the next applicable experiment gets installed
// * changing the experiments data later triggers re-evaluation

add_task(async function test_installFailure() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
      {
        id:               EXPERIMENT2_ID,
        xpiURL:           gDataRoot + EXPERIMENT2_XPI_NAME,
        xpiHash:          EXPERIMENT2_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  // Data to compare the result of Experiments.getExperiments() against.

  let experimentListData = [
    {
      id: EXPERIMENT1_ID,
      name: EXPERIMENT1_NAME,
      description: "Yet another experiment that experiments experimentally.",
    },
    {
      id: EXPERIMENT2_ID,
      name: "Test experiment 2",
      description: "And yet another experiment that experiments experimentally.",
    },
  ];

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for experiment 1 & 2 to start,
  // invalid hash for experiment 1.
  // Order in the manifest matters, so we should start experiment 1,
  // fail to install it & start experiment 2 instead.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].xpiHash = "sha1:0000000000000000000000000000000000000000";
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT2_ID, "Experiment 2 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 2 should be active.");

  // Trigger update, clock set for experiment 2 to stop.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  experimentListData[0].active = false;
  experimentListData[0].endDate = now;

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT2_ID, "Experiment 2 should be the sole entry.");
  Assert.equal(list[0].active, false, "Experiment should not be active.");

  // Trigger update with a fixed entry for experiment 1,
  // which should get re-evaluated & started now.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].xpiHash = EXPERIMENT1_XPI_SHA1;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  experimentListData[0].active = true;
  experimentListData[0].endDate = now.getTime() + 10 * MS_IN_ONE_DAY;

  list = await experiments.getExperiments();
  Assert.equal(list.length, 2, "Experiment list should have 2 entries now.");

  for (let i = 0; i < experimentListData.length; ++i) {
    let entry = experimentListData[i];
    for (let k of Object.keys(entry)) {
      Assert.equal(entry[k], list[i][k],
                   "Entry " + i + " - Property '" + k + "' should match reference data.");
    }
  }

  await testCleanup(experiments);
});

// Test that after an experiment was disabled by user action,
// the experiment is not activated again if manifest data changes.

add_task(async function test_userDisabledAndUpdated() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for experiment 1 to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");
  let todayActive = await experiments.lastActiveToday();
  Assert.ok(todayActive, "Last active for today reports a value.");
  Assert.equal(todayActive.id, list[0].id, "The entry is what we expect.");

  // Explicitly disable an experiment.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.disableExperiment("foo");
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, false, "Experiment should not be active anymore.");
  todayActive = await experiments.lastActiveToday();
  Assert.ok(todayActive, "Last active for today still returns a value.");
  Assert.equal(todayActive.id, list[0].id, "The ID is still the same.");

  // Trigger an update with a faked change for experiment 1.

  now = futureDate(now, 20 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  experiments._experiments.get(EXPERIMENT1_ID)._manifestData.xpiHash =
    "sha1:0000000000000000000000000000000000000000";
  await experiments.updateManifest();
  Assert.equal(observerFireCount, expectedObserverFireCount,
               "Experiments observer should not have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, false, "Experiment should still be inactive.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Test that changing the hash for an active experiments triggers an
// update for it.

add_task(async function test_updateActiveExperiment() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  let todayActive = await experiments.lastActiveToday();
  Assert.equal(todayActive, null, "No experiment active today.");

  // Trigger update, clock set for the experiment to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");
  Assert.equal(list[0].name, EXPERIMENT1_NAME, "Experiments name should match.");
  todayActive = await experiments.lastActiveToday();
  Assert.ok(todayActive, "todayActive() returns a value.");
  Assert.equal(todayActive.id, list[0].id, "It returns the active experiment.");

  // Trigger an update for the active experiment by changing it's hash (and xpi)
  // in the manifest.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].xpiHash = EXPERIMENT1A_XPI_SHA1;
  gManifestObject.experiments[0].xpiURL = gDataRoot + EXPERIMENT1A_XPI_NAME;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should still be active.");
  Assert.equal(list[0].name, EXPERIMENT1A_NAME, "Experiments name should have been updated.");
  todayActive = await experiments.lastActiveToday();
  Assert.equal(todayActive.id, list[0].id, "last active today is still sane.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Tests that setting the disable flag for an active experiment
// stops it.

add_task(async function test_disableActiveExperiment() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for the experiment to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");

  // Trigger an update with the experiment being disabled.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].disabled = true;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, false, "Experiment 1 should be disabled.");

  // Check that the experiment stays disabled.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  delete gManifestObject.experiments[0].disabled;
  await experiments.updateManifest();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, false, "Experiment 1 should still be disabled.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Test that:
// * setting the frozen flag for a not-yet-started experiment keeps
//   it from starting
// * after a removing the frozen flag, the experiment can still start

add_task(async function test_freezePendingExperiment() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for the experiment to start but frozen.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].frozen = true;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, expectedObserverFireCount,
               "Experiments observer should have not been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should have no entries yet.");

  // Trigger an update with the experiment not being frozen anymore.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  delete gManifestObject.experiments[0].frozen;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active now.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Test that setting the frozen flag for an active experiment doesn't
// stop it.

add_task(async function test_freezeActiveExperiment() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for the experiment to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");
  Assert.equal(list[0].name, EXPERIMENT1_NAME, "Experiments name should match.");

  // Trigger an update with the experiment being disabled.

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].frozen = true;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should still be active.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Test that removing an active experiment from the manifest doesn't
// stop it.

add_task(async function test_removeActiveExperiment() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate  = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate    = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);
  let startDate2 = futureDate(baseDate, 20000 * MS_IN_ONE_DAY);
  let endDate2   = futureDate(baseDate, 30000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
      {
        id:               EXPERIMENT2_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
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

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for the experiment to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");
  Assert.equal(list[0].name, EXPERIMENT1_NAME, "Experiments name should match.");

  // Trigger an update with experiment 1 missing from the manifest

  now = futureDate(now, 1 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gManifestObject.experiments[0].frozen = true;
  await experiments.updateManifest();
  Assert.equal(observerFireCount, expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should still be active.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Test that we correctly handle experiment start & install failures.

add_task(async function test_invalidUrl() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate   = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME + ".invalid",
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        0,
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set for the experiment to start.

  let now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  gTimerScheduleOffset = null;

  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  Assert.equal(gTimerScheduleOffset, null, "No new timer should have been scheduled.");

  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// Test that we handle it properly when active experiment addons are being
// uninstalled.

add_task(async function test_unexpectedUninstall() {
  const OBSERVER_TOPIC = "experiments-changed";
  let observerFireCount = 0;
  let expectedObserverFireCount = 0;
  let observer = () => ++observerFireCount;
  Services.obs.addObserver(observer, OBSERVER_TOPIC);

  // Dates the following tests are based on.

  let baseDate   = new Date(2014, 5, 1, 12);
  let startDate  = futureDate(baseDate, 100 * MS_IN_ONE_DAY);
  let endDate    = futureDate(baseDate, 10000 * MS_IN_ONE_DAY);

  // The manifest data we test with.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  // Trigger update, clock set to before any activation.

  let now = baseDate;
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");
  let list = await experiments.getExperiments();
  Assert.equal(list.length, 0, "Experiment list should be empty.");

  // Trigger update, clock set for the experiment to start.

  now = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();
  Assert.equal(observerFireCount, ++expectedObserverFireCount,
               "Experiments observer should have been called.");

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, true, "Experiment 1 should be active.");

  // Uninstall the addon through the addon manager instead of stopping it through
  // the experiments API.

  await AddonManagerTesting.uninstallAddonByID(EXPERIMENT1_ID);
  await experiments._mainTask;

  await experiments.notify();

  list = await experiments.getExperiments();
  Assert.equal(list.length, 1, "Experiment list should have 1 entry now.");
  Assert.equal(list[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.equal(list[0].active, false, "Experiment 1 should not be active anymore.");

  // Cleanup.

  Services.obs.removeObserver(observer, OBSERVER_TOPIC);
  await testCleanup(experiments);
});

// If the Addon Manager knows of an experiment that we don't, it should get
// uninstalled.
add_task(async function testUnknownExperimentsUninstalled() {
  let experiments = new Experiments.Experiments(gPolicy);

  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Precondition: No experiment add-ons are present.");

  // Simulate us not listening.
  experiments._unregisterWithAddonManager();
  await AddonManagerTesting.installXPIFromURL(gDataRoot + EXPERIMENT1_XPI_NAME, EXPERIMENT1_XPI_SHA1);
  experiments._registerWithAddonManager();

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "Experiment 1 installed via AddonManager");

  // Simulate no known experiments.
  gManifestObject = {
    "version": 1,
    experiments: [],
  };

  await experiments.updateManifest();
  let fromManifest = await experiments.getExperiments();
  Assert.equal(fromManifest.length, 0, "No experiments known in manifest.");

  // And the unknown add-on should be gone.
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Experiment 1 was uninstalled.");

  await testCleanup(experiments);
});

// If someone else installs an experiment add-on, we detect and stop that.
add_task(async function testForeignExperimentInstall() {
  let experiments = new Experiments.Experiments(gPolicy);

  gManifestObject = {
    "version": 1,
    experiments: [],
  };

  await experiments.init();

  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Precondition: No experiment add-ons present.");

  let failed = false;
  try {
    await AddonManagerTesting.installXPIFromURL(gDataRoot + EXPERIMENT1_XPI_NAME, EXPERIMENT1_XPI_SHA1);
  } catch (ex) {
    failed = true;
  }
  Assert.ok(failed, "Add-on install should not have completed successfully");
  addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Add-on install should have been cancelled.");

  await testCleanup(experiments);
});

// Experiment add-ons will be disabled after Addon Manager restarts. Ensure
// we enable them automatically.
add_task(async function testEnabledAfterRestart() {
  let experiments = new Experiments.Experiments(gPolicy);

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id: EXPERIMENT1_ID,
        xpiURL: gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash: EXPERIMENT1_XPI_SHA1,
        startTime: gPolicy.now().getTime() / 1000 - 60,
        endTime: gPolicy.now().getTime() / 1000 + 60,
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName: ["XPCShell"],
        channel: ["nightly"],
      },
    ],
  };

  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Precondition: No experiment add-ons installed.");

  await experiments.updateManifest();
  let fromManifest = await experiments.getExperiments();
  Assert.equal(fromManifest.length, 1, "A single experiment is known.");

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "A single experiment add-on is installed.");
  Assert.ok(addons[0].isActive, "That experiment is active.");

  dump("Restarting Addon Manager\n");
  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "The experiment is still there after restart.");
  Assert.ok(addons[0].userDisabled, "But it is disabled.");
  Assert.equal(addons[0].isActive, false, "And not active.");

  await experiments.updateManifest();
  Assert.ok(addons[0].isActive, "It activates when the manifest is evaluated.");

  await testCleanup(experiments);
});

// If experiment add-ons were ever started, maxStartTime shouldn't be evaluated
// anymore. Ensure that if maxStartTime is passed but experiment has started
// already, maxStartTime does not cause deactivation.

add_task(async function testMaxStartTimeEvaluation() {

  // Dates the following tests are based on.

  let startDate    = new Date(2014, 5, 1, 12);
  let now          = futureDate(startDate, 10 * MS_IN_ONE_DAY);
  let maxStartDate = futureDate(startDate, 100 * MS_IN_ONE_DAY);
  let endDate      = futureDate(startDate, 1000 * MS_IN_ONE_DAY);

  defineNow(gPolicy, now);

  // The manifest data we test with.
  // We set a value for maxStartTime.

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id:               EXPERIMENT1_ID,
        xpiURL:           gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash:          EXPERIMENT1_XPI_SHA1,
        startTime:        dateToSeconds(startDate),
        endTime:          dateToSeconds(endDate),
        maxActiveSeconds: 1000 * SEC_IN_ONE_DAY,
        maxStartTime:     dateToSeconds(maxStartDate),
        appName:          ["XPCShell"],
        channel:          ["nightly"],
      },
    ],
  };

  let experiments = new Experiments.Experiments(gPolicy);

  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Precondition: No experiment add-ons installed.");

  await experiments.updateManifest();
  let fromManifest = await experiments.getExperiments();
  Assert.equal(fromManifest.length, 1, "A single experiment is known.");

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "A single experiment add-on is installed.");
  Assert.ok(addons[0].isActive, "That experiment is active.");

  dump("Setting current time to maxStartTime + 100 days and reloading manifest\n");
  now = futureDate(maxStartDate, 100 * MS_IN_ONE_DAY);
  defineNow(gPolicy, now);
  await experiments.updateManifest();

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "The experiment is still there.");
  Assert.ok(addons[0].isActive, "It is still active.");

  await testCleanup(experiments);
});

// Test coverage for an add-on uninstall disabling the experiment and that it stays
// disabled over restarts.
add_task(async function test_foreignUninstallAndRestart() {
  let experiments = new Experiments.Experiments(gPolicy);

  gManifestObject = {
    "version": 1,
    experiments: [
      {
        id: EXPERIMENT1_ID,
        xpiURL: gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash: EXPERIMENT1_XPI_SHA1,
        startTime: gPolicy.now().getTime() / 1000 - 60,
        endTime: gPolicy.now().getTime() / 1000 + 60,
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName: ["XPCShell"],
        channel: ["nightly"],
      },
    ],
  };

  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Precondition: No experiment add-ons installed.");

  await experiments.updateManifest();
  let experimentList = await experiments.getExperiments();
  Assert.equal(experimentList.length, 1, "A single experiment is known.");

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 1, "A single experiment add-on is installed.");
  Assert.ok(addons[0].isActive, "That experiment is active.");

  await AddonManagerTesting.uninstallAddonByID(EXPERIMENT1_ID);
  await experiments._mainTask;

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "Experiment add-on should have been removed.");

  experimentList = await experiments.getExperiments();
  Assert.equal(experimentList.length, 1, "A single experiment is known.");
  Assert.equal(experimentList[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.ok(!experimentList[0].active, "Experiment 1 should not be active anymore.");

  // Fake restart behaviour.
  await promiseRestartManager();
  experiments = new Experiments.Experiments(gPolicy);
  await experiments.updateManifest();

  addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiment add-ons installed.");

  experimentList = await experiments.getExperiments();
  Assert.equal(experimentList.length, 1, "A single experiment is known.");
  Assert.equal(experimentList[0].id, EXPERIMENT1_ID, "Experiment 1 should be the sole entry.");
  Assert.ok(!experimentList[0].active, "Experiment 1 should not be active.");

  await testCleanup(experiments);
});

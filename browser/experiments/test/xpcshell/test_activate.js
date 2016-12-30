/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource:///modules/experiments/Experiments.jsm");

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;

var gHttpServer = null;
var gHttpRoot   = null;
var gPolicy     = null;

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
  gPolicy = new Experiments.Policy();

  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let port = gHttpServer.identity.primaryPort;
  gHttpRoot = "http://localhost:" + port + "/";
  gHttpServer.registerDirectory("/", do_get_cwd());
  do_register_cleanup(() => gHttpServer.stop(() => {}));

  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
  });

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
});

function isApplicable(experiment) {
  let deferred = Promise.defer();
  experiment.isApplicable().then(
    result => deferred.resolve({ applicable: true,  reason: null }),
    reason => deferred.resolve({ applicable: false, reason })
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

  // We need to associate it with the singleton so the onInstallStarted
  // Addon Manager listener will know about it.
  Experiments.instance()._experiments = new Map();
  Experiments.instance()._experiments.set(experiment.id, experiment);

  let result;

  defineNow(gPolicy, baseDate);
  result = yield isApplicable(experiment);
  Assert.equal(result.applicable, false, "Experiment should not be applicable.");
  Assert.equal(experiment.enabled, false, "Experiment should not be enabled.");

  let addons = yield getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiment add-ons are installed.");

  defineNow(gPolicy, futureDate(startDate, 5 * MS_IN_ONE_DAY));
  result = yield isApplicable(experiment);
  Assert.equal(result.applicable, true, "Experiment should now be applicable.");
  Assert.equal(experiment.enabled, false, "Experiment should not be enabled.");

  let changes = yield experiment.start();
  Assert.equal(changes, experiment.ADDON_CHANGE_INSTALL, "Add-on was installed.");
  addons = yield getExperimentAddons();
  Assert.equal(experiment.enabled, true, "Experiment should now be enabled.");
  Assert.equal(addons.length, 1, "1 experiment add-on is installed.");
  Assert.equal(addons[0].id, experiment._addonId, "The add-on is the one we expect.");
  Assert.equal(addons[0].userDisabled, false, "The add-on is not userDisabled.");
  Assert.ok(addons[0].isActive, "The add-on is active.");

  changes = yield experiment.stop();
  Assert.equal(changes, experiment.ADDON_CHANGE_UNINSTALL, "Add-on was uninstalled.");
  addons = yield getExperimentAddons();
  Assert.equal(experiment.enabled, false, "Experiment should not be enabled.");
  Assert.equal(addons.length, 0, "Experiment should be uninstalled from the Addon Manager.");

  changes = yield experiment.start();
  Assert.equal(changes, experiment.ADDON_CHANGE_INSTALL, "Add-on was installed.");
  addons = yield getExperimentAddons();
  Assert.equal(experiment.enabled, true, "Experiment should now be enabled.");
  Assert.equal(addons.length, 1, "1 experiment add-on is installed.");
  Assert.equal(addons[0].id, experiment._addonId, "The add-on is the one we expect.");
  Assert.equal(addons[0].userDisabled, false, "The add-on is not userDisabled.");
  Assert.ok(addons[0].isActive, "The add-on is active.");

  result = yield experiment.shouldStop();
  Assert.equal(result.shouldStop, false, "shouldStop should be false.");
  Assert.equal(experiment.enabled, true, "Experiment should be enabled.");
  addons = yield getExperimentAddons();
  Assert.equal(addons.length, 1, "Experiment still in add-ons manager.");
  Assert.ok(addons[0].isActive, "The add-on is still active.");

  defineNow(gPolicy, futureDate(endDate, MS_IN_ONE_DAY));
  result = yield experiment.shouldStop();
  Assert.equal(result.shouldStop, true, "shouldStop should now be true.");
  changes = yield experiment.stop();
  Assert.equal(changes, experiment.ADDON_CHANGE_UNINSTALL, "Add-on should be uninstalled.");
  Assert.equal(experiment.enabled, false, "Experiment should be disabled.");
  addons = yield getExperimentAddons();
  Assert.equal(addons.length, 0, "Experiment add-on is uninstalled.");

  // Ensure hash validation works.
  // We set an incorrect hash and expect the install to fail.
  experiment._manifestData.xpiHash = "sha1:41014dcc66b4dcedcd973491a1530a32f0517d8a";
  let errored = false;
  try {
    yield experiment.start();
  } catch (ex) {
    errored = true;
  }
  Assert.ok(experiment._failedStart, "Experiment failed to start.");
  Assert.ok(errored, "start() threw an exception.");

  // Make sure "ignore hashes" mode works.
  gPolicy.ignoreHashes = true;
  changes = yield experiment.start();
  Assert.equal(changes, experiment.ADDON_CHANGE_INSTALL);
  yield experiment.stop();
  gPolicy.ignoreHashes = false;
});

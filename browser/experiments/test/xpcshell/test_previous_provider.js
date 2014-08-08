/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource:///modules/experiments/Experiments.jsm");
Cu.import("resource://testing-common/httpd.js");

let gDataRoot;
let gHttpServer;
let gManifestObject;

function run_test() {
  run_next_test();
}

add_task(function test_setup() {
  loadAddonManager();
  do_get_profile();

  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let httpRoot = "http://localhost:" + gHttpServer.identity.primaryPort + "/";
  gDataRoot = httpRoot + "data/";
  gHttpServer.registerDirectory("/data/", do_get_cwd());
  gHttpServer.registerPathHandler("/manifests/handler", (req, res) => {
    res.setStatusLine(null, 200, "OK");
    res.write(JSON.stringify(gManifestObject));
    res.processAsync();
    res.finish();
  });
  do_register_cleanup(() => gHttpServer.stop(() => {}));

  Services.prefs.setBoolPref("experiments.enabled", true);
  Services.prefs.setCharPref("experiments.manifest.uri",
                             httpRoot + "manifests/handler");
  Services.prefs.setBoolPref("experiments.logging.dump", true);
  Services.prefs.setCharPref("experiments.logging.level", "Trace");
});

add_task(function* test_provider_basic() {
  let e = Experiments.instance();

  let provider = new Experiments.PreviousExperimentProvider(e);
  e._setPreviousExperimentsProvider(provider);

  let deferred = Promise.defer();
  provider.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  let addons = yield deferred.promise;
  Assert.ok(Array.isArray(addons), "getAddonsByTypes returns an Array.");
  Assert.equal(addons.length, 0, "No previous add-ons returned.");

  gManifestObject = {
    version: 1,
    experiments: [
      {
        id: EXPERIMENT1_ID,
        xpiURL: gDataRoot + EXPERIMENT1_XPI_NAME,
        xpiHash: EXPERIMENT1_XPI_SHA1,
        startTime: Date.now() / 1000 - 60,
        endTime: Date.now() / 1000 + 60,
        maxActiveSeconds: 60,
        appName: ["XPCShell"],
        channel: [e._policy.updatechannel()],
      },
    ],
  };

  yield e.updateManifest();

  deferred = Promise.defer();
  provider.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  addons = yield deferred.promise;
  Assert.equal(addons.length, 0, "Still no previous experiment.");

  let experiments = yield e.getExperiments();
  Assert.equal(experiments.length, 1, "1 experiment present.");
  Assert.ok(experiments[0].active, "It is active.");

  // Deactivate it.
  defineNow(e._policy, new Date(gManifestObject.experiments[0].endTime * 1000 + 1000));
  yield e.updateManifest();

  experiments = yield e.getExperiments();
  Assert.equal(experiments.length, 1, "1 experiment present.");
  Assert.equal(experiments[0].active, false, "It isn't active.");

  deferred = Promise.defer();
  provider.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  addons = yield deferred.promise;
  Assert.equal(addons.length, 1, "1 previous add-on known.");
  Assert.equal(addons[0].id, EXPERIMENT1_ID, "ID matches expected.");

  deferred = Promise.defer();
  provider.getAddonByID(EXPERIMENT1_ID, (addon) => {
    deferred.resolve(addon);
  });
  let addon = yield deferred.promise;
  Assert.ok(addon, "We got an add-on from its ID.");
  Assert.equal(addon.id, EXPERIMENT1_ID, "ID matches expected.");
  Assert.ok(addon.appDisabled, "Add-on is a previous experiment.");
  Assert.ok(addon.userDisabled, "Add-on is disabled.");
  Assert.equal(addon.type, "experiment", "Add-on is an experiment.");
  Assert.equal(addon.isActive, false, "Add-on is not active.");
  Assert.equal(addon.permissions, 0, "Add-on has no permissions.");

  deferred = Promise.defer();
  AddonManager.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  addons = yield deferred.promise;
  Assert.equal(addons.length, 1, "Got 1 experiment from add-on manager.");
  Assert.equal(addons[0].id, EXPERIMENT1_ID, "ID matches expected.");
  Assert.ok(addons[0].appDisabled, "It is a previous experiment add-on.");
});

add_task(function* test_active_and_previous() {
  // Building on the previous test, activate experiment 2.
  let e = Experiments.instance();
  let provider = new Experiments.PreviousExperimentProvider(e);
  e._setPreviousExperimentsProvider(provider);

  gManifestObject = {
    version: 1,
    experiments: [
      {
        id: EXPERIMENT2_ID,
        xpiURL: gDataRoot + EXPERIMENT2_XPI_NAME,
        xpiHash: EXPERIMENT2_XPI_SHA1,
        startTime: Date.now() / 1000 - 60,
        endTime: Date.now() / 1000 + 60,
        maxActiveSeconds: 60,
        appName: ["XPCShell"],
        channel: [e._policy.updatechannel()],
      },
    ],
  };

  defineNow(e._policy, new Date());
  yield e.updateManifest();

  let experiments = yield e.getExperiments();
  Assert.equal(experiments.length, 2, "2 experiments known.");

  let deferred = Promise.defer();
  provider.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  let addons = yield deferred.promise;
  Assert.equal(addons.length, 1, "1 previous experiment.");

  deferred = Promise.defer();
  AddonManager.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  addons = yield deferred.promise;
  Assert.equal(addons.length, 2, "2 experiment add-ons known.");

  for (let addon of addons) {
    if (addon.id == EXPERIMENT1_ID) {
      Assert.equal(addon.isActive, false, "Add-on is not active.");
      Assert.ok(addon.appDisabled, "Should be a previous experiment.");
    }
    else if (addon.id == EXPERIMENT2_ID) {
      Assert.ok(addon.isActive, "Add-on is active.");
      Assert.ok(!addon.appDisabled, "Should not be a previous experiment.");
    }
    else {
      throw new Error("Unexpected add-on ID: " + addon.id);
    }
  }
});

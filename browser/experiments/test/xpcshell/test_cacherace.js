/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Timer.jsm");

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
  registerCleanupFunction(() => gHttpServer.stop(() => {}));

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
  Services.prefs.setCharPref(PREF_MANIFEST_URI, gManifestHandlerURI);
  Services.prefs.setIntPref(PREF_FETCHINTERVAL, 0);

  let ExperimentsScope = Cu.import("resource:///modules/experiments/Experiments.jsm", {});
  let Experiments = ExperimentsScope.Experiments;

  gPolicy = new Experiments.Policy();
  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
    delayCacheWrite: (promise) => {
      return new Promise((resolve, reject) => {
        promise.then(
          (result) => { setTimeout(() => resolve(result), 500); },
          (err) => { reject(err); }
        );
      });
    },
  });

  let now = new Date(2014, 5, 1, 12);
  defineNow(gPolicy, now);

  let experimentName = "experiment-racybranch.xpi";
  let experimentPath = getExperimentPath(experimentName);
  let experimentHash = "sha1:" + sha1File(experimentPath);

  gManifestObject = {
    version: 1,
    experiments: [
      {
        id: "test-experiment-racybranch@tests.mozilla.org",
        xpiURL: gDataRoot + "experiment-racybranch.xpi",
        xpiHash: experimentHash,
        maxActiveSeconds: 10 * SEC_IN_ONE_DAY,
        appName: ["XPCShell"],
        channel: ["nightly"],
        startTime: dateToSeconds(futureDate(now, -MS_IN_ONE_DAY)),
        endTime: dateToSeconds(futureDate(now, MS_IN_ONE_DAY)),
      },
    ],
  };

  info("gManifestObject: " + JSON.stringify(gManifestObject));

  // In order for the addon manager to work properly, we hack
  // Experiments.instance which is used by the XPIProvider
  let experiments = new Experiments.Experiments(gPolicy);
  Assert.strictEqual(ExperimentsScope.gExperiments, null);
  ExperimentsScope.gExperiments = experiments;

  await experiments.updateManifest();
  let active = experiments._getActiveExperiment();
  Assert.ok(active);
  Assert.equal(active.branch, "racy-set");
  Assert.ok(!experiments._dirty);
});

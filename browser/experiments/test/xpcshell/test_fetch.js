/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource:///modules/experiments/Experiments.jsm");

var gHttpServer = null;
var gHttpRoot   = null;
var gPolicy     = new Experiments.Policy();

function run_test() {
  loadAddonManager();

  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let port = gHttpServer.identity.primaryPort;
  gHttpRoot = "http://localhost:" + port + "/";
  gHttpServer.registerDirectory("/", do_get_cwd());
  do_register_cleanup(() => gHttpServer.stop(() => {}));

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);

  patchPolicy(gPolicy, {
    updatechannel: () => "nightly",
  });

  run_next_test();
}

add_task(async function test_fetchAndCache() {
  Services.prefs.setCharPref(PREF_MANIFEST_URI, gHttpRoot + "experiments_1.manifest");
  let ex = new Experiments.Experiments(gPolicy);

  Assert.equal(ex._experiments, null, "There should be no cached experiments yet.");
  await ex.updateManifest();
  Assert.notEqual(ex._experiments.size, 0, "There should be cached experiments now.");

  await promiseRestartManager();
});

add_task(async function test_checkCache() {
  let ex = new Experiments.Experiments(gPolicy);
  await ex.notify();
  Assert.notEqual(ex._experiments.size, 0, "There should be cached experiments now.");

  await promiseRestartManager();
});

add_task(async function test_fetchInvalid() {
  await removeCacheFile();

  Services.prefs.setCharPref(PREF_MANIFEST_URI, gHttpRoot + "experiments_1.manifest");
  let ex = new Experiments.Experiments(gPolicy);
  await ex.updateManifest();
  Assert.notEqual(ex._experiments.size, 0, "There should be experiments");

  Services.prefs.setCharPref(PREF_MANIFEST_URI, gHttpRoot + "invalid.manifest");
  await ex.updateManifest();
  Assert.notEqual(ex._experiments.size, 0, "There should still be experiments: fetch failure shouldn't remove them.");

  await promiseRestartManager();
});

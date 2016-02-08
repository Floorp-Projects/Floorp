/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource:///modules/experiments/Experiments.jsm");

const MANIFEST_HANDLER         = "manifests/handler";

function run_test() {
  run_next_test();
}

add_task(function* test_setup() {
  loadAddonManager();
  do_get_profile();

  let httpServer = new HttpServer();
  httpServer.start(-1);
  let port = httpServer.identity.primaryPort;
  let httpRoot = "http://localhost:" + port + "/";
  let handlerURI = httpRoot + MANIFEST_HANDLER;
  httpServer.registerPathHandler("/" + MANIFEST_HANDLER,
    (request, response) => {
      response.processAsync();
      response.setStatus(null, 200, "OK");
      response.write("["); // never finish!
    });

  do_register_cleanup(() => httpServer.stop(() => {}));
  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
  Services.prefs.setCharPref(PREF_MANIFEST_URI, handlerURI);
  Services.prefs.setIntPref(PREF_FETCHINTERVAL, 0);

  let experiments = Experiments.instance();
  experiments.updateManifest().then(
    () => {
      Assert.ok(true, "updateManifest finished successfully");
    },
    (e) => {
      do_throw("updateManifest should not have failed: got error " + e);
    });
  yield experiments.uninit();
});

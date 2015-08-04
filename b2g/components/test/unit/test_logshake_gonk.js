/**
 * Test the log capturing capabilities of LogShake.jsm, checking
 * for Gonk-specific parts
 */

/* jshint moz: true, esnext: true */
/* global Cu, LogCapture, LogShake, ok, add_test, run_next_test, dump,
   setup_logshake_mocks, OS, sdcard */
/* exported run_test */

/* disable use strict warning */
/* jshint -W097 */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");

function run_test() {
  Cu.import("resource://gre/modules/LogShake.jsm");
  run_next_test();
}

add_test(setup_logshake_mocks);

add_test(function test_logShake_captureLogs_writes() {
  // Enable LogShake
  LogShake.init();

  let expectedFiles = [];

  LogShake.captureLogs().then(logResults => {
    LogShake.uninit();

    ok(logResults.logFilenames.length > 0, "Should have filenames");
    ok(logResults.logPaths.length > 0, "Should have paths");
    ok(!logResults.compressed, "Should not be compressed");

    logResults.logPaths.forEach(f => {
      let p = OS.Path.join(sdcard, f);
      ok(p, "Should have a valid result path: " + p);

      let t = OS.File.exists(p).then(rv => {
        ok(rv, "File exists: " + p);
      });

      expectedFiles.push(t);
    });

    Promise.all(expectedFiles).then(() => {
      ok(true, "Completed all files checks");
      run_next_test();
    });
  },
  error => {
    LogShake.uninit();

    ok(false, "Should not have received error: " + error);

    run_next_test();
  });
});

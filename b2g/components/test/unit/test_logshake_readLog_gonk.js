/**
 * Test the log capturing capabilities of LogShake.jsm under conditions that
 * could cause races
 */

/* jshint moz: true, esnext: true */
/* global Cu, LogCapture, LogShake, ok, add_test, run_next_test, dump,
   XPCOMUtils, do_get_profile, OS, volumeService, Promise, equal,
   setup_logshake_mocks */
/* exported run_test */

/* disable use strict warning */
/* jshint -W097 */

"use strict";

function run_test() {
  Cu.import("resource://gre/modules/LogShake.jsm");
  run_next_test();
}

add_test(setup_logshake_mocks);

add_test(function test_logShake_captureLogs_waits_to_read() {
  // Enable LogShake
  LogShake.init();

  // Save no logs synchronously (except properties)
  LogShake.LOGS_WITH_PARSERS = {};

  LogShake.captureLogs().then(logResults => {
    LogShake.uninit();

    ok(logResults.logFilenames.length > 0, "Should have filenames");
    ok(logResults.logPaths.length > 0, "Should have paths");
    ok(!logResults.compressed, "Should not be compressed");

    // This assumes that the about:memory reading will only fail under abnormal
    // circumstances. It does not check for screenshot.png because
    // systemAppFrame is unavailable during xpcshell tests.
    let hasAboutMemory = false;

    logResults.logFilenames.forEach(filename => {
      // Because the about:memory log's filename has the PID in it we can not
      // use simple equality but instead search for the "about_memory" part of
      // the filename which will look like logshake-about_memory-{PID}.json.gz
      if (filename.indexOf("about_memory") < 0) {
        return;
      }
      hasAboutMemory = true;
    });

    ok(hasAboutMemory,
       "LogShake's asynchronous read of about:memory should have succeeded.");

    run_next_test();
  },
  error => {
    LogShake.uninit();

    ok(false, "Should not have received error: " + error);

    run_next_test();
  });
});

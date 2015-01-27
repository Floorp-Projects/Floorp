/**
 * Test the log capturing capabilities of LogShake.jsm, checking
 * for Gonk-specific parts
 */

/* jshint moz: true */
/* global Components, LogCapture, LogShake, ok, add_test, run_next_test, dump */
/* exported run_test */

/* disable use strict warning */
/* jshint -W097 */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "volumeService",
                                   "@mozilla.org/telephony/volume-service;1",
                                   "nsIVolumeService");

let sdcard;

function run_test() {
  Cu.import("resource://gre/modules/LogShake.jsm");
  Cu.import("resource://gre/modules/Promise.jsm");
  Cu.import("resource://gre/modules/osfile.jsm");
  do_get_profile();
  debug("Starting");
  run_next_test();
}

function debug(msg) {
  var timestamp = Date.now();
  dump("LogShake: " + timestamp + ": " + msg + "\n");
}

add_test(function setup_fs() {
  OS.File.makeDir("/data/local/tmp/sdcard/", {from: "/data"}).then(function() {
    run_next_test();
  });
});

add_test(function setup_sdcard() {
  let volName = "sdcard";
  let mountPoint = "/data/local/tmp/sdcard";
  volumeService.createFakeVolume(volName, mountPoint);

  let vol = volumeService.getVolumeByName(volName);
  ok(vol, "volume shouldn't be null");
  equal(volName, vol.name, "name");

  volumeService.SetFakeVolumeState(volName, Ci.nsIVolume.STATE_MOUNTED);
  equal(Ci.nsIVolume.STATE_MOUNTED, vol.state, "state");

  run_next_test();
});

add_test(function test_ensure_sdcard() {
  sdcard = volumeService.getVolumeByName("sdcard").mountPoint;
  ok(sdcard, "Should have a valid sdcard mountpoint");
  run_next_test();
});

add_test(function test_logShake_captureLogs_returns() {
  // Enable LogShake
  LogShake.init();

  LogShake.captureLogs().then(logResults => {
    LogShake.uninit();

    ok(logResults.logFilenames.length > 0, "Should have filenames");
    ok(logResults.logPrefix.length > 0, "Should have prefix");

    run_next_test();
  },
  error => {
    LogShake.uninit();

    ok(false, "Should not have received error: " + error);

    run_next_test();
  });
});

add_test(function test_logShake_captureLogs_writes() {
  // Enable LogShake
  LogShake.init();

  let expectedFiles = [];

  LogShake.captureLogs().then(logResults => {
    LogShake.uninit();

    logResults.logFilenames.forEach(f => {
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

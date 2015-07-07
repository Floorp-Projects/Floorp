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
  Cu.import("resource://gre/modules/FileUtils.jsm");
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
  equal(Ci.nsIVolume.STATE_MOUNTED, vol.state, "state");

  run_next_test();
});

add_test(function test_ensure_sdcard() {
  sdcard = volumeService.getVolumeByName("sdcard").mountPoint;
  ok(sdcard, "Should have a valid sdcard mountpoint");
  run_next_test();
});

add_test(function test_logShake_captureLogs_writes_zip() {
  // Enable LogShake
  LogShake.init();

  let expectedFiles = [];

  LogShake.enableQAMode();

  LogShake.captureLogs().then(logResults => {
    LogShake.uninit();

    ok(logResults.logPaths.length === 1, "Should have zip path");
    ok(logResults.logFilenames.length >= 1, "Should have log filenames");
    ok(logResults.compressed, "Log files should be compressed");

    let zipPath = OS.Path.join(sdcard, logResults.logPaths[0]);
    ok(zipPath, "Should have a valid archive path: " + zipPath);

    let zipFile = new FileUtils.File(zipPath);
    ok(zipFile, "Should have a valid archive file: " + zipFile);

    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);
    zipReader.open(zipFile);

    let logFilenamesSeen = {};

    let zipEntries = zipReader.findEntries(null); // Find all entries
    while (zipEntries.hasMore()) {
      let entryName = zipEntries.getNext();
      let entry = zipReader.getEntry(entryName);
      logFilenamesSeen[entryName] = true;
      ok(!entry.isDirectory, "Archive entry " + entryName + " should be a file");
    }
    zipReader.close();

    // TODO: Verify archive contents
    logResults.logFilenames.forEach(filename => {
      ok(logFilenamesSeen[filename], "File " + filename + " should be present in archive");
    });
    run_next_test();
  },
  error => {
    LogShake.uninit();

    ok(false, "Should not have received error: " + error);

    run_next_test();
  });
});


/**
 * Test the log capturing capabilities of LogShake.jsm, checking
 * for Gonk-specific parts
 */

/* jshint moz: true, esnext: true */
/* global Cc, Ci, Cu, LogCapture, LogShake, ok, add_test, run_next_test, dump,
   setup_logshake_mocks, OS, sdcard, FileUtils */
/* exported run_test */

/* disable use strict warning */
/* jshint -W097 */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

function run_test() {
  Cu.import("resource://gre/modules/LogShake.jsm");
  run_next_test();
}

add_test(setup_logshake_mocks);

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


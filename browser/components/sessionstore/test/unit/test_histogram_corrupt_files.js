/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The primary purpose of this test is to ensure that
 * the sessionstore component records information about
 * corrupted backup files into a histogram.
 */

"use strict";
Cu.import("resource://gre/modules/osfile.jsm", this);

const Telemetry = Services.telemetry;
const Path = OS.Path;
const HistogramId = "FX_SESSION_RESTORE_ALL_FILES_CORRUPT";

// Prepare the session file.
let profd = do_get_profile();
Cu.import("resource:///modules/sessionstore/SessionFile.jsm", this);

/**
 * A utility function for resetting the histogram and the contents
 * of the backup directory.
 */
function reset_session(backups = {}) {

  // Reset the histogram.
  Telemetry.getHistogramById(HistogramId).clear();

  // Reset the contents of the backups directory
  OS.File.makeDir(SessionFile.Paths.backups);
  for (let key of SessionFile.Paths.loadOrder) {
    if (backups.hasOwnProperty(key)) {
      OS.File.copy(backups[key], SessionFile.Paths[key]);
    } else {
      OS.File.remove(SessionFile.Paths[key]);
    }
  }
}

/**
 * In order to use FX_SESSION_RESTORE_ALL_FILES_CORRUPT histogram
 * it has to be registered in "toolkit/components/telemetry/Histograms.json".
 * This test ensures that the histogram is registered and empty.
 */
add_task(function* test_ensure_histogram_exists_and_empty() {
  let s = Telemetry.getHistogramById(HistogramId).snapshot();
  Assert.equal(s.sum, 0, "Initially, the sum of probes is 0");
});

/**
 * Makes sure that the histogram is negatively updated when no
 * backup files are present.
 */
add_task(function* test_no_files_exist() {
  // No session files are available to SessionFile.
  reset_session();

  yield SessionFile.read();
  // Checking if the histogram is updated negatively
  let h = Telemetry.getHistogramById(HistogramId);
  let s = h.snapshot();
  Assert.equal(s.counts[0], 1, "One probe for the 'false' bucket.");
  Assert.equal(s.counts[1], 0, "No probes in the 'true' bucket.");
});

/**
 * Makes sure that the histogram is negatively updated when at least one
 * backup file is not corrupted.
 */
add_task(function* test_one_file_valid() {
  // Corrupting some backup files.
  let invalidSession = "data/sessionstore_invalid.js";
  let validSession = "data/sessionstore_valid.js";
  reset_session({
    clean : invalidSession,
    cleanBackup: validSession,
    recovery: invalidSession,
    recoveryBackup: invalidSession
  });

  yield SessionFile.read();
  // Checking if the histogram is updated negatively.
  let h = Telemetry.getHistogramById(HistogramId);
  let s = h.snapshot();
  Assert.equal(s.counts[0], 1, "One probe for the 'false' bucket.");
  Assert.equal(s.counts[1], 0, "No probes in the 'true' bucket.");
});

/**
 * Makes sure that the histogram is positively updated when all
 * backup files are corrupted.
 */
add_task(function* test_all_files_corrupt() {
  // Corrupting all backup files.
  let invalidSession = "data/sessionstore_invalid.js";
  reset_session({
    clean : invalidSession,
    cleanBackup: invalidSession,
    recovery: invalidSession,
    recoveryBackup: invalidSession
  });

  yield SessionFile.read();
  // Checking if the histogram is positively updated.
  let h = Telemetry.getHistogramById(HistogramId);
  let s = h.snapshot();
  Assert.equal(s.counts[1], 1, "One probe for the 'true' bucket.");
  Assert.equal(s.counts[0], 0, "No probes in the 'false' bucket.");
});

function run_test() {
  run_next_test();
}

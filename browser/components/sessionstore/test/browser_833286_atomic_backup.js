/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests are for a sessionstore.js atomic backup.

let tmp = {};
Cu.import("resource://gre/modules/osfile.jsm", tmp);
Cu.import("resource://gre/modules/Task.jsm", tmp);
Cu.import("resource:///modules/sessionstore/_SessionFile.jsm", tmp);

const {OS, Task, _SessionFile} = tmp;

const PREF_SS_INTERVAL = "browser.sessionstore.interval";
// Full paths for sessionstore.js and sessionstore.bak.
const path = OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.js");
const backupPath = OS.Path.join(OS.Constants.Path.profileDir,
  "sessionstore.bak");

// A text decoder.
let gDecoder = new TextDecoder();
// Global variables that contain sessionstore.js and sessionstore.bak data for
// comparison between tests.
let gSSData;
let gSSBakData;

// waitForSaveStateComplete waits for a state write completion.
function waitForSaveStateComplete(aSaveStateCallback) {
  let topic = "sessionstore-state-write-complete";

  function observer() {
    Services.prefs.clearUserPref(PREF_SS_INTERVAL);
    Services.obs.removeObserver(observer, topic);
    executeSoon(function taskCallback() {
      Task.spawn(aSaveStateCallback);
    });
  }

  Services.obs.addObserver(observer, topic, false);
}

// Register next test callback and trigger state saving change.
function nextTest(testFunc) {
  waitForSaveStateComplete(testFunc);
  Services.prefs.setIntPref(PREF_SS_INTERVAL, 0);
}

registerCleanupFunction(function() {
  // Cleaning up after the test: removing the sessionstore.bak file.
  Task.spawn(function cleanupTask() {
    yield OS.File.remove(backupPath);
  });
});

function test() {
  waitForExplicitFinish();
  nextTest(testInitialWriteNoBackup);
}

function testInitialWriteNoBackup() {
  // Ensure that sessionstore.js is created, but not sessionstore.bak.
  let ssExists = yield OS.File.exists(path);
  let ssBackupExists = yield OS.File.exists(backupPath);
  ok(ssExists, "sessionstore.js should be created.");
  ok(!ssBackupExists, "sessionstore.bak should not have been created, yet.");

  nextTest(testWriteNoBackup);
}

function testWriteNoBackup() {
  // Ensure sessionstore.bak is not created.
  let ssExists = yield OS.File.exists(path);
  let ssBackupExists = yield OS.File.exists(backupPath);
  ok(ssExists, "sessionstore.js should exist.");
  ok(!ssBackupExists, "sessionstore.bak should not have been created, yet");

  // Save sessionstore.js data to compare to the sessionstore.bak data in the
  // next test.
  let array = yield OS.File.read(path);
  gSSData = gDecoder.decode(array);

  // Manually trigger _SessionFile.createBackupCopy since the backup once
  // promise is already resolved and backup would not be triggered again.
  yield _SessionFile.createBackupCopy();

  nextTest(testWriteBackup);
}

function testWriteBackup() {
  // Ensure sessionstore.bak is finally created.
  let ssExists = yield OS.File.exists(path);
  let ssBackupExists = yield OS.File.exists(backupPath);
  ok(ssExists, "sessionstore.js exists.");
  ok(ssBackupExists, "sessionstore.bak should now be created.");

  // Read sessionstore.bak data.
  let array = yield OS.File.read(backupPath);
  gSSBakData = gDecoder.decode(array);

  // Make sure that the sessionstore.bak is identical to the last
  // sessionstore.js.
  is(gSSBakData, gSSData, "sessionstore.js is backed up correctly.");

  // Read latest sessionstore.js.
  array = yield OS.File.read(path);
  gSSData = gDecoder.decode(array);

  // Read sessionstore.js with _SessionFile.read.
  let ssDataRead = yield _SessionFile.read();
  is(ssDataRead, gSSData, "_SessionFile.read read sessionstore.js correctly.");

  // Read sessionstore.js with _SessionFile.syncRead.
  ssDataRead = _SessionFile.syncRead();
  is(ssDataRead, gSSData,
    "_SessionFile.syncRead read sessionstore.js correctly.");

  // Remove sessionstore.js to test fallback onto sessionstore.bak.
  yield OS.File.remove(path);
  ssExists = yield OS.File.exists(path);
  ok(!ssExists, "sessionstore.js should be removed now.");

  // Read sessionstore.bak with _SessionFile.read.
  ssDataRead = yield _SessionFile.read();
  is(ssDataRead, gSSBakData,
    "_SessionFile.read read sessionstore.bak correctly.");

  // Read sessionstore.bak with _SessionFile.syncRead.
  ssDataRead = _SessionFile.syncRead();
  is(ssDataRead, gSSBakData,
    "_SessionFile.syncRead read sessionstore.bak correctly.");
  nextTest(testNoWriteBackup);
}

function testNoWriteBackup() {
  // Ensure sessionstore.bak is backed up only once.

  // Read sessionstore.bak data.
  let array = yield OS.File.read(backupPath);
  let ssBakData = gDecoder.decode(array);
  // Ensure the sessionstore.bak did not change.
  is(ssBakData, gSSBakData, "sessionstore.bak is unchanged.");

  executeSoon(finish);
}
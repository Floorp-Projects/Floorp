/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests are for a sessionstore.js atomic backup.
// Each test will wait for a write to the Session Store
// before executing.

var OS = Cu.import("resource://gre/modules/osfile.jsm", {}).OS;
var {File, Constants, Path} = OS;

const PREF_SS_INTERVAL = "browser.sessionstore.interval";
const Paths = SessionFile.Paths;

// A text decoder.
var gDecoder = new TextDecoder();
// Global variables that contain sessionstore.js and sessionstore.bak data for
// comparison between tests.
var gSSData;
var gSSBakData;

function promiseRead(path) {
  return File.read(path, {encoding: "utf-8"});
}

add_task(async function init() {
  // Make sure that we are not racing with SessionSaver's time based
  // saves.
  Services.prefs.setIntPref(PREF_SS_INTERVAL, 10000000);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF_SS_INTERVAL));
});

add_task(async function test_creation() {
  // Create dummy sessionstore backups
  let OLD_BACKUP = Path.join(Constants.Path.profileDir, "sessionstore.bak");
  let OLD_UPGRADE_BACKUP = Path.join(Constants.Path.profileDir, "sessionstore.bak-0000000");

  await File.writeAtomic(OLD_BACKUP, "sessionstore.bak");
  await File.writeAtomic(OLD_UPGRADE_BACKUP, "sessionstore upgrade backup");

  await SessionFile.wipe();
  await SessionFile.read(); // Reinitializes SessionFile

  // Ensure none of the sessionstore files and backups exists
  for (let k of Paths.loadOrder) {
    ok(!(await File.exists(Paths[k])), "After wipe " + k + " sessionstore file doesn't exist");
  }
  ok(!(await File.exists(OLD_BACKUP)), "After wipe, old backup doesn't exist");
  ok(!(await File.exists(OLD_UPGRADE_BACKUP)), "After wipe, old upgrade backup doesn't exist");

  // Open a new tab, save session, ensure that the correct files exist.
  let URL_BASE = "http://example.com/?atomic_backup_test_creation=" + Math.random();
  let URL = URL_BASE + "?first_write";
  let tab = BrowserTestUtils.addTab(gBrowser, URL);

  info("Testing situation after a single write");
  await promiseBrowserLoaded(tab.linkedBrowser);
  await TabStateFlusher.flush(tab.linkedBrowser);
  await SessionSaver.run();

  ok((await File.exists(Paths.recovery)), "After write, recovery sessionstore file exists again");
  ok(!(await File.exists(Paths.recoveryBackup)), "After write, recoveryBackup sessionstore doesn't exist");
  ok((await promiseRead(Paths.recovery)).indexOf(URL) != -1, "Recovery sessionstore file contains the required tab");
  ok(!(await File.exists(Paths.clean)), "After first write, clean shutdown sessionstore doesn't exist, since we haven't shutdown yet");

  // Open a second tab, save session, ensure that the correct files exist.
  info("Testing situation after a second write");
  let URL2 = URL_BASE + "?second_write";
  tab.linkedBrowser.loadURI(URL2);
  await promiseBrowserLoaded(tab.linkedBrowser);
  await TabStateFlusher.flush(tab.linkedBrowser);
  await SessionSaver.run();

  ok((await File.exists(Paths.recovery)), "After second write, recovery sessionstore file still exists");
  ok((await promiseRead(Paths.recovery)).indexOf(URL2) != -1, "Recovery sessionstore file contains the latest url");
  ok((await File.exists(Paths.recoveryBackup)), "After write, recoveryBackup sessionstore now exists");
  let backup = await promiseRead(Paths.recoveryBackup);
  ok(backup.indexOf(URL2) == -1, "Recovery backup doesn't contain the latest url");
  ok(backup.indexOf(URL) != -1, "Recovery backup contains the original url");
  ok(!(await File.exists(Paths.clean)), "After first write, clean shutdown sessinstore doesn't exist, since we haven't shutdown yet");

  info("Reinitialize, ensure that we haven't leaked sensitive files");
  await SessionFile.read(); // Reinitializes SessionFile
  await SessionSaver.run();
  ok(!(await File.exists(Paths.clean)), "After second write, clean shutdown sessonstore doesn't exist, since we haven't shutdown yet");
  ok(!(await File.exists(Paths.upgradeBackup)), "After second write, clean shutdwn sessionstore doesn't exist, since we haven't shutdown yet");
  ok(!(await File.exists(Paths.nextUpgradeBackup)), "After second write, clean sutdown sessionstore doesn't exist, since we haven't shutdown yet");

  gBrowser.removeTab(tab);
  await SessionFile.wipe();
});

var promiseSource = async function(name) {
  let URL = "http://example.com/?atomic_backup_test_recovery=" + Math.random() + "&name=" + name;
  let tab = BrowserTestUtils.addTab(gBrowser, URL);

  await promiseBrowserLoaded(tab.linkedBrowser);
  await TabStateFlusher.flush(tab.linkedBrowser);
  await SessionSaver.run();
  gBrowser.removeTab(tab);

  let SOURCE = await promiseRead(Paths.recovery);
  await SessionFile.wipe();
  return SOURCE;
};

add_task(async function test_recovery() {
  // Remove all files.
  await SessionFile.wipe();
  info("Attempting to recover from the recovery file");

  // Create Paths.recovery, ensure that we can recover from it.
  let SOURCE = await promiseSource("Paths.recovery");
  await File.makeDir(Paths.backups);
  await File.writeAtomic(Paths.recovery, SOURCE);
  is((await SessionFile.read()).source, SOURCE, "Recovered the correct source from the recovery file");
  await SessionFile.wipe();

  info("Corrupting recovery file, attempting to recover from recovery backup");
  SOURCE = await promiseSource("Paths.recoveryBackup");
  await File.makeDir(Paths.backups);
  await File.writeAtomic(Paths.recoveryBackup, SOURCE);
  await File.writeAtomic(Paths.recovery, "<Invalid JSON>");
  is((await SessionFile.read()).source, SOURCE, "Recovered the correct source from the recovery file");
  await SessionFile.wipe();
});

add_task(async function test_recovery_inaccessible() {
  // Can't do chmod() on non-UNIX platforms, we need that for this test.
  if (AppConstants.platform != "macosx" && AppConstants.platform != "linux") {
    return;
  }

  info("Making recovery file inaccessible, attempting to recover from recovery backup");
  let SOURCE_RECOVERY = await promiseSource("Paths.recovery");
  let SOURCE = await promiseSource("Paths.recoveryBackup");
  await File.makeDir(Paths.backups);
  await File.writeAtomic(Paths.recoveryBackup, SOURCE);

  // Write a valid recovery file but make it inaccessible.
  await File.writeAtomic(Paths.recovery, SOURCE_RECOVERY);
  await File.setPermissions(Paths.recovery, { unixMode: 0 });

  is((await SessionFile.read()).source, SOURCE, "Recovered the correct source from the recovery file");
  await File.setPermissions(Paths.recovery, { unixMode: 0o644 });
});

add_task(async function test_clean() {
  await SessionFile.wipe();
  let SOURCE = await promiseSource("Paths.clean");
  await File.writeAtomic(Paths.clean, SOURCE);
  await SessionFile.read();
  await SessionSaver.run();
  is((await promiseRead(Paths.cleanBackup)), SOURCE, "After first read/write, clean shutdown file has been moved to cleanBackup");
});


/**
 * Tests loading of sessionstore when format version is known.
 */
add_task(async function test_version() {
  info("Preparing sessionstore");
  let SOURCE = await promiseSource("Paths.clean");

  // Check there's a format version number
  is(JSON.parse(SOURCE).version[0], "sessionrestore", "Found sessionstore format version");

  // Create Paths.clean file
  await File.makeDir(Paths.backups);
  await File.writeAtomic(Paths.clean, SOURCE);

  info("Attempting to recover from the clean file");
  // Ensure that we can recover from Paths.recovery
  is((await SessionFile.read()).source, SOURCE, "Recovered the correct source from the clean file");
});

/**
 * Tests fallback to previous backups if format version is unknown.
 */
add_task(async function test_version_fallback() {
  info("Preparing data, making sure that it has a version number");
  let SOURCE = await promiseSource("Paths.clean");
  let BACKUP_SOURCE = await promiseSource("Paths.cleanBackup");

  is(JSON.parse(SOURCE).version[0], "sessionrestore", "Found sessionstore format version");
  is(JSON.parse(BACKUP_SOURCE).version[0], "sessionrestore", "Found backup sessionstore format version");

  await File.makeDir(Paths.backups);

  info("Modifying format version number to something incorrect, to make sure that we disregard the file.");
  let parsedSource = JSON.parse(SOURCE);
  parsedSource.version[0] = "bookmarks";
  await File.writeAtomic(Paths.clean, JSON.stringify(parsedSource));
  await File.writeAtomic(Paths.cleanBackup, BACKUP_SOURCE);
  is((await SessionFile.read()).source, BACKUP_SOURCE, "Recovered the correct source from the backup recovery file");

  info("Modifying format version number to a future version, to make sure that we disregard the file.");
  parsedSource = JSON.parse(SOURCE);
  parsedSource.version[1] = Number.MAX_SAFE_INTEGER;
  await File.writeAtomic(Paths.clean, JSON.stringify(parsedSource));
  await File.writeAtomic(Paths.cleanBackup, BACKUP_SOURCE);
  is((await SessionFile.read()).source, BACKUP_SOURCE, "Recovered the correct source from the backup recovery file");
});

add_task(async function cleanup() {
  await SessionFile.wipe();
});

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);

const Paths = SessionFile.Paths;
const PREF_UPGRADE = "browser.sessionstore.upgradeBackup.latestBuildID";
const PREF_MAX_UPGRADE_BACKUPS = "browser.sessionstore.upgradeBackup.maxUpgradeBackups";

/**
 * Prepares tests by retrieving the current platform's build ID, clearing the
 * build where the last backup was created and creating arbitrary JSON data
 * for a new backup.
 */
var prepareTest = Task.async(function* () {
  let result = {};

  result.buildID = Services.appinfo.platformBuildID;
  Services.prefs.setCharPref(PREF_UPGRADE, "");
  result.contents = JSON.stringify({"browser_upgrade_backup.js": Math.random()});

  return result;
});

/**
 * Retrieves all upgrade backups and returns them in an array.
 */
var getUpgradeBackups = Task.async(function* () {
  let iterator;
  let backups = [];
  let upgradeBackupPrefix = Paths.upgradeBackupPrefix;

  try {
    iterator = new OS.File.DirectoryIterator(Paths.backups);

    // iterate over all files in the backup directory
    yield iterator.forEach(function (file) {
      // check the upgradeBackupPrefix
      if (file.path.startsWith(Paths.upgradeBackupPrefix)) {
        // the file is a backup
        backups.push(file.path);
      }
    }, this);
  } finally {
    if (iterator) {
      iterator.close();
    }
  }

  // return results
  return backups;
});

add_task(function* init() {
  // Wait until initialization is complete
  yield SessionStore.promiseInitialized;
  yield SessionFile.wipe();
});

add_task(function* test_upgrade_backup() {
  let test = yield prepareTest();
  info("Let's check if we create an upgrade backup");
  yield OS.File.writeAtomic(Paths.clean, test.contents);
  yield SessionFile.read(); // First call to read() initializes the SessionWorker
  yield SessionFile.write(""); // First call to write() triggers the backup

  is(Services.prefs.getCharPref(PREF_UPGRADE), test.buildID, "upgrade backup should be set");

  is((yield OS.File.exists(Paths.upgradeBackup)), true, "upgrade backup file has been created");

  let data = yield OS.File.read(Paths.upgradeBackup);
  is(test.contents, (new TextDecoder()).decode(data), "upgrade backup contains the expected contents");

  info("Let's check that we don't overwrite this upgrade backup");
  let newContents = JSON.stringify({"something else entirely": Math.random()});
  yield OS.File.writeAtomic(Paths.clean, newContents);
  yield SessionFile.read(); // Reinitialize the SessionWorker
  yield SessionFile.write(""); // Next call to write() shouldn't trigger the backup
  data = yield OS.File.read(Paths.upgradeBackup);
  is(test.contents, (new TextDecoder()).decode(data), "upgrade backup hasn't changed");
});

add_task(function* test_upgrade_backup_removal() {
  let test = yield prepareTest();
  let maxUpgradeBackups = Preferences.get(PREF_MAX_UPGRADE_BACKUPS, 3);
  info("Let's see if we remove backups if there are too many");
  yield OS.File.writeAtomic(Paths.clean, test.contents);

  // if the nextUpgradeBackup already exists (from another test), remove it
  if (OS.File.exists(Paths.nextUpgradeBackup)) {
    yield OS.File.remove(Paths.nextUpgradeBackup);
  }

  // create dummy backups
  yield OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20080101010101", "");
  yield OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20090101010101", "");
  yield OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20100101010101", "");
  yield OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20110101010101", "");
  yield OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20120101010101", "");
  yield OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20130101010101", "");

  // get currently existing backups
  let backups = yield getUpgradeBackups();

  // trigger new backup
  yield SessionFile.read(); // First call to read() initializes the SessionWorker
  yield SessionFile.write(""); // First call to write() triggers the backup and the cleanup

  // a new backup should have been created (and still exist)
  is(Services.prefs.getCharPref(PREF_UPGRADE), test.buildID, "upgrade backup should be set");
  is((yield OS.File.exists(Paths.upgradeBackup)), true, "upgrade backup file has been created");

  // get currently existing backups and check their count
  let newBackups = yield getUpgradeBackups();
  is(newBackups.length, maxUpgradeBackups, "expected number of backups are present after removing old backups");

  // find all backups that were created during the last call to `SessionFile.write("");`
  // ie, filter out all the backups that have already been present before the call
  newBackups = newBackups.filter(function (backup) {
    return backups.indexOf(backup) < 0;
  });

  // check that exactly one new backup was created
  is(newBackups.length, 1, "one new backup was created that was not removed");

  yield SessionFile.write(""); // Second call to write() should not trigger anything

  backups = yield getUpgradeBackups();
  is(backups.length, maxUpgradeBackups, "second call to SessionFile.write() didn't create or remove more backups");
});


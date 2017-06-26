/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);

const Paths = SessionFile.Paths;
const PREF_UPGRADE = "browser.sessionstore.upgradeBackup.latestBuildID";
const PREF_MAX_UPGRADE_BACKUPS = "browser.sessionstore.upgradeBackup.maxUpgradeBackups";

/**
 * Prepares tests by retrieving the current platform's build ID, clearing the
 * build where the last backup was created and creating arbitrary JSON data
 * for a new backup.
 */
var prepareTest = async function() {
  let result = {};

  result.buildID = Services.appinfo.platformBuildID;
  Services.prefs.setCharPref(PREF_UPGRADE, "");
  result.contents = JSON.stringify({"browser_upgrade_backup.js": Math.random()});

  return result;
};

/**
 * Retrieves all upgrade backups and returns them in an array.
 */
var getUpgradeBackups = async function() {
  let iterator;
  let backups = [];

  try {
    iterator = new OS.File.DirectoryIterator(Paths.backups);

    // iterate over all files in the backup directory
    await iterator.forEach(function(file) {
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
};

add_task(async function init() {
  // Wait until initialization is complete
  await SessionStore.promiseInitialized;
  await SessionFile.wipe();
});

add_task(async function test_upgrade_backup() {
  let test = await prepareTest();
  info("Let's check if we create an upgrade backup");
  await OS.File.writeAtomic(Paths.clean, test.contents, {encoding: "utf-8", compression: "lz4"});
  await SessionFile.read(); // First call to read() initializes the SessionWorker
  await SessionFile.write(""); // First call to write() triggers the backup

  is(Services.prefs.getCharPref(PREF_UPGRADE), test.buildID, "upgrade backup should be set");

  is((await OS.File.exists(Paths.upgradeBackup)), true, "upgrade backup file has been created");

  let data = await OS.File.read(Paths.upgradeBackup, {compression: "lz4"});
  is(test.contents, (new TextDecoder()).decode(data), "upgrade backup contains the expected contents");

  info("Let's check that we don't overwrite this upgrade backup");
  let newContents = JSON.stringify({"something else entirely": Math.random()});
  await OS.File.writeAtomic(Paths.clean, newContents, {encoding: "utf-8", compression: "lz4"});
  await SessionFile.read(); // Reinitialize the SessionWorker
  await SessionFile.write(""); // Next call to write() shouldn't trigger the backup
  data = await OS.File.read(Paths.upgradeBackup, {compression: "lz4"});
  is(test.contents, (new TextDecoder()).decode(data), "upgrade backup hasn't changed");
});

add_task(async function test_upgrade_backup_removal() {
  let test = await prepareTest();
  let maxUpgradeBackups = Preferences.get(PREF_MAX_UPGRADE_BACKUPS, 3);
  info("Let's see if we remove backups if there are too many");
  await OS.File.writeAtomic(Paths.clean, test.contents, {encoding: "utf-8", compression: "lz4"});

  // if the nextUpgradeBackup already exists (from another test), remove it
  if (OS.File.exists(Paths.nextUpgradeBackup)) {
    await OS.File.remove(Paths.nextUpgradeBackup);
  }

  // create dummy backups
  await OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20080101010101", "", {encoding: "utf-8", compression: "lz4"});
  await OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20090101010101", "", {encoding: "utf-8", compression: "lz4"});
  await OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20100101010101", "", {encoding: "utf-8", compression: "lz4"});
  await OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20110101010101", "", {encoding: "utf-8", compression: "lz4"});
  await OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20120101010101", "", {encoding: "utf-8", compression: "lz4"});
  await OS.File.writeAtomic(Paths.upgradeBackupPrefix + "20130101010101", "", {encoding: "utf-8", compression: "lz4"});

  // get currently existing backups
  let backups = await getUpgradeBackups();

  // trigger new backup
  await SessionFile.read(); // First call to read() initializes the SessionWorker
  await SessionFile.write(""); // First call to write() triggers the backup and the cleanup

  // a new backup should have been created (and still exist)
  is(Services.prefs.getCharPref(PREF_UPGRADE), test.buildID, "upgrade backup should be set");
  is((await OS.File.exists(Paths.upgradeBackup)), true, "upgrade backup file has been created");

  // get currently existing backups and check their count
  let newBackups = await getUpgradeBackups();
  is(newBackups.length, maxUpgradeBackups, "expected number of backups are present after removing old backups");

  // find all backups that were created during the last call to `SessionFile.write("");`
  // ie, filter out all the backups that have already been present before the call
  newBackups = newBackups.filter(function(backup) {
    return backups.indexOf(backup) < 0;
  });

  // check that exactly one new backup was created
  is(newBackups.length, 1, "one new backup was created that was not removed");

  await SessionFile.write(""); // Second call to write() should not trigger anything

  backups = await getUpgradeBackups();
  is(backups.length, maxUpgradeBackups, "second call to SessionFile.write() didn't create or remove more backups");
});

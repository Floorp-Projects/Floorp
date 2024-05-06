/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ActivityStreamStorage:
    "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs",
  ProfileAge: "resource://gre/modules/ProfileAge.sys.mjs",
});

const SNIPPETS_TABLE_NAME = "snippets";
const FILES_FOR_BACKUP = [
  "enumerate_devices.txt",
  "protections.sqlite",
  "SiteSecurityServiceState.bin",
];

/**
 * Class representing miscellaneous files for telemetry, site storage,
 * media device origin mapping, chrome privileged IndexedDB databases,
 * and Mozilla Accounts within a user profile.
 */
export class MiscDataBackupResource extends BackupResource {
  static get key() {
    return "miscellaneous";
  }

  static get requiresEncryption() {
    return false;
  }

  async backup(stagingPath, profilePath = PathUtils.profileDir) {
    const files = ["enumerate_devices.txt", "SiteSecurityServiceState.bin"];
    await BackupResource.copyFiles(profilePath, stagingPath, files);

    const sqliteDatabases = ["protections.sqlite"];
    await BackupResource.copySqliteDatabases(
      profilePath,
      stagingPath,
      sqliteDatabases
    );

    // Bug 1890585 - we don't currently have the ability to copy the
    // chrome-privileged IndexedDB databases under storage/permanent/chrome.
    // Instead, we'll manually export any IndexedDB data we need to backup
    // to a separate JSON file.

    // The first IndexedDB database we want to back up is the ActivityStream
    // one - specifically, the "snippets" table, as this contains information
    // on ASRouter impressions, blocked messages, message group impressions,
    // etc.
    let storage = new lazy.ActivityStreamStorage({
      storeNames: [SNIPPETS_TABLE_NAME],
    });
    let snippetsTable = await storage.getDbTable(SNIPPETS_TABLE_NAME);
    let snippetsObj = {};
    for (let key of await snippetsTable.getAllKeys()) {
      snippetsObj[key] = await snippetsTable.get(key);
    }
    let snippetsBackupFile = PathUtils.join(
      stagingPath,
      "activity-stream-snippets.json"
    );
    await IOUtils.writeJSON(snippetsBackupFile, snippetsObj);

    return null;
  }

  async recover(_manifestEntry, recoveryPath, destProfilePath) {
    await BackupResource.copyFiles(
      recoveryPath,
      destProfilePath,
      FILES_FOR_BACKUP
    );

    // The times.json file, the one that powers ProfileAge, works hand in hand
    // with the Telemetry client ID. We don't want to accidentally _overwrite_
    // a pre-existing times.json with data from a different profile, because
    // then the client ID wouldn't match the times.json data anymore.
    //
    // The rule that we're following for backups and recoveries is that the
    // recovered profile always inherits the client ID (and therefore the
    // times.json) from the profile that _initiated recovery_.
    //
    // This means we want to copy the times.json file from the profile that's
    // currently in use to the destProfilePath.
    await BackupResource.copyFiles(PathUtils.profileDir, destProfilePath, [
      "times.json",
    ]);

    // We also want to write the recoveredFromBackup timestamp now.
    let profileAge = await lazy.ProfileAge(destProfilePath);
    await profileAge.recordRecoveredFromBackup();

    // The activity-stream-snippets data will need to be written during the
    // postRecovery phase, so we'll stash the path to the JSON file in the
    // post recovery entry.
    let snippetsBackupFile = PathUtils.join(
      recoveryPath,
      "activity-stream-snippets.json"
    );
    return { snippetsBackupFile };
  }

  async postRecovery(postRecoveryEntry) {
    let { snippetsBackupFile } = postRecoveryEntry;

    // If for some reason, the activity-stream-snippets data file has been
    // removed already, there's nothing to do.
    if (!IOUtils.exists(snippetsBackupFile)) {
      return;
    }

    let snippetsData = await IOUtils.readJSON(snippetsBackupFile);
    let storage = new lazy.ActivityStreamStorage({
      storeNames: [SNIPPETS_TABLE_NAME],
    });
    let snippetsTable = await storage.getDbTable(SNIPPETS_TABLE_NAME);
    for (let key in snippetsData) {
      let value = snippetsData[key];
      await snippetsTable.set(key, value);
    }
  }

  async measure(profilePath = PathUtils.profileDir) {
    let fullSize = 0;

    for (let filePath of FILES_FOR_BACKUP) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        fullSize += resourceSize;
      }
    }

    let chromeIndexedDBDirPath = PathUtils.join(
      profilePath,
      "storage",
      "permanent",
      "chrome"
    );
    let chromeIndexedDBDirSize = await BackupResource.getDirectorySize(
      chromeIndexedDBDirPath
    );
    if (Number.isInteger(chromeIndexedDBDirSize)) {
      fullSize += chromeIndexedDBDirSize;
    }

    Glean.browserBackup.miscDataSize.set(fullSize);
  }
}

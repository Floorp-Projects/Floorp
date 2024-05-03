/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import {
  BackupResource,
  bytesToFuzzyKilobytes,
} from "resource:///modules/backup/BackupResource.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

/**
 * Class representing Session store related files within a user profile.
 */
export class SessionStoreBackupResource extends BackupResource {
  static get key() {
    return "sessionstore";
  }

  static get requiresEncryption() {
    // Session store data does not require encryption, but if encryption is
    // disabled, then session cookies will be cleared from the backup before
    // writing it to the disk.
    return false;
  }

  async backup(stagingPath, profilePath = PathUtils.profileDir) {
    let sessionStoreState = lazy.SessionStore.getCurrentState(true);
    let sessionStorePath = PathUtils.join(stagingPath, "sessionstore.jsonlz4");

    /* Bug 1891854 - remove cookies from session store state if the backup file is
     * not encrypted. */

    await IOUtils.writeJSON(sessionStorePath, sessionStoreState, {
      compress: true,
    });
    await BackupResource.copyFiles(profilePath, stagingPath, [
      "sessionstore-backups",
    ]);

    return null;
  }

  async recover(_manifestEntry, recoveryPath, destProfilePath) {
    await BackupResource.copyFiles(recoveryPath, destProfilePath, [
      "sessionstore.jsonlz4",
      "sessionstore-backups",
    ]);

    return null;
  }

  async measure(profilePath = PathUtils.profileDir) {
    // Get the current state of the session store JSON and
    // measure it's uncompressed size.
    let sessionStoreJson = lazy.SessionStore.getCurrentState(true);
    let sessionStoreSize = new TextEncoder().encode(
      JSON.stringify(sessionStoreJson)
    ).byteLength;
    let sessionStoreNearestTenthKb = bytesToFuzzyKilobytes(sessionStoreSize);

    Glean.browserBackup.sessionStoreSize.set(sessionStoreNearestTenthKb);

    let sessionStoreBackupsDirectoryPath = PathUtils.join(
      profilePath,
      "sessionstore-backups"
    );
    let sessionStoreBackupsDirectorySize =
      await BackupResource.getDirectorySize(sessionStoreBackupsDirectoryPath);

    Glean.browserBackup.sessionStoreBackupsDirectorySize.set(
      sessionStoreBackupsDirectorySize
    );
  }
}

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

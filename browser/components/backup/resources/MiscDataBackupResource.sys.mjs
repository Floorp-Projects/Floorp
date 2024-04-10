/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

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
    const files = [
      "times.json",
      "enumerate_devices.txt",
      "SiteSecurityServiceState.bin",
    ];

    for (let fileName of files) {
      let sourcePath = PathUtils.join(profilePath, fileName);
      let destPath = PathUtils.join(stagingPath, fileName);
      if (await IOUtils.exists(sourcePath)) {
        await IOUtils.copy(sourcePath, destPath, { recursive: true });
      }
    }

    const sqliteDatabases = ["protections.sqlite"];

    for (let fileName of sqliteDatabases) {
      let sourcePath = PathUtils.join(profilePath, fileName);
      let destPath = PathUtils.join(stagingPath, fileName);
      let connection;

      try {
        connection = await lazy.Sqlite.openConnection({
          path: sourcePath,
          readOnly: true,
        });

        await connection.backup(destPath);
      } finally {
        await connection.close();
      }
    }

    // Bug 1890585 - we don't currently have the ability to copy the
    // chrome-privileged IndexedDB databases under storage/permanent/chrome, so
    // we'll just skip that for now.

    return null;
  }

  async measure(profilePath = PathUtils.profileDir) {
    const files = [
      "times.json",
      "enumerate_devices.txt",
      "protections.sqlite",
      "SiteSecurityServiceState.bin",
    ];

    let fullSize = 0;

    for (let filePath of files) {
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

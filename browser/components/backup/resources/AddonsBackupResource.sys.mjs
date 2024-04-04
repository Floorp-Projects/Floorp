/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";

/**
 * Backup for addons and extensions files and data.
 */
export class AddonsBackupResource extends BackupResource {
  static get key() {
    return "addons";
  }

  async measure(profilePath = PathUtils.profileDir) {
    // Report the total size of the extension json files.
    const jsonFiles = [
      "extensions.json",
      "extension-settings.json",
      "extension-preferences.json",
      "addonStartup.json.lz4",
    ];
    let extensionsJsonSize = 0;
    for (const filePath of jsonFiles) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        extensionsJsonSize += resourceSize;
      }
    }
    Glean.browserBackup.extensionsJsonSize.set(extensionsJsonSize);

    // Report the size of permissions store data, if present.
    let extensionStorePermissionsDataPath = PathUtils.join(
      profilePath,
      "extension-store-permissions",
      "data.safe.bin"
    );
    let extensionStorePermissionsDataSize = await BackupResource.getFileSize(
      extensionStorePermissionsDataPath
    );
    if (Number.isInteger(extensionStorePermissionsDataSize)) {
      Glean.browserBackup.extensionStorePermissionsDataSize.set(
        extensionStorePermissionsDataSize
      );
    }

    // Report the size of extensions storage sync database.
    let storageSyncPath = PathUtils.join(profilePath, "storage-sync-v2.sqlite");
    let storageSyncSize = await BackupResource.getFileSize(storageSyncPath);
    Glean.browserBackup.storageSyncSize.set(storageSyncSize);

    // Report the total size of XPI files in the extensions directory.
    let extensionsXpiDirectoryPath = PathUtils.join(profilePath, "extensions");
    let extensionsXpiDirectorySize = await BackupResource.getDirectorySize(
      extensionsXpiDirectoryPath,
      {
        shouldExclude: (filePath, fileType) =>
          fileType !== "regular" || !filePath.endsWith(".xpi"),
      }
    );
    Glean.browserBackup.extensionsXpiDirectorySize.set(
      extensionsXpiDirectorySize
    );

    // Report the total size of the browser extension data.
    let browserExtensionDataPath = PathUtils.join(
      profilePath,
      "browser-extension-data"
    );
    let browserExtensionDataSize = await BackupResource.getDirectorySize(
      browserExtensionDataPath
    );
    Glean.browserBackup.browserExtensionDataSize.set(browserExtensionDataSize);

    // Report the size of all moz-extension IndexedDB databases.
    let defaultStoragePath = PathUtils.join(profilePath, "storage", "default");
    let extensionsStorageSize = await BackupResource.getDirectorySize(
      defaultStoragePath,
      {
        shouldExclude: (filePath, _fileType, parentPath) => {
          if (
            parentPath == defaultStoragePath &&
            !PathUtils.filename(filePath).startsWith("moz-extension")
          ) {
            return true;
          }
          return false;
        },
      }
    );
    if (Number.isInteger(extensionsStorageSize)) {
      Glean.browserBackup.extensionsStorageSize.set(extensionsStorageSize);
    }
  }
}

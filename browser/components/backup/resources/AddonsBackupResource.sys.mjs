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

  static get requiresEncryption() {
    return false;
  }

  async backup(stagingPath, profilePath = PathUtils.profileDir) {
    // Files and directories to backup.
    let toCopy = [
      "extensions.json",
      "extension-settings.json",
      "extension-preferences.json",
      "addonStartup.json.lz4",
      "browser-extension-data",
      "extension-store-permissions",
    ];
    await BackupResource.copyFiles(profilePath, stagingPath, toCopy);

    // Backup only the XPIs in the extensions directory.
    let xpiFiles = [];
    let extensionsXPIDirectoryPath = PathUtils.join(profilePath, "extensions");
    let xpiDirectoryChildren = await IOUtils.getChildren(
      extensionsXPIDirectoryPath,
      {
        ignoreAbsent: true,
      }
    );
    for (const childFilePath of xpiDirectoryChildren) {
      if (childFilePath.endsWith(".xpi")) {
        let childFileName = PathUtils.filename(childFilePath);
        xpiFiles.push(childFileName);
      }
    }
    // Create the extensions directory in the staging directory.
    let stagingExtensionsXPIDirectoryPath = PathUtils.join(
      stagingPath,
      "extensions"
    );
    await IOUtils.makeDirectory(stagingExtensionsXPIDirectoryPath);
    // Copy all found XPIs to the staging directory.
    await BackupResource.copyFiles(
      extensionsXPIDirectoryPath,
      stagingExtensionsXPIDirectoryPath,
      xpiFiles
    );

    // Copy storage sync database.
    let databases = ["storage-sync-v2.sqlite"];
    await BackupResource.copySqliteDatabases(
      profilePath,
      stagingPath,
      databases
    );

    return null;
  }

  async recover(_manifestEntry, recoveryPath, destProfilePath) {
    const files = [
      "extensions.json",
      "extension-settings.json",
      "extension-preferences.json",
      "addonStartup.json.lz4",
      "browser-extension-data",
      "extension-store-permissions",
      "extensions",
      "storage-sync-v2.sqlite",
    ];
    await BackupResource.copyFiles(recoveryPath, destProfilePath, files);

    return null;
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
    let extensionsXPIDirectoryPath = PathUtils.join(profilePath, "extensions");
    let extensionsXPIDirectorySize = await BackupResource.getDirectorySize(
      extensionsXPIDirectoryPath,
      {
        shouldExclude: (filePath, fileType) =>
          fileType !== "regular" || !filePath.endsWith(".xpi"),
      }
    );
    Glean.browserBackup.extensionsXpiDirectorySize.set(
      extensionsXPIDirectorySize
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

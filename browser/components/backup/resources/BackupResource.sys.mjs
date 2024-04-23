/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

// Convert from bytes to kilobytes (not kibibytes).
export const BYTES_IN_KB = 1000;

/**
 * Convert bytes to the nearest 10th kilobyte to make the measurements fuzzier.
 *
 * @param {number} bytes - size in bytes.
 * @returns {number} - size in kilobytes rounded to the nearest 10th kilobyte.
 */
export function bytesToFuzzyKilobytes(bytes) {
  let sizeInKb = Math.ceil(bytes / BYTES_IN_KB);
  let nearestTenthKb = Math.round(sizeInKb / 10) * 10;
  return Math.max(nearestTenthKb, 1);
}

/**
 * An abstract class representing a set of data within a user profile
 * that can be persisted to a separate backup archive file, and restored
 * to a new user profile from that backup archive file.
 */
export class BackupResource {
  /**
   * This must be overridden to return a simple string identifier for the
   * resource, for example "places" or "extensions". This key is used as
   * a unique identifier for the resource.
   *
   * @type {string}
   */
  static get key() {
    throw new Error("BackupResource::key needs to be overridden.");
  }

  /**
   * This must be overridden to return a boolean indicating whether the
   * resource requires encryption when being backed up. Encryption should be
   * required for particularly sensitive data, such as passwords / credentials,
   * cookies, or payment methods. If you're not sure, talk to someone from the
   * Privacy team.
   *
   * @type {boolean}
   */
  static get requiresEncryption() {
    throw new Error(
      "BackupResource::requiresEncryption needs to be overridden."
    );
  }

  /**
   * This can be overridden to return a number indicating the priority the
   * resource should have in the backup order.
   *
   * Resources with a higher priority will be backed up first.
   * The default priority of 0 indicates it can be processed in any order.
   *
   * @returns {number}
   */
  static get priority() {
    return 0;
  }

  /**
   * Get the size of a file.
   *
   * @param {string} filePath - path to a file.
   * @returns {Promise<number|null>} - the size of the file in kilobytes, or null if the
   * file does not exist, the path is a directory or the size is unknown.
   */
  static async getFileSize(filePath) {
    if (!(await IOUtils.exists(filePath))) {
      return null;
    }

    let { size } = await IOUtils.stat(filePath);

    if (size < 0) {
      return null;
    }

    let nearestTenthKb = bytesToFuzzyKilobytes(size);

    return nearestTenthKb;
  }

  /**
   * Get the total size of a directory.
   *
   * @param {string} directoryPath - path to a directory.
   * @param {object}   options - A set of additional optional parameters.
   * @param {Function} [options.shouldExclude] - an optional callback which based on file path and file type should return true
   * if the file should be excluded from the computed directory size.
   * @returns {Promise<number|null>} - the size of all descendants of the directory in kilobytes, or null if the
   * directory does not exist, the path is not a directory or the size is unknown.
   */
  static async getDirectorySize(
    directoryPath,
    { shouldExclude = () => false } = {}
  ) {
    if (!(await IOUtils.exists(directoryPath))) {
      return null;
    }

    let { type } = await IOUtils.stat(directoryPath);

    if (type != "directory") {
      return null;
    }

    let children = await IOUtils.getChildren(directoryPath, {
      ignoreAbsent: true,
    });

    let size = 0;
    for (const childFilePath of children) {
      let { size: childSize, type: childType } = await IOUtils.stat(
        childFilePath
      );

      if (shouldExclude(childFilePath, childType, directoryPath)) {
        continue;
      }

      if (childSize >= 0) {
        let nearestTenthKb = bytesToFuzzyKilobytes(childSize);

        size += nearestTenthKb;
      }

      if (childType == "directory") {
        let childDirectorySize = await this.getDirectorySize(childFilePath, {
          shouldExclude,
        });
        if (Number.isInteger(childDirectorySize)) {
          size += childDirectorySize;
        }
      }
    }

    return size;
  }

  /**
   * Copy a set of SQLite databases safely from a source directory to a
   * destination directory. A new read-only connection is opened for each
   * database, and then a backup is created. If the source database does not
   * exist, it is ignored.
   *
   * @param {string} sourcePath
   *   Path to the source directory of the SQLite databases.
   * @param {string} destPath
   *   Path to the destination directory where the SQLite databases should be
   *   copied to.
   * @param {Array<string>} sqliteDatabases
   *   An array of filenames of the SQLite databases to copy.
   * @returns {Promise<undefined>}
   */
  static async copySqliteDatabases(sourcePath, destPath, sqliteDatabases) {
    for (let fileName of sqliteDatabases) {
      let sourceFilePath = PathUtils.join(sourcePath, fileName);

      if (!(await IOUtils.exists(sourceFilePath))) {
        continue;
      }

      let destFilePath = PathUtils.join(destPath, fileName);
      let connection;

      try {
        connection = await lazy.Sqlite.openConnection({
          path: sourceFilePath,
          readOnly: true,
        });

        await connection.backup(
          destFilePath,
          BackupResource.SQLITE_PAGES_PER_STEP,
          BackupResource.SQLITE_STEP_DELAY_MS
        );
      } finally {
        await connection?.close();
      }
    }
  }

  /**
   * A helper function to copy a set of files from a source directory to a
   * destination directory. Callers should ensure that the source files can be
   * copied safely before invoking this function. Files that do not exist will
   * be ignored. Callers that wish to copy SQLite databases should use
   * copySqliteDatabases() instead.
   *
   * @param {string} sourcePath
   *   Path to the source directory of the files to be copied.
   * @param {string} destPath
   *   Path to the destination directory where the files should be
   *   copied to.
   * @param {string[]} fileNames
   *   An array of filenames of the files to copy.
   * @returns {Promise<undefined>}
   */
  static async copyFiles(sourcePath, destPath, fileNames) {
    for (let fileName of fileNames) {
      let sourceFilePath = PathUtils.join(sourcePath, fileName);
      let destFilePath = PathUtils.join(destPath, fileName);
      if (await IOUtils.exists(sourceFilePath)) {
        await IOUtils.copy(sourceFilePath, destFilePath, { recursive: true });
      }
    }
  }

  constructor() {}

  /**
   * This must be overridden to record telemetry on the size of any
   * data associated with this BackupResource.
   *
   * @param {string} profilePath - path to a profile directory.
   * @returns {Promise<undefined>}
   */
  // eslint-disable-next-line no-unused-vars
  async measure(profilePath) {
    throw new Error("BackupResource::measure needs to be overridden.");
  }

  /**
   * Perform a safe copy of the datastores that this resource manages and write
   * them into the backup database. The Promise should resolve with an object
   * that can be serialized to JSON, as it will be written to the manifest file.
   * This same object will be deserialized and passed to restore() when
   * restoring the backup. This object can be null if no additional information
   * is needed to restore the backup.
   *
   * @param {string} stagingPath
   *   The path to the staging folder where copies of the datastores for this
   *   BackupResource should be written to.
   * @param {string} [profilePath=null]
   *   This is null if the backup is being run on the currently running user
   *   profile. If, however, the backup is being run on a different user profile
   *   (for example, it's being run from a BackgroundTask on a user profile that
   *   just shut down, or during test), then this is a string set to that user
   *   profile path.
   *
   * @returns {Promise<object|null>}
   */
  // eslint-disable-next-line no-unused-vars
  async backup(stagingPath, profilePath = null) {
    throw new Error("BackupResource::backup must be overridden");
  }

  /**
   * Recovers the datastores that this resource manages from a backup archive
   * that has been decompressed into the recoveryPath. A pre-existing unlocked
   * user profile should be available to restore into, and destProfilePath
   * should point at its location on the file system.
   *
   * This method is not expected to be running in an app connected to the
   * destProfilePath. If the BackupResource needs to run some operations
   * while attached to the recovery profile, it should do that work inside of
   * postRecovery(). If data needs to be transferred to postRecovery(), it
   * should be passed as a JSON serializable object in the return value of this
   * method.
   *
   * @see BackupResource.postRecovery()
   * @param {object|null} manifestEntry
   *   The object that was returned by the backup() method when the backup was
   *   created. This object can be null if no additional information was needed
   *   for recovery.
   * @param {string} recoveryPath
   *   The path to the resource directory where the backup archive has been
   *   decompressed.
   * @param {string} destProfilePath
   *   The path to the profile directory where the backup should be restored to.
   * @returns {Promise<object|null>}
   *   This should return a JSON serializable object that will be passed to
   *   postRecovery() if any data needs to be passed to it. This object can be
   *   null if no additional information is needed for postRecovery().
   */
  // eslint-disable-next-line no-unused-vars
  async recover(manifestEntry, recoveryPath, destProfilePath) {
    throw new Error("BackupResource::recover must be overridden");
  }

  /**
   * Perform any post-recovery operations that need to be done after the
   * recovery has been completed and the recovered profile has been attached
   * to.
   *
   * This method is running in an app connected to the recovered profile. The
   * profile is locked, but this postRecovery method can be used to insert
   * data into connected datastores, or perform any other operations that can
   * only occur within the context of the recovered profile.
   *
   * @see BackupResource.recover()
   * @param {object|null} postRecoveryEntry
   *  The object that was returned by the recover() method when the recovery
   *  was originally done. This object can be null if no additional information
   *  is needed for post-recovery.
   */
  // eslint-disable-next-line no-unused-vars
  async postRecovery(postRecoveryEntry) {
    // no-op by default
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  BackupResource,
  "SQLITE_PAGES_PER_STEP",
  "browser.backup.sqlite.pages_per_step",
  5
);

XPCOMUtils.defineLazyPreferenceGetter(
  BackupResource,
  "SQLITE_STEP_DELAY_MS",
  "browser.backup.sqlite.step_delay_ms",
  250
);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as DefaultBackupResources from "resource:///modules/backup/BackupResources.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "logConsole", function () {
  return console.createInstance({
    prefix: "BackupService",
    maxLogLevel: Services.prefs.getBoolPref("browser.backup.log", false)
      ? "Debug"
      : "Warn",
  });
});

/**
 * The BackupService class orchestrates the scheduling and creation of profile
 * backups. It also does most of the heavy lifting for the restoration of a
 * profile backup.
 */
export class BackupService {
  /**
   * The BackupService singleton instance.
   *
   * @static
   * @type {BackupService|null}
   */
  static #instance = null;

  /**
   * Map of instantiated BackupResource classes.
   *
   * @type {Map<string, BackupResource>}
   */
  #resources = new Map();

  /**
   * True if a backup is currently in progress.
   *
   * @type {boolean}
   */
  #backupInProgress = false;

  /**
   * Returns a reference to a BackupService singleton. If this is the first time
   * that this getter is accessed, this causes the BackupService singleton to be
   * be instantiated.
   *
   * @static
   * @type {BackupService}
   */
  static init() {
    if (this.#instance) {
      return this.#instance;
    }
    this.#instance = new BackupService(DefaultBackupResources);
    this.#instance.takeMeasurements();

    return this.#instance;
  }

  /**
   * Returns a reference to the BackupService singleton. If the singleton has
   * not been initialized, an error is thrown.
   *
   * @static
   * @returns {BackupService}
   */
  static get() {
    if (!this.#instance) {
      throw new Error("BackupService not initialized");
    }
    return this.#instance;
  }

  /**
   * Create a BackupService instance.
   *
   * @param {object} [backupResources=DefaultBackupResources] - Object containing BackupResource classes to associate with this service.
   */
  constructor(backupResources = DefaultBackupResources) {
    lazy.logConsole.debug("Instantiated");

    for (const resourceName in backupResources) {
      let resource = backupResources[resourceName];
      this.#resources.set(resource.key, resource);
    }
  }

  /**
   * Create a backup of the user's profile.
   *
   * @param {object} [options]
   *   Options for the backup.
   * @param {string} [options.profilePath=PathUtils.profileDir]
   *   The path to the profile to backup. By default, this is the current
   *   profile.
   * @returns {Promise<undefined>}
   */
  async createBackup({ profilePath = PathUtils.profileDir } = {}) {
    // createBackup does not allow re-entry or concurrent backups.
    if (this.#backupInProgress) {
      lazy.logConsole.warn("Backup attempt already in progress");
      return;
    }

    this.#backupInProgress = true;

    try {
      lazy.logConsole.debug(`Creating backup for profile at ${profilePath}`);

      // First, check to see if a `backups` directory already exists in the
      // profile.
      let backupDirPath = PathUtils.join(profilePath, "backups");
      lazy.logConsole.debug("Creating backups folder");

      // ignoreExisting: true is the default, but we're being explicit that it's
      // okay if this folder already exists.
      await IOUtils.makeDirectory(backupDirPath, { ignoreExisting: true });

      let stagingPath = await this.#prepareStagingFolder(backupDirPath);

      // Perform the backup for each resource.
      for (let resourceClass of this.#resources.values()) {
        try {
          let resourcePath = PathUtils.join(stagingPath, resourceClass.key);
          await IOUtils.makeDirectory(resourcePath);

          // `backup` on each BackupResource should return us a ManifestEntry
          // that we eventually write to a JSON manifest file, but for now,
          // we're just going to log it.
          let manifestEntry = await new resourceClass().backup(
            resourcePath,
            profilePath
          );
          lazy.logConsole.debug(
            `Backup of resource with key ${resourceClass.key} completed`,
            manifestEntry
          );
        } catch (e) {
          lazy.logConsole.error(
            `Failed to backup resource: ${resourceClass.key}`,
            e
          );
        }
      }
    } finally {
      this.#backupInProgress = false;
    }
  }

  /**
   * Constructs the staging folder for the backup in the passed in backup
   * folder. If a pre-existing staging folder exists, it will be cleared out.
   *
   * @param {string} backupDirPath
   *   The path to the backup folder.
   * @returns {Promise<string>}
   *   The path to the empty staging folder.
   */
  async #prepareStagingFolder(backupDirPath) {
    let stagingPath = PathUtils.join(backupDirPath, "staging");
    lazy.logConsole.debug("Checking for pre-existing staging folder");
    if (await IOUtils.exists(stagingPath)) {
      // A pre-existing staging folder exists. A previous backup attempt must
      // have failed or been interrupted. We'll clear it out.
      lazy.logConsole.warn("A pre-existing staging folder exists. Clearing.");
      await IOUtils.remove(stagingPath, { recursive: true });
    }
    await IOUtils.makeDirectory(stagingPath);

    return stagingPath;
  }

  /**
   * Take measurements of the current profile state for Telemetry.
   *
   * @returns {Promise<undefined>}
   */
  async takeMeasurements() {
    lazy.logConsole.debug("Taking Telemetry measurements");

    // Note: We're talking about kilobytes here, not kibibytes. That means
    // 1000 bytes, and not 1024 bytes.
    const BYTES_IN_KB = 1000;
    const BYTES_IN_MB = 1000000;

    // We'll start by measuring the available disk space on the storage
    // device that the profile directory is on.
    let profileDir = await IOUtils.getFile(PathUtils.profileDir);

    let profDDiskSpaceBytes = profileDir.diskSpaceAvailable;

    // Make the measurement fuzzier by rounding to the nearest 10MB.
    let profDDiskSpaceMB =
      Math.round(profDDiskSpaceBytes / BYTES_IN_MB / 100) * 100;

    // And then record the value in kilobytes, since that's what everything
    // else is going to be measured in.
    Glean.browserBackup.profDDiskSpace.set(profDDiskSpaceMB * BYTES_IN_KB);

    // Measure the size of each file we are going to backup.
    for (let resourceClass of this.#resources.values()) {
      try {
        await new resourceClass().measure(PathUtils.profileDir);
      } catch (e) {
        lazy.logConsole.error(
          `Failed to measure for resource: ${resourceClass.key}`,
          e
        );
      }
    }
  }
}

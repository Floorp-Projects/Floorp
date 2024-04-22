/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as DefaultBackupResources from "resource:///modules/backup/BackupResources.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "logConsole", function () {
  return console.createInstance({
    prefix: "BackupService",
    maxLogLevel: Services.prefs.getBoolPref("browser.backup.log", false)
      ? "Debug"
      : "Warn",
  });
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

ChromeUtils.defineESModuleGetters(lazy, {
  JsonSchemaValidator:
    "resource://gre/modules/components-utils/JsonSchemaValidator.sys.mjs",
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
   * The name of the backup manifest file.
   *
   * @type {string}
   */
  static get MANIFEST_FILE_NAME() {
    return "backup-manifest.json";
  }

  /**
   * The current schema version of the backup manifest that this BackupService
   * uses when creating a backup.
   *
   * @type {number}
   */
  static get MANIFEST_SCHEMA_VERSION() {
    return 1;
  }

  /**
   * A promise that resolves to the schema for the backup manifest that this
   * BackupService uses when creating a backup. This should be accessed via
   * the `MANIFEST_SCHEMA` static getter.
   *
   * @type {Promise<object>}
   */
  static #manifestSchemaPromise = null;

  /**
   * The current schema version of the backup manifest that this BackupService
   * uses when creating a backup.
   *
   * @type {Promise<object>}
   */
  static get MANIFEST_SCHEMA() {
    if (!BackupService.#manifestSchemaPromise) {
      let schemaURL = `chrome://browser/content/backup/BackupManifest.${BackupService.MANIFEST_SCHEMA_VERSION}.schema.json`;
      BackupService.#manifestSchemaPromise = fetch(schemaURL).then(response =>
        response.json()
      );
    }

    return BackupService.#manifestSchemaPromise;
  }

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
      let manifest = this.#createBackupManifest();

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
          lazy.logConsole.debug(
            `Backing up resource with key ${resourceClass.key}. ` +
              `Requires encryption: ${resourceClass.requiresEncryption}`
          );
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
          manifest.resources[resourceClass.key] = manifestEntry;
        } catch (e) {
          lazy.logConsole.error(
            `Failed to backup resource: ${resourceClass.key}`,
            e
          );
        }
      }

      // Ensure that the manifest abides by the current schema, and log
      // an error if somehow it doesn't. We'll want to collect telemetry for
      // this case to make sure it's not happening in the wild. We debated
      // throwing an exception here too, but that's not meaningfully better
      // than creating a backup that's not schema-compliant. At least in this
      // case, a user so-inclined could theoretically repair the manifest
      // to make it valid.
      let manifestSchema = await BackupService.MANIFEST_SCHEMA;
      let schemaValidationResult = lazy.JsonSchemaValidator.validate(
        manifest,
        manifestSchema
      );
      if (!schemaValidationResult.valid) {
        lazy.logConsole.error(
          "Backup manifest does not conform to schema:",
          manifest,
          manifestSchema,
          schemaValidationResult
        );
        // TODO: Collect telemetry for this case. (bug 1891817)
      }

      // Write the manifest to the staging folder.
      let manifestPath = PathUtils.join(
        stagingPath,
        BackupService.MANIFEST_FILE_NAME
      );
      await IOUtils.writeJSON(manifestPath, manifest);

      await this.#finalizeStagingFolder(stagingPath);
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
   * Renames the staging folder to an ISO 8601 date string with dashes replacing colons and fractional seconds stripped off.
   * The ISO date string should be formatted from YYYY-MM-DDTHH:mm:ss.sssZ to YYYY-MM-DDTHH-mm-ssZ
   *
   * @param {string} stagingPath
   *  The path to the populated staging folder.
   */
  async #finalizeStagingFolder(stagingPath) {
    if (!(await IOUtils.exists(stagingPath))) {
      // If we somehow can't find the specified staging folder, cancel this step.
      lazy.logConsole.error(
        `Failed to finalize staging folder. Cannot find ${stagingPath}.`
      );
      return;
    }

    try {
      lazy.logConsole.debug("Finalizing and renaming staging folder");
      let currentDateISO = new Date().toISOString();
      // First strip the fractional seconds
      let dateISOStripped = currentDateISO.replace(/\.\d+\Z$/, "Z");
      // Now replace all colons with dashes
      let dateISOFormatted = dateISOStripped.replaceAll(":", "-");

      let stagingPathParent = PathUtils.parent(stagingPath);
      let renamedBackupPath = PathUtils.join(
        stagingPathParent,
        dateISOFormatted
      );
      await IOUtils.move(stagingPath, renamedBackupPath);

      let existingBackups = await IOUtils.getChildren(stagingPathParent);

      /**
       * Bug 1892532: for now, we only support a single backup file.
       * If there are other pre-existing backup folders, delete them.
       */
      for (let existingBackupPath of existingBackups) {
        if (existingBackupPath !== renamedBackupPath) {
          await IOUtils.remove(existingBackupPath, {
            recursive: true,
          });
        }
      }
    } catch (e) {
      lazy.logConsole.error(
        `Something went wrong while finalizing the staging folder. ${e}`
      );
    }
  }

  /**
   * Creates and returns a backup manifest object with an empty resources
   * property.
   *
   * @returns {object}
   */
  #createBackupManifest() {
    let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
      Ci.nsIToolkitProfileService
    );
    let profileName;
    if (!profileSvc.currentProfile) {
      // We're probably running on a local build or in some special configuration.
      // Let's pull in a profile name from the profile directory.
      let profileFolder = PathUtils.split(PathUtils.profileDir).at(-1);
      profileName = profileFolder.substring(profileFolder.indexOf(".") + 1);
    } else {
      profileName = profileSvc.currentProfile.name;
    }

    return {
      version: BackupService.MANIFEST_SCHEMA_VERSION,
      meta: {
        date: new Date().toISOString(),
        appName: AppConstants.MOZ_APP_NAME,
        appVersion: AppConstants.MOZ_APP_VERSION,
        buildID: AppConstants.MOZ_BUILDID,
        profileName,
        machineName: lazy.fxAccounts.device.getLocalName(),
        osName: Services.sysinfo.getProperty("name"),
        osVersion: Services.sysinfo.getProperty("version"),
      },
      resources: {},
    };
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

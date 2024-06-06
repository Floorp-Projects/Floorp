/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as DefaultBackupResources from "resource:///modules/backup/BackupResources.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const SCHEDULED_BACKUPS_ENABLED_PREF_NAME = "browser.backup.scheduled.enabled";
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
  ArchiveEncryptionState:
    "resource:///modules/backup/ArchiveEncryptionState.sys.mjs",
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  JsonSchemaValidator:
    "resource://gre/modules/components-utils/JsonSchemaValidator.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "ZipWriter", () =>
  Components.Constructor("@mozilla.org/zipwriter;1", "nsIZipWriter", "open")
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "scheduledBackupsPref",
  SCHEDULED_BACKUPS_ENABLED_PREF_NAME,
  false,
  function onUpdateScheduledBackups(_pref, _prevVal, newVal) {
    let bs = BackupService.get();
    if (bs) {
      bs.onUpdateScheduledBackups(newVal);
    }
  }
);

/**
 * The BackupService class orchestrates the scheduling and creation of profile
 * backups. It also does most of the heavy lifting for the restoration of a
 * profile backup.
 */
export class BackupService extends EventTarget {
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
   * Set to true if a backup is currently in progress. Causes stateUpdate()
   * to be called.
   *
   * @see BackupService.stateUpdate()
   * @param {boolean} val
   *   True if a backup is in progress.
   */
  set #backupInProgress(val) {
    if (this.#_state.backupInProgress != val) {
      this.#_state.backupInProgress = val;
      this.stateUpdate();
    }
  }

  /**
   * True if a backup is currently in progress.
   *
   * @type {boolean}
   */
  get #backupInProgress() {
    return this.#_state.backupInProgress;
  }

  /**
   * Dispatches an event to let listeners know that the BackupService state
   * object has been updated.
   */
  stateUpdate() {
    this.dispatchEvent(new CustomEvent("BackupService:StateUpdate"));
  }

  /**
   * An object holding the current state of the BackupService instance, for
   * the purposes of representing it in the user interface. Ideally, this would
   * be named #state instead of #_state, but sphinx-js seems to be fairly
   * unhappy with that coupled with the ``state`` getter.
   *
   * @type {object}
   */
  #_state = {
    backupFilePath: "Documents", // TODO: make save location configurable (bug 1895943)
    backupInProgress: false,
    scheduledBackupsEnabled: lazy.scheduledBackupsPref,
    encryptionEnabled: false,
  };

  /**
   * A Promise that will resolve once the postRecovery steps are done. It will
   * also resolve if postRecovery steps didn't need to run.
   *
   * @see BackupService.checkForPostRecovery()
   * @type {Promise<undefined>}
   */
  #postRecoveryPromise;

  /**
   * The resolving function for #postRecoveryPromise, which should be called
   * by checkForPostRecovery() before exiting.
   *
   * @type {Function}
   */
  #postRecoveryResolver;

  /**
   * The currently used ArchiveEncryptionState. Callers should use
   * loadEncryptionState() instead, to ensure that any pre-serialized
   * encryption state has been read in and deserialized.
   *
   * This member can be in 3 states:
   *
   * 1. undefined - no attempt has been made to load encryption state from
   *    disk yet.
   * 2. null - encryption is not enabled.
   * 3. ArchiveEncryptionState - encryption is enabled.
   *
   * @see BackupService.loadEncryptionState()
   * @type {ArchiveEncryptionState|null|undefined}
   */
  #encState = undefined;

  /**
   * The name of the folder within the profile folder where this service reads
   * and writes state to.
   *
   * @type {string}
   */
  static get PROFILE_FOLDER_NAME() {
    return "backups";
  }

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
      BackupService.#manifestSchemaPromise = BackupService._getSchemaForVersion(
        BackupService.MANIFEST_SCHEMA_VERSION
      );
    }

    return BackupService.#manifestSchemaPromise;
  }

  /**
   * The name of the post recovery file written into the newly created profile
   * directory just after a profile is recovered from a backup.
   *
   * @type {string}
   */
  static get POST_RECOVERY_FILE_NAME() {
    return "post-recovery.json";
  }

  /**
   * The name of the serialized ArchiveEncryptionState that is written to disk
   * if encryption is enabled.
   *
   * @type {string}
   */
  static get ARCHIVE_ENCRYPTION_STATE_FILE() {
    return "enc-state.json";
  }

  /**
   * Returns the schema for the backup manifest for a given version.
   *
   * This should really be #getSchemaForVersion, but for some reason,
   * sphinx-js seems to choke on static async private methods (bug 1893362).
   * We workaround this breakage by using the `_` prefix to indicate that this
   * method should be _considered_ private, and ask that you not use this method
   * outside of this class. The sphinx-js issue is tracked at
   * https://github.com/mozilla/sphinx-js/issues/240.
   *
   * @private
   * @param {number} version
   *   The version of the schema to return.
   * @returns {Promise<object>}
   */
  static async _getSchemaForVersion(version) {
    let schemaURL = `chrome://browser/content/backup/BackupManifest.${version}.schema.json`;
    let response = await fetch(schemaURL);
    return response.json();
  }

  /**
   * The level of Zip compression to use on the zipped staging folder.
   *
   * @type {number}
   */
  static get COMPRESSION_LEVEL() {
    return Ci.nsIZipWriter.COMPRESSION_BEST;
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

    this.#instance.checkForPostRecovery().then(() => {
      this.#instance.takeMeasurements();
    });

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
   * @param {object} [backupResources=DefaultBackupResources]
   *   Object containing BackupResource classes to associate with this service.
   */
  constructor(backupResources = DefaultBackupResources) {
    super();
    lazy.logConsole.debug("Instantiated");

    for (const resourceName in backupResources) {
      let resource = backupResources[resourceName];
      this.#resources.set(resource.key, resource);
    }

    let { promise, resolve } = Promise.withResolvers();
    this.#postRecoveryPromise = promise;
    this.#postRecoveryResolver = resolve;
  }

  /**
   * Returns a reference to a Promise that will resolve with undefined once
   * postRecovery steps have had a chance to run. This will also be resolved
   * with undefined if no postRecovery steps needed to be run.
   *
   * @see BackupService.checkForPostRecovery()
   * @returns {Promise<undefined>}
   */
  get postRecoveryComplete() {
    return this.#postRecoveryPromise;
  }

  /**
   * Returns a state object describing the state of the BackupService for the
   * purposes of representing it in the user interface. The returned state
   * object is immutable.
   *
   * @type {object}
   */
  get state() {
    return Object.freeze(structuredClone(this.#_state));
  }

  /**
   * @typedef {object} CreateBackupResult
   * @property {string} stagingPath
   *   The staging path for where the backup was created.
   */

  /**
   * Create a backup of the user's profile.
   *
   * @param {object} [options]
   *   Options for the backup.
   * @param {string} [options.profilePath=PathUtils.profileDir]
   *   The path to the profile to backup. By default, this is the current
   *   profile.
   * @returns {Promise<CreateBackupResult|null>}
   *   A promise that resolves to an object containing the path to the staging
   *   folder where the backup was created, or null if the backup failed.
   */
  async createBackup({ profilePath = PathUtils.profileDir } = {}) {
    // createBackup does not allow re-entry or concurrent backups.
    if (this.#backupInProgress) {
      lazy.logConsole.warn("Backup attempt already in progress");
      return null;
    }

    this.#backupInProgress = true;

    try {
      lazy.logConsole.debug(`Creating backup for profile at ${profilePath}`);
      let manifest = await this.#createBackupManifest();

      // First, check to see if a `backups` directory already exists in the
      // profile.
      let backupDirPath = PathUtils.join(
        profilePath,
        BackupService.PROFILE_FOLDER_NAME
      );
      lazy.logConsole.debug("Creating backups folder");

      // ignoreExisting: true is the default, but we're being explicit that it's
      // okay if this folder already exists.
      await IOUtils.makeDirectory(backupDirPath, { ignoreExisting: true });

      let stagingPath = await this.#prepareStagingFolder(backupDirPath);

      // Sort resources be priority.
      let sortedResources = Array.from(this.#resources.values()).sort(
        (a, b) => {
          return b.priority - a.priority;
        }
      );

      let encState = await this.loadEncryptionState(profilePath);
      let encryptionEnabled = !!encState;
      lazy.logConsole.debug("Encryption enabled: ", encryptionEnabled);

      // Perform the backup for each resource.
      for (let resourceClass of sortedResources) {
        try {
          lazy.logConsole.debug(
            `Backing up resource with key ${resourceClass.key}. ` +
              `Requires encryption: ${resourceClass.requiresEncryption}`
          );

          if (resourceClass.requiresEncryption && !encryptionEnabled) {
            lazy.logConsole.debug(
              "Encryption is not currently enabled. Skipping."
            );
            continue;
          }

          let resourcePath = PathUtils.join(stagingPath, resourceClass.key);
          await IOUtils.makeDirectory(resourcePath);

          // `backup` on each BackupResource should return us a ManifestEntry
          // that we eventually write to a JSON manifest file, but for now,
          // we're just going to log it.
          let manifestEntry = await new resourceClass().backup(
            resourcePath,
            profilePath
          );

          if (manifestEntry === undefined) {
            lazy.logConsole.error(
              `Backup of resource with key ${resourceClass.key} returned undefined
              as its ManifestEntry instead of null or an object`
            );
          } else {
            lazy.logConsole.debug(
              `Backup of resource with key ${resourceClass.key} completed`,
              manifestEntry
            );
            manifest.resources[resourceClass.key] = manifestEntry;
          }
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

      let renamedStagingPath = await this.#finalizeStagingFolder(stagingPath);
      lazy.logConsole.log(
        "Wrote backup to staging directory at ",
        renamedStagingPath
      );

      let compressedStagingPath = await this.#compressStagingFolder(
        renamedStagingPath,
        backupDirPath
      );

      return { stagingPath: renamedStagingPath, compressedStagingPath };
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
   * Compresses a staging folder into a Zip file. If a pre-existing Zip file
   * for a staging folder resides in destFolderPath, it is overwritten. The
   * Zip file will have the same name as the stagingPath folder, with `.zip`
   * as the extension.
   *
   * @param {string} stagingPath
   *   The path to the staging folder to be compressed.
   * @param {string} destFolderPath
   *   The parent folder to write the Zip file to.
   * @returns {Promise<string>}
   *   Resolves with the path to the created Zip file.
   */
  async #compressStagingFolder(stagingPath, destFolderPath) {
    const PR_RDWR = 0x04;
    const PR_CREATE_FILE = 0x08;
    const PR_TRUNCATE = 0x20;

    let archivePath = PathUtils.join(
      destFolderPath,
      `${PathUtils.filename(stagingPath)}.zip`
    );
    let archiveFile = await IOUtils.getFile(archivePath);

    let writer = new lazy.ZipWriter(
      archiveFile,
      PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE
    );

    lazy.logConsole.log("Compressing staging folder to ", archivePath);
    let rootPathNSIFile = await IOUtils.getDirectory(stagingPath);
    await this.#compressChildren(rootPathNSIFile, stagingPath, writer);
    await new Promise(resolve => {
      let observer = {
        onStartRequest(_request) {
          lazy.logConsole.debug("Starting to write out archive file");
        },
        onStopRequest(_request, status) {
          lazy.logConsole.log("Done writing archive file");
          resolve(status);
        },
      };
      writer.processQueue(observer, null);
    });
    writer.close();

    return archivePath;
  }

  /**
   * A helper function for #compressStagingFolder that iterates through a
   * directory, and adds each file to a nsIZipWriter. For each directory it
   * finds, it recurses.
   *
   * @param {nsIFile} rootPathNSIFile
   *   An nsIFile pointing at the root of the folder being compressed.
   * @param {string} parentPath
   *   The path to the folder whose children should be iterated.
   * @param {nsIZipWriter} writer
   *   The writer to add all of the children to.
   * @returns {Promise<undefined>}
   */
  async #compressChildren(rootPathNSIFile, parentPath, writer) {
    let children = await IOUtils.getChildren(parentPath);
    for (let childPath of children) {
      let childState = await IOUtils.stat(childPath);
      if (childState.type == "directory") {
        await this.#compressChildren(rootPathNSIFile, childPath, writer);
      } else {
        let childFile = await IOUtils.getFile(childPath);
        // nsIFile.getRelativePath returns paths using the "/" separator,
        // regardless of which platform we're on. That's handy, because this
        // is the same separator that nsIZipWriter expects for entries.
        let pathRelativeToRoot = childFile.getRelativePath(rootPathNSIFile);
        writer.addEntryFile(
          pathRelativeToRoot,
          BackupService.COMPRESSION_LEVEL,
          childFile,
          true
        );
      }
    }
  }

  /**
   * Renames the staging folder to an ISO 8601 date string with dashes replacing colons and fractional seconds stripped off.
   * The ISO date string should be formatted from YYYY-MM-DDTHH:mm:ss.sssZ to YYYY-MM-DDTHH-mm-ssZ
   *
   * @param {string} stagingPath
   *   The path to the populated staging folder.
   * @returns {Promise<string|null>}
   *   The path to the renamed staging folder, or null if the stagingPath was
   *   not pointing to a valid folder.
   */
  async #finalizeStagingFolder(stagingPath) {
    if (!(await IOUtils.exists(stagingPath))) {
      // If we somehow can't find the specified staging folder, cancel this step.
      lazy.logConsole.error(
        `Failed to finalize staging folder. Cannot find ${stagingPath}.`
      );
      return null;
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
       * If there are other pre-existing backup folders, delete them - but don't
       * delete anything that doesn't match the backup folder naming scheme.
       */
      let expectedFormatRegex = /\d{4}(-\d{2}){2}T(\d{2}-){2}\d{2}Z/;
      for (let existingBackupPath of existingBackups) {
        if (
          existingBackupPath !== renamedBackupPath &&
          existingBackupPath.match(expectedFormatRegex)
        ) {
          await IOUtils.remove(existingBackupPath, {
            recursive: true,
          });
        }
      }
      return renamedBackupPath;
    } catch (e) {
      lazy.logConsole.error(
        `Something went wrong while finalizing the staging folder. ${e}`
      );
      throw e;
    }
  }

  /**
   * Creates and resolves with a backup manifest object with an empty resources
   * property.
   *
   * @returns {Promise<object>}
   */
  async #createBackupManifest() {
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

    let meta = {
      date: new Date().toISOString(),
      appName: AppConstants.MOZ_APP_NAME,
      appVersion: AppConstants.MOZ_APP_VERSION,
      buildID: AppConstants.MOZ_BUILDID,
      profileName,
      machineName: lazy.fxAccounts.device.getLocalName(),
      osName: Services.sysinfo.getProperty("name"),
      osVersion: Services.sysinfo.getProperty("version"),
      legacyClientID: await lazy.ClientID.getClientID(),
    };

    let fxaState = lazy.UIState.get();
    if (fxaState.status == lazy.UIState.STATUS_SIGNED_IN) {
      meta.accountID = fxaState.uid;
      meta.accountEmail = fxaState.email;
    }

    return {
      version: BackupService.MANIFEST_SCHEMA_VERSION,
      meta,
      resources: {},
    };
  }

  /**
   * Given a decompressed backup archive at recoveryPath, this method does the
   * following:
   *
   * 1. Reads in the backup manifest from the archive and ensures that it is
   *    valid.
   * 2. Creates a new named profile directory using the same name as the one
   *    found in the backup manifest, but with a different prefix.
   * 3. Iterates over each resource in the manifest and calls the recover()
   *    method on each found BackupResource, passing in the associated
   *    ManifestEntry from the backup manifest, and collects any post-recovery
   *    data from those resources.
   * 4. Writes a `post-recovery.json` file into the newly created profile
   *    directory.
   * 5. Returns the name of the newly created profile directory.
   *
   * @param {string} recoveryPath
   *   The path to the decompressed backup archive on the file system.
   * @param {boolean} [shouldLaunch=false]
   *   An optional argument that specifies whether an instance of the app
   *   should be launched with the newly recovered profile after recovery is
   *   complete.
   * @param {string} [profileRootPath=null]
   *   An optional argument that specifies the root directory where the new
   *   profile directory should be created. If not provided, the default
   *   profile root directory will be used. This is primarily meant for
   *   testing.
   * @returns {Promise<nsIToolkitProfile>}
   *   The nsIToolkitProfile that was created for the recovered profile.
   * @throws {Exception}
   *   In the event that recovery somehow failed.
   */
  async recoverFromBackup(
    recoveryPath,
    shouldLaunch = false,
    profileRootPath = null
  ) {
    lazy.logConsole.debug("Recovering from backup at ", recoveryPath);

    try {
      // Read in the backup manifest.
      let manifestPath = PathUtils.join(
        recoveryPath,
        BackupService.MANIFEST_FILE_NAME
      );
      let manifest = await IOUtils.readJSON(manifestPath);
      if (!manifest.version) {
        throw new Error("Backup manifest version not found");
      }

      if (manifest.version > BackupService.MANIFEST_SCHEMA_VERSION) {
        throw new Error(
          "Cannot recover from a manifest newer than the current schema version"
        );
      }

      // Make sure that it conforms to the schema.
      let manifestSchema = await BackupService._getSchemaForVersion(
        manifest.version
      );
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
        throw new Error("Cannot recover from an invalid backup manifest");
      }

      // In the future, if we ever bump the MANIFEST_SCHEMA_VERSION and need to
      // do any special behaviours to interpret older schemas, this is where we
      // can do that, and we can remove this comment.

      let meta = manifest.meta;

      // Okay, we have a valid backup-manifest.json. Let's create a new profile
      // and start invoking the recover() method on each BackupResource.
      let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
        Ci.nsIToolkitProfileService
      );
      let profile = profileSvc.createUniqueProfile(
        profileRootPath ? await IOUtils.getDirectory(profileRootPath) : null,
        meta.profileName
      );

      let postRecovery = {};

      // Iterate over each resource in the manifest and call recover() on each
      // associated BackupResource.
      for (let resourceKey in manifest.resources) {
        let manifestEntry = manifest.resources[resourceKey];
        let resourceClass = this.#resources.get(resourceKey);
        if (!resourceClass) {
          lazy.logConsole.error(
            `No BackupResource found for key ${resourceKey}`
          );
          continue;
        }

        try {
          lazy.logConsole.debug(
            `Restoring resource with key ${resourceKey}. ` +
              `Requires encryption: ${resourceClass.requiresEncryption}`
          );
          let resourcePath = PathUtils.join(recoveryPath, resourceKey);
          let postRecoveryEntry = await new resourceClass().recover(
            manifestEntry,
            resourcePath,
            profile.rootDir.path
          );
          postRecovery[resourceKey] = postRecoveryEntry;
        } catch (e) {
          lazy.logConsole.error(
            `Failed to recover resource: ${resourceKey}`,
            e
          );
        }
      }

      // Make sure that a legacy telemetry client ID exists and is written to
      // disk.
      let clientID = await lazy.ClientID.getClientID();
      lazy.logConsole.debug("Current client ID: ", clientID);
      // Next, copy over the legacy telemetry client ID state from the currently
      // running profile. The newly created profile that we're recovering into
      // should inherit this client ID.
      const TELEMETRY_STATE_FILENAME = "state.json";
      const TELEMETRY_STATE_FOLDER = "datareporting";
      await IOUtils.makeDirectory(
        PathUtils.join(profile.rootDir.path, TELEMETRY_STATE_FOLDER)
      );
      await IOUtils.copy(
        /* source */
        PathUtils.join(
          PathUtils.profileDir,
          TELEMETRY_STATE_FOLDER,
          TELEMETRY_STATE_FILENAME
        ),
        /* destination */
        PathUtils.join(
          profile.rootDir.path,
          TELEMETRY_STATE_FOLDER,
          TELEMETRY_STATE_FILENAME
        )
      );

      let postRecoveryPath = PathUtils.join(
        profile.rootDir.path,
        BackupService.POST_RECOVERY_FILE_NAME
      );
      await IOUtils.writeJSON(postRecoveryPath, postRecovery);

      profileSvc.flush();

      if (shouldLaunch) {
        Services.startup.createInstanceWithProfile(profile);
      }

      return profile;
    } catch (e) {
      lazy.logConsole.error(
        "Failed to recover from backup at ",
        recoveryPath,
        e
      );
      throw e;
    }
  }

  /**
   * Checks for the POST_RECOVERY_FILE_NAME in the current profile directory.
   * If one exists, instantiates any relevant BackupResource's, and calls
   * postRecovery() on them with the appropriate entry from the file. Once
   * this is done, deletes the file.
   *
   * The file is deleted even if one of the postRecovery() steps rejects or
   * fails.
   *
   * This function resolves silently if the POST_RECOVERY_FILE_NAME file does
   * not exist, which should be the majority of cases.
   *
   * @param {string} [profilePath=PathUtils.profileDir]
   *  The profile path to look for the POST_RECOVERY_FILE_NAME file. Defaults
   *  to the current profile.
   * @returns {Promise<undefined>}
   */
  async checkForPostRecovery(profilePath = PathUtils.profileDir) {
    lazy.logConsole.debug(`Checking for post-recovery file in ${profilePath}`);
    let postRecoveryFile = PathUtils.join(
      profilePath,
      BackupService.POST_RECOVERY_FILE_NAME
    );

    if (!(await IOUtils.exists(postRecoveryFile))) {
      lazy.logConsole.debug("Did not find post-recovery file.");
      this.#postRecoveryResolver();
      return;
    }

    lazy.logConsole.debug("Found post-recovery file. Loading...");

    try {
      let postRecovery = await IOUtils.readJSON(postRecoveryFile);
      for (let resourceKey in postRecovery) {
        let postRecoveryEntry = postRecovery[resourceKey];
        let resourceClass = this.#resources.get(resourceKey);
        if (!resourceClass) {
          lazy.logConsole.error(
            `Invalid resource for post-recovery step: ${resourceKey}`
          );
          continue;
        }

        lazy.logConsole.debug(`Running post-recovery step for ${resourceKey}`);
        await new resourceClass().postRecovery(postRecoveryEntry);
        lazy.logConsole.debug(`Done post-recovery step for ${resourceKey}`);
      }
    } finally {
      await IOUtils.remove(postRecoveryFile, { ignoreAbsent: true });
      this.#postRecoveryResolver();
    }
  }

  /**
   * Sets browser.backup.scheduled.enabled to true or false.
   *
   * @param { boolean } shouldEnableScheduledBackups true if scheduled backups should be enabled. Else, false.
   */
  setScheduledBackups(shouldEnableScheduledBackups) {
    Services.prefs.setBoolPref(
      SCHEDULED_BACKUPS_ENABLED_PREF_NAME,
      shouldEnableScheduledBackups
    );
  }

  /**
   * Updates scheduledBackupsEnabled in the backup service state. Should be called every time
   * the value for browser.backup.scheduled.enabled changes.
   *
   * @param {boolean} isScheduledBackupsEnabled True if scheduled backups are enabled. Else false.
   */
  onUpdateScheduledBackups(isScheduledBackupsEnabled) {
    if (this.#_state.scheduledBackupsEnabled != isScheduledBackupsEnabled) {
      lazy.logConsole.debug(
        "Updating scheduled backups",
        isScheduledBackupsEnabled
      );
      this.#_state.scheduledBackupsEnabled = isScheduledBackupsEnabled;
      this.stateUpdate();
    }
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

  /**
   * The internal promise that is created on the first call to
   * loadEncryptionState.
   *
   * @type {Promise}
   */
  #loadEncryptionStatePromise = null;

  /**
   * Returns the current ArchiveEncryptionState. This method will only attempt
   * to read the state from the disk the first time it is called.
   *
   * @param {string} [profilePath=PathUtils.profileDir]
   *   The profile path where the encryption state might exist. This is only
   *   used for testing.
   * @returns {Promise<ArchiveEncryptionState>}
   */
  loadEncryptionState(profilePath = PathUtils.profileDir) {
    if (this.#encState !== undefined) {
      return Promise.resolve(this.#encState);
    }

    // This little dance makes it so that we only attempt to read the state off
    // of the disk the first time `loadEncryptionState` is called. Any
    // subsequent calls will await this same promise, OR, after the state has
    // been read in, they'll just get the #encState which is set after the
    // state has been read in.
    if (!this.#loadEncryptionStatePromise) {
      this.#loadEncryptionStatePromise = (async () => {
        // Default this to null here - that way, if we fail to read it in,
        // the null will indicate that we have at least _tried_ to load the
        // state.
        let encState = null;
        let encStateFile = PathUtils.join(
          profilePath,
          BackupService.PROFILE_FOLDER_NAME,
          BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
        );

        // Try to read in any pre-existing encryption state. If that fails,
        // we fallback to not encrypting, and only backing up non-sensitive data.
        try {
          if (await IOUtils.exists(encStateFile)) {
            let stateObject = await IOUtils.readJSON(encStateFile);
            ({ instance: encState } =
              await lazy.ArchiveEncryptionState.initialize(stateObject));
          }
        } catch (e) {
          lazy.logConsole.error(
            "Failed to read / deserialize archive encryption state file: ",
            e
          );
          // TODO: This kind of error might be worth collecting telemetry on.
        }

        this.#_state.encryptionEnabled = !!encState;
        this.stateUpdate();

        this.#encState = encState;
        return encState;
      })();
    }

    return this.#loadEncryptionStatePromise;
  }

  /**
   * Enables encryption for backups, allowing sensitive data to be backed up.
   * Throws if encryption is already enabled. After enabling encryption, that
   * state is written to disk.
   *
   * @throws Exception
   * @param {string} password
   *   A non-blank password ("recovery code") that can be used to derive keys
   *   for encrypting the backup.
   * @param {string} [profilePath=PathUtils.profileDir]
   *   The profile path where the encryption state will be written. This is only
   *   used for testing.
   */
  async enableEncryption(password, profilePath = PathUtils.profileDir) {
    lazy.logConsole.debug("Enabling encryption.");
    let encState = await this.loadEncryptionState(profilePath);
    if (encState) {
      throw new Error("Encryption is already enabled.");
    }

    if (!password) {
      throw new Error("Cannot supply a blank password.");
    }

    if (password.length < 8) {
      throw new Error("Password must be at least 8 characters.");
    }

    // TODO: Enforce other password rules here, such as ensuring that the
    // password is not considered common.
    ({ instance: encState } = await lazy.ArchiveEncryptionState.initialize(
      password
    ));
    if (!encState) {
      throw new Error("Failed to construct ArchiveEncryptionState");
    }

    this.#encState = encState;

    let encStateFile = PathUtils.join(
      profilePath,
      BackupService.PROFILE_FOLDER_NAME,
      BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
    );

    let stateObj = await encState.serialize();
    await IOUtils.writeJSON(encStateFile, stateObj);

    this.#_state.encryptionEnabled = true;
    this.stateUpdate();
  }

  /**
   * Disables encryption of backups. Throws is encryption is already disabled.
   *
   * @throws Exception
   * @param {string} [profilePath=PathUtils.profileDir]
   *   The profile path where the encryption state exists. This is only used for
   *   testing.
   * @returns {Promise<undefined>}
   */
  async disableEncryption(profilePath = PathUtils.profileDir) {
    lazy.logConsole.debug("Disabling encryption.");
    let encState = await this.loadEncryptionState(profilePath);
    if (!encState) {
      throw new Error("Encryption is already disabled.");
    }

    let encStateFile = PathUtils.join(
      profilePath,
      BackupService.PROFILE_FOLDER_NAME,
      BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
    );
    // It'd be pretty strange, but not impossible, for something else to have
    // gotten rid of the encryption state file at this point. We'll ignore it
    // if that's the case.
    await IOUtils.remove(encStateFile, { ignoreAbsent: true });

    this.#encState = null;
    this.#_state.encryptionEnabled = false;
    this.stateUpdate();
  }
}

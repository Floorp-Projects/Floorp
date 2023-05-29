/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { E10SUtils } from "resource://gre/modules/E10SUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "gFluentStrings", function () {
  return new Localization([
    "branding/brand.ftl",
    "browser/migrationWizard.ftl",
  ]);
});

ChromeUtils.defineESModuleGetters(lazy, {
  InternalTestingProfileMigrator:
    "resource:///modules/InternalTestingProfileMigrator.sys.mjs",
  MigrationWizardConstants:
    "chrome://browser/content/migration/migration-wizard-constants.mjs",
  PasswordFileMigrator: "resource:///modules/FileMigrators.sys.mjs",
});

if (AppConstants.platform == "macosx") {
  ChromeUtils.defineESModuleGetters(lazy, {
    SafariProfileMigrator: "resource:///modules/SafariProfileMigrator.sys.mjs",
  });
}

XPCOMUtils.defineLazyModuleGetters(lazy, {
  LoginCSVImport: "resource://gre/modules/LoginCSVImport.jsm",
});

/**
 * This class is responsible for communicating with MigrationUtils to do the
 * actual heavy-lifting of any kinds of migration work, based on messages from
 * the associated MigrationWizardChild.
 */
export class MigrationWizardParent extends JSWindowActorParent {
  constructor() {
    super();
    Services.telemetry.setEventRecordingEnabled("browser.migration", true);
  }

  didDestroy() {
    Services.obs.notifyObservers(this, "MigrationWizard:Destroyed");
  }

  /**
   * General message handler function for messages received from the
   * associated MigrationWizardChild JSWindowActor.
   *
   * @param {ReceiveMessageArgument} message
   *   The message received from the MigrationWizardChild.
   * @returns {Promise}
   */
  async receiveMessage(message) {
    // Some belt-and-suspenders here, mainly because the migration-wizard
    // component can be embedded in less privileged content pages, so let's
    // make sure that any messages from content are coming from the privileged
    // about content process type.
    if (
      !this.browsingContext.currentWindowGlobal.isInProcess &&
      this.browsingContext.currentRemoteType !=
        E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE
    ) {
      throw new Error(
        "MigrationWizardParent: received message from the wrong content process type."
      );
    }

    switch (message.name) {
      case "GetAvailableMigrators": {
        let availableMigrators = [];
        for (const key of MigrationUtils.availableMigratorKeys) {
          availableMigrators.push(this.#getMigratorAndProfiles(key));
        }

        // Wait for all getMigrator calls to resolve in parallel
        let results = await Promise.all(availableMigrators);

        for (const migrator of MigrationUtils.availableFileMigrators.values()) {
          results.push(await this.#serializeFileMigrator(migrator));
        }

        // Each migrator might give us a single MigratorProfileInstance,
        // or an Array of them, so we flatten them out and filter out
        // any that ended up going wrong and returning null from the
        // #getMigratorAndProfiles call.
        let filteredResults = results
          .flat()
          .filter(result => result)
          .sort((a, b) => {
            return b.lastModifiedDate - a.lastModifiedDate;
          });

        for (let result of filteredResults) {
          Services.telemetry.keyedScalarAdd(
            "migration.discovered_migrators",
            result.key,
            1
          );
        }
        return filteredResults;
      }

      case "Migrate": {
        if (
          message.data.type ==
          lazy.MigrationWizardConstants.MIGRATOR_TYPES.BROWSER
        ) {
          await this.#doBrowserMigration(
            message.data.key,
            message.data.resourceTypes,
            message.data.profile,
            message.data.safariPasswordFilePath
          );
        } else if (
          message.data.type == lazy.MigrationWizardConstants.MIGRATOR_TYPES.FILE
        ) {
          let window = this.browsingContext.topChromeWindow;
          await this.#doFileMigration(window, message.data.key);
        }
        break;
      }

      case "CheckPermissions": {
        if (
          message.data.type ==
          lazy.MigrationWizardConstants.MIGRATOR_TYPES.BROWSER
        ) {
          let migrator = await MigrationUtils.getMigrator(message.data.key);
          return migrator.hasPermissions();
        }
        return true;
      }

      case "RequestSafariPermissions": {
        let safariMigrator = await MigrationUtils.getMigrator("safari");
        return safariMigrator.getPermissions(
          this.browsingContext.topChromeWindow
        );
      }

      case "SelectSafariPasswordFile": {
        return this.#selectSafariPasswordFile(
          this.browsingContext.topChromeWindow
        );
      }

      case "RecordEvent": {
        this.#recordEvent(message.data.type, message.data.args);
        break;
      }
    }

    return null;
  }

  /**
   * Used for recording telemetry in the migration wizard.
   *
   * @param {string} type
   *   The type of event being recorded.
   * @param {object} args
   *   The data to pass to telemetry when the event is recorded.
   */
  #recordEvent(type, args = null) {
    Services.telemetry.recordEvent(
      "browser.migration",
      type,
      "wizard",
      null,
      args
    );
  }

  /**
   * Gets the FileMigrator associated with the passed in key, and then opens
   * a native file picker configured for that migrator. Once the user selects
   * a file from the native file picker, this is then passed to the
   * FileMigrator.migrate method.
   *
   * As the migration occurs, this will send UpdateProgress messages to the
   * MigrationWizardChild to show the beginning and then the ending state of
   * the migration.
   *
   * @param {DOMWindow} window
   *   The window that the native file picker should be associated with. This
   *   cannot be null. See nsIFilePicker.init for more details.
   * @param {string} key
   *   The unique identification key for a file migrator.
   * @returns {Promise<undefined>}
   *   Resolves once the file migrator's migrate method has resolved.
   */
  async #doFileMigration(window, key) {
    let fileMigrator = MigrationUtils.getFileMigrator(key);
    let filePickerConfig = await fileMigrator.getFilePickerConfig();

    let { result, path } = await new Promise(resolve => {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(window, filePickerConfig.title, Ci.nsIFilePicker.modeOpen);

      for (let filter of filePickerConfig.filters) {
        fp.appendFilter(filter.title, filter.extensionPattern);
      }
      fp.appendFilters(Ci.nsIFilePicker.filterAll);
      fp.open(async fileOpenResult => {
        resolve({ result: fileOpenResult, path: fp.file.path });
      });
    });

    if (result == Ci.nsIFilePicker.returnCancel) {
      // If the user cancels out of the file picker, the migration wizard should
      // still be in the state that lets the user re-open the file picker if
      // they closed it by accident, so we don't have to do anything else here.
      return;
    }

    let progress = {};
    for (let resourceType of fileMigrator.displayedResourceTypes) {
      progress[resourceType] = {
        inProgress: true,
        message: "",
      };
    }

    let [progressHeaderString, successHeaderString] =
      await lazy.gFluentStrings.formatValues([
        fileMigrator.progressHeaderL10nID,
        fileMigrator.successHeaderL10nID,
      ]);

    this.sendAsyncMessage("UpdateFileImportProgress", {
      title: progressHeaderString,
      progress,
    });
    let migrationResult = await fileMigrator.migrate(path);
    let successProgress = {};
    for (let resourceType in migrationResult) {
      successProgress[resourceType] = {
        inProgress: false,
        message: migrationResult[resourceType],
      };
    }
    this.sendAsyncMessage("UpdateFileImportProgress", {
      title: successHeaderString,
      progress: successProgress,
    });
  }

  /**
   * Handles a request to open a native file picker to get the path to a
   * CSV file that contains passwords exported from Safari. The returned
   * path is in the form of a string, or `null` if the user cancelled the
   * native picker.
   *
   * @param {DOMWindow} window
   *   The window that the native file picker should be associated with. This
   *   cannot be null. See nsIFilePicker.init for more details.
   * @returns {Promise<string|null>}
   */
  async #selectSafariPasswordFile(window) {
    let fileMigrator = MigrationUtils.getFileMigrator(
      lazy.PasswordFileMigrator.key
    );
    let filePickerConfig = await fileMigrator.getFilePickerConfig();

    let { result, path } = await new Promise(resolve => {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(window, filePickerConfig.title, Ci.nsIFilePicker.modeOpen);

      for (let filter of filePickerConfig.filters) {
        fp.appendFilter(filter.title, filter.extensionPattern);
      }
      fp.appendFilters(Ci.nsIFilePicker.filterAll);
      fp.open(async fileOpenResult => {
        resolve({ result: fileOpenResult, path: fp.file.path });
      });
    });

    if (result == Ci.nsIFilePicker.returnCancel) {
      // If the user cancels out of the file picker, the migration wizard should
      // still be in the state that lets the user re-open the file picker if
      // they closed it by accident, so we don't have to do anything else here.
      return null;
    }

    return path;
  }

  /**
   * Calls into MigrationUtils to perform a migration given the parameters
   * sent via the wizard.
   *
   * @param {string} migratorKey
   *   The unique identification key for a migrator.
   * @param {string[]} resourceTypeNames
   *   An array of strings, where each string represents a resource type
   *   that can be imported for this migrator and profile. The strings
   *   should be one of the key values of
   *   MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
   * @param {object|null} profileObj
   *   A description of the user profile that the migrator can import.
   * @param {string} profileObj.id
   *   A unique ID for the user profile.
   * @param {string} profileObj.name
   *   The display name for the user profile.
   * @param {string} [safariPasswordFilePath=null]
   *   An optional string argument that points to the path of a passwords
   *   export file from Safari. This file will have password imported from if
   *   supplied. This argument is ignored if the migratorKey is not for the
   *   Safari browser.
   * @returns {Promise<undefined>}
   *   Resolves once the Migration:Ended observer notification has fired.
   */
  async #doBrowserMigration(
    migratorKey,
    resourceTypeNames,
    profileObj,
    safariPasswordFilePath = null
  ) {
    let migrator = await MigrationUtils.getMigrator(migratorKey);
    let availableResourceTypes = await migrator.getMigrateData(profileObj);
    let resourceTypesToMigrate = 0;
    let progress = {};

    for (let resourceTypeName of resourceTypeNames) {
      let resourceType = MigrationUtils.resourceTypes[resourceTypeName];
      if (availableResourceTypes & resourceType) {
        resourceTypesToMigrate |= resourceType;
        progress[resourceTypeName] = {
          inProgress: true,
          message: "",
        };
      }
    }

    if (
      migratorKey == lazy.SafariProfileMigrator?.key &&
      safariPasswordFilePath
    ) {
      // The caller supplied a password export file for Safari. We're going to
      // pretend that there was a PASSWORDS resource for Safari to represent
      // the state of importing from that file.
      progress[
        lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS
      ] = {
        inProgress: true,
        message: "",
      };

      this.sendAsyncMessage("UpdateProgress", { key: migratorKey, progress });

      let summary = await lazy.LoginCSVImport.importFromCSV(
        safariPasswordFilePath
      );
      let quantity = summary.filter(entry => entry.result == "added").length;

      progress[
        lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS
      ] = {
        inProgress: false,
        message: await lazy.gFluentStrings.formatValue(
          "migration-wizard-progress-success-passwords",
          {
            quantity,
          }
        ),
      };
    }

    this.sendAsyncMessage("UpdateProgress", { key: migratorKey, progress });

    // It's possible that only a Safari password file path was sent up, and
    // there's nothing left to migrate, in which case we're done here.
    if (safariPasswordFilePath && !resourceTypeNames.length) {
      return;
    }

    try {
      await migrator.migrate(
        resourceTypesToMigrate,
        false,
        profileObj,
        async resourceTypeNum => {
          // Unfortunately, MigratorBase hands us the the numeric value of the
          // MigrationUtils.resourceType for this callback. For now, we'll just
          // do a look-up to map it to the right constant.
          let foundResourceTypeName;
          for (let resourceTypeName in MigrationUtils.resourceTypes) {
            if (
              MigrationUtils.resourceTypes[resourceTypeName] == resourceTypeNum
            ) {
              foundResourceTypeName = resourceTypeName;
              break;
            }
          }

          if (!foundResourceTypeName) {
            console.error(
              "Could not find a resource type for value: ",
              resourceTypeNum
            );
          } else {
            // For now, we ignore errors in migration, and simply display
            // the success state.
            progress[foundResourceTypeName] = {
              inProgress: false,
              message: await this.#getStringForImportQuantity(
                migratorKey,
                foundResourceTypeName
              ),
            };
            this.sendAsyncMessage("UpdateProgress", {
              key: migratorKey,
              progress,
            });
          }
        }
      );
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * @typedef {object} MigratorProfileInstance
   *   An object that describes a single user profile (or the default
   *   user profile) for a particular migrator.
   * @property {string} key
   *   The unique identification key for a migrator.
   * @property {string} displayName
   *   The display name for the migrator that will be shown to the user
   *   in the wizard.
   * @property {string[]} resourceTypes
   *   An array of strings, where each string represents a resource type
   *   that can be imported for this migrator and profile. The strings
   *   should be one of the key values of
   *   MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
   *
   *   Example: ["HISTORY", "FORMDATA", "PASSWORDS", "BOOKMARKS"]
   * @property {object|null} profile
   *   A description of the user profile that the migrator can import.
   * @property {string} profile.id
   *   A unique ID for the user profile.
   * @property {string} profile.name
   *   The display name for the user profile.
   */

  /**
   * Asynchronously fetches a migrator for a particular key, and then
   * also gets any user profiles that exist on for that migrator. Resolves
   * to null if something goes wrong getting information about the migrator
   * or any of the user profiles.
   *
   * @param {string} key
   *   The unique identification key for a migrator.
   * @returns {Promise<MigratorProfileInstance[]|null>}
   */
  async #getMigratorAndProfiles(key) {
    try {
      let migrator = await MigrationUtils.getMigrator(key);
      if (!migrator?.enabled) {
        return null;
      }

      let sourceProfiles = await migrator.getSourceProfiles();
      if (Array.isArray(sourceProfiles)) {
        if (!sourceProfiles.length) {
          return null;
        }

        let result = [];
        for (let profile of sourceProfiles) {
          result.push(
            await this.#serializeMigratorAndProfile(migrator, profile)
          );
        }
        return result;
      }
      return this.#serializeMigratorAndProfile(migrator, sourceProfiles);
    } catch (e) {
      console.error(`Could not get migrator with key ${key}`, e);
    }
    return null;
  }

  /**
   * Asynchronously fetches information about what resource types can be
   * migrated for a particular migrator and user profile, and then packages
   * the migrator, user profile data, and resource type data into an object
   * that can be sent down to the MigrationWizardChild.
   *
   * @param {MigratorBase} migrator
   *   A migrator subclass of MigratorBase.
   * @param {object|null} profileObj
   *   The user profile object representing the profile to get information
   *   about. This object is usually gotten by calling getSourceProfiles on
   *   the migrator.
   * @returns {Promise<MigratorProfileInstance>}
   */
  async #serializeMigratorAndProfile(migrator, profileObj) {
    let [profileMigrationData, lastModifiedDate] = await Promise.all([
      migrator.getMigrateData(profileObj),
      migrator.getLastUsedDate(),
    ]);

    let availableResourceTypes = [];

    for (let resourceType in MigrationUtils.resourceTypes) {
      // Normally, we check each possible resourceType to see if we have one or
      // more corresponding resourceTypes in profileMigrationData. The exception
      // is for Safari, where the migrator does not expose a PASSWORDS resource
      // type, but we allow the user to express that they'd like to import
      // passwords from it anyways. This is because the Safari migration flow is
      // special, and allows the user to import passwords from a file exported
      // from Safari.
      if (
        profileMigrationData & MigrationUtils.resourceTypes[resourceType] ||
        (migrator.constructor.key == lazy.SafariProfileMigrator?.key &&
          MigrationUtils.resourceTypes[resourceType] ==
            MigrationUtils.resourceTypes.PASSWORDS &&
          Services.prefs.getBoolPref(
            "signon.management.page.fileImport.enabled",
            false
          ))
      ) {
        availableResourceTypes.push(resourceType);
      }
    }

    let displayName;

    if (migrator.constructor.key == lazy.InternalTestingProfileMigrator.key) {
      // In the case of the InternalTestingProfileMigrator, which is never seen
      // by users outside of testing, we don't make our localization community
      // localize it's display name, and just display the ID instead.
      displayName = migrator.constructor.displayNameL10nID;
    } else {
      displayName = await lazy.gFluentStrings.formatValue(
        migrator.constructor.displayNameL10nID
      );
    }

    return {
      type: lazy.MigrationWizardConstants.MIGRATOR_TYPES.BROWSER,
      key: migrator.constructor.key,
      displayName,
      brandImage: migrator.constructor.brandImage,
      resourceTypes: availableResourceTypes,
      profile: profileObj,
      lastModifiedDate,
    };
  }

  /**
   * Returns the "success" string for a particular resource type after
   * migration has completed.
   *
   * @param {string} migratorKey
   *   The key for the migrator being used.
   * @param {string} resourceTypeStr
   *   A string mapping to one of the key values of
   *   MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
   * @returns {Promise<string>}
   *   The success string for the resource type after migration has completed.
   */
  #getStringForImportQuantity(migratorKey, resourceTypeStr) {
    switch (resourceTypeStr) {
      case lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS: {
        let quantity = MigrationUtils.getImportedCount("bookmarks");
        let stringID = "migration-wizard-progress-success-bookmarks";

        if (
          lazy.MigrationWizardConstants.USES_FAVORITES.includes(migratorKey)
        ) {
          stringID = "migration-wizard-progress-success-favorites";
        }

        return lazy.gFluentStrings.formatValue(stringID, {
          quantity,
        });
      }
      case lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY: {
        return lazy.gFluentStrings.formatValue(
          "migration-wizard-progress-success-history",
          {
            maxAgeInDays: MigrationUtils.HISTORY_MAX_AGE_IN_DAYS,
          }
        );
      }
      case lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS: {
        let quantity = MigrationUtils.getImportedCount("logins");
        return lazy.gFluentStrings.formatValue(
          "migration-wizard-progress-success-passwords",
          {
            quantity,
          }
        );
      }
      case lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA: {
        return lazy.gFluentStrings.formatValue(
          "migration-wizard-progress-success-formdata"
        );
      }
      case lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES
        .PAYMENT_METHODS: {
        let quantity = MigrationUtils.getImportedCount("cards");
        return lazy.gFluentStrings.formatValue(
          "migration-wizard-progress-success-payment-methods",
          {
            quantity,
          }
        );
      }
      default: {
        return "";
      }
    }
  }

  /**
   * Returns a Promise that resolves to a serializable representation of a
   * FileMigrator for sending down to the MigrationWizard.
   *
   * @param {FileMigrator} fileMigrator
   *   The FileMigrator to serialize.
   * @returns {Promise<object|null>}
   *   The serializable representation of the FileMigrator, or null if the
   *   migrator is disabled.
   */
  async #serializeFileMigrator(fileMigrator) {
    if (!fileMigrator.enabled) {
      return null;
    }

    return {
      type: lazy.MigrationWizardConstants.MIGRATOR_TYPES.FILE,
      key: fileMigrator.constructor.key,
      displayName: await lazy.gFluentStrings.formatValue(
        fileMigrator.constructor.displayNameL10nID
      ),
      brandImage: fileMigrator.constructor.brandImage,
      resourceTypes: [],
    };
  }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { E10SUtils } from "resource://gre/modules/E10SUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "gFluentStrings", function () {
  return new Localization([
    "branding/brand.ftl",
    "browser/migrationWizard.ftl",
  ]);
});

ChromeUtils.defineESModuleGetters(lazy, {
  FirefoxProfileMigrator: "resource:///modules/FirefoxProfileMigrator.sys.mjs",
  InternalTestingProfileMigrator:
    "resource:///modules/InternalTestingProfileMigrator.sys.mjs",
  LoginCSVImport: "resource://gre/modules/LoginCSVImport.sys.mjs",
  MigrationWizardConstants:
    "chrome://browser/content/migration/migration-wizard-constants.mjs",
  PasswordFileMigrator: "resource:///modules/FileMigrators.sys.mjs",
});

if (AppConstants.platform == "macosx") {
  ChromeUtils.defineESModuleGetters(lazy, {
    SafariProfileMigrator: "resource:///modules/SafariProfileMigrator.sys.mjs",
  });
}

/**
 * Set to true once the first instance of MigrationWizardParent has received
 * a "GetAvailableMigrators" message.
 */
let gHasOpenedBefore = false;

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
    MigrationUtils.finishMigration();
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
        let start = Cu.now();

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

        let elapsed = Cu.now() - start;
        if (!gHasOpenedBefore) {
          gHasOpenedBefore = true;
          Services.telemetry.scalarSet(
            "migration.time_to_produce_migrator_list",
            elapsed
          );
        }

        return filteredResults;
      }

      case "Migrate": {
        let { migrationDetails, extraArgs } = message.data;
        if (
          migrationDetails.type ==
          lazy.MigrationWizardConstants.MIGRATOR_TYPES.BROWSER
        ) {
          return this.#doBrowserMigration(migrationDetails, extraArgs);
        } else if (
          migrationDetails.type ==
          lazy.MigrationWizardConstants.MIGRATOR_TYPES.FILE
        ) {
          let window = this.browsingContext.topChromeWindow;
          await this.#doFileMigration(window, migrationDetails.key);
          return extraArgs;
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

      case "OpenAboutAddons": {
        let browser = this.browsingContext.top.embedderElement;
        this.#openAboutAddons(browser);
        break;
      }

      case "GetPermissions": {
        let migrator = await MigrationUtils.getMigrator(message.data.key);
        return migrator.getPermissions(this.browsingContext.topChromeWindow);
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
        value: lazy.MigrationWizardConstants.PROGRESS_VALUE.LOADING,
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

    let migrationResult;
    try {
      migrationResult = await fileMigrator.migrate(path);
    } catch (e) {
      this.sendAsyncMessage("FileImportProgressError", {
        migratorKey: key,
        fileImportErrorMessage: e.message,
      });
      return;
    }

    let successProgress = {};
    for (let resourceType in migrationResult) {
      successProgress[resourceType] = {
        value: lazy.MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
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
   * @param {MigrationDetails} migrationDetails
   *   See migration-wizard.mjs for a definition of MigrationDetails.
   * @param {object} extraArgs
   *   Extra argument object that will be passed to the Event Telemetry for
   *   finishing the migration. This was initialized in the child actor, and
   *   will be sent back down to it to write to Telemetry once migration
   *   completes.
   *
   * @returns {Promise<object>}
   *   Resolves once the Migration:Ended observer notification has fired,
   *   passing the extraArgs for Telemetry back with any relevant properties
   *   updated.
   */
  async #doBrowserMigration(migrationDetails, extraArgs) {
    Services.telemetry
      .getHistogramById("FX_MIGRATION_SOURCE_BROWSER")
      .add(MigrationUtils.getSourceIdForTelemetry(migrationDetails.key));

    let migrator = await MigrationUtils.getMigrator(migrationDetails.key);
    let availableResourceTypes = await migrator.getMigrateData(
      migrationDetails.profile
    );
    let resourceTypesToMigrate = 0;
    let progress = {};
    let migrationUsageHist =
      Services.telemetry.getKeyedHistogramById("FX_MIGRATION_USAGE");

    for (let resourceTypeName of migrationDetails.resourceTypes) {
      let resourceType = MigrationUtils.resourceTypes[resourceTypeName];
      if (availableResourceTypes & resourceType) {
        resourceTypesToMigrate |= resourceType;
        progress[resourceTypeName] = {
          value: lazy.MigrationWizardConstants.PROGRESS_VALUE.LOADING,
          message: "",
        };

        if (!migrationDetails.autoMigration) {
          migrationUsageHist.add(migrationDetails.key, Math.log2(resourceType));
        }
      }
    }

    if (
      migrationDetails.key == lazy.SafariProfileMigrator?.key &&
      migrationDetails.safariPasswordFilePath
    ) {
      // The caller supplied a password export file for Safari. We're going to
      // pretend that there was a PASSWORDS resource for Safari to represent
      // the state of importing from that file.
      progress[
        lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS
      ] = {
        value: lazy.MigrationWizardConstants.PROGRESS_VALUE.LOADING,
        message: "",
      };

      this.sendAsyncMessage("UpdateProgress", {
        key: migrationDetails.key,
        progress,
      });

      try {
        let summary = await lazy.LoginCSVImport.importFromCSV(
          migrationDetails.safariPasswordFilePath
        );
        let quantity = summary.filter(entry => entry.result == "added").length;

        progress[
          lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS
        ] = {
          value: lazy.MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
          message: await lazy.gFluentStrings.formatValue(
            "migration-wizard-progress-success-passwords",
            {
              quantity,
            }
          ),
        };
      } catch (e) {
        progress[
          lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS
        ] = {
          value: lazy.MigrationWizardConstants.PROGRESS_VALUE.WARNING,
          message: await lazy.gFluentStrings.formatValue(
            "migration-passwords-from-file-no-valid-data"
          ),
        };
      }
    }

    this.sendAsyncMessage("UpdateProgress", {
      key: migrationDetails.key,
      progress,
    });

    // It's possible that only a Safari password file path was sent up, and
    // there's nothing left to migrate, in which case we're done here.
    if (
      migrationDetails.safariPasswordFilePath &&
      !migrationDetails.resourceTypes.length
    ) {
      return extraArgs;
    }

    try {
      await migrator.migrate(
        resourceTypesToMigrate,
        false,
        migrationDetails.profile,
        async (resourceTypeNum, success, details) => {
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
            if (!success) {
              Services.telemetry
                .getKeyedHistogramById("FX_MIGRATION_ERRORS")
                .add(migrationDetails.key, Math.log2(resourceTypeNum));
            }
            if (
              foundResourceTypeName ==
              lazy.MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.EXTENSIONS
            ) {
              if (!success) {
                // did not match any extensions
                extraArgs.extensions =
                  lazy.MigrationWizardConstants.EXTENSIONS_IMPORT_RESULT.NONE_MATCHED;
                progress[foundResourceTypeName] = {
                  value: lazy.MigrationWizardConstants.PROGRESS_VALUE.WARNING,
                  message: await lazy.gFluentStrings.formatValue(
                    "migration-wizard-progress-no-matched-extensions"
                  ),
                  linkURL: Services.urlFormatter.formatURLPref(
                    "extensions.getAddons.link.url"
                  ),
                  linkText: await lazy.gFluentStrings.formatValue(
                    "migration-wizard-progress-extensions-addons-link"
                  ),
                };
              } else if (
                details?.progressValue ==
                lazy.MigrationWizardConstants.PROGRESS_VALUE.SUCCESS
              ) {
                // did match all extensions
                extraArgs.extensions =
                  lazy.MigrationWizardConstants.EXTENSIONS_IMPORT_RESULT.ALL_MATCHED;
                progress[foundResourceTypeName] = {
                  value: lazy.MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
                  message: await lazy.gFluentStrings.formatValue(
                    "migration-wizard-progress-success-extensions",
                    {
                      quantity: details.totalExtensions.length,
                    }
                  ),
                };
              } else if (
                details?.progressValue ==
                lazy.MigrationWizardConstants.PROGRESS_VALUE.INFO
              ) {
                // did match some extensions
                extraArgs.extensions =
                  lazy.MigrationWizardConstants.EXTENSIONS_IMPORT_RESULT.PARTIAL_MATCH;
                progress[foundResourceTypeName] = {
                  value: lazy.MigrationWizardConstants.PROGRESS_VALUE.INFO,
                  message: await lazy.gFluentStrings.formatValue(
                    "migration-wizard-progress-partial-success-extensions",
                    {
                      matched: details.importedExtensions.length,
                      quantity: details.totalExtensions.length,
                    }
                  ),
                  linkURL:
                    Services.urlFormatter.formatURLPref("app.support.baseURL") +
                    "import-data-another-browser",
                  linkText: await lazy.gFluentStrings.formatValue(
                    "migration-wizard-progress-extensions-support-link"
                  ),
                };
              }
            } else {
              progress[foundResourceTypeName] = {
                value: success
                  ? lazy.MigrationWizardConstants.PROGRESS_VALUE.SUCCESS
                  : lazy.MigrationWizardConstants.PROGRESS_VALUE.WARNING,
                message: await this.#getStringForImportQuantity(
                  migrationDetails.key,
                  foundResourceTypeName
                ),
              };
            }
            this.sendAsyncMessage("UpdateProgress", {
              key: migrationDetails.key,
              progress,
            });
          }
        }
      );
    } catch (e) {
      console.error(e);
    }

    return extraArgs;
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

      if (!(await migrator.hasPermissions())) {
        // If we're unable to get permissions for this migrator, then we
        // just don't bother showing it.
        let permissionsPath = await migrator.canGetPermissions();
        if (!permissionsPath) {
          return null;
        }
        return this.#serializeMigratorAndProfile(
          migrator,
          null,
          false /* hasPermissions */,
          permissionsPath
        );
      }

      let sourceProfiles = await migrator.getSourceProfiles();
      if (Array.isArray(sourceProfiles)) {
        if (!sourceProfiles.length) {
          return null;
        }

        Services.telemetry.keyedScalarAdd(
          "migration.discovered_migrators",
          key,
          sourceProfiles.length
        );

        let result = [];
        for (let profile of sourceProfiles) {
          result.push(
            await this.#serializeMigratorAndProfile(migrator, profile)
          );
        }
        return result;
      }

      Services.telemetry.keyedScalarAdd(
        "migration.discovered_migrators",
        key,
        1
      );
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
   * @param {boolean} [hasPermissions=true]
   *   Whether or not the migrator has permission to read the data for the
   *   other browser. It is expected that the caller will have already
   *   computed this by calling hasPermissions() on the migrator, and
   *   passing the result into this method. This is true by default.
   * @param {string} [permissionsPath=undefined]
   *   The path that the selected migrator needs read access to in order to
   *   do a migration, in the event that hasPermissions is false. This is
   *   undefined if hasPermissions is true.
   * @returns {Promise<MigratorProfileInstance>}
   */
  async #serializeMigratorAndProfile(
    migrator,
    profileObj,
    hasPermissions = true,
    permissionsPath
  ) {
    let [profileMigrationData, lastModifiedDate] = await Promise.all([
      migrator.getMigrateData(profileObj),
      migrator.getLastUsedDate(),
    ]);

    let availableResourceTypes = [];

    // Even if we don't have permissions, we'll show the resources available
    // for Safari. For Safari, the workflow is to request permissions only
    // after the resources have been selected.
    if (
      hasPermissions ||
      migrator.constructor.key == lazy.SafariProfileMigrator?.key
    ) {
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
      hasPermissions,
      permissionsPath,
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
    if (migratorKey == lazy.FirefoxProfileMigrator.key) {
      return "";
    }

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

  /**
   * Opens the about:addons page in a new background tab in the same window
   * as the passed browser.
   *
   * @param {Element} browser
   *   The browser element requesting that about:addons opens.
   */
  #openAboutAddons(browser) {
    let window = browser.ownerGlobal;
    window.openTrustedLinkIn("about:addons", "tab", { inBackground: true });
  }
}

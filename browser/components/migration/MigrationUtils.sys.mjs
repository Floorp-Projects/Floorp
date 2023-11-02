/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AMBrowserExtensionsImport: "resource://gre/modules/AddonManager.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  MigrationWizardConstants:
    "chrome://browser/content/migration/migration-wizard-constants.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "gCanGetPermissionsOnPlatformPromise",
  () => {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    return fp.isModeSupported(Ci.nsIFilePicker.modeGetFolder);
  }
);

var gMigrators = null;
var gFileMigrators = null;
var gProfileStartup = null;
var gL10n = null;

let gForceExitSpinResolve = false;
let gKeepUndoData = false;
let gUndoData = null;

function getL10n() {
  if (!gL10n) {
    gL10n = new Localization(["browser/migrationWizard.ftl"]);
  }
  return gL10n;
}

const MIGRATOR_MODULES = Object.freeze({
  EdgeProfileMigrator: {
    moduleURI: "resource:///modules/EdgeProfileMigrator.sys.mjs",
    platforms: ["win"],
  },
  FirefoxProfileMigrator: {
    moduleURI: "resource:///modules/FirefoxProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
  IEProfileMigrator: {
    moduleURI: "resource:///modules/IEProfileMigrator.sys.mjs",
    platforms: ["win"],
  },
  SafariProfileMigrator: {
    moduleURI: "resource:///modules/SafariProfileMigrator.sys.mjs",
    platforms: ["macosx"],
  },

  // The following migrators are all variants of the ChromeProfileMigrator

  BraveProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
  CanaryProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["macosx", "win"],
  },
  ChromeProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
  ChromeBetaMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux", "win"],
  },
  ChromeDevMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux"],
  },
  ChromiumProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
  Chromium360seMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["win"],
  },
  ChromiumEdgeMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["macosx", "win"],
  },
  ChromiumEdgeBetaMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["macosx", "win"],
  },
  OperaProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
  VivaldiProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
  OperaGXProfileMigrator: {
    moduleURI: "resource:///modules/ChromeProfileMigrator.sys.mjs",
    platforms: ["macosx", "win"],
  },

  InternalTestingProfileMigrator: {
    moduleURI: "resource:///modules/InternalTestingProfileMigrator.sys.mjs",
    platforms: ["linux", "macosx", "win"],
  },
});

const FILE_MIGRATOR_MODULES = Object.freeze({
  PasswordFileMigrator: {
    moduleURI: "resource:///modules/FileMigrators.sys.mjs",
  },
  BookmarksFileMigrator: {
    moduleURI: "resource:///modules/FileMigrators.sys.mjs",
  },
});

/**
 * The singleton MigrationUtils service. This service is the primary mechanism
 * by which migrations from other browsers to this browser occur. The singleton
 * instance of this class is exported from this module as `MigrationUtils`.
 */
class MigrationUtils {
  constructor() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "HISTORY_MAX_AGE_IN_DAYS",
      "browser.migrate.history.maxAgeInDays",
      180
    );

    ChromeUtils.registerWindowActor("MigrationWizard", {
      parent: {
        esModuleURI: "resource:///actors/MigrationWizardParent.sys.mjs",
      },

      child: {
        esModuleURI: "resource:///actors/MigrationWizardChild.sys.mjs",
        events: {
          "MigrationWizard:RequestState": { wantUntrusted: true },
          "MigrationWizard:BeginMigration": { wantUntrusted: true },
          "MigrationWizard:RequestSafariPermissions": { wantUntrusted: true },
          "MigrationWizard:SelectSafariPasswordFile": { wantUntrusted: true },
          "MigrationWizard:OpenAboutAddons": { wantUntrusted: true },
          "MigrationWizard:PermissionsNeeded": { wantUntrusted: true },
          "MigrationWizard:GetPermissions": { wantUntrusted: true },
        },
      },

      includeChrome: true,
      allFrames: true,
      matches: [
        "about:welcome",
        "about:welcome?*",
        "about:preferences",
        "chrome://browser/content/migration/migration-dialog-window.html",
        "chrome://browser/content/spotlight.html",
        "about:firefoxview-next",
      ],
    });

    XPCOMUtils.defineLazyGetter(this, "IS_LINUX_SNAP_PACKAGE", () => {
      if (
        AppConstants.platform != "linux" ||
        !Cc["@mozilla.org/gio-service;1"]
      ) {
        return false;
      }

      let gIOSvc = Cc["@mozilla.org/gio-service;1"].getService(
        Ci.nsIGIOService
      );
      return gIOSvc.isRunningUnderSnap;
    });
  }

  resourceTypes = Object.freeze({
    ALL: 0x0000,
    /* 0x01 used to be used for settings, but was removed. */
    COOKIES: 0x0002,
    HISTORY: 0x0004,
    FORMDATA: 0x0008,
    PASSWORDS: 0x0010,
    BOOKMARKS: 0x0020,
    OTHERDATA: 0x0040,
    SESSION: 0x0080,
    PAYMENT_METHODS: 0x0100,
    EXTENSIONS: 0x0200,
  });

  /**
   * Helper for implementing simple asynchronous cases of migration resources'
   * |migrate(aCallback)| (see MigratorBase).  If your |migrate| method
   * just waits for some file to be read, for example, and then migrates
   * everything right away, you can wrap the async-function with this helper
   * and not worry about notifying the callback.
   *
   * @example
   * // For example, instead of writing:
   * setTimeout(function() {
   *   try {
   *     ....
   *     aCallback(true);
   *   }
   *   catch() {
   *     aCallback(false);
   *   }
   * }, 0);
   *
   * // You may write:
   * setTimeout(MigrationUtils.wrapMigrateFunction(function() {
   *   if (importingFromMosaic)
   *     throw Cr.NS_ERROR_UNEXPECTED;
   * }, aCallback), 0);
   *
   * // ... and aCallback will be called with aSuccess=false when importing
   * // from Mosaic, or with aSuccess=true otherwise.
   *
   * @param {Function} aFunction
   *   the function that will be called sometime later.  If aFunction
   *   throws when it's called, aCallback(false) is called, otherwise
   *   aCallback(true) is called.
   * @param {Function} aCallback
   *   the callback function passed to |migrate|.
   * @returns {Function}
   *   the wrapped function.
   */
  wrapMigrateFunction(aFunction, aCallback) {
    return function () {
      let success = false;
      try {
        aFunction.apply(null, arguments);
        success = true;
      } catch (ex) {
        console.error(ex);
      }
      // Do not change this to call aCallback directly in try try & catch
      // blocks, because if aCallback throws, we may end up calling aCallback
      // twice.
      aCallback(success);
    };
  }

  /**
   * Gets localized string corresponding to l10n-id
   *
   * @param {string} aKey
   *   The key of the id of the localization to retrieve.
   * @param {object} [aArgs=undefined]
   *   An optional map of arguments to the id.
   * @returns {Promise<string>}
   *   A promise that resolves to the retrieved localization.
   */
  getLocalizedString(aKey, aArgs) {
    let l10n = getL10n();
    return l10n.formatValue(aKey, aArgs);
  }

  /**
   * Get all the rows corresponding to a select query from a database, without
   * requiring a lock on the database. If fetching data fails (because someone
   * else tried to write to the DB at the same time, for example), we will
   * retry the fetch after a 100ms timeout, up to 10 times.
   *
   * @param {string} path
   *   The file path to the database we want to open.
   * @param {string} description
   *   A developer-readable string identifying what kind of database we're
   *   trying to open.
   * @param {string} selectQuery
   *   The SELECT query to use to fetch the rows.
   * @param {Promise} [testDelayPromise]
   *   An optional promise to await for after the first loop, used in tests.
   *
   * @returns {Promise<object[]|Error>}
   *   A promise that resolves to an array of rows. The promise will be
   *   rejected if the read/fetch failed even after retrying.
   */
  getRowsFromDBWithoutLocks(
    path,
    description,
    selectQuery,
    testDelayPromise = null
  ) {
    let dbOptions = {
      readOnly: true,
      ignoreLockingMode: true,
      path,
    };

    const RETRYLIMIT = 10;
    const RETRYINTERVAL = 100;
    return (async function innerGetRows() {
      let rows = null;
      for (let retryCount = RETRYLIMIT; retryCount; retryCount--) {
        // Attempt to get the rows. If this succeeds, we will bail out of the loop,
        // close the database in a failsafe way, and pass the rows back.
        // If fetching the rows throws, we will wait RETRYINTERVAL ms
        // and try again. This will repeat a maximum of RETRYLIMIT times.
        let db;
        let didOpen = false;
        let previousExceptionMessage = null;
        try {
          db = await lazy.Sqlite.openConnection(dbOptions);
          didOpen = true;
          rows = await db.execute(selectQuery);
          break;
        } catch (ex) {
          if (previousExceptionMessage != ex.message) {
            console.error(ex);
          }
          previousExceptionMessage = ex.message;
          if (ex.name == "NS_ERROR_FILE_CORRUPTED") {
            break;
          }
        } finally {
          try {
            if (didOpen) {
              await db.close();
            }
          } catch (ex) {}
        }
        await Promise.all([
          new Promise(resolve => lazy.setTimeout(resolve, RETRYINTERVAL)),
          testDelayPromise,
        ]);
      }
      if (!rows) {
        throw new Error(
          "Couldn't get rows from the " + description + " database."
        );
      }
      return rows;
    })();
  }

  get #migrators() {
    if (!gMigrators) {
      gMigrators = new Map();
      for (let [symbol, { moduleURI, platforms }] of Object.entries(
        MIGRATOR_MODULES
      )) {
        if (platforms.includes(AppConstants.platform)) {
          let { [symbol]: migratorClass } =
            ChromeUtils.importESModule(moduleURI);
          if (gMigrators.has(migratorClass.key)) {
            console.error(
              "A pre-existing migrator exists with key " +
                `${migratorClass.key}. Not registering.`
            );
            continue;
          }
          gMigrators.set(migratorClass.key, new migratorClass());
        }
      }
    }
    return gMigrators;
  }

  get #fileMigrators() {
    if (!gFileMigrators) {
      gFileMigrators = new Map();
      for (let [symbol, { moduleURI }] of Object.entries(
        FILE_MIGRATOR_MODULES
      )) {
        let { [symbol]: migratorClass } = ChromeUtils.importESModule(moduleURI);
        if (gFileMigrators.has(migratorClass.key)) {
          console.error(
            "A pre-existing file migrator exists with key " +
              `${migratorClass.key}. Not registering.`
          );
          continue;
        }
        gFileMigrators.set(migratorClass.key, new migratorClass());
      }
    }
    return gFileMigrators;
  }

  forceExitSpinResolve() {
    gForceExitSpinResolve = true;
  }

  spinResolve(promise) {
    if (!(promise instanceof Promise)) {
      return promise;
    }
    let done = false;
    let result = null;
    let error = null;
    gForceExitSpinResolve = false;
    promise
      .catch(e => {
        error = e;
      })
      .then(r => {
        result = r;
        done = true;
      });

    Services.tm.spinEventLoopUntil(
      "MigrationUtils.jsm:MU_spinResolve",
      () => done || gForceExitSpinResolve
    );
    if (!done) {
      throw new Error("Forcefully exited event loop.");
    } else if (error) {
      throw error;
    } else {
      return result;
    }
  }

  /**
   * Returns the migrator for the given source, if any data is available
   * for this source, or if permissions are required in order to read
   * data from this source. Returns null otherwise.
   *
   * @param {string} aKey
   *   Internal name of the migration source. See `availableMigratorKeys`
   *   for supported values by OS.
   * @returns {Promise<MigratorBase|null>}
   *   A profile migrator implementing nsIBrowserProfileMigrator, if it can
   *   import any data, null otherwise.
   */
  async getMigrator(aKey) {
    let migrator = this.#migrators.get(aKey);
    if (!migrator) {
      console.error(`Could not find a migrator class for key ${aKey}`);
      return null;
    }

    try {
      if (!migrator) {
        return null;
      }

      if (
        (await migrator.isSourceAvailable()) ||
        (!(await migrator.hasPermissions()) && migrator.canGetPermissions())
      ) {
        return migrator;
      }

      return null;
    } catch (ex) {
      console.error(ex);
      return null;
    }
  }

  getFileMigrator(aKey) {
    let migrator = this.#fileMigrators.get(aKey);
    if (!migrator) {
      console.error(`Could not find a file migrator class for key ${aKey}`);
      return null;
    }
    return migrator;
  }

  /**
   * Returns true if a migrator is registered with key aKey. No check is made
   * to determine if a profile exists that the migrator can migrate from.
   *
   * @param {string} aKey
   *   Internal name of the migration source. See `availableMigratorKeys`
   *   for supported values by OS.
   * @returns {boolean}
   */
  migratorExists(aKey) {
    return this.#migrators.has(aKey);
  }

  /**
   * Figure out what is the default browser, and if there is a migrator
   * for it, return that migrator's internal name.
   *
   * For the time being, the "internal name" of a migrator is its contract-id
   * trailer (e.g. ie for @mozilla.org/profile/migrator;1?app=browser&type=ie),
   * but it will soon be exposed properly.
   *
   * @returns {string}
   */
  getMigratorKeyForDefaultBrowser() {
    // Canary uses the same description as Chrome so we can't distinguish them.
    // Edge Beta on macOS uses "Microsoft Edge" with no "beta" indication.
    const APP_DESC_TO_KEY = {
      "Internet Explorer": "ie",
      "Microsoft Edge": "edge",
      Safari: "safari",
      Firefox: "firefox",
      Nightly: "firefox",
      Opera: "opera",
      Vivaldi: "vivaldi",
      "Opera GX": "opera-gx",
      "Brave Web Browser": "brave", // Windows, Linux
      Brave: "brave", // OS X
      "Google Chrome": "chrome", // Windows, Linux
      Chrome: "chrome", // OS X
      Chromium: "chromium", // Windows, OS X
      "Chromium Web Browser": "chromium", // Linux
      "360\u5b89\u5168\u6d4f\u89c8\u5668": "chromium-360se",
    };

    let key = "";
    try {
      let browserDesc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
        .getService(Ci.nsIExternalProtocolService)
        .getApplicationDescription("http");
      key = APP_DESC_TO_KEY[browserDesc] || "";
      // Handle devedition, as well as "FirefoxNightly" on OS X.
      if (!key && browserDesc.startsWith("Firefox")) {
        key = "firefox";
      }
    } catch (ex) {
      console.error("Could not detect default browser: ", ex);
    }

    return key;
  }

  /**
   * True if we're in the process of a startup migration.
   *
   * @type {boolean}
   */
  get isStartupMigration() {
    return gProfileStartup != null;
  }

  /**
   * In the case of startup migration, this is set to the nsIProfileStartup
   * instance passed to ProfileMigrator's migrate.
   *
   * @see showMigrationWizard
   * @type {nsIProfileStartup|null}
   */
  get profileStartup() {
    return gProfileStartup;
  }

  /**
   * Show the migration wizard in about:preferences, or if there is not an existing
   * browser window open, in a new top-level dialog window.
   *
   * NB: If you add new consumers, please add a migration entry point constant to
   * MIGRATION_ENTRYPOINTS and supply that entrypoint with the entrypoint property
   * in the aOptions argument.
   *
   * @param {Window} [aOpener=null]
   *   optional; the window that asks to open the wizard.
   * @param {object} [aOptions=null]
   *   optional named arguments for the migration wizard.
   * @param {string} [aOptions.entrypoint=undefined]
   *   migration entry point constant. See MIGRATION_ENTRYPOINTS.
   * @param {string} [aOptions.migratorKey=undefined]
   *   The key for which migrator to use automatically. This is the key that is exposed
   *   as a static getter on the migrator class.
   * @param {MigratorBase} [aOptions.migrator=undefined]
   *   A migrator instance to use automatically.
   * @param {boolean} [aOptions.isStartupMigration=undefined]
   *   True if this is a startup migration.
   * @param {boolean} [aOptions.skipSourceSelection=undefined]
   *   True if the source selection page of the wizard should be skipped.
   * @param {string} [aOptions.profileId]
   *   An identifier for the profile to use when migrating.
   * @returns {Promise<undefined>}
   *   If an about:preferences tab can be opened, this will resolve when
   *   that tab has been switched to. Otherwise, this will resolve
   *   just after opening the top-level dialog window.
   */
  showMigrationWizard(aOpener, aOptions) {
    // When migration is kicked off from about:welcome, there are
    // a few different behaviors that we want to test, controlled
    // by a preference that is instrumented for Nimbus. The pref
    // has the following possible states:
    //
    // "autoclose":
    //   The user will be directed to the migration wizard in
    //   about:preferences, but once the wizard is dismissed,
    //   the tab will close.
    //
    // "standalone":
    //   The migration wizard will open in a new top-level content
    //   window.
    //
    // "default" / other
    //   The user will be directed to the migration wizard in
    //   about:preferences. The tab will not close once the
    //   user closes the wizard.
    let aboutWelcomeBehavior = Services.prefs.getCharPref(
      "browser.migrate.content-modal.about-welcome-behavior",
      "default"
    );

    let entrypoint = aOptions.entrypoint || this.MIGRATION_ENTRYPOINTS.UNKNOWN;
    Services.telemetry
      .getHistogramById("FX_MIGRATION_ENTRY_POINT_CATEGORICAL")
      .add(entrypoint);

    let openStandaloneWindow = blocking => {
      let features = "dialog,centerscreen,resizable=no";

      if (blocking) {
        features += ",modal";
      }

      Services.ww.openWindow(
        aOpener,
        "chrome://browser/content/migration/migration-dialog-window.html",
        "_blank",
        features,
        {
          options: aOptions,
        }
      );
      return Promise.resolve();
    };

    if (aOptions.isStartupMigration) {
      // Record that the uninstaller requested a profile refresh
      if (Services.env.get("MOZ_UNINSTALLER_PROFILE_REFRESH")) {
        Services.env.set("MOZ_UNINSTALLER_PROFILE_REFRESH", "");
        Services.telemetry.scalarSet(
          "migration.uninstaller_profile_refresh",
          true
        );
      }

      openStandaloneWindow(true /* blocking */);
      return Promise.resolve();
    }

    if (aOpener?.openPreferences) {
      if (aOptions.entrypoint == this.MIGRATION_ENTRYPOINTS.NEWTAB) {
        if (aboutWelcomeBehavior == "autoclose") {
          return aOpener.openPreferences("general-migrate-autoclose");
        } else if (aboutWelcomeBehavior == "standalone") {
          openStandaloneWindow(false /* blocking */);
          return Promise.resolve();
        }
      }
      return aOpener.openPreferences("general-migrate");
    }

    // If somehow we failed to open about:preferences, fall back to opening
    // the top-level window.
    openStandaloneWindow(false /* blocking */);
    return Promise.resolve();
  }

  /**
   * Show the migration wizard for startup-migration.  This should only be
   * called by ProfileMigrator (see ProfileMigrator.js), which implements
   * nsIProfileMigrator. This runs asynchronously if we are running an
   * automigration.
   *
   * @param {nsIProfileStartup} aProfileStartup
   *   the nsIProfileStartup instance provided to ProfileMigrator.migrate.
   * @param {string|null} [aMigratorKey=null]
   *   If set, the migration wizard will import from the corresponding
   *   migrator, bypassing the source-selection page.  Otherwise, the
   *   source-selection page will be displayed, either with the default
   *   browser selected, if it could be detected and if there is a
   *   migrator for it, or with the first option selected as a fallback
   * @param {string|null} [aProfileToMigrate=null]
   *   If set, the migration wizard will import from the profile indicated.
   * @throws
   *   if aMigratorKey is invalid or if it points to a non-existent
   *   source.
   */
  startupMigration(aProfileStartup, aMigratorKey, aProfileToMigrate) {
    this.spinResolve(
      this.asyncStartupMigration(
        aProfileStartup,
        aMigratorKey,
        aProfileToMigrate
      )
    );
  }

  async asyncStartupMigration(
    aProfileStartup,
    aMigratorKey,
    aProfileToMigrate
  ) {
    if (!aProfileStartup) {
      throw new Error(
        "an profile-startup instance is required for startup-migration"
      );
    }
    gProfileStartup = aProfileStartup;

    let skipSourceSelection = false,
      migrator = null,
      migratorKey = "";
    if (aMigratorKey) {
      migrator = await this.getMigrator(aMigratorKey);
      if (!migrator) {
        // aMigratorKey must point to a valid source, so, if it doesn't
        // cleanup and throw.
        this.finishMigration();
        throw new Error(
          "startMigration was asked to open auto-migrate from " +
            "a non-existent source: " +
            aMigratorKey
        );
      }
      migratorKey = aMigratorKey;
      skipSourceSelection = true;
    } else {
      let defaultBrowserKey = this.getMigratorKeyForDefaultBrowser();
      if (defaultBrowserKey) {
        migrator = await this.getMigrator(defaultBrowserKey);
        if (migrator) {
          migratorKey = defaultBrowserKey;
        }
      }
    }

    if (!migrator) {
      let migrators = await Promise.all(
        this.availableMigratorKeys.map(key => this.getMigrator(key))
      );
      // If there's no migrator set so far, ensure that there is at least one
      // migrator available before opening the wizard.
      // Note that we don't need to check the default browser first, because
      // if that one existed we would have used it in the block above this one.
      if (!migrators.some(m => m)) {
        // None of the keys produced a usable migrator, so finish up here:
        this.finishMigration();
        return;
      }
    }

    let isRefresh =
      migrator &&
      skipSourceSelection &&
      migratorKey == AppConstants.MOZ_APP_NAME;

    let entrypoint = this.MIGRATION_ENTRYPOINTS.FIRSTRUN;
    if (isRefresh) {
      entrypoint = this.MIGRATION_ENTRYPOINTS.FXREFRESH;
    }

    this.showMigrationWizard(null, {
      entrypoint,
      migratorKey,
      migrator,
      isStartupMigration: !!aProfileStartup,
      skipSourceSelection,
      profileId: aProfileToMigrate,
    });
  }

  /**
   * This is only pseudo-private because some tests and helper functions
   * still expect to be able to directly access it.
   */
  _importQuantities = {
    bookmarks: 0,
    logins: 0,
    history: 0,
    cards: 0,
    extensions: 0,
  };

  getImportedCount(type) {
    if (!this._importQuantities.hasOwnProperty(type)) {
      throw new Error(
        `Unknown import data type "${type}" passed to getImportedCount`
      );
    }
    return this._importQuantities[type];
  }

  insertBookmarkWrapper(bookmark) {
    this._importQuantities.bookmarks++;
    let insertionPromise = lazy.PlacesUtils.bookmarks.insert(bookmark);
    if (!gKeepUndoData) {
      return insertionPromise;
    }
    // If we keep undo data, add a promise handler that stores the undo data once
    // the bookmark has been inserted in the DB, and then returns the bookmark.
    let { parentGuid } = bookmark;
    return insertionPromise.then(bm => {
      let { guid, lastModified, type } = bm;
      gUndoData.get("bookmarks").push({
        parentGuid,
        guid,
        lastModified,
        type,
      });
      return bm;
    });
  }

  insertManyBookmarksWrapper(bookmarks, parent) {
    let insertionPromise = lazy.PlacesUtils.bookmarks.insertTree({
      guid: parent,
      children: bookmarks,
    });
    return insertionPromise.then(
      insertedItems => {
        this._importQuantities.bookmarks += insertedItems.length;
        if (gKeepUndoData) {
          let bmData = gUndoData.get("bookmarks");
          for (let bm of insertedItems) {
            let { parentGuid, guid, lastModified, type } = bm;
            bmData.push({ parentGuid, guid, lastModified, type });
          }
        }
        if (parent == lazy.PlacesUtils.bookmarks.toolbarGuid) {
          lazy.PlacesUIUtils.maybeToggleBookmarkToolbarVisibility(
            true /* aForceVisible */
          ).catch(console.error);
        }
      },
      ex => console.error(ex)
    );
  }

  insertVisitsWrapper(pageInfos) {
    let now = new Date();
    // Ensure that none of the dates are in the future. If they are, rewrite
    // them to be now. This means we don't loose history entries, but they will
    // be valid for the history store.
    for (let pageInfo of pageInfos) {
      for (let visit of pageInfo.visits) {
        if (visit.date && visit.date > now) {
          visit.date = now;
        }
      }
    }
    this._importQuantities.history += pageInfos.length;
    if (gKeepUndoData) {
      this.#updateHistoryUndo(pageInfos);
    }
    return lazy.PlacesUtils.history.insertMany(pageInfos);
  }

  async insertLoginsWrapper(logins) {
    this._importQuantities.logins += logins.length;
    let inserted = await lazy.LoginHelper.maybeImportLogins(logins);
    // Note that this means that if we import a login that has a newer password
    // than we know about, we will update the login, and an undo of the import
    // will not revert this. This seems preferable over removing the login
    // outright or storing the old password in the undo file.
    if (gKeepUndoData) {
      for (let { guid, timePasswordChanged } of inserted) {
        gUndoData.get("logins").push({ guid, timePasswordChanged });
      }
    }
  }

  /**
   * Iterates through the favicons, sniffs for a mime type,
   * and uses the mime type to properly import the favicon.
   *
   * @param {object[]} favicons
   *   An array of Objects with these properties:
   *     {Uint8Array} faviconData: The binary data of a favicon
   *     {nsIURI} uri: The URI of the associated page
   */
  insertManyFavicons(favicons) {
    let sniffer = Cc["@mozilla.org/image/loader;1"].createInstance(
      Ci.nsIContentSniffer
    );
    for (let faviconDataItem of favicons) {
      let mimeType = sniffer.getMIMETypeFromContent(
        null,
        faviconDataItem.faviconData,
        faviconDataItem.faviconData.length
      );
      let fakeFaviconURI = Services.io.newURI(
        "fake-favicon-uri:" + faviconDataItem.uri.spec
      );
      lazy.PlacesUtils.favicons.replaceFaviconData(
        fakeFaviconURI,
        faviconDataItem.faviconData,
        mimeType
      );
      lazy.PlacesUtils.favicons.setAndFetchFaviconForPage(
        faviconDataItem.uri,
        fakeFaviconURI,
        true,
        lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    }
  }

  async insertCreditCardsWrapper(cards) {
    this._importQuantities.cards += cards.length;
    let { formAutofillStorage } = ChromeUtils.importESModule(
      "resource://autofill/FormAutofillStorage.sys.mjs"
    );

    await formAutofillStorage.initialize();
    for (let card of cards) {
      try {
        await formAutofillStorage.creditCards.add(card);
      } catch (e) {
        console.error("Failed to insert credit card due to error: ", e, card);
      }
    }
  }

  /**
   * Responsible for calling the AddonManager API that ultimately installs the
   * matched add-ons.
   *
   * @param {string} migratorKey a migrator key that we pass to
   *                             `AMBrowserExtensionsImport` as the "browser
   *                             identifier" used to match add-ons
   * @param {string[]} extensionIDs a list of extension IDs from another browser
   */
  async installExtensionsWrapper(migratorKey, extensionIDs) {
    const totalExtensions = extensionIDs.length;

    let importedAddonIDs = [];
    try {
      const result = await lazy.AMBrowserExtensionsImport.stageInstalls(
        migratorKey,
        extensionIDs
      );
      importedAddonIDs = result.importedAddonIDs;
    } catch (e) {
      console.error(`Failed to import extensions: ${e}`);
    }

    this._importQuantities.extensions += importedAddonIDs.length;

    if (!importedAddonIDs.length) {
      return [
        lazy.MigrationWizardConstants.PROGRESS_VALUE.WARNING,
        importedAddonIDs,
      ];
    }
    if (totalExtensions == importedAddonIDs.length) {
      return [
        lazy.MigrationWizardConstants.PROGRESS_VALUE.SUCCESS,
        importedAddonIDs,
      ];
    }
    return [
      lazy.MigrationWizardConstants.PROGRESS_VALUE.INFO,
      importedAddonIDs,
    ];
  }

  initializeUndoData() {
    gKeepUndoData = true;
    gUndoData = new Map([
      ["bookmarks", []],
      ["visits", []],
      ["logins", []],
    ]);
  }

  async #postProcessUndoData(state) {
    if (!state) {
      return state;
    }
    let bookmarkFolders = state
      .get("bookmarks")
      .filter(b => b.type == lazy.PlacesUtils.bookmarks.TYPE_FOLDER);

    let bookmarkFolderData = [];
    let bmPromises = bookmarkFolders.map(({ guid }) => {
      // Ignore bookmarks where the promise doesn't resolve (ie that are missing)
      // Also check that the bookmark fetch returns isn't null before adding it.
      return lazy.PlacesUtils.bookmarks.fetch(guid).then(
        bm => bm && bookmarkFolderData.push(bm),
        () => {}
      );
    });

    await Promise.all(bmPromises);
    let folderLMMap = new Map(
      bookmarkFolderData.map(b => [b.guid, b.lastModified])
    );
    for (let bookmark of bookmarkFolders) {
      let lastModified = folderLMMap.get(bookmark.guid);
      // If the bookmark was deleted, the map will be returning null, so check:
      if (lastModified) {
        bookmark.lastModified = lastModified;
      }
    }
    return state;
  }

  stopAndRetrieveUndoData() {
    let undoData = gUndoData;
    gUndoData = null;
    gKeepUndoData = false;
    return this.#postProcessUndoData(undoData);
  }

  #updateHistoryUndo(pageInfos) {
    let visits = gUndoData.get("visits");
    let visitMap = new Map(visits.map(v => [v.url, v]));
    for (let pageInfo of pageInfos) {
      let visitCount = pageInfo.visits.length;
      let first, last;
      if (visitCount > 1) {
        let dates = pageInfo.visits.map(v => v.date);
        first = Math.min.apply(Math, dates);
        last = Math.max.apply(Math, dates);
      } else {
        first = last = pageInfo.visits[0].date;
      }
      let url = pageInfo.url;
      if (url instanceof Ci.nsIURI) {
        url = pageInfo.url.spec;
      } else if (typeof url != "string") {
        pageInfo.url.href;
      }

      try {
        new URL(url);
      } catch (ex) {
        // This won't save and we won't need to 'undo' it, so ignore this URL.
        continue;
      }
      if (!visitMap.has(url)) {
        visitMap.set(url, { url, visitCount, first, last });
      } else {
        let currentData = visitMap.get(url);
        currentData.visitCount += visitCount;
        currentData.first = Math.min(currentData.first, first);
        currentData.last = Math.max(currentData.last, last);
      }
    }
    gUndoData.set("visits", Array.from(visitMap.values()));
  }

  /**
   * Cleans up references to migrators and nsIProfileInstance instances.
   */
  finishMigration() {
    gMigrators = null;
    gProfileStartup = null;
    gL10n = null;
  }

  get availableMigratorKeys() {
    return [...this.#migrators.keys()];
  }

  get availableFileMigrators() {
    return [...this.#fileMigrators.values()];
  }

  /**
   * Enum for the entrypoint that is being used to start migration.
   * Callers can use the MIGRATION_ENTRYPOINTS getter to use these.
   *
   * These values are what's written into the
   * FX_MIGRATION_ENTRY_POINT_CATEGORICAL histogram after a migration.
   *
   * @see MIGRATION_ENTRYPOINTS
   * @readonly
   * @enum {string}
   */
  #MIGRATION_ENTRYPOINTS_ENUM = Object.freeze({
    /** The entrypoint was not supplied */
    UNKNOWN: "unknown",

    /** Migration is occurring at startup */
    FIRSTRUN: "firstrun",

    /** Migration is occurring at after a profile refresh */
    FXREFRESH: "fxrefresh",

    /** Migration is being started from the Library window */
    PLACES: "places",

    /** Migration is being started from our password management UI */
    PASSWORDS: "passwords",

    /** Migration is being started from the default about:home/about:newtab */
    NEWTAB: "newtab",

    /** Migration is being started from the File menu */
    FILE_MENU: "file_menu",

    /** Migration is being started from the Help menu */
    HELP_MENU: "help_menu",

    /** Migration is being started from the Bookmarks Toolbar */
    BOOKMARKS_TOOLBAR: "bookmarks_toolbar",

    /** Migration is being started from about:preferences */
    PREFERENCES: "preferences",

    /** Migration is being started from about:firefoxview-next */
    FIREFOX_VIEW: "firefox_view",
  });

  /**
   * Returns an enum that should be used to record the entrypoint for
   * starting a migration.
   *
   * @returns {number}
   */
  get MIGRATION_ENTRYPOINTS() {
    return this.#MIGRATION_ENTRYPOINTS_ENUM;
  }

  /**
   * Enum for the numeric value written to the FX_MIGRATION_SOURCE_BROWSER.
   * histogram
   *
   * @see getSourceIdForTelemetry
   * @readonly
   * @enum {number}
   */
  #SOURCE_NAME_TO_ID_MAPPING_ENUM = Object.freeze({
    nothing: 1,
    firefox: 2,
    edge: 3,
    ie: 4,
    chrome: 5,
    "chrome-beta": 5,
    "chrome-dev": 5,
    chromium: 6,
    canary: 7,
    safari: 8,
    "chromium-360se": 9,
    "chromium-edge": 10,
    "chromium-edge-beta": 10,
    brave: 11,
    opera: 12,
    "opera-gx": 14,
    vivaldi: 13,
  });

  getSourceIdForTelemetry(sourceName) {
    return this.#SOURCE_NAME_TO_ID_MAPPING_ENUM[sourceName] || 0;
  }

  get HISTORY_MAX_AGE_IN_MILLISECONDS() {
    return this.HISTORY_MAX_AGE_IN_DAYS * 24 * 60 * 60 * 1000;
  }

  /**
   * Determines whether or not the underlying platform supports creating
   * native file pickers that can do folder selection, which is a
   * pre-requisite for getting read-access permissions for data from other
   * browsers that we can import from.
   *
   * @returns {Promise<boolean>}
   */
  canGetPermissionsOnPlatform() {
    return lazy.gCanGetPermissionsOnPlatformPromise;
  }
}

const MigrationUtilsSingleton = new MigrationUtils();

export { MigrationUtilsSingleton as MigrationUtils };

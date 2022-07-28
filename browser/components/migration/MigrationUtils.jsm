/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MigrationUtils", "MigratorPrototype"];

const TOPIC_WILL_IMPORT_BOOKMARKS =
  "initial-migration-will-import-default-bookmarks";
const TOPIC_DID_IMPORT_BOOKMARKS =
  "initial-migration-did-import-default-bookmarks";
const TOPIC_PLACES_DEFAULTS_FINISHED = "places-browser-init-complete";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BookmarkHTMLUtils: "resource://gre/modules/BookmarkHTMLUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  lazy,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ResponsivenessMonitor",
  "resource://gre/modules/ResponsivenessMonitor.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "Sqlite",
  "resource://gre/modules/Sqlite.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "WindowsRegistry",
  "resource://gre/modules/WindowsRegistry.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);

var gMigrators = null;
var gProfileStartup = null;
var gL10n = null;
var gPreviousDefaultBrowserKey = "";

let gForceExitSpinResolve = false;
let gKeepUndoData = false;
let gUndoData = null;

const gAvailableMigratorKeys = (function() {
  if (AppConstants.platform == "win") {
    return [
      "firefox",
      "edge",
      "ie",
      "brave",
      "chrome",
      "chromium-edge",
      "chromium-edge-beta",
      "chrome-beta",
      "chromium",
      "chromium-360se",
      "canary",
    ];
  }
  if (AppConstants.platform == "macosx") {
    return [
      "firefox",
      "safari",
      "brave",
      "chrome",
      "chromium-edge",
      "chromium-edge-beta",
      "chromium",
      "canary",
    ];
  }
  if (AppConstants.XP_UNIX) {
    return [
      "firefox",
      "brave",
      "chrome",
      "chrome-beta",
      "chrome-dev",
      "chromium",
    ];
  }
  return [];
})();

function getL10n() {
  if (!gL10n) {
    gL10n = new Localization(["browser/migration.ftl"]);
  }
  return gL10n;
}

/**
 * Shared prototype for migrators, implementing nsIBrowserProfileMigrator.
 *
 * To implement a migrator:
 * 1. Import this module.
 * 2. Create the prototype for the migrator, extending MigratorPrototype.
 *    Namely: MosaicMigrator.prototype = Object.create(MigratorPrototype);
 * 3. Set classDescription, contractID and classID for your migrator, and set
 *    NSGetFactory appropriately.
 * 4. If the migrator supports multiple profiles, override the sourceProfiles
 *    Here we default for single-profile migrator.
 * 5. Implement getResources(aProfile) (see below).
 * 6. For startup-only migrators, override |startupOnlyMigrator|.
 */
var MigratorPrototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIBrowserProfileMigrator"]),

  /**
   * OVERRIDE IF AND ONLY IF the source supports multiple profiles.
   *
   * Returns array of profile objects from which data may be imported. The object
   * should have the following keys:
   *   id - a unique string identifier for the profile
   *   name - a pretty name to display to the user in the UI
   *
   * Only profiles from which data can be imported should be listed.  Otherwise
   * the behavior of the migration wizard isn't well-defined.
   *
   * For a single-profile source (e.g. safari, ie), this returns null,
   * and not an empty array.  That is the default implementation.
   */
  getSourceProfiles() {
    return null;
  },

  /**
   * MUST BE OVERRIDDEN.
   *
   * Returns an array of "migration resources" objects for the given profile,
   * or for the "default" profile, if the migrator does not support multiple
   * profiles.
   *
   * Each migration resource should provide:
   * - a |type| getter, returning any of the migration types (see
   *   nsIBrowserProfileMigrator).
   *
   * - a |migrate| method, taking a single argument, aCallback(bool success),
   *   for migrating the data for this resource.  It may do its job
   *   synchronously or asynchronously.  Either way, it must call
   *   aCallback(bool aSuccess) when it's done.  In the case of an exception
   *   thrown from |migrate|, it's taken as if aCallback(false) is called.
   *
   *   Note: In the case of a simple asynchronous implementation, you may find
   *   MigrationUtils.wrapMigrateFunction handy for handling aCallback easily.
   *
   * For each migration type listed in nsIBrowserProfileMigrator, multiple
   * migration resources may be provided.  This practice is useful when the
   * data for a certain migration type is independently stored in few
   * locations.  For example, the mac version of Safari stores its "reading list"
   * bookmarks in a separate property list.
   *
   * Note that the importation of a particular migration type is reported as
   * successful if _any_ of its resources succeeded to import (that is, called,
   * |aCallback(true)|).  However, completion-status for a particular migration
   * type is reported to the UI only once all of its migrators have called
   * aCallback.
   *
   * @note  The returned array should only include resources from which data
   *        can be imported.  So, for example, before adding a resource for the
   *        BOOKMARKS migration type, you should check if you should check that the
   *        bookmarks file exists.
   *
   * @param aProfile
   *        The profile from which data may be imported, or an empty string
   *        in the case of a single-profile migrator.
   *        In the case of multiple-profiles migrator, it is guaranteed that
   *        aProfile is a value returned by the sourceProfiles getter (see
   *        above).
   */
  getResources: function MP_getResources(/* aProfile */) {
    throw new Error("getResources must be overridden");
  },

  /**
   * OVERRIDE in order to provide an estimate of when the last time was
   * that somebody used the browser. It is OK that this is somewhat fuzzy -
   * history may not be available (or be wiped or not present due to e.g.
   * incognito mode).
   *
   * @return a Promise that resolves to the last used date.
   *
   * @note If not overridden, the promise will resolve to the unix epoch.
   */
  getLastUsedDate() {
    return Promise.resolve(new Date(0));
  },

  /**
   * OVERRIDE IF AND ONLY IF the migrator is a startup-only migrator (For now,
   * that is just the Firefox migrator, see bug 737381).  Default: false.
   *
   * Startup-only migrators are different in two ways:
   * - they may only be used during startup.
   * - the user-profile is half baked during migration.  The folder exists,
   *   but it's only accessible through MigrationUtils.profileStartup.
   *   The migrator can call MigrationUtils.profileStartup.doStartup
   *   at any point in order to initialize the profile.
   */
  get startupOnlyMigrator() {
    return false;
  },

  /**
   * Override if the data to migrate is locked/in-use and the user should
   * probably shutdown the source browser.
   */
  get sourceLocked() {
    return false;
  },

  /**
   * DO NOT OVERRIDE - After deCOMing migration, the UI will just call
   * getResources.
   *
   * @see nsIBrowserProfileMigrator
   */
  getMigrateData: async function MP_getMigrateData(aProfile) {
    let resources = await this._getMaybeCachedResources(aProfile);
    if (!resources) {
      return 0;
    }
    let types = resources.map(r => r.type);
    return types.reduce((a, b) => {
      a |= b;
      return a;
    }, 0);
  },

  getBrowserKey: function MP_getBrowserKey() {
    return this.contractID.match(/\=([^\=]+)$/)[1];
  },

  /**
   * DO NOT OVERRIDE - After deCOMing migration, the UI will just call
   * migrate for each resource.
   *
   * @see nsIBrowserProfileMigrator
   */
  migrate: async function MP_migrate(aItems, aStartup, aProfile) {
    let resources = await this._getMaybeCachedResources(aProfile);
    if (!resources.length) {
      throw new Error("migrate called for a non-existent source");
    }

    if (aItems != Ci.nsIBrowserProfileMigrator.ALL) {
      resources = resources.filter(r => aItems & r.type);
    }

    // Used to periodically give back control to the main-thread loop.
    let unblockMainThread = function() {
      return new Promise(resolve => {
        Services.tm.dispatchToMainThread(resolve);
      });
    };

    let getHistogramIdForResourceType = (resourceType, template) => {
      if (resourceType == MigrationUtils.resourceTypes.HISTORY) {
        return template.replace("*", "HISTORY");
      }
      if (resourceType == MigrationUtils.resourceTypes.BOOKMARKS) {
        return template.replace("*", "BOOKMARKS");
      }
      if (resourceType == MigrationUtils.resourceTypes.PASSWORDS) {
        return template.replace("*", "LOGINS");
      }
      return null;
    };

    let browserKey = this.getBrowserKey();

    let maybeStartTelemetryStopwatch = resourceType => {
      let histogramId = getHistogramIdForResourceType(
        resourceType,
        "FX_MIGRATION_*_IMPORT_MS"
      );
      if (histogramId) {
        TelemetryStopwatch.startKeyed(histogramId, browserKey);
      }
      return histogramId;
    };

    let maybeStartResponsivenessMonitor = resourceType => {
      let responsivenessMonitor;
      let responsivenessHistogramId = getHistogramIdForResourceType(
        resourceType,
        "FX_MIGRATION_*_JANK_MS"
      );
      if (responsivenessHistogramId) {
        responsivenessMonitor = new lazy.ResponsivenessMonitor();
      }
      return { responsivenessMonitor, responsivenessHistogramId };
    };

    let maybeFinishResponsivenessMonitor = (
      responsivenessMonitor,
      histogramId
    ) => {
      if (responsivenessMonitor) {
        let accumulatedDelay = responsivenessMonitor.finish();
        if (histogramId) {
          try {
            Services.telemetry
              .getKeyedHistogramById(histogramId)
              .add(browserKey, accumulatedDelay);
          } catch (ex) {
            Cu.reportError(histogramId + ": " + ex);
          }
        }
      }
    };

    let collectQuantityTelemetry = () => {
      for (let resourceType of Object.keys(MigrationUtils._importQuantities)) {
        let histogramId =
          "FX_MIGRATION_" + resourceType.toUpperCase() + "_QUANTITY";
        try {
          Services.telemetry
            .getKeyedHistogramById(histogramId)
            .add(browserKey, MigrationUtils._importQuantities[resourceType]);
        } catch (ex) {
          Cu.reportError(histogramId + ": " + ex);
        }
      }
    };

    // Called either directly or through the bookmarks import callback.
    let doMigrate = async function() {
      let resourcesGroupedByItems = new Map();
      resources.forEach(function(resource) {
        if (!resourcesGroupedByItems.has(resource.type)) {
          resourcesGroupedByItems.set(resource.type, new Set());
        }
        resourcesGroupedByItems.get(resource.type).add(resource);
      });

      if (resourcesGroupedByItems.size == 0) {
        throw new Error("No items to import");
      }

      let notify = function(aMsg, aItemType) {
        Services.obs.notifyObservers(null, aMsg, aItemType);
      };

      for (let resourceType of Object.keys(MigrationUtils._importQuantities)) {
        MigrationUtils._importQuantities[resourceType] = 0;
      }
      notify("Migration:Started");
      for (let [migrationType, itemResources] of resourcesGroupedByItems) {
        notify("Migration:ItemBeforeMigrate", migrationType);

        let stopwatchHistogramId = maybeStartTelemetryStopwatch(migrationType);

        let {
          responsivenessMonitor,
          responsivenessHistogramId,
        } = maybeStartResponsivenessMonitor(migrationType);

        let itemSuccess = false;
        for (let res of itemResources) {
          let completeDeferred = lazy.PromiseUtils.defer();
          let resourceDone = function(aSuccess) {
            itemResources.delete(res);
            itemSuccess |= aSuccess;
            if (itemResources.size == 0) {
              notify(
                itemSuccess
                  ? "Migration:ItemAfterMigrate"
                  : "Migration:ItemError",
                migrationType
              );
              resourcesGroupedByItems.delete(migrationType);

              if (stopwatchHistogramId) {
                TelemetryStopwatch.finishKeyed(
                  stopwatchHistogramId,
                  browserKey
                );
              }

              maybeFinishResponsivenessMonitor(
                responsivenessMonitor,
                responsivenessHistogramId
              );

              if (resourcesGroupedByItems.size == 0) {
                collectQuantityTelemetry();
                notify("Migration:Ended");
              }
            }
            completeDeferred.resolve();
          };

          // If migrate throws, an error occurred, and the callback
          // (itemMayBeDone) might haven't been called.
          try {
            res.migrate(resourceDone);
          } catch (ex) {
            Cu.reportError(ex);
            resourceDone(false);
          }

          await completeDeferred.promise;
          await unblockMainThread();
        }
      }
    };

    if (
      MigrationUtils.isStartupMigration &&
      !this.startupOnlyMigrator &&
      Services.policies.isAllowed("defaultBookmarks")
    ) {
      MigrationUtils.profileStartup.doStartup();
      // First import the default bookmarks.
      // Note: We do not need to do so for the Firefox migrator
      // (=startupOnlyMigrator), as it just copies over the places database
      // from another profile.
      (async function() {
        // Tell nsBrowserGlue we're importing default bookmarks.
        let browserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
          Ci.nsIObserver
        );
        browserGlue.observe(null, TOPIC_WILL_IMPORT_BOOKMARKS, "");

        // Import the default bookmarks. We ignore whether or not we succeed.
        await lazy.BookmarkHTMLUtils.importFromURL(
          "chrome://browser/content/default-bookmarks.html",
          {
            replace: true,
            source: lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
          }
        ).catch(Cu.reportError);

        // We'll tell nsBrowserGlue we've imported bookmarks, but before that
        // we need to make sure we're going to know when it's finished
        // initializing places:
        let placesInitedPromise = new Promise(resolve => {
          let onPlacesInited = function() {
            Services.obs.removeObserver(
              onPlacesInited,
              TOPIC_PLACES_DEFAULTS_FINISHED
            );
            resolve();
          };
          Services.obs.addObserver(
            onPlacesInited,
            TOPIC_PLACES_DEFAULTS_FINISHED
          );
        });
        browserGlue.observe(null, TOPIC_DID_IMPORT_BOOKMARKS, "");
        await placesInitedPromise;
        doMigrate();
      })();
      return;
    }
    doMigrate();
  },

  /**
   * DO NOT OVERRIDE - After deCOMing migration, this code
   * won't be part of the migrator itself.
   *
   * @see nsIBrowserProfileMigrator
   */
  async isSourceAvailable() {
    if (this.startupOnlyMigrator && !MigrationUtils.isStartupMigration) {
      return false;
    }

    // For a single-profile source, check if any data is available.
    // For multiple-profiles source, make sure that at least one
    // profile is available.
    let exists = false;
    try {
      let profiles = await this.getSourceProfiles();
      if (!profiles) {
        let resources = await this._getMaybeCachedResources("");
        if (resources && resources.length) {
          exists = true;
        }
      } else {
        exists = !!profiles.length;
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
    return exists;
  },

  /** * PRIVATE STUFF - DO NOT OVERRIDE ***/
  _getMaybeCachedResources: async function PMB__getMaybeCachedResources(
    aProfile
  ) {
    let profileKey = aProfile ? aProfile.id : "";
    if (this._resourcesByProfile) {
      if (profileKey in this._resourcesByProfile) {
        return this._resourcesByProfile[profileKey];
      }
    } else {
      this._resourcesByProfile = {};
    }
    this._resourcesByProfile[profileKey] = await this.getResources(aProfile);
    return this._resourcesByProfile[profileKey];
  },
};

var MigrationUtils = Object.seal({
  resourceTypes: {
    COOKIES: Ci.nsIBrowserProfileMigrator.COOKIES,
    HISTORY: Ci.nsIBrowserProfileMigrator.HISTORY,
    FORMDATA: Ci.nsIBrowserProfileMigrator.FORMDATA,
    PASSWORDS: Ci.nsIBrowserProfileMigrator.PASSWORDS,
    BOOKMARKS: Ci.nsIBrowserProfileMigrator.BOOKMARKS,
    OTHERDATA: Ci.nsIBrowserProfileMigrator.OTHERDATA,
    SESSION: Ci.nsIBrowserProfileMigrator.SESSION,
  },

  /**
   * Helper for implementing simple asynchronous cases of migration resources'
   * |migrate(aCallback)| (see MigratorPrototype).  If your |migrate| method
   * just waits for some file to be read, for example, and then migrates
   * everything right away, you can wrap the async-function with this helper
   * and not worry about notifying the callback.
   *
   * For example, instead of writing:
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
   * You may write:
   * setTimeout(MigrationUtils.wrapMigrateFunction(function() {
   *   if (importingFromMosaic)
   *     throw Cr.NS_ERROR_UNEXPECTED;
   * }, aCallback), 0);
   *
   * ... and aCallback will be called with aSuccess=false when importing
   * from Mosaic, or with aSuccess=true otherwise.
   *
   * @param aFunction
   *        the function that will be called sometime later.  If aFunction
   *        throws when it's called, aCallback(false) is called, otherwise
   *        aCallback(true) is called.
   * @param aCallback
   *        the callback function passed to |migrate|.
   * @return the wrapped function.
   */
  wrapMigrateFunction: function MU_wrapMigrateFunction(aFunction, aCallback) {
    return function() {
      let success = false;
      try {
        aFunction.apply(null, arguments);
        success = true;
      } catch (ex) {
        Cu.reportError(ex);
      }
      // Do not change this to call aCallback directly in try try & catch
      // blocks, because if aCallback throws, we may end up calling aCallback
      // twice.
      aCallback(success);
    };
  },

  /**
   * Gets localized string corresponding to l10n-id
   *
   * @param aKey
   *        The key of the id of the localization to retrieve.
   * @param aArgs
   *        [optional] map of arguments to the id.
   * @return A promise that resolves to the retrieved localization.
   */
  getLocalizedString: function MU_getLocalizedString(aKey, aArgs) {
    let l10n = getL10n();
    return l10n.formatValue(aKey, aArgs);
  },

  /**
   * Get all the rows corresponding to a select query from a database, without
   * requiring a lock on the database. If fetching data fails (because someone
   * else tried to write to the DB at the same time, for example), we will
   * retry the fetch after a 100ms timeout, up to 10 times.
   *
   * @param path
   *        the file path to the database we want to open.
   * @param description
   *        a developer-readable string identifying what kind of database we're
   *        trying to open.
   * @param selectQuery
   *        the SELECT query to use to fetch the rows.
   *
   * @return a promise that resolves to an array of rows. The promise will be
   *         rejected if the read/fetch failed even after retrying.
   */
  getRowsFromDBWithoutLocks(path, description, selectQuery) {
    let dbOptions = {
      readOnly: true,
      ignoreLockingMode: true,
      path,
    };

    const RETRYLIMIT = 10;
    const RETRYINTERVAL = 100;
    return (async function innerGetRows() {
      let rows = null;
      for (let retryCount = RETRYLIMIT; retryCount && !rows; retryCount--) {
        // Attempt to get the rows. If this succeeds, we will bail out of the loop,
        // close the database in a failsafe way, and pass the rows back.
        // If fetching the rows throws, we will wait RETRYINTERVAL ms
        // and try again. This will repeat a maximum of RETRYLIMIT times.
        let db;
        let didOpen = false;
        let previousException = { message: null };
        try {
          db = await lazy.Sqlite.openConnection(dbOptions);
          didOpen = true;
          rows = await db.execute(selectQuery);
        } catch (ex) {
          if (previousException.message != ex.message) {
            Cu.reportError(ex);
          }
          previousException = ex;
        } finally {
          try {
            if (didOpen) {
              await db.close();
            }
          } catch (ex) {}
        }
        if (previousException) {
          await new Promise(resolve => lazy.setTimeout(resolve, RETRYINTERVAL));
        }
      }
      if (!rows) {
        throw new Error(
          "Couldn't get rows from the " + description + " database."
        );
      }
      return rows;
    })();
  },

  get _migrators() {
    if (!gMigrators) {
      gMigrators = new Map();
    }
    return gMigrators;
  },

  forceExitSpinResolve: function MU_forceExitSpinResolve() {
    gForceExitSpinResolve = true;
  },

  spinResolve: function MU_spinResolve(promise) {
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
  },

  /*
   * Returns the migrator for the given source, if any data is available
   * for this source, or null otherwise.
   *
   * @param aKey internal name of the migration source.
   *             See `gAvailableMigratorKeys` for supported values by OS.
   *
   * If null is returned,  either no data can be imported
   * for the given migrator, or aMigratorKey is invalid  (e.g. ie on mac,
   * or mosaic everywhere).  This method should be used rather than direct
   * getService for future compatibility (see bug 718280).
   *
   * @return profile migrator implementing nsIBrowserProfileMigrator, if it can
   *         import any data, null otherwise.
   */
  getMigrator: async function MU_getMigrator(aKey) {
    let migrator = null;
    if (this._migrators.has(aKey)) {
      migrator = this._migrators.get(aKey);
    } else {
      try {
        migrator = Cc[
          "@mozilla.org/profile/migrator;1?app=browser&type=" + aKey
        ].createInstance(Ci.nsIBrowserProfileMigrator);
      } catch (ex) {
        Cu.reportError(ex);
      }
      this._migrators.set(aKey, migrator);
    }

    try {
      return migrator && (await migrator.isSourceAvailable()) ? migrator : null;
    } catch (ex) {
      Cu.reportError(ex);
      return null;
    }
  },

  /**
   * Figure out what is the default browser, and if there is a migrator
   * for it, return that migrator's internal name.
   * For the time being, the "internal name" of a migrator is its contract-id
   * trailer (e.g. ie for @mozilla.org/profile/migrator;1?app=browser&type=ie),
   * but it will soon be exposed properly.
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
      Cu.reportError("Could not detect default browser: " + ex);
    }

    // "firefox" is the least useful entry here, and might just be because we've set
    // ourselves as the default (on Windows 7 and below). In that case, check if we
    // have a registry key that tells us where to go:
    if (
      key == "firefox" &&
      AppConstants.isPlatformAndVersionAtMost("win", "6.2")
    ) {
      // Because we remove the registry key, reading the registry key only works once.
      // We save the value for subsequent calls to avoid hard-to-trace bugs when multiple
      // consumers ask for this key.
      if (gPreviousDefaultBrowserKey) {
        key = gPreviousDefaultBrowserKey;
      } else {
        // We didn't have a saved value, so check the registry.
        const kRegPath = "Software\\Mozilla\\Firefox";
        let oldDefault = lazy.WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          kRegPath,
          "OldDefaultBrowserCommand"
        );
        if (oldDefault) {
          // Remove the key:
          lazy.WindowsRegistry.removeRegKey(
            Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
            kRegPath,
            "OldDefaultBrowserCommand"
          );
          try {
            let file = Cc["@mozilla.org/file/local;1"].createInstance(
              Ci.nsILocalFileWin
            );
            file.initWithCommandLine(oldDefault);
            key =
              APP_DESC_TO_KEY[file.getVersionInfoField("FileDescription")] ||
              key;
            // Save the value for future callers.
            gPreviousDefaultBrowserKey = key;
          } catch (ex) {
            Cu.reportError(
              "Could not convert old default browser value to description."
            );
          }
        }
      }
    }
    return key;
  },

  // Whether or not we're in the process of startup migration
  get isStartupMigration() {
    return gProfileStartup != null;
  },

  /**
   * In the case of startup migration, this is set to the nsIProfileStartup
   * instance passed to ProfileMigrator's migrate.
   *
   * @see showMigrationWizard
   */
  get profileStartup() {
    return gProfileStartup;
  },

  /**
   * Show the migration wizard.  On mac, this may just focus the wizard if it's
   * already running, in which case aOpener and aParams are ignored.
   *
   * @param {Window} [aOpener]
   *        optional; the window that asks to open the wizard.
   * @param {Array} [aParams]
   *        optional arguments for the migration wizard, in the form of an array
   *        This is passed as-is for the params argument of
   *        nsIWindowWatcher.openWindow. The array elements we expect are, in
   *        order:
   *        - {Number} migration entry point constant (see below)
   *        - {String} source browser identifier
   *        - {nsIBrowserProfileMigrator} actual migrator object
   *        - {Boolean} whether this is a startup migration
   *        - {Boolean} whether to skip the 'source' page
   *        - {String} an identifier for the profile to use when migrating
   *        NB: If you add new consumers, please add a migration entry point
   *        constant below, and specify at least the first element of the array
   *        (the migration entry point for purposes of telemetry).
   */
  showMigrationWizard: function MU_showMigrationWizard(aOpener, aParams) {
    let features = "chrome,dialog,modal,centerscreen,titlebar,resizable=no";
    if (AppConstants.platform == "macosx" && !this.isStartupMigration) {
      let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
      if (win) {
        win.focus();
        return;
      }
      // On mac, the migration wiazrd should only be modal in the case of
      // startup-migration.
      features = "centerscreen,chrome,resizable=no";
    }

    // nsIWindowWatcher doesn't deal with raw arrays, so we convert the input
    let params;
    if (Array.isArray(aParams)) {
      params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      for (let item of aParams) {
        let comtaminatedVal;
        if (item && item instanceof Ci.nsISupports) {
          comtaminatedVal = item;
        } else {
          switch (typeof item) {
            case "boolean":
              comtaminatedVal = Cc[
                "@mozilla.org/supports-PRBool;1"
              ].createInstance(Ci.nsISupportsPRBool);
              comtaminatedVal.data = item;
              break;
            case "number":
              comtaminatedVal = Cc[
                "@mozilla.org/supports-PRUint32;1"
              ].createInstance(Ci.nsISupportsPRUint32);
              comtaminatedVal.data = item;
              break;
            case "string":
              comtaminatedVal = Cc[
                "@mozilla.org/supports-cstring;1"
              ].createInstance(Ci.nsISupportsCString);
              comtaminatedVal.data = item;
              break;

            case "undefined":
            case "object":
              if (!item) {
                comtaminatedVal = null;
                break;
              }
            /* intentionally falling through to error out here for
                 non-null/undefined things: */
            default:
              throw new Error(
                "Unexpected parameter type " + typeof item + ": " + item
              );
          }
        }
        params.appendElement(comtaminatedVal);
      }
    } else {
      params = aParams;
    }

    Services.ww.openWindow(
      aOpener,
      "chrome://browser/content/migration/migration.xhtml",
      "_blank",
      features,
      params
    );
  },

  /**
   * Show the migration wizard for startup-migration.  This should only be
   * called by ProfileMigrator (see ProfileMigrator.js), which implements
   * nsIProfileMigrator. This runs asynchronously if we are running an
   * automigration.
   *
   * @param aProfileStartup
   *        the nsIProfileStartup instance provided to ProfileMigrator.migrate.
   * @param [optional] aMigratorKey
   *        If set, the migration wizard will import from the corresponding
   *        migrator, bypassing the source-selection page.  Otherwise, the
   *        source-selection page will be displayed, either with the default
   *        browser selected, if it could be detected and if there is a
   *        migrator for it, or with the first option selected as a fallback
   *        (The first option is hardcoded to be the most common browser for
   *         the OS we run on.  See migration.xhtml).
   * @param [optional] aProfileToMigrate
   *        If set, the migration wizard will import from the profile indicated.
   * @throws if aMigratorKey is invalid or if it points to a non-existent
   *         source.
   */
  startupMigration: function MU_startupMigrator(
    aProfileStartup,
    aMigratorKey,
    aProfileToMigrate
  ) {
    this.spinResolve(
      this.asyncStartupMigration(
        aProfileStartup,
        aMigratorKey,
        aProfileToMigrate
      )
    );
  },

  asyncStartupMigration: async function MU_asyncStartupMigrator(
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

    let skipSourcePage = false,
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
      skipSourcePage = true;
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
        gAvailableMigratorKeys.map(key => this.getMigrator(key))
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
      migrator && skipSourcePage && migratorKey == AppConstants.MOZ_APP_NAME;

    let migrationEntryPoint = this.MIGRATION_ENTRYPOINT_FIRSTRUN;
    if (isRefresh) {
      migrationEntryPoint = this.MIGRATION_ENTRYPOINT_FXREFRESH;
    }

    let params = [
      migrationEntryPoint,
      migratorKey,
      migrator,
      aProfileStartup,
      skipSourcePage,
      aProfileToMigrate,
    ];
    this.showMigrationWizard(null, params);
  },

  _importQuantities: {
    bookmarks: 0,
    logins: 0,
    history: 0,
  },

  getImportedCount(type) {
    if (!this._importQuantities.hasOwnProperty(type)) {
      throw new Error(
        `Unknown import data type "${type}" passed to getImportedCount`
      );
    }
    return this._importQuantities[type];
  },

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
  },

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
      },
      ex => Cu.reportError(ex)
    );
  },

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
      this._updateHistoryUndo(pageInfos);
    }
    return lazy.PlacesUtils.history.insertMany(pageInfos);
  },

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
  },

  initializeUndoData() {
    gKeepUndoData = true;
    gUndoData = new Map([
      ["bookmarks", []],
      ["visits", []],
      ["logins", []],
    ]);
  },

  async _postProcessUndoData(state) {
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
  },

  stopAndRetrieveUndoData() {
    let undoData = gUndoData;
    gUndoData = null;
    gKeepUndoData = false;
    return this._postProcessUndoData(undoData);
  },

  _updateHistoryUndo(pageInfos) {
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
  },

  /**
   * Cleans up references to migrators and nsIProfileInstance instances.
   */
  finishMigration: function MU_finishMigration() {
    gMigrators = null;
    gProfileStartup = null;
    gL10n = null;
  },

  gAvailableMigratorKeys,

  MIGRATION_ENTRYPOINT_UNKNOWN: 0,
  MIGRATION_ENTRYPOINT_FIRSTRUN: 1,
  MIGRATION_ENTRYPOINT_FXREFRESH: 2,
  MIGRATION_ENTRYPOINT_PLACES: 3,
  MIGRATION_ENTRYPOINT_PASSWORDS: 4,
  MIGRATION_ENTRYPOINT_NEWTAB: 5,
  MIGRATION_ENTRYPOINT_FILE_MENU: 6,
  MIGRATION_ENTRYPOINT_HELP_MENU: 7,
  MIGRATION_ENTRYPOINT_BOOKMARKS_TOOLBAR: 8,

  _sourceNameToIdMapping: {
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
  },
  getSourceIdForTelemetry(sourceName) {
    return this._sourceNameToIdMapping[sourceName] || 0;
  },

  /* Enum of locations where bookmarks were found in the
     source browser that we import from */
  SOURCE_BOOKMARK_ROOTS_BOOKMARKS_TOOLBAR: 1,
  SOURCE_BOOKMARK_ROOTS_BOOKMARKS_MENU: 2,
  SOURCE_BOOKMARK_ROOTS_READING_LIST: 4,
  SOURCE_BOOKMARK_ROOTS_UNFILED: 8,
});
